#include "D3D9On12LaunchProtocol.h"
#include <tlhelp32.h>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <vector>

// This executable creates and supervises one private developer game. It never
// attaches to an existing game, changes installed files, or installs a proxy.
namespace {
namespace fs=std::filesystem;
using namespace kfvr::on12;
void Require(bool okay,const char* message){if(!okay)throw std::runtime_error(std::string(message)+" (Win32="+std::to_string(GetLastError())+")");}
struct Handle {
    HANDLE h=nullptr;
    explicit Handle(HANDLE value=nullptr):h(value){}
    ~Handle(){if(h&&h!=INVALID_HANDLE_VALUE)CloseHandle(h);}
    Handle(const Handle&)=delete;Handle& operator=(const Handle&)=delete;
};
void Write(HANDLE process,DWORD at,const void* value,SIZE_T bytes){SIZE_T done=0;Require(WriteProcessMemory(process,reinterpret_cast<void*>(at),value,bytes,&done)&&done==bytes,"Write owned startup memory");}
void Read(HANDLE process,DWORD at,void* value,SIZE_T bytes){SIZE_T done=0;Require(ReadProcessMemory(process,reinterpret_cast<void*>(at),value,bytes,&done)&&done==bytes,"Read owned startup memory");}
bool ReadSnapshot(HANDLE process,DWORD at,Request& previous,DWORD& error){
    // ReadProcessMemory may modify part of its destination even when it fails.
    // Keep the last complete, identity-checked request for shutdown reporting.
    Request candidate{};SIZE_T done=0;SetLastError(ERROR_SUCCESS);
    const bool read=ReadProcessMemory(process,reinterpret_cast<void*>(at),&candidate,sizeof(candidate),&done)!=0;
    if(!read||done!=sizeof(candidate)){error=GetLastError();if(!error)error=ERROR_PARTIAL_COPY;return false;}
    Require(candidate.magic==Magic&&candidate.bytes==sizeof(Request)&&candidate.version==Version&&
        candidate.imageBase==previous.imageBase&&candidate.gateAddress==previous.gateAddress&&candidate.startupMs==previous.startupMs&&
        !std::memcmp(candidate.executable,previous.executable,sizeof(candidate.executable))&&
        !std::memcmp(candidate.renderer,previous.renderer,sizeof(candidate.renderer))&&
        !std::memcmp(candidate.bootstrap,previous.bootstrap,sizeof(candidate.bootstrap))&&
        !std::memcmp(candidate.exportName,previous.exportName,sizeof(candidate.exportName))&&
        candidate.stage>=Failed&&candidate.stage<=FactoryCreated&&
        candidate.runtimeExpectedPath[MAX_PATH-1]==0&&candidate.runtimeActualPath[MAX_PATH-1]==0,"Owned startup request identity changed");
    previous=candidate;error=ERROR_SUCCESS;return true;
}
bool SuccessfulOwnedExit(DWORD waitResult,bool exitKnown,DWORD exitCode,bool factoryObserved) noexcept {
    return waitResult==WAIT_OBJECT_0&&exitKnown&&exitCode==ERROR_SUCCESS&&factoryObserved;
}
struct OwnedChild {
    Handle process,thread,job;
    DWORD pid=0;bool beforeEntry=true;
    DWORD lastRequestReadError=0;bool readDuringNormalExit=false;
    ~OwnedChild(){
        // Only a failed pre-entry child may be terminated. Once handed off, a
        // live game is left intact if graceful close fails.
        if(beforeEntry&&process.h&&WaitForSingleObject(process.h,0)==WAIT_TIMEOUT){TerminateProcess(process.h,ERROR_PROCESS_ABORTED);WaitForSingleObject(process.h,2000);}
    }
};
std::wstring Canonical(const fs::path& path){return fs::canonical(path).wstring();}
bool Below(const fs::path& path,const fs::path& directory){
    const auto p=fs::weakly_canonical(path).wstring(),d=fs::canonical(directory).wstring()+L"\\";
    return p.size()>=d.size()&&!_wcsnicmp(p.c_str(),d.c_str(),d.size());
}
void NoExistingGame(){
    Handle snapshot(CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0));Require(snapshot.h!=INVALID_HANDLE_VALUE,"Enumerate game processes");
    PROCESSENTRY32W item{};item.dwSize=sizeof(item);
    if(Process32FirstW(snapshot.h,&item))do{Require(_wcsicmp(item.szExeFile,L"KillingFloor.exe")!=0,"Close the existing Killing Floor process before this isolated launch");}while(Process32NextW(snapshot.h,&item));
}
struct Pe {
    std::vector<BYTE> data;IMAGE_NT_HEADERS32 nt{};std::vector<IMAGE_SECTION_HEADER> sections;
    explicit Pe(const fs::path& path){
        std::ifstream f(path,std::ios::binary|std::ios::ate);Require(bool(f),"Read launch image");const auto bytes=f.tellg();
        Require(bytes>0&&bytes<64*1024*1024,"Launch image has an unsupported size");data.resize(static_cast<std::size_t>(bytes));f.seekg(0);f.read(reinterpret_cast<char*>(data.data()),bytes);Require(bool(f),"Read complete launch image");
        const auto dos=At<IMAGE_DOS_HEADER>(0);Require(dos.e_magic==IMAGE_DOS_SIGNATURE&&dos.e_lfanew>0,"Invalid DOS image header");nt=At<IMAGE_NT_HEADERS32>(static_cast<std::size_t>(dos.e_lfanew));
        Require(nt.Signature==IMAGE_NT_SIGNATURE&&nt.FileHeader.Machine==IMAGE_FILE_MACHINE_I386&&nt.OptionalHeader.Magic==IMAGE_NT_OPTIONAL_HDR32_MAGIC&&nt.FileHeader.NumberOfSections>0&&nt.FileHeader.NumberOfSections<64,"Expected an x86 PE image");
        const auto start=static_cast<std::size_t>(dos.e_lfanew)+4+sizeof(IMAGE_FILE_HEADER)+nt.FileHeader.SizeOfOptionalHeader;
        for(unsigned i=0;i<nt.FileHeader.NumberOfSections;++i)sections.push_back(At<IMAGE_SECTION_HEADER>(start+i*sizeof(IMAGE_SECTION_HEADER)));
    }
    template<class T>T At(std::size_t offset)const{Require(offset<=data.size()&&sizeof(T)<=data.size()-offset,"PE field outside file");T value{};std::memcpy(&value,data.data()+offset,sizeof(value));return value;}
    std::size_t Offset(DWORD rva,std::size_t bytes=1)const{
        for(const auto& section:sections)if(rva>=section.VirtualAddress&&rva-section.VirtualAddress<section.SizeOfRawData){
            const auto delta=rva-section.VirtualAddress;Require(bytes<=section.SizeOfRawData-delta,"PE RVA spans unbacked data");const auto at=std::size_t(section.PointerToRawData)+delta;Require(at<=data.size()&&bytes<=data.size()-at,"PE RVA outside file");return at;}
        throw std::runtime_error("PE RVA has no file backing");
    }
    std::string Text(DWORD rva)const{const auto at=Offset(rva);std::string result;for(std::size_t i=0;i<256&&at+i<data.size();++i){const char c=char(data[at+i]);if(!c)return result;result+=c;}throw std::runtime_error("Unterminated PE import name");}
    DWORD Import(const char* module,const char* name)const{
        const auto dir=nt.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
        for(DWORD index=0;index<dir.Size/sizeof(IMAGE_IMPORT_DESCRIPTOR);++index){const auto d=At<IMAGE_IMPORT_DESCRIPTOR>(Offset(dir.VirtualAddress+index*sizeof(IMAGE_IMPORT_DESCRIPTOR),sizeof(IMAGE_IMPORT_DESCRIPTOR)));if(!d.Name)break;if(_stricmp(Text(d.Name).c_str(),module))continue;
            Require(d.OriginalFirstThunk!=0,"Import lacks audited original thunk names");
            for(DWORD i=0;i<4096;++i){const auto t=At<DWORD>(Offset(d.OriginalFirstThunk+i*4,4));if(!t)break;if(!(t&IMAGE_ORDINAL_FLAG32)&&Text(t+2)==name)return d.FirstThunk+i*4;}}
        return 0;
    }
    DWORD Export(const char* name)const{
        const auto dir=nt.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];if(!dir.Size)return 0;
        const auto d=At<IMAGE_EXPORT_DIRECTORY>(Offset(dir.VirtualAddress,sizeof(IMAGE_EXPORT_DIRECTORY)));Require(d.NumberOfNames<65536,"Too many PE exports");
        for(DWORD i=0;i<d.NumberOfNames;++i)if(Text(At<DWORD>(Offset(d.AddressOfNames+i*4,4)))==name){
            const auto ordinal=At<WORD>(Offset(d.AddressOfNameOrdinals+i*2,2));Require(ordinal<d.NumberOfFunctions,"Invalid PE export ordinal");const auto rva=At<DWORD>(Offset(d.AddressOfFunctions+ordinal*4,4));
            Require(!(rva>=dir.VirtualAddress&&rva-dir.VirtualAddress<dir.Size),"Bootstrap export must not be forwarded");Offset(rva);return rva;}
        return 0;
    }
};
void ValidateImages(const fs::path& game,const fs::path& dll){
    const auto system=game/L"System",exe=system/L"KillingFloor.exe",core=system/L"Core.dll",renderer=system/L"D3D9Drv.dll";
    Require(FileHash(exe.c_str(),ExeHash)&&FileHash(core.c_str(),CoreHash)&&FileHash(renderer.c_str(),RendererHash),"Installed game/Core/renderer does not match the audited startup profile");
    const Pe e(exe),c(core),r(renderer),b(dll);
    Require(e.nt.OptionalHeader.ImageBase==ImageBase&&e.nt.OptionalHeader.AddressOfEntryPoint==EntryRva&&!(e.nt.OptionalHeader.DllCharacteristics&IMAGE_DLLCHARACTERISTICS_DYNAMIC_BASE)&&!e.nt.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_TLS].Size,"Executable startup layout changed");
    Require(!std::memcmp(e.data.data()+e.Offset(EntryRva,EntryBytes.size()),EntryBytes.data(),EntryBytes.size()),"Executable entry bytes changed");
    Require(e.Import("kernel32.dll","LoadLibraryW")==LoadLibraryWIat&&e.Import("kernel32.dll","GetProcAddress")==GetProcAddressIat&&e.Import("kernel32.dll","Sleep")==SleepIat,"Executable startup imports changed");
    Require(c.Import("kernel32.dll","LoadLibraryA")==CoreLoadA&&c.Import("kernel32.dll","LoadLibraryW")==CoreLoadW&&c.Import("kernel32.dll","LoadLibraryExW")==CoreLoadExW,"Core loader imports changed");
    Require(r.Import("d3d9.dll","Direct3DCreate9")==RendererCreateIat,"Renderer factory import changed");
    Require((b.nt.FileHeader.Characteristics&IMAGE_FILE_DLL)&&b.Export("KfvrInitializeD3D9On12")!=0,"Bootstrap DLL lacks its explicit initialization export");
}
bool SafeToken(const std::wstring& text){return !text.empty()&&std::all_of(text.begin(),text.end(),[](wchar_t c){return (c>=L'a'&&c<=L'z')||(c>=L'A'&&c<=L'Z')||(c>=L'0'&&c<=L'9')||c==L'_'||c==L'-';});}
void ValidateUrl(const fs::path& game,const std::wstring& url){
    const auto q=url.find(L'?');Require(q!=std::wstring::npos,"Private map URL options are required");const auto map=url.substr(0,q);
    Require(map.size()>7&&map.substr(0,3)==L"KF-"&&map.substr(map.size()-4)==L".rom"&&SafeToken(map.substr(0,map.size()-4)),"Only a local KF map is supported");
    Require(fs::is_regular_file(game/L"Maps"/map),"Selected local map is not installed");std::map<std::wstring,std::wstring> options;
    for(auto begin=q+1;begin<url.size();){const auto end=url.find(L'?',begin);const auto part=url.substr(begin,end==std::wstring::npos?end:end-begin);const auto eq=part.find(L'=');Require(eq!=std::wstring::npos,"Malformed private map option");Require(options.emplace(part.substr(0,eq),part.substr(eq+1)).second,"Duplicate private map option");if(end==std::wstring::npos)break;begin=end+1;}
    Require(options[L"Game"]==L"KFMod.KFGameType"&&options[L"Mutator"]==L"KFVRGameplay.KFVRDevMutator"&&options[L"Difficulty"]==L"1"&&options[L"KFVRUnattended"]==L"1","Only the explicit standalone unattended KFVR fixture is supported");
    const std::set<std::wstring> flags={L"KFVRUnattended",L"KFVRMeleeTest",L"KFVRHealTest",L"KFVRDoorTest",L"KFVRArmTest",L"KFVRNativeHands",L"KFVRTraderTest",L"KFVRGrenadeTest",L"KFVRDualTest",L"KFVRMK23Test",L"KFVRSCARTest",L"KFVRReloadTimeTest",L"KFVRPhysicalReloads",L"KFVRRendererTest"};
    for(const auto& [key,value]:options)if(key!=L"Game"&&key!=L"Mutator"&&key!=L"Difficulty")Require(flags.count(key)&&value==L"1","Unsupported or network map option");
    if(options.count(L"KFVRRendererTest"))Require(map==L"KF-Offices.rom","RendererTest requires exactly the KF-Offices map");
}
void ValidateProfile(const fs::path& game,const fs::path& engine,const fs::path& user){
    for(const auto& path:{engine,user})Require(!Below(path,game)&&path.wstring().find_first_of(L" \t\r\n\"")==std::wstring::npos,"Separate VR INIs must be outside the game and contain no spaces/quotes");
    Require(!SamePath(engine.wstring(),user.c_str()),"Engine and user profiles must differ");
    wchar_t value[256]{};GetPrivateProfileStringW(L"Engine.Engine",L"RenderDevice",L"",value,256,engine.c_str());Require(!_wcsicmp(value,L"D3D9Drv.D3D9RenderDevice"),"Separate VR profile must select the audited D3D9 renderer");
    GetPrivateProfileStringW(L"Engine.Engine",L"GameEngine",L"",value,256,engine.c_str());Require(value[0]!=0,"Separate VR engine profile is incomplete");
}
std::wstring Quote(const std::wstring& value){
    std::wstring result=L"\"";unsigned slashes=0;
    for(const auto c:value){if(c==L'\\'){++slashes;continue;}result.append(slashes*(c==L'"'?2:1),L'\\');slashes=0;if(c==L'"')result+=L'\\';result+=c;}result.append(slashes*2,L'\\');return result+L"\"";
}
// A tiny x86 entry gate executes only after Windows has finished initial DLL
// loading. No remote thread waits for a still-suspended process loader.
struct Gate {
    std::vector<BYTE> code;std::vector<std::size_t> failures;
    void Bytes(std::initializer_list<BYTE> bytes){code.insert(code.end(),bytes.begin(),bytes.end());}
    void Word(DWORD value){for(unsigned i=0;i<4;++i)code.push_back(BYTE(value>>(i*8)));}
    void Push(DWORD value){Bytes({0x68});Word(value);}
    void CallIat(DWORD address){Bytes({0xff,0x15});Word(address);}
    void FailureIfZero(){Bytes({0x85,0xc0,0x0f,0x84});failures.push_back(code.size());Word(0);}
    void Store(DWORD address,DWORD value){Bytes({0xc7,0x05});Word(address);Word(value);}
    Gate(DWORD address,DWORD argument){
        Bytes({0x9c,0x60,0x89,0xe5,0x81,0xec});Word(0x210);Bytes({0x83,0xe4,0xf0,0x0f,0xae,0x04,0x24}); // flags, registers, aligned FXSAVE
        Push(argument+offsetof(Request,bootstrap));CallIat(ImageBase+LoadLibraryWIat);FailureIfZero();
        Push(argument+offsetof(Request,exportName));Bytes({0x50});CallIat(ImageBase+GetProcAddressIat);FailureIfZero();
        Push(argument);Bytes({0xff,0xd0});FailureIfZero();
        Bytes({0x0f,0xae,0x0c,0x24,0x89,0xec,0x61,0x9d,0xe9});Word((ImageBase+EntryRva)-(address+DWORD(code.size())+4));
        const auto failure=code.size();for(const auto at:failures){const auto delta=DWORD(failure-at-4);std::memcpy(code.data()+at,&delta,4);}
        // Retain a more precise bootstrap error. The failed gate never enters
        // game code and is reclaimed only by terminating this owned child.
        Bytes({0x83,0x3d});Word(argument+offsetof(Request,error));Bytes({0x00,0x75,0x0a});Store(argument+offsetof(Request,error),GateFailure);
        Store(argument+offsetof(Request,stage),DWORD(Failed));const auto park=code.size();Push(25);CallIat(ImageBase+SleepIat);Bytes({0xe9});Word(DWORD(park-code.size()-4));
    }
};
std::string Json(const std::string& value){std::string s="\"";for(const auto c:value){if(c=='"'||c=='\\')s+='\\';if(static_cast<unsigned char>(c)<32){s+='?';continue;}s+=c;}return s+'"';}
std::string Utf8(const std::wstring& text){if(text.empty())return {};const auto n=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,text.data(),int(text.size()),nullptr,0,nullptr,nullptr);Require(n>0,"Encode report path");std::string result(std::size_t(n),'\0');Require(WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,text.data(),int(text.size()),result.data(),n,nullptr,nullptr)==n,"Encode report path");return result;}
const char* RuntimeStepName(LONG step){switch(step){
    case RuntimeNotStarted:return "not_started";case RuntimeWow64:return "query_wow64";
    case RuntimeDirectory:return "system_directory";case RuntimeLoad:return "load_library";
    case RuntimeModulePath:return "module_path";case RuntimeVerifyPath:return "verify_path";
    case RuntimeResolveOn12:return "resolve_on12";case RuntimeResolveOriginal:return "resolve_original";
    case RuntimeReady:return "ready";case RuntimeVerifyMachine:return "verify_machine";default:return "invalid";
}}
void Report(HANDLE file,const OwnedChild& child,const Request& request,bool handshake,bool restored,bool closed,const std::string& error){
    DWORD exitCode=STILL_ACTIVE;
    const bool exitKnown=child.process.h&&WaitForSingleObject(child.process.h,0)==WAIT_OBJECT_0&&GetExitCodeProcess(child.process.h,&exitCode);
    std::ostringstream s;s<<"{\"version\":"<<Version<<",\"pid\":"<<child.pid<<",\"handshake\":"<<(handshake?"true":"false")<<",\"entry_restored\":"<<(restored?"true":"false")<<",\"stage\":"<<request.stage<<",\"error_code\":"<<request.error<<",\"renderer_import_patches\":"<<request.rendererPatches<<",\"factory_calls\":"<<request.factoryCalls<<",\"factory_ex_query_hresult\":"<<request.factoryExResult;
    s<<",\"runtime_step\":"<<request.runtimeStep<<",\"runtime_step_name\":"<<Json(RuntimeStepName(request.runtimeStep))<<",\"runtime_win32_error\":"<<request.runtimeWin32Error<<",\"runtime_wow64\":"<<request.runtimeWow64<<",\"runtime_module\":"<<request.runtimeModule<<",\"runtime_on12\":"<<request.runtimeOn12<<",\"runtime_original\":"<<request.runtimeOriginal<<",\"runtime_expected_path\":"<<Json(Utf8(request.runtimeExpectedPath))<<",\"runtime_actual_path\":"<<Json(Utf8(request.runtimeActualPath));
    s<<",\"runtime_identity_verified\":"<<request.runtimeIdentityVerified<<",\"runtime_machine\":"<<request.runtimeMachine<<",\"runtime_pe_magic\":"<<request.runtimePeMagic<<",\"runtime_expected_identity\":["<<request.runtimeExpectedIdentity.volume<<','<<request.runtimeExpectedIdentity.indexHigh<<','<<request.runtimeExpectedIdentity.indexLow<<"],\"runtime_actual_identity\":["<<request.runtimeActualIdentity.volume<<','<<request.runtimeActualIdentity.indexHigh<<','<<request.runtimeActualIdentity.indexLow<<']';
    s<<",\"process_exit_known\":"<<(exitKnown?"true":"false")<<",\"process_exit_code\":";if(exitKnown)s<<exitCode;else s<<"null";
    s<<",\"last_request_read_error\":"<<child.lastRequestReadError<<",\"read_during_normal_exit\":"<<(child.readDuringNormalExit?"true":"false");
    s<<",\"closed\":"<<(closed?"true":"false")<<",\"game_files_modified\":false,\"actual_on12_device_proven\":false,\"game\":"<<Json(Utf8(request.executable))<<",\"error\":"<<Json(error)<<"}\n";
    const auto text=s.str();LARGE_INTEGER zero{};DWORD written=0;Require(SetFilePointerEx(file,zero,nullptr,FILE_BEGIN)&&WriteFile(file,text.data(),DWORD(text.size()),&written,nullptr)&&written==text.size()&&SetEndOfFile(file)&&FlushFileBuffers(file),"Write owned startup report");
}
BOOL CALLBACK CloseOwnedWindow(HWND window,LPARAM argument){DWORD pid=0;GetWindowThreadProcessId(window,&pid);if(pid==DWORD(argument)&&GetWindow(window,GW_OWNER)==nullptr)PostMessageW(window,WM_CLOSE,0,0);return TRUE;}
bool GracefulClose(const OwnedChild& child){if(!child.process.h||WaitForSingleObject(child.process.h,0)==WAIT_OBJECT_0)return true;EnumWindows(CloseOwnedWindow,LPARAM(child.pid));return WaitForSingleObject(child.process.h,8000)==WAIT_OBJECT_0;}
void SelfTest(){
    Require(Quote(L"hello")==L"\"hello\""&&Quote(L"a\\")==L"\"a\\\\\""&&Quote(L"a\"b")==L"\"a\\\"b\"","Windows command quoting regression");
    const DWORD address=0x30000000,argument=0x31000000;const Gate g(address,argument);Require(g.code.size()<4096&&g.failures.size()==3,"Gate layout regression");
    DWORD target=0;for(const auto at:g.failures){DWORD delta=0;std::memcpy(&delta,g.code.data()+at,4);if(!target)target=DWORD(at)+4+delta;Require(DWORD(at)+4+delta==target&&target<g.code.size(),"Gate failures do not share a bounded-launch park");}
    Require(g.code[0]==0x9c&&g.code[1]==0x60&&g.code[target]==0x83,"Gate save/failure layout regression");
    const FileIdentity identity{7,11,13};Require(SameFileIdentity(identity,identity)&&!SameFileIdentity(identity,{8,11,13})&&!SameFileIdentity(identity,{7,12,13})&&!SameFileIdentity(identity,{7,11,14})&&!SameFileIdentity({},{}),"File identity must reject a different volume/file or absent identifier");
    BOOL wow64=FALSE;Require(IsWow64Process(GetCurrentProcess(),&wow64)!=0,"Self-test WOW64 query");
    wchar_t expectedDirectory[MAX_PATH]{},aliasDirectory[MAX_PATH]{};
    const auto expectedLength=wow64?GetSystemWow64DirectoryW(expectedDirectory,MAX_PATH):GetSystemDirectoryW(expectedDirectory,MAX_PATH);
    const auto aliasLength=GetSystemDirectoryW(aliasDirectory,MAX_PATH);Require(expectedLength&&expectedLength<MAX_PATH&&aliasLength&&aliasLength<MAX_PATH,"Self-test system directories");
    FileIdentity expected{},alias{},different{};DWORD identityError=0;
    Require(ReadFileIdentity((fs::path(expectedDirectory)/L"d3d9.dll").c_str(),expected,identityError)&&ReadFileIdentity((fs::path(aliasDirectory)/L"d3d9.dll").c_str(),alias,identityError)&&SameFileIdentity(expected,alias),"x86 system path aliases must resolve to the same file");
    Require(ReadFileIdentity((fs::path(expectedDirectory)/L"kernel32.dll").c_str(),different,identityError)&&!SameFileIdentity(expected,different),"Runtime identity must reject a different system DLL");
    Require(SuccessfulOwnedExit(WAIT_OBJECT_0,true,0,true)&&
        !SuccessfulOwnedExit(WAIT_TIMEOUT,true,0,true)&&!SuccessfulOwnedExit(WAIT_FAILED,true,0,true)&&
        !SuccessfulOwnedExit(WAIT_OBJECT_0,false,0,true)&&!SuccessfulOwnedExit(WAIT_OBJECT_0,true,0xc0000005u,true)&&
        !SuccessfulOwnedExit(WAIT_OBJECT_0,true,STILL_ACTIVE,true)&&!SuccessfulOwnedExit(WAIT_OBJECT_0,true,0,false),
        "Only a signaled successful owned exit after factory creation can resolve a failed poll");
    SYSTEM_INFO info{};GetSystemInfo(&info);Require(info.dwPageSize>=sizeof(Request),"Self-test request must fit one page");
    struct LocalPages {void* p=nullptr;~LocalPages(){if(p)VirtualFree(p,0,MEM_RELEASE);}} pages;
    pages.p=VirtualAlloc(nullptr,SIZE_T(info.dwPageSize)*2,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);Require(pages.p!=nullptr,"Allocate read-preservation test pages");
    auto* boundary=static_cast<BYTE*>(pages.p)+info.dwPageSize-sizeof(Request)/2;
    Request incoming{};incoming.stage=FactoryCreated;incoming.factoryCalls=7;std::memcpy(boundary,&incoming,sizeof(incoming));
    DWORD protection=0;Require(VirtualProtect(static_cast<BYTE*>(pages.p)+info.dwPageSize,info.dwPageSize,PAGE_NOACCESS,&protection),"Protect second test page");
    Request last=incoming;last.factoryCalls=13;DWORD readError=0;
    Require(!ReadSnapshot(GetCurrentProcess(),DWORD(reinterpret_cast<std::uintptr_t>(boundary)),last,readError)&&readError!=0&&last.factoryCalls==13&&last.stage==FactoryCreated,"Failed or partial reads must preserve the last validated request");
    Require(VirtualProtect(static_cast<BYTE*>(pages.p)+info.dwPageSize,info.dwPageSize,PAGE_READWRITE,&protection),"Restore second test page");
    Require(ReadSnapshot(GetCurrentProcess(),DWORD(reinterpret_cast<std::uintptr_t>(boundary)),last,readError)&&readError==0&&last.factoryCalls==7,"Complete valid reads update the request");
    std::cout<<"{\"gate_layout\":true,\"quoting\":true,\"runtime_file_identity\":true,\"owned_exit_decision\":true,\"partial_read_preservation\":true,\"game_launched\":false}\n";
}
}

