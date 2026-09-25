#include <Windows.h>
#include <tlhelp32.h>
#include "NativeProbeConfig.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
namespace fs=std::filesystem;
namespace {
void Require(bool okay,const char* message){if(!okay)throw std::runtime_error(std::string(message)+" (Win32="+std::to_string(GetLastError())+")");}
struct Handle {HANDLE h=nullptr;~Handle(){if(h&&h!=INVALID_HANDLE_VALUE)CloseHandle(h);}};
struct Allocation {HANDLE process;void* pointer=nullptr;bool release=true;~Allocation(){if(pointer&&release)VirtualFreeEx(process,pointer,0,MEM_RELEASE);}Allocation(HANDLE p,std::size_t size):process(p){pointer=VirtualAllocEx(p,nullptr,size,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);Require(pointer!=nullptr,"Allocate probe request");}};
void Write(HANDLE process,void* at,const void* data,std::size_t size){SIZE_T written=0;Require(WriteProcessMemory(process,at,data,size,&written)&&written==size,"Write bootstrap request");}
std::wstring ProcessPath(HANDLE process){wchar_t path[32768]{};DWORD count=32768;Require(QueryFullProcessImageNameW(process,0,path,&count)!=0,"Query target path");return fs::path(path).lexically_normal().wstring();}
DWORD FindProcess(const fs::path& path){Handle snapshot{CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0)};Require(snapshot.h!=INVALID_HANDLE_VALUE,"Enumerate processes");PROCESSENTRY32W item{};item.dwSize=sizeof(item);DWORD found=0;
    if(Process32FirstW(snapshot.h,&item))do{if(_wcsicmp(item.szExeFile,L"KillingFloor.exe"))continue;Handle process{OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,item.th32ProcessID)};
        if(process.h&&!_wcsicmp(ProcessPath(process.h).c_str(),path.c_str())){Require(!found,"More than one matching game process");found=item.th32ProcessID;}}
    while(Process32NextW(snapshot.h,&item));Require(found!=0,"KFVR must already be running");return found;
}
std::uintptr_t RemoteFunction(DWORD pid,FARPROC function){
    HMODULE owner=nullptr;Require(GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,reinterpret_cast<LPCWSTR>(function),&owner)!=0,"Resolve bootstrap code owner");
    wchar_t path[32768]{};Require(GetModuleFileNameW(owner,path,32768)>0,"Bootstrap module path");const auto rva=reinterpret_cast<std::uintptr_t>(function)-reinterpret_cast<std::uintptr_t>(owner);
    Handle snapshot{CreateToolhelp32Snapshot(TH32CS_SNAPMODULE,pid)};Require(snapshot.h!=INVALID_HANDLE_VALUE,"Enumerate target modules");MODULEENTRY32W item{};item.dwSize=sizeof(item);
    if(Module32FirstW(snapshot.h,&item))do{if(!_wcsicmp(item.szExePath,path)&&rva<item.modBaseSize)return reinterpret_cast<std::uintptr_t>(item.modBaseAddr)+rva;}while(Module32NextW(snapshot.h,&item));
    throw std::runtime_error("Matching 32-bit bootstrap module not found");
}
DWORD Run(HANDLE process,std::uintptr_t function,Allocation& memory,DWORD timeout){
    Handle thread{CreateRemoteThread(process,nullptr,0,reinterpret_cast<LPTHREAD_START_ROUTINE>(function),memory.pointer,0,nullptr)};Require(thread.h!=nullptr,"Start bounded probe entry");
    // If the remote operation times out, its argument must remain valid until
    // the game exits. Never terminate a game thread or free memory under it.
    memory.release=false;Require(WaitForSingleObject(thread.h,timeout)==WAIT_OBJECT_0,"Probe operation did not finish within its budget");memory.release=true;
    DWORD result=0;Require(GetExitCodeThread(thread.h,&result)!=0,"Read probe result");return result;
}
}
int wmain(int argc,wchar_t** argv){try {
    static_assert(sizeof(void*)==4);Require(argc==7&&std::wstring(argv[1])==L"--game"&&std::wstring(argv[3])==L"--dll"&&std::wstring(argv[5])==L"--request","Usage: KFVRNativeTrace --game folder --dll absolute-dll --request profile-bin");
    const auto game=(fs::absolute(argv[2])/L"System"/L"KillingFloor.exe").lexically_normal(),dll=fs::absolute(argv[4]).lexically_normal(),requestPath=fs::absolute(argv[6]).lexically_normal();
    Require(fs::is_regular_file(dll)&&fs::is_regular_file(requestPath),"Probe DLL missing or report already exists");DWORD type=0;Require(GetBinaryTypeW(game.c_str(),&type)&&type==SCS_32BIT_BINARY,"Expected the installed 32-bit game");
    const auto pid=FindProcess(game);Handle process{OpenProcess(PROCESS_QUERY_INFORMATION|PROCESS_VM_READ|PROCESS_VM_WRITE|PROCESS_VM_OPERATION|PROCESS_CREATE_THREAD|SYNCHRONIZE,FALSE,pid)};Require(process.h!=nullptr,"Open authorized game process");Require(!_wcsicmp(ProcessPath(process.h).c_str(),game.c_str()),"Target process identity changed");
    kfvr::native::NativeProbeConfig config{};std::ifstream profile(requestPath,std::ios::binary);profile.read(reinterpret_cast<char*>(&config),sizeof(config));Require(profile.gcount()==sizeof(config)&&profile.peek()==std::char_traits<char>::eof()&&config.bytes==sizeof(config),"Invalid native request");Require(config.version==3&&config.targetTrial<=1&&config.durationMs>=100&&config.durationMs<=15000&&config.maxFrames>0&&config.maxFrames<=240&&config.reportPath[0]&&config.reportPath[259]==0,"Unsupported or unbounded native request");Require(!fs::exists(config.reportPath),"Report must be a new file");
    const auto local=LoadLibraryExW(dll.c_str(),nullptr,DONT_RESOLVE_DLL_REFERENCES);Require(local!=nullptr,"Inspect probe DLL exports");const auto entry=GetProcAddress(local,"KfvrRunNativeProbe");
    const auto entryRva=entry?reinterpret_cast<std::uintptr_t>(entry)-reinterpret_cast<std::uintptr_t>(local):0;FreeLibrary(local);Require(entryRva!=0,"Probe export missing");
    const auto loader=GetProcAddress(GetModuleHandleW(L"kernel32.dll"),"LoadLibraryW");Require(loader!=nullptr,"LoadLibrary entry unavailable");const auto remoteLoader=RemoteFunction(pid,loader);
    const auto dllText=dll.wstring();Allocation pathMemory(process.h,(dllText.size()+1)*sizeof(wchar_t));Write(process.h,pathMemory.pointer,dllText.c_str(),(dllText.size()+1)*sizeof(wchar_t));
    const auto module=Run(process.h,remoteLoader,pathMemory,8000);Require(module!=0,"Game did not load the probe module");
    Allocation request(process.h,sizeof(config));Write(process.h,request.pointer,&config,sizeof(config));const auto result=Run(process.h,std::uintptr_t(module)+entryRva,request,config.durationMs+8000);
    std::cout<<"{\"pid\":"<<pid<<",\"probe_exit\":"<<result<<",\"module_retained_until_game_exit\":true,\"game_files_modified\":false}\n";return result?2:0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
