#include "kfvr/ipc/FrameChannel.h"
#include "kfvr/graphics/D3D11SharedPair.h"
#include <Windows.h>
#include <dxgi.h>
#include <wrl/client.h>
#include <algorithm>
#include <array>
#include <cstring>
#include <cwchar>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <stdexcept>
#include <vector>
using namespace kfvr;using Microsoft::WRL::ComPtr;
namespace {
void Require(bool okay,const char* message){if(!okay)throw std::runtime_error(message);}
void Hr(HRESULT hr,const char* message){Require(SUCCEEDED(hr),message);}
std::vector<unsigned> Pixels(ID3D11DeviceContext* context,ID3D11Texture2D* output,ID3D11Texture2D* staging,unsigned eye,unsigned width,unsigned height){
 context->CopySubresourceRegion(staging,0,0,0,0,output,eye,nullptr);D3D11_MAPPED_SUBRESOURCE map{};Hr(context->Map(staging,0,D3D11_MAP_READ,0,&map),"Map received native eye");
 std::vector<unsigned> pixels(std::size_t(width)*height);for(unsigned y=0;y<height;++y)std::memcpy(pixels.data()+std::size_t(y)*width,static_cast<const char*>(map.pData)+std::size_t(y)*map.RowPitch,width*4);context->Unmap(staging,0);return pixels;
}
void Bitmap(const std::filesystem::path& path,const std::vector<unsigned>& pixels,unsigned width,unsigned height){
 auto bgra=pixels;for(auto& p:bgra)p=(p&0xff00ff00)|((p&0xff)<<16)|((p>>16)&0xff);
 BITMAPFILEHEADER f{};BITMAPINFOHEADER i{};f.bfType=0x4d42;f.bfOffBits=sizeof(f)+sizeof(i);f.bfSize=f.bfOffBits+DWORD(bgra.size()*4);i.biSize=sizeof(i);i.biWidth=LONG(width);i.biHeight=-LONG(height);i.biPlanes=1;i.biBitCount=32;i.biCompression=BI_RGB;
 std::ofstream out(path,std::ios::binary);out.write(reinterpret_cast<const char*>(&f),sizeof(f));out.write(reinterpret_cast<const char*>(&i),sizeof(i));out.write(reinterpret_cast<const char*>(bgra.data()),bgra.size()*4);
}
}
int wmain(int argc,wchar_t** argv){try{
 Require(argc>=2,"Usage: KFVRNativeBridgeProbe output-directory [--movement] [--turning] [--width W --height H]");bool movement=false,turning=false,weapon=false,inventory=false,reload=false;unsigned width=640,height=480;
 for(int i=2;i<argc;++i){const std::wstring arg=argv[i];if(arg==L"--reload")reload=weapon=movement=true;else if(arg==L"--inventory")inventory=weapon=movement=true;else if(arg==L"--weapon")weapon=movement=true;else if(arg==L"--movement")movement=true;else if(arg==L"--turning")turning=movement=true;else if(arg==L"--width"&&i+1<argc)width=std::stoul(argv[++i]);else if(arg==L"--height"&&i+1<argc)height=std::stoul(argv[++i]);else throw std::runtime_error("Unknown receiver option");}
 Require(width>=64&&height>=64&&width<=4096&&height<=4096&&std::uint64_t(width)*height*40<=256*1024*1024,"Receiver dimensions exceed CPU bridge budget");
 const auto folder=std::filesystem::absolute(argv[1]);std::filesystem::create_directories(folder);
 ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> context;D3D_FEATURE_LEVEL level{};const D3D_FEATURE_LEVEL levels[]={D3D_FEATURE_LEVEL_11_0};
 Hr(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,D3D11_CREATE_DEVICE_BGRA_SUPPORT,levels,1,D3D11_SDK_VERSION,&device,&level,&context),"Create native receiving device");
 ComPtr<IDXGIDevice> dxgi;Hr(device.As(&dxgi),"DXGI device");ComPtr<IDXGIAdapter> adapter;Hr(dxgi->GetAdapter(&adapter),"Adapter");DXGI_ADAPTER_DESC ad{};Hr(adapter->GetDesc(&ad),"Adapter descriptor");
 const runtime::PresentationRequirements req{width,height,29,ad.AdapterLuid.LowPart,ad.AdapterLuid.HighPart};
 D3D11_TEXTURE2D_DESC desc{};desc.Width=req.width;desc.Height=req.height;desc.MipLevels=1;desc.ArraySize=2;desc.Format=DXGI_FORMAT_R8G8B8A8_TYPELESS;desc.SampleDesc.Count=1;desc.BindFlags=D3D11_BIND_RENDER_TARGET;
 ComPtr<ID3D11Texture2D> output,staging;Hr(device->CreateTexture2D(&desc,nullptr,&output),"Create receiving stereo targets");desc.ArraySize=1;desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;desc.BindFlags=0;desc.Usage=D3D11_USAGE_STAGING;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;Hr(device->CreateTexture2D(&desc,nullptr,&staging),"Create receiving readback");
 ipc::FrameChannel channel;Require(channel.CreateHost(),"Create native receiving channel");std::wcout<<channel.Token()<<std::endl;
 graphics::D3D11PairConsumer consumer;graphics::TextureDescriptor opened{};unsigned hapticEvents=0;unsigned pairs=0,timeouts=0,minColors=UINT_MAX,minDifferences=UINT_MAX;std::uint64_t lastFrame=0;std::vector<double> latencies;
 const auto start=GetTickCount64();ULONGLONG firstPair=0;unsigned capturePhase=0;ULONGLONG capturePhaseStart=0;std::array<bool,64> captured{};std::ofstream controls;if(movement)controls.open(folder/L"controls.jsonl");LARGE_INTEGER frequency;QueryPerformanceFrequency(&frequency);
 const unsigned trialMs=reload?51000:inventory?23700:7500;
 for(std::uint64_t attempt=1;(movement?(!firstPair||GetTickCount64()-firstPair<trialMs):pairs<12)&&GetTickCount64()-start<(reload?80000:inventory?60000:35000);++attempt){
  const auto feedback=channel.ReceiveHaptics();if(feedback[0].milliseconds||feedback[1].milliseconds)++hapticEvents;
  const auto now=GetTickCount64();
  if(movement){
   const auto elapsed=firstPair?now-firstPair:0;
   const unsigned ends[]={700,1300,1800,2400,3100,3800,4300,4900,5600,6300,7000,7500};unsigned phase=0;while(phase<11&&elapsed>=ends[phase])++phase;
   if(inventory){const unsigned inventoryEnds[]={700,1200,1600,1950,4650,4950,5650,6450,6950,7250,7600,10600,10900,11600,12300,13000,13900,14250,17550,17850,18450,19150,19650,20650,21650,22050,22550,23700};phase=0;while(phase<27&&elapsed>=inventoryEnds[phase])++phase;}
   if(reload)phase=std::min(50u,unsigned(elapsed/1000));
   xr::ControllerFrame input{};input.spaceGeneration=0x200000001ULL;input.predictedNs=std::int64_t(now)*1000000;input.focused=phase!=4;input.hands[0].gripValid=input.hands[1].gripValid=true;
   if(phase==1||phase==4||phase==5||phase==9)input.hands[0].stickY=.4f;
   if(phase==3)input.hands[0].stickX=.4f;
   if(phase==7)input.hands[0].stickY=-.4f;
   if(turning&&(phase==1||phase==7))input.hands[1].stickX=1;
   if(turning&&(phase==3||phase==4||phase==5||phase==9))input.hands[1].stickX=-1;
   if(weapon){
    input.focused=phase!=8;input.hands[0].stickX=input.hands[0].stickY=input.hands[1].stickX=0;
    for(unsigned h=0;h<2;++h){auto& hand=input.hands[h];hand.gripPose.position={h?.2f:-.2f,-.25f,-.5f};hand.aimValid=true;hand.aimPose=hand.gripPose;hand.triggerTouchActive=hand.thumbTouchActive=true;hand.triggerTouched=phase%2;hand.thumbTouched=phase%3;hand.squeeze=phase==2?.4f:0;}
    if(phase==1){auto& left=input.hands[0];left.gripPose.position={-.35f,-.1f,-.45f};left.gripPose.orientation={-.1f,.05f,-.1f,.988686f};left.aimPose=left.gripPose;}
    auto& right=input.hands[1];
    if(phase==1||phase==7){right.gripPose.position={.35f,-.35f,-.6f};right.gripPose.orientation={.1f,.2f,.1f,.96953597f};}
    if(phase>=3&&phase<=5)right.gripPose.position={.22f,-.62f,.05f};
    if(phase==3||phase==5||phase==6)right.squeeze=1;
    if(phase==2||phase==7||phase==9||phase==11)right.trigger=1;
    if(inventory){
     input={};input.focused=phase!=23;input.spaceGeneration=0x200000001ULL;input.predictedNs=std::int64_t(now)*1000000;
     for(unsigned h=0;h<2;++h){auto& hand=input.hands[h];hand.gripValid=hand.aimValid=true;hand.gripPose.position={h?.2f:-.2f,-.25f,-.5f};hand.triggerTouchActive=hand.thumbTouchActive=true;hand.thumbTouched=hand.triggerTouched=true;}
     auto& selected=input.hands[1];
     if(phase==1||phase==8||phase==15||phase==22||phase==24||phase==26)selected.trigger=1;
     if(phase==3||phase==10||phase==17)selected.secondary=true;
     if(phase==5||phase==6)selected.gripPose.position={-.22f,-.62f,.05f};
     if(phase==12||phase==13)selected.gripPose.position={.22f,-.15f,.18f};
     if(phase==19||phase==20)selected.gripPose.position={.22f,-.62f,.05f};
     if(phase==6||phase==13||phase==20)selected.squeeze=1;
     if(phase>=14&&phase<=17){input.hands[0].gripPose.position={.115f,-.227f,-.637f};input.hands[0].squeeze=phase==14?0.f:1.f;}
     if(phase==16||phase==17)input.hands[0].gripPose.position={.215f,-.18f,-.76f};
     for(auto& hand:input.hands)hand.aimPose=hand.gripPose;
    }
    if(reload){
     input={};input.focused=phase!=35;input.spaceGeneration=0x200000001ULL;input.predictedNs=std::int64_t(now)*1000000;
     auto& lh=input.hands[0];auto& rh=input.hands[1];
     for(unsigned h=0;h<2;++h){auto& hand=input.hands[h];hand.gripValid=hand.aimValid=true;hand.gripPose.position={h?.2f:-.2f,-.25f,-.5f};hand.triggerTouchActive=hand.thumbTouchActive=true;}
     if(phase==1)lh.secondary=true;
     if(phase>=3&&phase<=9){lh.squeeze=1;lh.gripPose.position=phase<=4?math::Vec3{.2f,-.36f,-.5f}:math::Vec3{0,-.28f,-.45f};}
     if(phase==4)rh.secondary=true;
     if(phase==5||phase==6||phase==7)lh.gripPose.orientation={0,0,.3826834f,.9238795f};
     if(phase==6||phase==8||phase==14||phase==19||phase==25||phase==34||phase==36||phase==38)rh.trigger=1;
     if(phase==9||phase==10||phase==11){lh.gripPose.position={-.2f,-.6f,-.12f};lh.squeeze=phase==10?0.f:1.f;}
     if(phase==12||phase==13){lh.gripPose.position={.2f,-.30f,-.5f};lh.squeeze=phase==12?1.f:0.f;}
     // A short pull must not feed; a full pull must return before firing.
     if(phase==16||phase==17||phase==18||phase==21||phase==22||phase==23||phase==24){
      lh.gripPose.position={.2f,-.22f,-.5f};lh.trigger=1;lh.squeeze=(phase==18||phase==24)?0.f:1.f;
      if(phase==17)lh.gripPose.position.z+=.02f;
      if(phase==22||phase==23)lh.gripPose.position.z+=.06f;
     }
     if(phase==23)rh.trigger=1;
     // Retain a partial magazine, seat a fresh one with the chamber intact.
     if(phase==27||phase==28){lh.gripPose.position={.2f,-.36f,-.5f};lh.squeeze=1;}
     if(phase==28)rh.secondary=true;
     if(phase==29||phase==30||phase==31){lh.gripPose.position={-.2f,-.6f,-.12f};lh.squeeze=phase==30?0.f:1.f;}
     if(phase==32||phase==33){lh.gripPose.position={.2f,-.30f,-.5f};lh.squeeze=phase==32?1.f:0.f;}
     if(phase==41||phase==42){lh.gripPose.position={.2f,-.36f,-.5f};lh.squeeze=1;}
     if(phase==42)rh.secondary=true;
     if(phase==43||phase==45||phase==47)lh.primary=true;
     if(phase==49)rh.trigger=1;
     for(auto& hand:input.hands)hand.aimPose=hand.gripPose;
    }
    right.aimPose=right.gripPose;
    if(capturePhase!=phase||!capturePhaseStart){capturePhase=phase;capturePhaseStart=now;}
   }
   const auto sent=phase==8&&!weapon?ipc::ChannelResult::Timeout:channel.SendInput(input);
   controls<<"{\"uptime_ms\":"<<now<<",\"phase\":"<<phase<<",\"sent\":"<<(sent==ipc::ChannelResult::Ok?"true":"false")<<"}\n";
  }
  runtime::TrackingFrame tracking{};tracking.generation=0x100000000ULL+attempt;tracking.spaceGeneration=0x200000001ULL;tracking.predictedNs=std::int64_t(now)*1000000+std::int64_t(attempt%1000);tracking.focused=tracking.headValid=true;
  if(weapon&&capturePhase==(inventory?6u:4u))tracking.head.orientation={-.537300f,0,0,.843391f};
  tracking.head.position.x=movement?0.f:float(pairs%4)*.04f;tracking.eyes={tracking.head,tracking.head};tracking.eyes[0].position.x-=.032f;tracking.eyes[1].position.x+=.032f;tracking.fov={math::FovTangents{-1.05f,.95f,.75f,-.75f},math::FovTangents{-.95f,1.05f,.75f,-.75f}};
  graphics::TextureDescriptor wire;graphics::PairTicket ticket;LARGE_INTEGER begin,end;QueryPerformanceCounter(&begin);
  const auto result=channel.RequestPair(req,tracking,50,wire,ticket);if(result!=ipc::ChannelResult::Ok){++timeouts;Sleep(1);continue;}
  Require(runtime::PairMatchesFrame(req,tracking,wire,ticket),"Received pair must match exact tracking sample");Require(ticket.frameId>lastFrame,"Received stale native frame");lastFrame=ticket.frameId;
  if(std::memcmp(&wire,&opened,sizeof(wire))){Require(consumer.Open(device.Get(),wire)==graphics::TransferResult::Ok,"Open native shared textures");opened=wire;}
  const bool copied=consumer.Copy(ticket,{graphics::TextureSlice{output.Get(),0},graphics::TextureSlice{output.Get(),1}})==graphics::TransferResult::Ok;channel.Feedback(ticket,copied);Require(copied,"Copy native shared eye pair");QueryPerformanceCounter(&end);latencies.push_back(double(end.QuadPart-begin.QuadPart)*1000/double(frequency.QuadPart));
  // Exhaustive CPU colour counting is deliberately outside the continuing
  // movement cadence. Its >100 ms cost can expire a valid controller sample.
  // Every pair still checks tracking identity and both GPU texture copies.
  if(!movement||pairs==0){
  const auto left=Pixels(context.Get(),output.Get(),staging.Get(),0,req.width,req.height),right=Pixels(context.Get(),output.Get(),staging.Get(),1,req.width,req.height);
  for(const auto* pixels:{&left,&right}){std::set<unsigned> colors(pixels->begin(),pixels->end());minColors=std::min(minColors,unsigned(colors.size()));}
  unsigned different=0;for(std::size_t i=0;i<left.size();++i)different+=left[i]!=right[i];minDifferences=std::min(minDifferences,different);
  Require(minColors>64&&minDifferences>1024,"Missing native geometry or differing eye views");
  if(pairs==0||(!movement&&pairs==11)){Bitmap(folder/(pairs?L"last-left.bmp":L"first-left.bmp"),left,req.width,req.height);Bitmap(folder/(pairs?L"last-right.bmp":L"first-right.bmp"),right,req.width,req.height);}
  }
  if(weapon&&!captured[capturePhase]&&firstPair&&now-capturePhaseStart>150){
   const auto left=Pixels(context.Get(),output.Get(),staging.Get(),0,req.width,req.height);
   Bitmap(folder/(L"weapon-phase-"+std::to_wstring(capturePhase)+L".bmp"),left,req.width,req.height);captured[capturePhase]=true;
  }
  ++pairs;if(!firstPair)firstPair=GetTickCount64();Sleep(2);
 }
 // Leave time for producer to observe the final consumed acknowledgement.
 Sleep(40);channel.Close();double sum=0,max=0;for(double v:latencies){sum+=v;max=std::max(max,v);}
 std::ofstream report(folder/L"receiver.json");report<<"{\"pairs\":"<<pairs<<",\"timeouts_or_busy\":"<<timeouts<<",\"minimum_eye_colors\":"<<minColors<<",\"minimum_differing_eye_pixels\":"<<minDifferences<<",\"width\":"<<req.width<<",\"height\":"<<req.height<<",\"request_to_copy_average_ms\":"<<(pairs?sum/pairs:0)<<",\"request_to_copy_max_ms\":"<<max<<",\"pixel_checked_pairs\":"<<(movement?(pairs?1:0):pairs)<<",\"haptic_events\":"<<hapticEvents<<",\"synthetic_tracking\":true,\"headset_presented\":false}\n";report.close();Require(movement?(pairs>=12&&firstPair&&GetTickCount64()-firstPair>=trialMs):pairs==12,"Incomplete bounded native stereo trial");std::cout<<"Received "<<pairs<<" live KF stereo pairs across x86 to x64\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
