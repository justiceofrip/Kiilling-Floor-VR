#include "D3D9On12LaunchProtocol.h"
#include <d3d9on12.h>
#include <algorithm>
#include <filesystem>

namespace {
using namespace kfvr::on12;
using LoadA=HMODULE(WINAPI*)(LPCSTR);
using LoadW=HMODULE(WINAPI*)(LPCWSTR);
using LoadExW=HMODULE(WINAPI*)(LPCWSTR,HANDLE,DWORD);
Request* request=nullptr;
HMODULE runtime=nullptr;
PFN_Direct3DCreate9On12 createOn12=nullptr;
FARPROC createOriginal=nullptr;
LoadA loadA=nullptr;LoadW loadW=nullptr;LoadExW loadExW=nullptr;
struct Patch {void** slot=nullptr;void* original=nullptr;void* replacement=nullptr;bool active=false;};
std::array<Patch,3> loaderPatches{};
SRWLOCK patchLock=SRWLOCK_INIT;

void Fail(Error error) noexcept {if(request){InterlockedCompareExchange(&request->error,error,None);InterlockedExchange(&request->stage,Failed);}}
void RuntimeFailure(DWORD error) noexcept {request->runtimeWin32Error=error;Fail(MissingRuntime);}
bool Exchange(Patch& patch,void** slot,void* expected,void* replacement) noexcept {
    DWORD protection=0;if(!VirtualProtect(slot,sizeof(void*),PAGE_READWRITE,&protection))return false;
    const bool exchanged=InterlockedCompareExchangePointer(slot,replacement,expected)==expected;
    DWORD discarded=0;const bool restored=VirtualProtect(slot,sizeof(void*),protection,&discarded)!=0;
    if(exchanged)patch={slot,expected,replacement,true};
    return exchanged&&restored;
}
void UndoLoaders() noexcept {
    for(auto& patch:loaderPatches)if(patch.active){
        DWORD protection=0;if(VirtualProtect(patch.slot,sizeof(void*),PAGE_READWRITE,&protection)){
            InterlockedCompareExchangePointer(patch.slot,patch.original,patch.replacement);
            DWORD discarded=0;VirtualProtect(patch.slot,sizeof(void*),protection,&discarded);patch.active=false;
        }
    }
}
bool RelocatedCode(HMODULE module,DWORD rva,const BYTE* expected,SIZE_T size,unsigned relocation,DWORD preferred) noexcept {
    std::array<BYTE,16> wanted{},actual{};if(size>wanted.size())return false;
    std::memcpy(wanted.data(),expected,size);DWORD value=0;std::memcpy(&value,wanted.data()+relocation,4);
    value+=DWORD(reinterpret_cast<std::uintptr_t>(module))-preferred;std::memcpy(wanted.data()+relocation,&value,4);
    return ReadLocal(reinterpret_cast<BYTE*>(module)+rva,actual.data(),size)&&!std::memcmp(actual.data(),wanted.data(),size);
}
IDirect3D9* WINAPI Create9(UINT sdk){
    if(!request||request->stage<Running||request->stage==Failed||sdk!=D3D_SDK_VERSION){Fail(FactoryFailed);return nullptr;}
    InterlockedIncrement(&request->factoryCalls);
    D3D9ON12_ARGS args{};args.Enable9On12=TRUE;
    auto* result=createOn12(sdk,&args,1);
    if(!result){Fail(FactoryFailed);return nullptr;}
    // This is the non-Ex creation entry. Report its actual Ex query result;
    // a non-null factory is not evidence of a successfully created GPU device.
    IDirect3D9Ex* ex=nullptr;const auto hr=result->QueryInterface(__uuidof(IDirect3D9Ex),reinterpret_cast<void**>(&ex));
    InterlockedExchange(&request->factoryExResult,hr);if(ex)ex->Release();
    InterlockedExchange(&request->stage,FactoryCreated);return result;
}
bool InspectRenderer(HMODULE module) noexcept {try {
    if(!module)return true;
    const auto path=ModulePath(module);const auto filename=std::filesystem::path(path).filename().wstring();
    if(_wcsicmp(filename.c_str(),L"D3D9Drv.dll"))return true;
    if(!SamePath(path,request->renderer)||!FileHash(path.c_str(),RendererHash)){Fail(WrongRenderer);return false;}
    const BYTE prologue[]={0x55,0x8b,0xec,0x6a,0xff,0x68,0x11,0x3a,0x02,0x10,0x64,0xa1,0,0,0,0};
    if(!RelocatedCode(module,0x14520,prologue,sizeof(prologue),6,0x10000000)){Fail(WrongRenderer);return false;}
    const auto base=reinterpret_cast<BYTE*>(module);std::array<BYTE,6> thunk{};
    if(!ReadLocal(base+RendererThunk,thunk.data(),thunk.size())||thunk[0]!=0xff||thunk[1]!=0x25){Fail(ImportMismatch);return false;}
    DWORD target=0;std::memcpy(&target,thunk.data()+2,4);
    if(target!=DWORD(reinterpret_cast<std::uintptr_t>(base+RendererCreateIat))){Fail(ImportMismatch);return false;}
    AcquireSRWLockExclusive(&patchLock);
    auto** slot=reinterpret_cast<void**>(base+RendererCreateIat);void* current=nullptr;
    bool okay=ReadLocal(slot,&current,sizeof(current));
    if(okay&&current==reinterpret_cast<void*>(&Create9)){ReleaseSRWLockExclusive(&patchLock);return true;}
    Patch patch;
    okay=okay&&current==reinterpret_cast<void*>(createOriginal)&&Exchange(patch,slot,current,reinterpret_cast<void*>(&Create9));
    if(okay)InterlockedIncrement(&request->rendererPatches);
    ReleaseSRWLockExclusive(&patchLock);
    if(!okay)Fail(ImportMismatch);return okay;
}catch(...){Fail(WrongRenderer);return false;}}
HMODULE WINAPI LoadLibraryAIntercept(LPCSTR name){const auto module=loadA(name);const auto error=GetLastError();if(module&&!InspectRenderer(module)){SetLastError(ERROR_BAD_EXE_FORMAT);return nullptr;}SetLastError(error);return module;}
HMODULE WINAPI LoadLibraryWIntercept(LPCWSTR name){const auto module=loadW(name);const auto error=GetLastError();if(module&&!InspectRenderer(module)){SetLastError(ERROR_BAD_EXE_FORMAT);return nullptr;}SetLastError(error);return module;}
HMODULE WINAPI LoadLibraryExWIntercept(LPCWSTR name,HANDLE file,DWORD flags){const auto module=loadExW(name,file,flags);const auto error=GetLastError();if(module&&!(flags&(DONT_RESOLVE_DLL_REFERENCES|LOAD_LIBRARY_AS_DATAFILE|LOAD_LIBRARY_AS_IMAGE_RESOURCE|LOAD_LIBRARY_AS_DATAFILE_EXCLUSIVE))&&!InspectRenderer(module)){SetLastError(ERROR_BAD_EXE_FORMAT);return nullptr;}SetLastError(error);return module;}
bool RestoreEntry() noexcept {
    auto* entry=reinterpret_cast<BYTE*>(request->imageBase+EntryRva);auto expected=EntryBytes;expected[0]=0xe9;
    const DWORD displacement=request->gateAddress-(request->imageBase+EntryRva+5);std::memcpy(expected.data()+1,&displacement,4);
    std::array<BYTE,16> actual{};if(!ReadLocal(entry,actual.data(),actual.size())||actual!=expected)return false;
    DWORD protection=0;if(!VirtualProtect(entry,5,PAGE_EXECUTE_READWRITE,&protection))return false;
    std::memcpy(entry,EntryBytes.data(),5);FlushInstructionCache(GetCurrentProcess(),entry,5);
    DWORD discarded=0;return VirtualProtect(entry,5,protection,&discarded)!=0;
}
}

