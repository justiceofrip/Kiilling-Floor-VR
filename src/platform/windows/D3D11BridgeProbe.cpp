#include <Windows.h>
#include <dxgi1_2.h>
#include <wrl/client.h>
#include "kfvr/graphics/D3D11SharedPair.h"
#include <array>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace kfvr::graphics;
using Microsoft::WRL::ComPtr;
namespace {
void Require(bool okay,const char* message){if(!okay)throw std::runtime_error(message);}
void Hr(HRESULT hr,const char* message){if(FAILED(hr))throw std::runtime_error(std::string(message)+" HRESULT="+std::to_string(std::uint32_t(hr)));}
struct Handle {
    HANDLE value=nullptr;
    ~Handle(){if(value&&value!=INVALID_HANDLE_VALUE)CloseHandle(value);}
    Handle()=default;explicit Handle(HANDLE h):value(h){}
    Handle(const Handle&)=delete;Handle& operator=(const Handle&)=delete;
    void Close(){if(value&&value!=INVALID_HANDLE_VALUE)CloseHandle(value);value=nullptr;}
};
struct Device {
    ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> context;
    explicit Device(const TextureDescriptor* match=nullptr){
        ComPtr<IDXGIFactory1> factory;Hr(CreateDXGIFactory1(IID_PPV_ARGS(&factory)),"DXGI factory");
        ComPtr<IDXGIAdapter1> selected;
        for(UINT index=0;;++index){
            ComPtr<IDXGIAdapter1> candidate;const auto hr=factory->EnumAdapters1(index,&candidate);
            if(hr==DXGI_ERROR_NOT_FOUND)break;Hr(hr,"Enumerate adapter");DXGI_ADAPTER_DESC1 d{};Hr(candidate->GetDesc1(&d),"Adapter descriptor");
            if(match?(d.AdapterLuid.LowPart==match->adapterLow&&d.AdapterLuid.HighPart==match->adapterHigh):!(d.Flags&DXGI_ADAPTER_FLAG_SOFTWARE)){
                selected=candidate;break;
            }
        }
        Require(bool(selected),"Matching hardware adapter missing");
        D3D_FEATURE_LEVEL level{};const D3D_FEATURE_LEVEL levels[]={D3D_FEATURE_LEVEL_11_0};
        Hr(D3D11CreateDevice(selected.Get(),D3D_DRIVER_TYPE_UNKNOWN,nullptr,D3D11_CREATE_DEVICE_BGRA_SUPPORT,levels,1,D3D11_SDK_VERSION,&device,&level,&context),"Create independent D3D11 device");
    }
};
// Test subresource 1 in each array layer, ensuring the bridge handles mip/slice
// indexing rather than accidentally copying only subresource zero.
struct Images {
    ComPtr<ID3D11Texture2D> texture;
    std::array<TextureSlice,2> slices{};
    Images(Device& d,UINT width,UINT height,DXGI_FORMAT format){
        D3D11_TEXTURE2D_DESC desc{};desc.Width=width*2;desc.Height=height*2;desc.MipLevels=2;desc.ArraySize=2;
        desc.Format=format;desc.SampleDesc.Count=1;desc.Usage=D3D11_USAGE_DEFAULT;desc.BindFlags=D3D11_BIND_SHADER_RESOURCE|D3D11_BIND_RENDER_TARGET;
        Hr(d.device->CreateTexture2D(&desc,nullptr,&texture),"Create local eye array");slices={TextureSlice{texture.Get(),1},TextureSlice{texture.Get(),3}};
    }
};
std::vector<std::uint32_t> Pattern(UINT width,UINT height,std::uint64_t frame,unsigned eye){
    std::vector<std::uint32_t> pixels(std::size_t(width)*height);
    for(UINT y=0;y<height;++y)for(UINT x=0;x<width;++x)pixels[std::size_t(y)*width+x]=
        0xff000000u | (UINT(frame*41+eye*93+x)&255) | ((y*13+eye*61)<<8) | ((x+y*2+UINT(frame)*19)<<16);
    return pixels;
}
void Fill(Device& d,Images& images,UINT width,UINT height,std::uint64_t frame){
    for(unsigned eye=0;eye<2;++eye){const auto pixels=Pattern(width,height,frame,eye);d.context->UpdateSubresource(images.texture.Get(),images.slices[eye].subresource,nullptr,pixels.data(),width*4,0);}
}
void VerifyPixels(Device& d,Images& images,const TextureDescriptor& wire,std::uint64_t frame){
    D3D11_TEXTURE2D_DESC desc{};desc.Width=wire.width;desc.Height=wire.height;desc.MipLevels=desc.ArraySize=1;
    desc.Format=DXGI_FORMAT(wire.format);desc.SampleDesc.Count=1;desc.Usage=D3D11_USAGE_STAGING;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
    ComPtr<ID3D11Texture2D> staging;Hr(d.device->CreateTexture2D(&desc,nullptr,&staging),"Create verification readback");
    for(unsigned eye=0;eye<2;++eye){
        d.context->CopySubresourceRegion(staging.Get(),0,0,0,0,images.texture.Get(),images.slices[eye].subresource,nullptr);
        D3D11_MAPPED_SUBRESOURCE mapped{};Hr(d.context->Map(staging.Get(),0,D3D11_MAP_READ,0,&mapped),"Read verification pixels");
        const auto expected=Pattern(wire.width,wire.height,frame,eye);bool equal=true;
        for(UINT y=0;y<wire.height;++y)equal&=std::memcmp(static_cast<const char*>(mapped.pData)+std::size_t(y)*mapped.RowPitch,expected.data()+std::size_t(y)*wire.width,wire.width*4)==0;
        d.context->Unmap(staging.Get(),0);Require(equal,"Transferred eye pixels differ");
    }
}
struct Request {TextureDescriptor descriptor;PairTicket ticket;};
struct Reply {std::uint32_t magic=0x31505246,bytes=16,status=0,pointerBits=sizeof(void*)*8;};
static_assert(sizeof(Request)==160 && sizeof(Reply)==16);
bool ReadExact(HANDLE pipe,void* buffer,DWORD bytes){
    auto* p=static_cast<char*>(buffer);while(bytes){DWORD count=0;if(!ReadFile(pipe,p,bytes,&count,nullptr)||!count)return false;p+=count;bytes-=count;}return true;
}
bool WriteExact(HANDLE pipe,const void* buffer,DWORD bytes){
    auto* p=static_cast<const char*>(buffer);while(bytes){DWORD count=0;if(!WriteFile(pipe,p,bytes,&count,nullptr)||!count)return false;p+=count;bytes-=count;}return true;
}
int Consume(){
    Reply reply{};
    try {
        Request request{};Require(ReadExact(GetStdHandle(STD_INPUT_HANDLE),&request,sizeof(request)),"Read probe request");
        Require(Valid(request.descriptor)&&Valid(request.ticket,request.descriptor),"Wire protocol mismatch");
        Device device(&request.descriptor);const auto format=request.descriptor.format;
        const auto backing=format==29?DXGI_FORMAT_R8G8B8A8_TYPELESS:format==91?DXGI_FORMAT_B8G8R8A8_TYPELESS:DXGI_FORMAT(format);
        Images images(device,request.descriptor.width,request.descriptor.height,backing);
        D3D11PairConsumer consumer;auto bad=request.descriptor;bad.adapterLow^=1;
        Require(consumer.Open(device.device.Get(),bad)==TransferResult::Invalid,"Reject adapter mismatch");
        bad=request.descriptor;bad.width++;
        Require(consumer.Open(device.device.Get(),bad)==TransferResult::Invalid,"Reject mismatched shared texture dimensions");
        Require(consumer.Open(device.device.Get(),request.descriptor)==TransferResult::Ok,"Open producer resources");
        auto ticket=request.ticket;ticket.resourceEpoch++;
        Require(consumer.Copy(ticket,images.slices)==TransferResult::Invalid,"Reject stale epoch");
        ticket=request.ticket;ticket.session[0]^=1;
        Require(consumer.Copy(ticket,images.slices)==TransferResult::Invalid,"Reject wrong session");
        ticket=request.ticket;ticket.sequence++;
        Require(consumer.Copy(ticket,images.slices)==TransferResult::Busy,"Future ticket must not acquire current frame");
        auto alias=images.slices;alias[1]=alias[0];
        Require(consumer.Copy(request.ticket,alias)==TransferResult::Invalid,"Reject aliased destination eyes");
        Require(consumer.Copy(request.ticket,images.slices)==TransferResult::Ok,"Copy complete pair");
        VerifyPixels(device,images,request.descriptor,request.ticket.frameId);
        Require(consumer.Copy(request.ticket,images.slices)==TransferResult::Invalid,"Reject repeated pair");
    }catch(...){reply.status=1;}
    const auto written=WriteExact(GetStdHandle(STD_OUTPUT_HANDLE),&reply,sizeof(reply));return written&&reply.status==0?0:1;
}
Reply RunConsumer(const std::filesystem::path& exe,const Request& request){
    SECURITY_ATTRIBUTES security{sizeof(security),nullptr,TRUE};Handle inputRead,inputWrite,outputRead,outputWrite;
    Require(CreatePipe(&inputRead.value,&inputWrite.value,&security,0)!=0,"Create request pipe");
    Require(CreatePipe(&outputRead.value,&outputWrite.value,&security,0)!=0,"Create response pipe");
    Require(SetHandleInformation(inputWrite.value,HANDLE_FLAG_INHERIT,0)!=0,"Exclude producer request handle");
    Require(SetHandleInformation(outputRead.value,HANDLE_FLAG_INHERIT,0)!=0,"Exclude producer response handle");
    SIZE_T size=0;InitializeProcThreadAttributeList(nullptr,1,0,&size);std::vector<char> attributes(size);
    auto* list=reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(attributes.data());
    Require(InitializeProcThreadAttributeList(list,1,0,&size)!=0,"Create child handle list");
    struct AttributeGuard{LPPROC_THREAD_ATTRIBUTE_LIST p;~AttributeGuard(){DeleteProcThreadAttributeList(p);}} guard{list};
    HANDLE inherited[]={inputRead.value,outputWrite.value};
    Require(UpdateProcThreadAttribute(list,0,PROC_THREAD_ATTRIBUTE_HANDLE_LIST,inherited,sizeof(inherited),nullptr,nullptr)!=0,"Restrict inherited handles");
    STARTUPINFOEXW startup{};startup.StartupInfo.cb=sizeof(startup);startup.StartupInfo.dwFlags=STARTF_USESTDHANDLES;
    startup.StartupInfo.hStdInput=inputRead.value;startup.StartupInfo.hStdOutput=outputWrite.value;startup.StartupInfo.hStdError=outputWrite.value;startup.lpAttributeList=list;
    PROCESS_INFORMATION process{};std::wstring command=L"\""+exe.wstring()+L"\" --consume";
    Require(CreateProcessW(exe.c_str(),command.data(),nullptr,nullptr,TRUE,CREATE_NO_WINDOW|EXTENDED_STARTUPINFO_PRESENT,nullptr,nullptr,&startup.StartupInfo,&process)!=0,"Start private bridge consumer");
    Handle child(process.hProcess),thread(process.hThread);inputRead.Close();outputWrite.Close();
    const bool written=WriteExact(inputWrite.value,&request,sizeof(request));inputWrite.Close();
    const auto wait=WaitForSingleObject(child.value,15000);
    if(wait!=WAIT_OBJECT_0){TerminateProcess(child.value,1);WaitForSingleObject(child.value,5000);throw std::runtime_error("Bridge probe child timed out");}
    Require(written,"Write consumer request");Reply reply{};Require(ReadExact(outputRead.value,&reply,sizeof(reply)),"Read consumer verification");
    DWORD exitCode=1;Require(GetExitCodeProcess(child.value,&exitCode)!=0&&exitCode==0,"Consumer verification failed");
    Require(reply.magic==0x31505246&&reply.bytes==sizeof(reply)&&reply.status==0,"Invalid consumer verification reply");return reply;
}
}
int wmain(int argc,wchar_t** argv){
    if(argc==2&&std::wstring(argv[1])==L"--consume")return Consume();
    try {
        wchar_t module[32768]{};Require(GetModuleFileNameW(nullptr,module,32768)!=0,"Locate probe executable");
        std::filesystem::path consumerExe=module;
        if(argc==3&&std::wstring(argv[1])==L"--consumer-exe")consumerExe=std::filesystem::absolute(argv[2]);
        else Require(argc==1,"Usage: KFVRBridgeProbe [--consumer-exe path]");
        Require(std::filesystem::is_regular_file(consumerExe),"Consumer executable missing");
        Device device;UINT consumerBits=0;unsigned pairs=0;
        for(const auto format:{DXGI_FORMAT_R8G8B8A8_UNORM,DXGI_FORMAT_B8G8R8A8_UNORM,DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,DXGI_FORMAT_B8G8R8A8_UNORM_SRGB}){
            D3D11PairProducer producer;Require(producer.Create(device.device.Get(),32,16,format,1)==TransferResult::Ok,"Create shared pair");
            const auto descriptor=producer.Descriptor();const auto backing=format==DXGI_FORMAT_R8G8B8A8_UNORM_SRGB?DXGI_FORMAT_R8G8B8A8_UNORM:format==DXGI_FORMAT_B8G8R8A8_UNORM_SRGB?DXGI_FORMAT_B8G8R8A8_UNORM:format;
            Images source(device,32,16,backing);PairTicket ticket{};
            auto alias=source.slices;alias[1]=alias[0];
            Require(producer.Publish(alias,{1,1,1,1000000},ticket)==TransferResult::Invalid,"Reject aliased source eyes");
            for(std::uint64_t frame=1;frame<=2;++frame){
                Fill(device,source,32,16,frame);
                Require(producer.Publish(source.slices,{frame,1,frame,1000000},ticket)==TransferResult::Ok,"Publish complete pair");
                PairTicket unused{};
                Require(producer.Publish(source.slices,{frame+1,1,frame+1,1000000},unused)==TransferResult::Busy,"Do not overwrite unconsumed pair");
                Require(!Valid(unused,descriptor),"Busy publication must have no valid ticket");
                const auto reply=RunConsumer(consumerExe,{descriptor,ticket});consumerBits=reply.pointerBits;++pairs;
                Require(producer.Publish(source.slices,{frame,1,frame,1000000},unused)==TransferResult::Invalid,"Reject repeated native frame");
            }
            producer.Reset();Require(!Valid(producer.Descriptor()),"Reset invalidates descriptor");
        }
        std::cout<<"{\"state\":\"verified\",\"producer_bits\":"<<sizeof(void*)*8<<",\"consumer_bits\":"<<consumerBits
            <<",\"pairs_checked\":"<<pairs<<",\"both_eye_pixels_match\":true,\"backpressure_checked\":true,\"game_memory_written\":false,\"headset_tested\":false}\n";
        return 0;
    }catch(const std::exception& e){std::cerr<<"D3D11 bridge probe failed: "<<e.what()<<'\n';return 1;}
}