int wmain(int argc,wchar_t** argv){
    static_assert(sizeof(void*)==4);OwnedChild child;Handle report;Request snapshot{};bool handshake=false,restored=false;DWORD requestAddress=0;
    try{
        if(argc==2&&std::wstring(argv[1])==L"--self-test"){SelfTest();return 0;}
        Require(argc==15,"Usage: KFVRD3D9On12Launch --game folder --dll bootstrap --url private-map --engine-ini separate-ini --user-ini separate-ini --report new-json --seconds 15..180");
        std::map<std::wstring,std::wstring> options;for(int i=1;i<argc;i+=2)Require(options.emplace(argv[i],argv[i+1]).second,"Duplicate launcher argument");
        for(const auto* name:{L"--game",L"--dll",L"--url",L"--engine-ini",L"--user-ini",L"--report",L"--seconds"})Require(options.count(name)==1,"Missing launcher argument");
        std::size_t used=0;const int seconds=std::stoi(options[L"--seconds"],&used);Require(used==options[L"--seconds"].size()&&seconds>=15&&seconds<=180,"Session duration must be 15..180 seconds");
        const fs::path game=Canonical(options[L"--game"]),dll=Canonical(options[L"--dll"]),engine=Canonical(options[L"--engine-ini"]),user=Canonical(options[L"--user-ini"]),output=fs::weakly_canonical(fs::absolute(options[L"--report"]));
        Require(fs::is_regular_file(dll)&&fs::is_regular_file(engine)&&fs::is_regular_file(user)&&!Below(dll,game)&&!Below(output,game)&&fs::is_directory(output.parent_path()),"Inputs must exist; bootstrap/report must remain outside the game installation");
        const auto exe=Canonical(game/L"System"/L"KillingFloor.exe"),renderer=Canonical(game/L"System"/L"D3D9Drv.dll");
        for(const auto& path:{exe,renderer,dll.wstring()})Require(path.size()<MAX_PATH,"Startup paths exceed the audited request limit");
        ValidateImages(game,dll);ValidateUrl(game,options[L"--url"]);ValidateProfile(game,engine,user);NoExistingGame();
        report.h=CreateFileW(output.c_str(),GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);Require(report.h!=INVALID_HANDLE_VALUE,"Report must be a new file in an existing directory");
        wcscpy_s(snapshot.executable,exe.c_str());wcscpy_s(snapshot.renderer,renderer.c_str());wcscpy_s(snapshot.bootstrap,dll.c_str());
        std::wstring command=Quote(exe)+L" "+options[L"--url"]+L" -windowed ini="+engine.wstring()+L" userini="+user.wstring();
        STARTUPINFOW startup{};startup.cb=sizeof(startup);startup.dwFlags=STARTF_USESHOWWINDOW;startup.wShowWindow=SW_HIDE;PROCESS_INFORMATION process{};
        Require(CreateProcessW(exe.c_str(),command.data(),nullptr,nullptr,FALSE,CREATE_SUSPENDED,nullptr,(game/L"System").c_str(),&startup,&process)!=0,"Create owned suspended game");
        child.process.h=process.hProcess;child.thread.h=process.hThread;child.pid=process.dwProcessId;
        child.job.h=CreateJobObjectW(nullptr,nullptr);Require(child.job.h!=nullptr,"Create startup-only ownership guard");JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};limits.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE|JOB_OBJECT_LIMIT_SILENT_BREAKAWAY_OK;
        Require(SetInformationJobObject(child.job.h,JobObjectExtendedLimitInformation,&limits,sizeof(limits))&&AssignProcessToJobObject(child.job.h,child.process.h),"Guard the newly owned pre-entry child");
        wchar_t actual[MAX_PATH]{};DWORD chars=MAX_PATH;Require(QueryFullProcessImageNameW(child.process.h,0,actual,&chars)&&SamePath(exe,actual),"Owned child image identity changed");
        std::array<BYTE,16> entry{};Read(child.process.h,ImageBase+EntryRva,entry.data(),entry.size());Require(entry==EntryBytes,"Suspended executable entry differs from audited bytes");
        requestAddress=DWORD(reinterpret_cast<std::uintptr_t>(VirtualAllocEx(child.process.h,nullptr,4096,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE)));snapshot.gateAddress=DWORD(reinterpret_cast<std::uintptr_t>(VirtualAllocEx(child.process.h,nullptr,4096,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE)));
        Require(requestAddress&&snapshot.gateAddress,"Allocate owned entry gate/request");const Gate gate(snapshot.gateAddress,requestAddress);Require(gate.code.size()<4096,"Entry gate exceeds one page");Write(child.process.h,requestAddress,&snapshot,sizeof(snapshot));Write(child.process.h,snapshot.gateAddress,gate.code.data(),gate.code.size());
        DWORD old=0;Require(VirtualProtectEx(child.process.h,reinterpret_cast<void*>(snapshot.gateAddress),4096,PAGE_EXECUTE_READ,&old)&&FlushInstructionCache(child.process.h,reinterpret_cast<void*>(snapshot.gateAddress),gate.code.size()),"Make entry gate executable");
        BYTE patch[5]={0xe9};const DWORD delta=snapshot.gateAddress-(ImageBase+EntryRva+5);std::memcpy(patch+1,&delta,4);
        Require(VirtualProtectEx(child.process.h,reinterpret_cast<void*>(ImageBase+EntryRva),sizeof(patch),PAGE_EXECUTE_READWRITE,&old),"Open owned entry patch");Write(child.process.h,ImageBase+EntryRva,patch,sizeof(patch));DWORD discarded=0;Require(VirtualProtectEx(child.process.h,reinterpret_cast<void*>(ImageBase+EntryRva),sizeof(patch),old,&discarded)&&FlushInstructionCache(child.process.h,reinterpret_cast<void*>(ImageBase+EntryRva),sizeof(patch)),"Restore entry protection");
        Report(report.h,child,snapshot,false,false,false,"");Require(ResumeThread(child.thread.h)==1,"Resume loader to the owned entry gate");
        const auto startupAt=GetTickCount64();
        while(true){Require(WaitForSingleObject(child.process.h,0)==WAIT_TIMEOUT,"Game exited during bootstrap");DWORD readError=0;if(!ReadSnapshot(child.process.h,requestAddress,snapshot,readError)){child.lastRequestReadError=readError;SetLastError(readError);Require(false,"Read owned startup request");}Require(snapshot.stage!=Failed,"Bootstrap rejected the audited startup");if(snapshot.stage==Ready)break;Require(GetTickCount64()-startupAt<snapshot.startupMs,"Bootstrap did not become ready within its bound");Sleep(10);}
        Read(child.process.h,ImageBase+EntryRva,entry.data(),entry.size());Require(entry==EntryBytes&&snapshot.error==None,"Bootstrap failed to restore the original entry");restored=true;handshake=true;
        // The successful game is no longer subject to job-close termination.
        // Descendants also stay outside this startup job.
        limits.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_SILENT_BREAKAWAY_OK;Require(SetInformationJobObject(child.job.h,JobObjectExtendedLimitInformation,&limits,sizeof(limits)),"Release startup-only kill guard");
        const LONG proceed=Continue;Write(child.process.h,requestAddress+offsetof(Request,command),&proceed,sizeof(proceed));child.beforeEntry=false;
        const auto sessionAt=GetTickCount64();auto lastReport=ULONGLONG(0);bool factory=false;
        while(GetTickCount64()-sessionAt<ULONGLONG(seconds)*1000){
            const auto wait=WaitForSingleObject(child.process.h,0);
            if(wait==WAIT_OBJECT_0){DWORD exitCode=STILL_ACTIVE;const bool exitKnown=GetExitCodeProcess(child.process.h,&exitCode)!=0;Require(SuccessfulOwnedExit(wait,exitKnown,exitCode,factory),"Owned game exited unsuccessfully or before factory creation");break;}
            Require(wait==WAIT_TIMEOUT,"Inspect owned game lifetime");
            DWORD readError=0;
            if(!ReadSnapshot(child.process.h,requestAddress,snapshot,readError)){
                child.lastRequestReadError=readError;
                // Teardown can unmap the request immediately before signaling
                // the process. This bounded wait is used only after a failed
                // read; it cannot convert a still-live failure into success.
                const auto stopped=WaitForSingleObject(child.process.h,200);DWORD exitCode=STILL_ACTIVE;
                const bool exitKnown=stopped==WAIT_OBJECT_0&&GetExitCodeProcess(child.process.h,&exitCode);
                if(SuccessfulOwnedExit(stopped,exitKnown,exitCode,factory)){child.readDuringNormalExit=true;break;}
                SetLastError(readError);Require(false,"Read owned session request");
            }
            Require(snapshot.stage!=Failed,"On12 bootstrap/factory reported failure");factory=factory||snapshot.stage==FactoryCreated;
            Require(factory||GetTickCount64()-sessionAt<30000,"Renderer did not reach the audited On12 factory within 30 seconds");
            if(GetTickCount64()-lastReport>=1000){Report(report.h,child,snapshot,handshake,restored,false,"");lastReport=GetTickCount64();}Sleep(20);
        }
        const bool closed=GracefulClose(child);Require(factory&&snapshot.rendererPatches>0,"Game never requested the intercepted renderer factory");
        if(closed){DWORD exitCode=STILL_ACTIVE;const bool exitKnown=GetExitCodeProcess(child.process.h,&exitCode)!=0;Require(SuccessfulOwnedExit(WaitForSingleObject(child.process.h,0),exitKnown,exitCode,factory),"Owned game exit code was not successful");}
        Report(report.h,child,snapshot,handshake,restored,closed,closed?"":"Owned game did not close gracefully; left running");
        std::cout<<"{\"pid\":"<<child.pid<<",\"handshake\":true,\"factory_calls\":"<<snapshot.factoryCalls<<",\"closed\":"<<(closed?"true":"false")<<",\"actual_on12_device_proven\":false}\n";return closed?0:2;
    }catch(const std::exception& error){
        bool closed=!child.process.h||WaitForSingleObject(child.process.h,0)==WAIT_OBJECT_0;
        if(child.process.h&&!closed){if(child.beforeEntry){TerminateProcess(child.process.h,ERROR_PROCESS_ABORTED);closed=WaitForSingleObject(child.process.h,2000)==WAIT_OBJECT_0;}else closed=GracefulClose(child);}
        if(report.h&&report.h!=INVALID_HANDLE_VALUE)try{Report(report.h,child,snapshot,handshake,restored,closed,error.what());}catch(...){}
        std::cerr<<error.what()<<'\n';return 1;
    }
}