extern "C" __declspec(dllexport) DWORD WINAPI KfvrInitializeD3D9On12(void* argument){
    using namespace kfvr::on12;
    static_assert(sizeof(void*)==4);
    if(request)return 0;
    auto* r=static_cast<Request*>(argument);Request header{};
    if(!ReadLocal(r,&header,sizeof(header))||header.magic!=Magic||header.bytes!=sizeof(Request)||header.version!=Version||
       header.imageBase!=ImageBase||header.startupMs<1000||header.startupMs>15000||!header.gateAddress||
       header.executable[MAX_PATH-1]||header.renderer[MAX_PATH-1]||header.bootstrap[MAX_PATH-1])return 0;
    request=r;InterlockedExchange(&r->stage,Entered);
    try {
        const auto image=GetModuleHandleW(nullptr);
        if(reinterpret_cast<std::uintptr_t>(image)!=r->imageBase||!SamePath(ModulePath(image),r->executable)||!FileHash(r->executable,ExeHash)){Fail(WrongImage);return 0;}
        const auto core=GetModuleHandleW(L"Core.dll");const auto corePath=std::filesystem::path(r->executable).parent_path()/L"Core.dll";
        const BYTE corePrologue[]={0x55,0x8b,0xec,0x6a,0xff,0x68,0x1b,0x90,0x16,0x10,0x64,0xa1,0,0,0,0};
        if(!core||!SamePath(ModulePath(core),corePath.c_str())||!FileHash(corePath.c_str(),CoreHash)||!RelocatedCode(core,0x62790,corePrologue,sizeof(corePrologue),6,0x10100000)){Fail(WrongCore);return 0;}
        InterlockedExchange(&r->runtimeStep,RuntimeWow64);SetLastError(ERROR_SUCCESS);
        BOOL wow64=FALSE;if(!IsWow64Process(GetCurrentProcess(),&wow64)){RuntimeFailure(GetLastError());return 0;}r->runtimeWow64=wow64?1u:0u;
        InterlockedExchange(&r->runtimeStep,RuntimeDirectory);SetLastError(ERROR_SUCCESS);
        wchar_t system[MAX_PATH]{};const auto systemLength=wow64?GetSystemWow64DirectoryW(system,MAX_PATH):GetSystemDirectoryW(system,MAX_PATH);
        if(!systemLength||systemLength>=MAX_PATH){RuntimeFailure(GetLastError());return 0;}
        const auto systemD3D=std::filesystem::path(system)/L"d3d9.dll";
        if(systemD3D.wstring().size()>=MAX_PATH){RuntimeFailure(ERROR_FILENAME_EXCED_RANGE);return 0;}
        wcscpy_s(r->runtimeExpectedPath,systemD3D.c_str());
        InterlockedExchange(&r->runtimeStep,RuntimeLoad);SetLastError(ERROR_SUCCESS);
        runtime=LoadLibraryExW(systemD3D.c_str(),nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
        if(!runtime){RuntimeFailure(GetLastError());return 0;}r->runtimeModule=DWORD(reinterpret_cast<std::uintptr_t>(runtime));
        InterlockedExchange(&r->runtimeStep,RuntimeModulePath);SetLastError(ERROR_SUCCESS);
        const auto actualLength=GetModuleFileNameW(runtime,r->runtimeActualPath,MAX_PATH);
        if(!actualLength||actualLength>=MAX_PATH){r->runtimeActualPath[MAX_PATH-1]=0;RuntimeFailure(GetLastError());return 0;}
        InterlockedExchange(&r->runtimeStep,RuntimeVerifyPath);
        DWORD identityError=0;
        if(!ReadFileIdentity(r->runtimeExpectedPath,r->runtimeExpectedIdentity,identityError)||
           !ReadFileIdentity(r->runtimeActualPath,r->runtimeActualIdentity,identityError)){RuntimeFailure(identityError);return 0;}
        if(!SameFileIdentity(r->runtimeExpectedIdentity,r->runtimeActualIdentity)){RuntimeFailure(ERROR_INVALID_DATA);return 0;}
        r->runtimeIdentityVerified=1;
        InterlockedExchange(&r->runtimeStep,RuntimeVerifyMachine);
        IMAGE_DOS_HEADER dos{};IMAGE_NT_HEADERS32 nt{};
        if(!ReadLocal(runtime,&dos,sizeof(dos))||dos.e_magic!=IMAGE_DOS_SIGNATURE||dos.e_lfanew<=0||dos.e_lfanew>0x100000||
           !ReadLocal(reinterpret_cast<BYTE*>(runtime)+dos.e_lfanew,&nt,sizeof(nt))||nt.Signature!=IMAGE_NT_SIGNATURE){RuntimeFailure(ERROR_BAD_EXE_FORMAT);return 0;}
        r->runtimeMachine=nt.FileHeader.Machine;r->runtimePeMagic=nt.OptionalHeader.Magic;
        if(r->runtimeMachine!=IMAGE_FILE_MACHINE_I386||r->runtimePeMagic!=IMAGE_NT_OPTIONAL_HDR32_MAGIC){RuntimeFailure(ERROR_BAD_EXE_FORMAT);return 0;}
        InterlockedExchange(&r->runtimeStep,RuntimeResolveOn12);SetLastError(ERROR_SUCCESS);
        createOn12=reinterpret_cast<PFN_Direct3DCreate9On12>(GetProcAddress(runtime,"Direct3DCreate9On12"));
        if(!createOn12){RuntimeFailure(GetLastError());return 0;}r->runtimeOn12=DWORD(reinterpret_cast<std::uintptr_t>(createOn12));
        InterlockedExchange(&r->runtimeStep,RuntimeResolveOriginal);SetLastError(ERROR_SUCCESS);
        createOriginal=GetProcAddress(runtime,"Direct3DCreate9");
        if(!createOriginal){RuntimeFailure(GetLastError());return 0;}r->runtimeOriginal=DWORD(reinterpret_cast<std::uintptr_t>(createOriginal));
        InterlockedExchange(&r->runtimeStep,RuntimeReady);
        const auto kernel=GetModuleHandleW(L"kernel32.dll");
        loadA=reinterpret_cast<LoadA>(GetProcAddress(kernel,"LoadLibraryA"));loadW=reinterpret_cast<LoadW>(GetProcAddress(kernel,"LoadLibraryW"));loadExW=reinterpret_cast<LoadExW>(GetProcAddress(kernel,"LoadLibraryExW"));
        auto* base=reinterpret_cast<BYTE*>(core);
        bool okay=loadA&&loadW&&loadExW;
        if(okay)okay=Exchange(loaderPatches[0],reinterpret_cast<void**>(base+CoreLoadA),reinterpret_cast<void*>(loadA),reinterpret_cast<void*>(&LoadLibraryAIntercept));
        if(okay)okay=Exchange(loaderPatches[1],reinterpret_cast<void**>(base+CoreLoadW),reinterpret_cast<void*>(loadW),reinterpret_cast<void*>(&LoadLibraryWIntercept));
        if(okay)okay=Exchange(loaderPatches[2],reinterpret_cast<void**>(base+CoreLoadExW),reinterpret_cast<void*>(loadExW),reinterpret_cast<void*>(&LoadLibraryExWIntercept));
        // An already loaded renderer could already own a device. Do not treat
        // patching its factory now as proof that startup preceded device creation.
        if(okay&&GetModuleHandleW(L"D3D9Drv.dll")){UndoLoaders();Fail(WrongRenderer);return 0;}
        if(!okay){UndoLoaders();Fail(ImportMismatch);return 0;}
        if(!RestoreEntry()){UndoLoaders();Fail(EntryMismatch);return 0;}
        InterlockedExchange(&r->stage,Ready);
        const auto start=GetTickCount64();
        while(InterlockedCompareExchange(&r->command,Wait,Wait)==Wait&&GetTickCount64()-start<r->startupMs)Sleep(1);
        if(r->command!=Continue){UndoLoaders();Fail(HandshakeTimeout);return 0;}
        InterlockedExchange(&r->stage,Running);return 1;
    }catch(...){UndoLoaders();Fail(BadRequest);return 0;}
}
BOOL WINAPI DllMain(HINSTANCE instance,DWORD reason,void*){
    if(reason==DLL_PROCESS_ATTACH)DisableThreadLibraryCalls(instance);
    // No loading, hook installation, waiting or GPU work under the loader lock.
    // Hooks and this module intentionally remain valid until the owned process exits.
    return TRUE;
}
