#include "kfvr/ipc/FrameChannel.h"
#include "kfvr/graphics/D3D11SharedPair.h"
#include <Windows.h>
#include <dxgi.h>
#include <wrl/client.h>
#include <algorithm>
#include <array>
#include <cstring>
#include <cmath>
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
// Give the hand audit a real non-zero, rotated OpenXR reference. The game
// must remove it identically for head, eyes and both controller grips.
math::Pose HandAuditReference(){return {{.6f,1.65f,-.25f},{0,.38268343f,0,.92387953f}};}
math::Pose InHandAuditSpace(math::Pose p){
 const auto reference=HandAuditReference();const auto a=reference.orientation,b=p.orientation;
 p.orientation={a.w*b.x+a.x*b.w+a.y*b.z-a.z*b.y,a.w*b.y-a.x*b.z+a.y*b.w+a.z*b.x,a.w*b.z+a.x*b.y-a.y*b.x+a.z*b.w,a.w*b.w-a.x*b.x-a.y*b.y-a.z*b.z};
 const float c=.70710678f,s=.70710678f;const auto v=p.position;
 p.position={reference.position.x+c*v.x+s*v.z,reference.position.y+v.y,reference.position.z-s*v.x+c*v.z};return p;
}
void Bitmap(const std::filesystem::path& path,const std::vector<unsigned>& pixels,unsigned width,unsigned height){
 auto bgra=pixels;for(auto& p:bgra)p=(p&0xff00ff00)|((p&0xff)<<16)|((p>>16)&0xff);
 BITMAPFILEHEADER f{};BITMAPINFOHEADER i{};f.bfType=0x4d42;f.bfOffBits=sizeof(f)+sizeof(i);f.bfSize=f.bfOffBits+DWORD(bgra.size()*4);i.biSize=sizeof(i);i.biWidth=LONG(width);i.biHeight=-LONG(height);i.biPlanes=1;i.biBitCount=32;i.biCompression=BI_RGB;
 std::ofstream out(path,std::ios::binary);out.write(reinterpret_cast<const char*>(&f),sizeof(f));out.write(reinterpret_cast<const char*>(&i),sizeof(i));out.write(reinterpret_cast<const char*>(bgra.data()),bgra.size()*4);
}
}
int wmain(int argc,wchar_t** argv){try{
 Require(argc>=2,"Usage: KFVRNativeBridgeProbe output-directory [--movement] [--turning] [--width W --height H]");bool movement=false,turning=false,weapon=false,inventory=false,reload=false,deagleReload=false,melee=false,dual=false,dualBelt=false,heal=false,weld=false,arms=false,trader=false,grenade=false,cadence=false,hands=false,hud=false;unsigned width=640,height=480;
 for(int i=2;i<argc;++i){const std::wstring arg=argv[i];if(arg==L"--hud")hud=weapon=movement=true;else if(arg==L"--hands")hands=turning=weapon=movement=true;else if(arg==L"--cadence")cadence=movement=true;else if(arg==L"--grenade")grenade=turning=weapon=movement=true;else if(arg==L"--trader")trader=weapon=movement=true;else if(arg==L"--arms")arms=weapon=movement=true;else if(arg==L"--weld")weld=weapon=movement=true;else if(arg==L"--heal")heal=weapon=movement=true;else if(arg==L"--dual-belt")dualBelt=weapon=movement=true;else if(arg==L"--dual")dual=weapon=movement=true;else if(arg==L"--melee")melee=turning=weapon=movement=true;else if(arg==L"--deagle-reload")deagleReload=reload=weapon=movement=true;else if(arg==L"--reload")reload=weapon=movement=true;else if(arg==L"--inventory")inventory=weapon=movement=true;else if(arg==L"--weapon")weapon=movement=true;else if(arg==L"--movement")movement=true;else if(arg==L"--turning")turning=movement=true;else if(arg==L"--width"&&i+1<argc)width=std::stoul(argv[++i]);else if(arg==L"--height"&&i+1<argc)height=std::stoul(argv[++i]);else throw std::runtime_error("Unknown receiver option");}
 Require(width>=64&&height>=64&&width<=4096&&height<=4096&&std::uint64_t(width)*height*40<=256*1024*1024,"Receiver dimensions exceed CPU bridge budget");
 const auto folder=std::filesystem::absolute(argv[1]);std::filesystem::create_directories(folder);
 ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> context;D3D_FEATURE_LEVEL level{};const D3D_FEATURE_LEVEL levels[]={D3D_FEATURE_LEVEL_11_0};
 Hr(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,D3D11_CREATE_DEVICE_BGRA_SUPPORT,levels,1,D3D11_SDK_VERSION,&device,&level,&context),"Create native receiving device");
 ComPtr<IDXGIDevice> dxgi;Hr(device.As(&dxgi),"DXGI device");ComPtr<IDXGIAdapter> adapter;Hr(dxgi->GetAdapter(&adapter),"Adapter");DXGI_ADAPTER_DESC ad{};Hr(adapter->GetDesc(&ad),"Adapter descriptor");
 const runtime::PresentationRequirements req{width,height,29,ad.AdapterLuid.LowPart,ad.AdapterLuid.HighPart};
 D3D11_TEXTURE2D_DESC desc{};desc.Width=req.width;desc.Height=req.height;desc.MipLevels=1;desc.ArraySize=2;desc.Format=DXGI_FORMAT_R8G8B8A8_TYPELESS;desc.SampleDesc.Count=1;desc.BindFlags=D3D11_BIND_RENDER_TARGET;
 ComPtr<ID3D11Texture2D> output,staging;Hr(device->CreateTexture2D(&desc,nullptr,&output),"Create receiving stereo targets");desc.ArraySize=1;desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;desc.BindFlags=0;desc.Usage=D3D11_USAGE_STAGING;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;Hr(device->CreateTexture2D(&desc,nullptr,&staging),"Create receiving readback");
 ipc::FrameChannel channel;Require(channel.CreateHost(),"Create native receiving channel");std::wcout<<channel.Token()<<std::endl;
 graphics::D3D11PairConsumer consumer;graphics::TextureDescriptor opened{};unsigned hapticEvents=0,hapticPulses=0;unsigned pairs=0,timeouts=0,minColors=UINT_MAX,minDifferences=UINT_MAX;std::uint64_t lastFrame=0;std::vector<double> latencies;
 const auto start=GetTickCount64();ULONGLONG firstPair=0;unsigned capturePhase=0;ULONGLONG capturePhaseStart=0;std::array<bool,64> captured{};std::array<unsigned,64> phasePairs{};std::ofstream controls;if(movement)controls.open(folder/L"controls.jsonl");LARGE_INTEGER frequency;QueryPerformanceFrequency(&frequency);
 const unsigned trialMs=hud?27000:hands?36000:cadence?20000:grenade?80000:trader?128000:arms?28000:weld?37000:heal?28000:dualBelt?62000:dual?49000:melee?22000:reload?54000:inventory?26700:7500;
 for(std::uint64_t attempt=1;(movement?(!firstPair||GetTickCount64()-firstPair<trialMs):pairs<12)&&GetTickCount64()-start<(trader?175000:grenade?150000:reload||dual||dualBelt||heal||weld||arms||hands||hud?90000:inventory?60000:35000);++attempt){
  const auto feedback=channel.ReceiveHaptics();if(feedback[0].milliseconds||feedback[1].milliseconds)++hapticEvents;for(const auto& pulse:feedback)if(pulse.milliseconds)++hapticPulses;
  const auto now=GetTickCount64();
  if(movement){
   const auto elapsed=firstPair?now-firstPair:0;
   const unsigned ends[]={700,1300,1800,2400,3100,3800,4300,4900,5600,6300,7000,7500};unsigned phase=0;while(phase<11&&elapsed>=ends[phase])++phase;
   if(inventory){const unsigned inventoryEnds[]={700,1200,1600,1950,4650,4950,5650,6450,6950,7250,7600,10600,10900,11600,12300,13000,13900,14250,17550,17850,18450,19150,19650,20650,21650,22050,22550,23700};phase=0;if(elapsed<3000)phase=28u+unsigned(elapsed/1000);else while(phase<27&&elapsed-3000>=inventoryEnds[phase])++phase;}
   if(grenade)phase=std::min(39u,unsigned(elapsed/2000));
   if(trader)phase=std::min(63u,unsigned(elapsed/2000));
   if(arms)phase=std::min(13u,unsigned(elapsed/2000));
   if(weld)phase=std::min(35u,unsigned((elapsed<5000?elapsed:elapsed-1000)/1000));
   if(heal)phase=std::min(27u,unsigned(elapsed/1000));
   if(dualBelt)phase=std::min(61u,unsigned(elapsed/1000));
   if(dual)phase=std::min(47u,unsigned((elapsed<24000?elapsed:elapsed-1000)/1000));
   if(melee)phase=std::min(21u,unsigned(elapsed/1000));
   if(reload)phase=elapsed<3000?51u+unsigned(elapsed/1000):std::min(50u,unsigned((elapsed-3000)/1000));
   if(hands)phase=std::min(17u,unsigned(elapsed/2000));
   if(hud)phase=std::min(17u,unsigned(elapsed/1500));
   if(cadence)phase=0;
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
     if(phase<14||phase>17)input.hands[0].gripPose.orientation={0,-.70710678f,0,.70710678f};
     auto& selected=input.hands[1];
     if(phase==28||phase==29){selected.gripPose.position={.22f,-.62f,.05f};selected.squeeze=phase==29?1.f:0.f;}
     if(phase==1||phase==8||phase==15||phase==22||phase==24||phase==26)selected.trigger=1;
     if(phase==3||phase==10||phase==17)selected.secondary=true;
     if(phase==2)selected.primary=true;
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
     if(phase==43||phase==45)lh.primary=true;
     if(phase==47||phase==48){rh.gripPose.position={deagleReload?-.22f:.22f,-.62f,.05f};rh.squeeze=phase==48?1.f:0.f;}
     if(phase==49)rh.trigger=1;
     if(phase==51||phase==52){rh.gripPose.position={deagleReload?-.22f:.22f,-.62f,.05f};rh.squeeze=phase==52?1.f:0.f;}
     for(auto& hand:input.hands)hand.aimPose=hand.gripPose;
    }
    if(melee){
     input={};input.focused=phase!=9;input.spaceGeneration=0x200000001ULL;input.predictedNs=std::int64_t(now)*1000000;
     const float t=float(elapsed%1000)/1000.f;
     for(unsigned h=0;h<2;++h){auto& hand=input.hands[h];hand.gripValid=hand.aimValid=true;hand.gripPose.position={h?.2f:-.2f,-.25f,-.35f};hand.triggerTouchActive=hand.thumbTouchActive=true;}
     auto& rh=input.hands[1];
     if(phase==0||phase==1||phase==15||phase==16||phase==18||phase==19)rh.gripPose.position={-.22f,-.15f,.18f};
     if(phase==1||phase==15||phase==19)rh.squeeze=1;
     if(phase==3)input.hands[0].secondary=true;
     if(phase==4)rh.gripPose.position.z-=.1f*t;
     if(phase==5)rh.gripPose.position.z-=.1f*(1-t);
     if(phase==6||phase==17)rh.gripPose.position.z-=.02f*std::sin(t*50.f);
     if(phase==7||phase==12)rh.gripPose.position.z-=.45f*std::min(1.f,t/.22f);
     if(phase>=8&&phase<=10||phase==13||phase==14)rh.gripPose.position.z-=.45f;
     if(phase==11)rh.gripPose.position.z-=.45f*(1-t);
     if(phase>=8&&phase<=10)rh.trigger=1;
     if(phase==13)input.hands[0].stickY=.4f;
     if(phase==14)rh.stickX=1;
     for(auto& hand:input.hands)hand.aimPose=hand.gripPose;
    }
    if(dual){
     input={};input.focused=phase!=9;input.spaceGeneration=0x200000001ULL;input.predictedNs=std::int64_t(now)*1000000;
     for(unsigned h=0;h<2;++h){auto& hand=input.hands[h];hand.gripValid=hand.aimValid=true;hand.gripPose.position={h?.25f:-.28f,-.28f,-.5f};hand.triggerTouchActive=hand.thumbTouchActive=true;}
     auto& rh=input.hands[1];auto& lh=input.hands[0];
     lh.gripPose.orientation={0,.3f,0,.9539392f};rh.gripPose.orientation={0,-.2f,0,.9797959f};
     if(phase==0||phase==1||phase==21||phase==22||phase==24||phase==25)rh.gripPose.position={.22f,-.62f,.05f};
     if(phase==1||phase==21||phase==25)rh.squeeze=1;
     if(phase==3)rh.trigger=1;
     if(phase==5)lh.trigger=1;
     if(phase==7||phase==9||phase==10||phase==12||phase==13||phase==14||phase==20||phase==23||phase==27||phase==29||phase==30)lh.trigger=rh.trigger=1;
     if(phase==13)lh.gripValid=lh.aimValid=false;
     if(phase==29)rh.gripValid=rh.aimValid=false;
     if(phase==16||phase==41)rh.secondary=true;
     if(phase>=32&&phase<=37){const auto drain=elapsed-33000;lh.trigger=drain<5000&&drain%400<200?1.f:0.f;}
     if(phase==38||phase==39)lh.trigger=1;
     if(phase==39)rh.trigger=1;
     if(phase==46)lh.trigger=rh.trigger=1;
     for(auto& hand:input.hands)hand.aimPose=hand.gripPose;
    }
    if(dualBelt){
     input={};input.focused=phase!=32;input.spaceGeneration=0x200000001ULL;input.predictedNs=std::int64_t(now)*1000000;
     const float t=float(elapsed%1000)/1000.f;
     for(unsigned h=0;h<2;++h){auto& hand=input.hands[h];hand.gripValid=hand.aimValid=true;hand.gripPose.position={h?.25f:-.28f,-.28f,-.5f};hand.triggerTouchActive=hand.thumbTouchActive=true;}
     auto& rh=input.hands[1];auto& lh=input.hands[0];
     auto down=[&](unsigned h,float approach){auto& hand=input.hands[h];hand.gripPose.orientation={-.70710678f,0,0,.70710678f};
      // Measured Dual9mm magwell minus palm at 300 model units/metre.
      hand.gripPose.position=h?math::Vec3{.2439944f,-.5460573f,-.1574245f-approach}:math::Vec3{-.2361496f,-.5442500f,-.1564950f-approach};};
     if(phase==0||phase==1||phase==45||phase==46||phase==48||phase==49)rh.gripPose.position={.22f,-.62f,.05f};
     if(phase==1||phase==45||phase==49)rh.squeeze=1;
     if(phase==3||phase==58)lh.secondary=true;
     // Forward/upward ejection is rejected. Downward B presents both belt mags.
     if(phase==5||phase==7||phase==17||phase==30||phase==44||phase==51)rh.secondary=true;
     if(phase==7||phase==8||phase==17||phase==30||phase==44||phase==51){down(0,.22f);down(1,.22f);}
     if(phase==9||phase==22||phase==31||phase==35||phase==52){down(0,.10f);down(1,.10f);}
     if(phase==10||phase==23||phase==36||phase==53){down(0,.10f-.125f*std::min(1.f,t/.6f));down(1,.10f-.125f*std::min(1.f,t/.6f));}
     if(phase==11||phase==24||phase==32||phase==33||phase==34||phase==37||phase==54){down(0,-.025f);down(1,-.025f);}
     if(phase==12||phase==13||phase==25||phase==26||phase==40)down(0,-.025f);
     if(phase==38)down(0,.10f);
     if(phase==39)down(0,.10f-.125f*std::min(1.f,t/.6f));
     if(phase==36)lh.gripValid=lh.aimValid=false;
     if(phase==11||phase==13||phase==15||phase==18||phase==20||phase==24||phase==26||phase==28||phase==42||phase==47||phase==54||phase==56||phase==59)lh.trigger=rh.trigger=1;
     for(auto& hand:input.hands)hand.aimPose=hand.gripPose;
    }
    if(grenade){
     input={};input.focused=phase!=11;input.spaceGeneration=0x200000001ULL;input.predictedNs=std::int64_t(now)*1000000;
     for(unsigned h=0;h<2;++h){auto& hand=input.hands[h];hand.gripValid=hand.aimValid=true;hand.gripPose.position={h?.2f:-.28f,-.2f,-.35f};hand.triggerTouchActive=hand.thumbTouchActive=true;}
     auto& rh=input.hands[1];rh.squeeze=1;
     if(phase==0||phase==1||phase==7||phase==8||phase==14||phase==15||phase==23||phase==24)rh.gripPose.position={.22f,-.48f,.28f};
     if(phase==0||phase==7||phase==14||phase==23||phase==13||phase==20||phase==29||phase==33)rh.squeeze=0;
     if(phase==3||phase==10||phase==11||phase==12||phase==17||phase==26||phase==27||phase==28||phase==31||phase==35)rh.trigger=1;
     if(phase==4||phase==36){
      const float t=std::clamp((float(elapsed%2000)-500.f)/250.f,0.f,1.f);
      rh.gripPose.position={.2f+(phase==36?.25f:0.f)*t,-.2f+.08f*t,-.35f-.6f*t};
      if(elapsed%2000>=750)rh.squeeze=0;
     }
     if(phase==5||phase==6||phase==21||phase==22||phase>=37)rh.squeeze=0;
     if(phase==18){input.hands[0].stickY=.25f;rh.stickX=1;}
     if(phase==27)rh.gripValid=rh.aimValid=false;
     if(phase==32)rh.gripPose.position={2.f,0,-.2f};
     for(auto& hand:input.hands)hand.aimPose=hand.gripPose;
    }
    if(trader){
     input={};input.focused=phase!=15;input.spaceGeneration=0x200000001ULL;input.predictedNs=std::int64_t(now)*1000000;
     unsigned row=0;
     if(phase>=7&&phase<=8)row=1;
     if(phase>=9&&phase<=10)row=5;
     if(phase>=11&&phase<=12)row=6;
     if(phase>=13&&phase<=14)row=7;
     if(phase>=19&&phase<=21||phase>=24&&phase<=25)row=2;
     if(phase>=22&&phase<=23)row=3;
     if(phase>=55)row=8;
     const float rowV=(244.f+row*80.f)/1024.f;
     for(unsigned h=0;h<2;++h){auto& hand=input.hands[h];hand.gripValid=hand.aimValid=true;hand.gripPose.position=h?math::Vec3{.1f,0,-.45f}:math::Vec3{-.3f,-.3f,-.4f};hand.triggerTouchActive=hand.thumbTouchActive=true;}
     auto& rh=input.hands[1];
     const float pointAngle=std::atan2(-.08f+(.5f-rowV)*.9f,.55f);rh.gripPose.orientation={std::sin(pointAngle*.5f),0,0,std::cos(pointAngle*.5f)};
     if(phase==1||phase==18||phase==29||phase==33||phase==40||phase==42||phase==54||phase==60)rh.primary=true;
     if(phase==3||phase==4||phase==6||phase==8||phase==10||phase==12||phase==14||phase==16||phase==20||phase==21||phase==23||phase==25||phase==27||phase==31||phase==35||phase==36||phase==38||phase==45||phase==48||phase==56||phase==57||phase==59)rh.trigger=1;
     if(phase==43||phase==46||phase==49||phase==52)input.hands[0].secondary=true;
     if(phase==30){rh.gripPose.position={.22f,-.62f,.05f};rh.squeeze=1;}
     if(phase==35)rh.gripValid=rh.aimValid=false;
     for(auto& hand:input.hands)hand.aimPose=hand.gripPose;
    }
    if(arms){
     input={};input.focused=phase!=7;input.spaceGeneration=0x200000001ULL;input.predictedNs=std::int64_t(now)*1000000;
     for(unsigned h=0;h<2;++h){auto& hand=input.hands[h];const float side=h?1.f:-1.f;hand.gripValid=hand.aimValid=true;hand.gripPose.position={side*.25f,-.25f,-.4f};hand.triggerTouchActive=hand.thumbTouchActive=true;hand.triggerTouched=phase%2;hand.thumbTouched=phase%3;
      if(phase==1)hand.gripPose.position.z=-.65f;
      if(phase==12)hand.gripPose.position.z=-1.2f;
      if(phase==2)hand.gripPose.position={side*.65f,-.25f,-.2f};
      if(phase==3)hand.gripPose.position={-side*.2f,-.3f,-.3f};
      if(phase==4)hand.gripPose.position={side*.2f,.2f,-.1f};
      if(phase==5)hand.gripPose.orientation={0,0,side*.38268343f,.92387953f};
      if(phase==6)hand.gripPose.position={side*.25f,-.65f,-.1f};
      if(phase==9&&h==0||phase==10&&h==1)hand.gripValid=hand.aimValid=false;
      hand.aimPose=hand.gripPose;
     }
    }
    if(weld){
     input={};input.focused=phase!=10;input.spaceGeneration=0x200000001ULL;input.predictedNs=std::int64_t(now)*1000000;
     for(unsigned h=0;h<2;++h){auto& hand=input.hands[h];hand.gripValid=hand.aimValid=true;hand.gripPose.position=h?math::Vec3{0,-.30f,-.72f}:math::Vec3{-.28f,-.28f,-.5f};hand.triggerTouchActive=hand.thumbTouchActive=true;}
     auto& rh=input.hands[1];auto& lh=input.hands[0];
     if(phase==0||phase==1||phase==28||phase==29||phase==31||phase==32)rh.gripPose.position={.18f,-.28f,-.08f};
     if(phase==1||phase==28||phase==32)rh.squeeze=1;
     if(phase==3){rh.gripPose.orientation={0,.70710678f,0,.70710678f};}
     if(phase==3||phase==5||phase==6||phase==8||phase==10||phase==11||phase==13||phase==14||phase==15||phase==17||phase==18||phase==26||phase==30||phase==34)rh.trigger=1;
     if(phase==8||phase==17||phase==18)lh.trigger=1;
     if(phase==14)rh.gripValid=rh.aimValid=false;
     if(phase==20||phase==21||phase==22||phase==24)rh.primary=true;
     for(auto& hand:input.hands)hand.aimPose=hand.gripPose;
    }
    if(heal){
     input={};input.focused=!(phase==5&&elapsed%1000>=40);input.spaceGeneration=0x200000001ULL;input.predictedNs=std::int64_t(now)*1000000;
     for(unsigned h=0;h<2;++h){auto& hand=input.hands[h];hand.gripValid=hand.aimValid=true;hand.gripPose.position={h?.25f:-.28f,-.28f,-.5f};hand.triggerTouchActive=hand.thumbTouchActive=true;}
     auto& rh=input.hands[1];
     if(phase==0||phase==1||phase==20||phase==21||phase==23||phase==24)rh.gripPose.position={-.18f,-.28f,-.08f};
     if(phase==1||phase==20||phase==24)rh.squeeze=1;
     if(phase==4||phase==5||phase==6||phase==7||phase==9||phase==10||phase==11||phase==12||phase==13||phase==15||phase==16||phase==17){
      // Measured syringe tip/palm offset, rotated +90 degrees to point into chest.
      rh.gripPose.position={.049625f,-.441210f,-.289998f};rh.gripPose.orientation={.70710678f,0,0,.70710678f};
     }
     if(phase==3||phase==5||phase==6||phase==8||phase==10||phase==11||phase==13||phase==15||phase==16||phase==22||phase==26)rh.trigger=1;
     if(phase==15)rh.gripValid=rh.aimValid=false;
     for(auto& hand:input.hands)hand.aimPose=hand.gripPose;
    }
    if(hands){
     input={};input.focused=phase!=11;input.spaceGeneration=0x200000001ULL;input.predictedNs=std::int64_t(now)*1000000;
     for(unsigned h=0;h<2;++h){auto& hand=input.hands[h];const float side=h?1.f:-1.f;
      hand.gripValid=hand.aimValid=true;hand.gripPose.position={side*.18f,-.18f,-.38f};
      hand.triggerTouchActive=hand.thumbTouchActive=true;
      if(phase==1||phase==2){input.hands[1].gripPose.position={.22f,-.62f,.05f};input.hands[1].squeeze=phase==2&&elapsed%2000<900?1.f:0.f;}
      if(phase==4)hand.triggerTouched=hand.thumbTouched=true;
      if(phase==5||phase==7||phase==9)hand.trigger=hand.squeeze=1.f;
      if(phase>=5&&phase<=9)hand.thumbTouched=true;
      if(phase==6)hand.gripPose.orientation={0,side*.70710678f,0,.70710678f};
      if(phase==7)hand.gripPose.orientation={side*.70710678f,0,0,.70710678f};
      if(phase==8)hand.gripPose.orientation={0,0,side*.70710678f,.70710678f};
      if(phase==9)hand.gripPose.orientation={side*.3535534f,.3535534f,side*.1464466f,.8535534f};
      if(phase==13&&h==0||phase==14&&h==1)hand.gripValid=hand.aimValid=false;
      if(phase==16&&h==1)hand.stickX=1.f;
      hand.gripPose=InHandAuditSpace(hand.gripPose);hand.aimPose=hand.gripPose;
     }
    }
    if(hud){
     input={};input.focused=phase!=9;input.spaceGeneration=0x200000001ULL;input.predictedNs=std::int64_t(now)*1000000;
     for(unsigned h=0;h<2;++h){auto& hand=input.hands[h];hand.gripValid=hand.aimValid=true;hand.gripPose.position={h?.2f:-.2f,-.25f,-.5f};hand.triggerTouchActive=hand.thumbTouchActive=true;}
     auto& left=input.hands[0];left.gripPose.orientation={0,-.70710678f,0,.70710678f};
     if(phase==0||phase==2||phase==14||phase==17)left.gripPose.orientation={};
     if(phase==3)left.gripPose.position={-.2f,-.65f,-.45f};
     if(phase==4)left.gripPose.position={-.02f,-.05f,-.1f};
     if(phase==5)left.gripPose.position={-.2f,-.25f,-.9f};
     if(phase==6)left.squeeze=1;
     if(phase==7)left.trigger=1;
     if(phase==11)left.gripValid=left.aimValid=false;
     if(phase==13||phase==15)left.gripPose.orientation={0,-.13052619f,0,.99144486f};
     for(auto& hand:input.hands)hand.aimPose=hand.gripPose;
    }
    right.aimPose=right.gripPose;
    if(capturePhase!=phase||!capturePhaseStart){capturePhase=phase;capturePhaseStart=now;}
   }
   const auto sent=phase==8&&!weapon?ipc::ChannelResult::Timeout:channel.SendInput(input);
   controls<<"{\"uptime_ms\":"<<now<<",\"phase\":"<<phase<<",\"sent\":"<<(sent==ipc::ChannelResult::Ok?"true":"false")<<"}\n";
  }
  runtime::TrackingFrame tracking{};tracking.generation=0x100000000ULL+attempt;tracking.spaceGeneration=0x200000001ULL;tracking.predictedNs=std::int64_t(now)*1000000+std::int64_t(attempt%1000);tracking.focused=tracking.headValid=true;
  if(dualBelt&&(capturePhase==8||capturePhase==9||capturePhase==24))tracking.head.orientation={-.537300f,0,0,.843391f};
  if(arms&&capturePhase==6)tracking.head.orientation={-.537300f,0,0,.843391f};
  if(arms&&capturePhase==4)tracking.head.orientation={.382683f,0,0,.923880f};
  if(heal&&(capturePhase==4||capturePhase==10))tracking.head.orientation={-.537300f,0,0,.843391f};
  if(weapon&&!melee&&!dual&&!dualBelt&&!heal&&!weld&&!arms&&!trader&&!grenade&&!hands&&!hud&&capturePhase==(inventory?6u:4u))tracking.head.orientation={-.537300f,0,0,.843391f};
  if(hud)tracking.head.orientation={-.17364818f,0,0,.98480775f};
  if(hands){tracking.head.orientation={-.17364818f,0,0,.98480775f};
   if(capturePhase==10){tracking.head.position={.1f,.2f,.05f};tracking.head.orientation={-.2391176f,.3696438f,.0990458f,.8923991f};}
  }
  tracking.head.position.x+=movement?0.f:float(pairs%4)*.04f;
  if(hands&&capturePhase==4)tracking.head.orientation={-.70710678f,0,0,.70710678f};
  if(hands&&capturePhase==6)tracking.head.orientation={.70710678f,0,0,.70710678f};
  if(hands){tracking.referenceHead=HandAuditReference();tracking.head=InHandAuditSpace(tracking.head);}
  const auto eyePoses=math::ComputeEyePoses(tracking.head,.064f);Require(bool(eyePoses),"Valid synthetic head pose");tracking.eyes={eyePoses->left,eyePoses->right};tracking.fov={math::FovTangents{-1.05f,.95f,.75f,-.75f},math::FovTangents{-.95f,1.05f,.75f,-.75f}};
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
  if(weapon&&!captured[capturePhase]&&firstPair&&now-capturePhaseStart>(hud?400u:150u)){
   const auto left=Pixels(context.Get(),output.Get(),staging.Get(),0,req.width,req.height);
   Bitmap(folder/(L"weapon-phase-"+std::to_wstring(capturePhase)+L".bmp"),left,req.width,req.height);captured[capturePhase]=true;
  }
  ++phasePairs[capturePhase];++pairs;if(!firstPair)firstPair=GetTickCount64();if(!cadence)Sleep(2);
 }
 // Leave time for producer to observe the final consumed acknowledgement.
 Sleep(40);channel.Close();double sum=0,max=0;for(double v:latencies){sum+=v;max=std::max(max,v);}
 std::ofstream report(folder/L"receiver.json");report<<"{\"pairs\":"<<pairs<<",\"trial_elapsed_ms\":"<<(firstPair?GetTickCount64()-firstPair:0)<<",\"timeouts_or_busy\":"<<timeouts<<",\"minimum_eye_colors\":"<<minColors<<",\"minimum_differing_eye_pixels\":"<<minDifferences<<",\"width\":"<<req.width<<",\"height\":"<<req.height<<",\"request_to_copy_average_ms\":"<<(pairs?sum/pairs:0)<<",\"request_to_copy_max_ms\":"<<max<<",\"pixel_checked_pairs\":"<<(movement?(pairs?1:0):pairs)<<",\"haptic_events\":"<<hapticEvents<<",\"haptic_pulses\":"<<hapticPulses<<",\"synthetic_tracking\":true,\"headset_presented\":false}\n";report.close();
 if(hands){std::ofstream phaseReport(folder/L"hand-eye-pairs.json");phaseReport<<'[';for(unsigned i=0;i<18;++i){if(i)phaseReport<<',';phaseReport<<phasePairs[i];}phaseReport<<"]\n";}
 Require(movement?(pairs>=12&&firstPair&&GetTickCount64()-firstPair>=trialMs):pairs==12,"Incomplete bounded native stereo trial");std::cout<<"Received "<<pairs<<" live KF stereo pairs across x86 to x64\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
