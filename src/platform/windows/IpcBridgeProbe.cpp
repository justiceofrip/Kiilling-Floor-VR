#include "kfvr/ipc/RemoteFrameProvider.h"
#include "kfvr/xr/DiagnosticScene.h"
#include "kfvr/graphics/D3D11SharedPair.h"
#include <Windows.h>
#include <dxgi1_2.h>
#include <wrl/client.h>
#include <array>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
using Microsoft::WRL::ComPtr;
using namespace kfvr;
namespace {
void Require(bool okay,const char* message){if(!okay)throw std::runtime_error(message);}
void Hr(HRESULT hr,const char* message){Require(SUCCEEDED(hr),message);}
struct Child {
    HANDLE process=nullptr;
    ~Child(){if(process){if(WaitForSingleObject(process,0)==WAIT_TIMEOUT){TerminateProcess(process,3);WaitForSingleObject(process,2000);}CloseHandle(process);}}
    Child(const std::filesystem::path& executable,const std::wstring& mode,const std::wstring& token){
        STARTUPINFOW startup{};startup.cb=sizeof(startup);PROCESS_INFORMATION info{};
        std::wstring command=L"\""+executable.wstring()+L"\" "+mode+L" "+token;
        Require(CreateProcessW(executable.c_str(),command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,nullptr,&startup,&info)!=0,"Start isolated IPC producer");
        process=info.hProcess;CloseHandle(info.hThread);
    }
    void Wait(){Require(WaitForSingleObject(process,15000)==WAIT_OBJECT_0,"IPC probe child timed out");DWORD code=1;Require(GetExitCodeProcess(process,&code)&&code==0,"IPC probe child failed");}
};
struct CountedScene final:xr::IFrameProvider {
    xr::DiagnosticScene scene;unsigned consumed=0,discarded=0;
    bool TryGetPair(const runtime::PresentationRequirements& r,const runtime::TrackingFrame& t,graphics::TextureDescriptor& d,graphics::PairTicket& p)noexcept override{return scene.TryGetPair(r,t,d,p);}
    void PairConsumed(const graphics::PairTicket& t,bool okay)noexcept override{if(okay)++consumed;else ++discarded;scene.PairConsumed(t,okay);}
};
int Produce(const std::wstring& token){
    CountedScene scene;ipc::RemoteFrameProducer producer(scene);Require(producer.Connect(token),"Connect remote producer");
    ipc::FrameChannel duplicate;Require(!duplicate.ConnectProducer(token),"Second producer must be rejected");
    const auto started=GetTickCount64();
    while(GetTickCount64()-started<15000){producer.Pump();if(scene.consumed>=4&&scene.discarded>=1)return 0;Sleep(1);}
    throw std::runtime_error("Remote producer did not receive consumption feedback");
}
std::vector<std::uint32_t> Pixels(ID3D11DeviceContext* context,ID3D11Texture2D* output,ID3D11Texture2D* staging,unsigned eye,UINT width,UINT height){
    context->CopySubresourceRegion(staging,0,0,0,0,output,eye,nullptr);D3D11_MAPPED_SUBRESOURCE map{};Hr(context->Map(staging,0,D3D11_MAP_READ,0,&map),"Read IPC eye pixels");
    std::vector<std::uint32_t> pixels(std::size_t(width)*height);
    for(UINT row=0;row<height;++row)std::memcpy(pixels.data()+std::size_t(row)*width,static_cast<const char*>(map.pData)+std::size_t(row)*map.RowPitch,width*4);
    context->Unmap(staging,0);std::size_t geometry=0;for(auto pixel:pixels)geometry+=pixel!=pixels.front();Require(geometry>pixels.size()/20,"IPC scene geometry missing");return pixels;
}
}
int wmain(int argc,wchar_t** argv){try {
    if(argc==3&&std::wstring(argv[1])==L"--produce")return Produce(argv[2]);
    if(argc==3&&(std::wstring(argv[1])==L"--abandon"||std::wstring(argv[1])==L"--connect-exit")){
        ipc::FrameChannel endpoint;Require(endpoint.ConnectProducer(argv[2]),"Connect failure-test endpoint");
        if(std::wstring(argv[1])==L"--abandon"){
            const auto name=L"Local\\KFVR.Control.v2."+std::wstring(argv[2])+L".mutex";
            HANDLE mutex=OpenMutexW(SYNCHRONIZE|MUTEX_MODIFY_STATE,FALSE,name.c_str());Require(mutex&&WaitForSingleObject(mutex,1000)==WAIT_OBJECT_0,"Acquire abandonment-test mutex");
        }
        ExitProcess(0); // Deliberately bypass RAII to exercise an actual peer crash.
    }
    Require(argc==3&&std::wstring(argv[1])==L"--producer-exe","Usage: KFVRIpcProbe --producer-exe path");
    const auto executable=std::filesystem::absolute(argv[2]);Require(std::filesystem::is_regular_file(executable),"Producer executable missing");
    DWORD binaryType=0;Require(GetBinaryTypeW(executable.c_str(),&binaryType)!=0,"Producer PE type");
    const auto producerBits=binaryType==SCS_32BIT_BINARY?32:binaryType==SCS_64BIT_BINARY?64:0;Require(producerBits!=0,"Unsupported producer architecture");
    // Independent sessions exercise real process-handle death and abandoned mutexes.
    for(const auto* mode:{L"--connect-exit",L"--abandon"}){ipc::FrameChannel host;Require(host.CreateHost(),"Create failure-test channel");Child child(executable,mode,host.Token());child.Wait();Require(!host.Connected(),"Dead or abandoned peer must fail closed");}
    ipc::FrameChannel invalid;Require(!invalid.ConnectProducer(L"invalid/token"),"Reject malformed channel name");
    ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> context;D3D_FEATURE_LEVEL level{};
    const D3D_FEATURE_LEVEL levels[]={D3D_FEATURE_LEVEL_11_0};Hr(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,D3D11_CREATE_DEVICE_BGRA_SUPPORT,levels,1,D3D11_SDK_VERSION,&device,&level,&context),"Create IPC verification device");
    ComPtr<IDXGIDevice> dxgi;Hr(device.As(&dxgi),"DXGI device");ComPtr<IDXGIAdapter> adapter;Hr(dxgi->GetAdapter(&adapter),"Adapter");DXGI_ADAPTER_DESC ad{};Hr(adapter->GetDesc(&ad),"Adapter LUID");
    runtime::PresentationRequirements requirements{480,512,29,ad.AdapterLuid.LowPart,ad.AdapterLuid.HighPart};
    D3D11_TEXTURE2D_DESC desc{};desc.Width=requirements.width;desc.Height=requirements.height;desc.MipLevels=1;desc.ArraySize=2;desc.Format=DXGI_FORMAT_R8G8B8A8_TYPELESS;desc.SampleDesc.Count=1;desc.Usage=D3D11_USAGE_DEFAULT;desc.BindFlags=D3D11_BIND_RENDER_TARGET;
    ComPtr<ID3D11Texture2D> output;Hr(device->CreateTexture2D(&desc,nullptr,&output),"Create IPC destinations");
    desc.ArraySize=1;desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;desc.BindFlags=0;desc.Usage=D3D11_USAGE_STAGING;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
    ComPtr<ID3D11Texture2D> staging;Hr(device->CreateTexture2D(&desc,nullptr,&staging),"Create IPC readback");
    ipc::RemoteFrameProvider host;Require(host.Create()&&host.SetBudget(50),"Create receiver channel");Child child(executable,L"--produce",host.Token());
    const auto start=GetTickCount64();while(!host.Connected()&&GetTickCount64()-start<3000)Sleep(1);Require(host.Connected(),"Producer handshake timed out");
    graphics::D3D11PairConsumer consumer;graphics::TextureDescriptor opened{},discardedDescriptor{};bool dropped=false,recreated=false;
    unsigned consumed=0,timeouts=0;std::array<std::vector<std::uint32_t>,2> previous;
    for(std::uint64_t attempt=1;consumed<4&&GetTickCount64()-start<12000;++attempt){
        runtime::TrackingFrame tracking{};tracking.generation=0x100000000ULL+attempt;tracking.spaceGeneration=0x200000003ULL;tracking.predictedNs=10000000000LL+std::int64_t(attempt);
        tracking.focused=tracking.headValid=true;tracking.head.position.x=float(consumed)*.15f;tracking.eyes={tracking.head,tracking.head};tracking.eyes[0].position.x-=.032f;tracking.eyes[1].position.x+=.032f;
        tracking.fov={math::FovTangents{-1,1,1,-1},math::FovTangents{-1,1,1,-1}};
        graphics::TextureDescriptor wire;graphics::PairTicket ticket;
        if(!host.TryGetPair(requirements,tracking,wire,ticket)){++timeouts;Sleep(1);continue;}
        Require(runtime::PairMatchesFrame(requirements,tracking,wire,ticket),"Cross-process frame must match exact 64-bit tracking/time/space identifiers");
        if(consumed==1&&!dropped){host.PairConsumed(ticket,false);discardedDescriptor=wire;dropped=true;Sleep(2);continue;}
        if(dropped&&consumed==1){Require(wire.session!=discardedDescriptor.session,"Dropped GPU stream was not recreated");recreated=true;}
        if(std::memcmp(&wire,&opened,sizeof(wire))){Require(consumer.Open(device.Get(),wire)==graphics::TransferResult::Ok,"Open remote shared eye textures");opened=wire;}
        const std::array<graphics::TextureSlice,2> targets={graphics::TextureSlice{output.Get(),0},graphics::TextureSlice{output.Get(),1}};
        const bool copied=consumer.Copy(ticket,targets)==graphics::TransferResult::Ok;host.PairConsumed(ticket,copied);Require(copied,"Consume remote stereo pair");
        auto left=Pixels(context.Get(),output.Get(),staging.Get(),0,requirements.width,requirements.height);auto right=Pixels(context.Get(),output.Get(),staging.Get(),1,requirements.width,requirements.height);
        Require(left!=right,"Remote eyes have no stereo parallax");if(consumed)Require(left!=previous[0]&&right!=previous[1],"Cross-process head translation did not move both eyes");
        previous={std::move(left),std::move(right)};++consumed;Sleep(2);
    }
    Require(consumed==4&&dropped&&recreated,"Incomplete IPC stereo verification");child.Wait();Require(!host.Connected(),"Producer exit not detected");
    std::cout<<"{\"state\":\"verified\",\"producer_bits\":"<<producerBits<<",\"host_bits\":"<<sizeof(void*)*8
        <<",\"consumed_pairs\":"<<consumed<<",\"startup_or_busy_frames\":"<<timeouts
        <<",\"tracking_roundtrip\":true,\"stereo_parallax\":true,\"head_translation\":true,\"dropped_pair_recovery\":true,\"peer_exit\":true,\"abandoned_mutex\":true,\"headset_used\":false,\"game_connected\":false}\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
