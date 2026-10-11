#pragma once
#include <Windows.h>
#include <bcrypt.h>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

namespace kfvr::on12 {
constexpr DWORD Magic=0x3231394b,Version=3;
constexpr DWORD ImageBase=0x10900000,EntryRva=0x18a20;
constexpr DWORD LoadLibraryWIat=0x1b3c8,GetProcAddressIat=0x1b3c4,SleepIat=0x1b41c;
constexpr DWORD CoreLoadA=0x72044,CoreLoadW=0x72048,CoreLoadExW=0x72058;
constexpr DWORD RendererCreateIat=0x25544,RendererThunk=0x221f0;
constexpr char ExeHash[]="e4110d7c2b4971a764fc2ff0af457fff362aaf38d38f16f58e1cac5cbfb9876e";
constexpr char CoreHash[]="570c6edc97a61d3f8ae8096e30b1ec8860d1239d05f4f532e65965cd1660953c";
constexpr char RendererHash[]="13e554b730f246d5361742348eb1c1409cf5068d027374121ee4d3d61cb79690";
constexpr std::array<BYTE,16> EntryBytes={0x6a,0x74,0x68,0x98,0x2f,0x92,0x10,0xe8,0xd8,0x03,0,0,0x33,0xdb,0x89,0x5d};
enum Stage:LONG {Allocated=0,Entered=1,Ready=2,Running=3,FactoryCreated=4,Failed=-1};
enum Command:LONG {Wait=0,Continue=1,Abort=2};
enum Error:LONG {None=0,BadRequest=1,WrongImage=2,WrongCore=3,MissingRuntime=4,
    ImportMismatch=5,PatchFailed=6,HandshakeTimeout=7,WrongRenderer=8,FactoryFailed=9,EntryMismatch=10,GateFailure=11};
enum RuntimeStep:LONG {RuntimeNotStarted=0,RuntimeWow64=1,RuntimeDirectory=2,
    RuntimeLoad=3,RuntimeModulePath=4,RuntimeVerifyPath=5,RuntimeResolveOn12=6,
    RuntimeResolveOriginal=7,RuntimeReady=8,RuntimeVerifyMachine=9};
struct FileIdentity {DWORD volume=0,indexHigh=0,indexLow=0;};
#pragma pack(push,4)
struct Request {
    DWORD magic=Magic,bytes=sizeof(Request),version=Version;
    volatile LONG stage=Allocated,command=Wait,error=None;
    DWORD imageBase=ImageBase,gateAddress=0,startupMs=12000;
    volatile LONG factoryCalls=0,rendererPatches=0,factoryExResult=0;
    wchar_t executable[MAX_PATH]{},renderer[MAX_PATH]{},bootstrap[MAX_PATH]{};
    char exportName[32]="KfvrInitializeD3D9On12";
    // Startup diagnosis is separate from the launcher thread's GetLastError.
    volatile LONG runtimeStep=RuntimeNotStarted;
    DWORD runtimeWin32Error=0,runtimeWow64=0,runtimeModule=0;
    DWORD runtimeOn12=0,runtimeOriginal=0;
    wchar_t runtimeExpectedPath[MAX_PATH]{},runtimeActualPath[MAX_PATH]{};
    DWORD runtimeIdentityVerified=0,runtimeMachine=0,runtimePeMagic=0;
    FileIdentity runtimeExpectedIdentity{},runtimeActualIdentity{};
};
#pragma pack(pop)
static_assert(sizeof(Request)<4096 && offsetof(Request,stage)==12);

inline bool FileHash(const wchar_t* path,const char* expected) noexcept {
    HANDLE file=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE)return false;
    BCRYPT_ALG_HANDLE algorithm=nullptr;BCRYPT_HASH_HANDLE hash=nullptr;
    bool okay=BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)>=0;
    if(okay)okay=BCryptCreateHash(algorithm,&hash,nullptr,0,nullptr,0,0)>=0;
    std::array<BYTE,65536> buffer{};DWORD got=0;
    while(okay){if(!ReadFile(file,buffer.data(),DWORD(buffer.size()),&got,nullptr)){okay=false;break;}if(!got)break;okay=BCryptHashData(hash,buffer.data(),got,0)>=0;}
    std::array<BYTE,32> digest{};if(okay)okay=BCryptFinishHash(hash,digest.data(),DWORD(digest.size()),0)>=0;
    if(hash)BCryptDestroyHash(hash);if(algorithm)BCryptCloseAlgorithmProvider(algorithm,0);CloseHandle(file);
    constexpr char hex[]="0123456789abcdef";
    if(okay)for(unsigned i=0;i<digest.size();++i)if(hex[digest[i]>>4]!=expected[i*2]||hex[digest[i]&15]!=expected[i*2+1])return false;
    return okay;
}
inline bool ReadLocal(const void* source,void* destination,SIZE_T size) noexcept {
    SIZE_T read=0;return ReadProcessMemory(GetCurrentProcess(),source,destination,size,&read)&&read==size;
}
inline bool ReadFileIdentity(const wchar_t* path,FileIdentity& identity,DWORD& error) noexcept {
    error=ERROR_SUCCESS;
    // Keep normal per-process WOW64 redirection: System32 and SysWOW64 can
    // name the same x86 image. Directory-name equality cannot establish this.
    HANDLE file=CreateFileW(path,FILE_READ_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE){error=GetLastError();return false;}
    BY_HANDLE_FILE_INFORMATION info{};const bool okay=GetFileInformationByHandle(file,&info)!=0;
    if(!okay)error=GetLastError();CloseHandle(file);
    if(!okay)return false;
    if(info.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY){error=ERROR_DIRECTORY;return false;}
    identity={info.dwVolumeSerialNumber,info.nFileIndexHigh,info.nFileIndexLow};return true;
}
inline bool SameFileIdentity(const FileIdentity& a,const FileIdentity& b) noexcept {
    return (a.indexHigh||a.indexLow)&&a.volume==b.volume&&a.indexHigh==b.indexHigh&&a.indexLow==b.indexLow;
}
inline std::wstring ModulePath(HMODULE module){
    wchar_t path[MAX_PATH]{};const auto n=GetModuleFileNameW(module,path,MAX_PATH);
    return n&&n<MAX_PATH?std::wstring(path,n):std::wstring{};
}
inline bool SamePath(const std::wstring& a,const wchar_t* b) noexcept {return !a.empty()&&!_wcsicmp(a.c_str(),b);}
}
