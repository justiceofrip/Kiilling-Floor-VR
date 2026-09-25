#include <Windows.h>
#include <MinHook.h>
#include <d3d9.h>
#include <wrl/client.h>
#include <vector>
#include "NativeProbeConfig.h"
#include "NativeCameraTrial.h"
#include "NativeEyeBridge.h"
#include "NativeInputAudit.h"
#include <array>
#include <atomic>
#include <cstring>
#include <filesystem>
#include <fstream>
namespace {
using Draw=void(__thiscall*)(void*,void*,int,unsigned char*,int*);
using Tick=void(__thiscall*)(void*,float);
using Render=void(__thiscall*)(void*,void*);
using Present=void(__thiscall*)(void*,void*);
using UpdateAnimation=int(__thiscall*)(void*,float);
using GetFrame=void(__thiscall*)(void*,void*,void*,void*,int,int*,unsigned long);
using ProcessEvent=void(__thiscall*)(void*,void*,void*,void*);
Render levelOriginal=nullptr;Tick actorAnimationOriginal=nullptr;UpdateAnimation skeletalAnimationOriginal=nullptr;
GetFrame getFrameOriginal=nullptr;ProcessEvent processEventOriginal=nullptr;
thread_local unsigned playerDepth=0,levelDepth=0;
void ProbeTarget(void*,void*);
void ReadTarget();
void ProbeStereo(void*,void*);
void ReadStereo();
void WriteStereo(const std::filesystem::path&);
thread_local bool privateEye=false,nativeDrawValid=false;
void BridgeLevel(void*,void*);
std::atomic<bool> replayed=false;
using AuxCtor=void(__thiscall*)(void*,int,int,int);
using SimpleCall=void(__thiscall*)(void*);
AuxCtor auxCtor=nullptr;SimpleCall auxDtor=nullptr;
std::array<void*,5> targetMethods{};
struct ScriptCount {unsigned name=0,ownerName=0,phase=0,calls=0;};
Draw drawOriginal=nullptr;Tick tickOriginal=nullptr;Render renderOriginal=nullptr;Present presentOriginal=nullptr;
std::array<void*,9> targets{};std::atomic<bool> sampling=false;std::atomic<unsigned> count=0,ticks=0,presents=0;
LONG started=0;kfvr::native::NativeProbeConfig config;
struct Record {unsigned levelRenders=0,actorAnimation=0,skeletalAnimation=0,skeletalFrames=0,scriptEvents=0,scriptOverflow=0;std::array<ScriptCount,48> scripts{};std::atomic<bool> ready=false;unsigned frame=0,thread=0,tickBefore=0,tickAfter=0,renders=0,presentBefore=0,presentAfter=0;std::uint32_t scene=0,renderInterface=0;std::array<float,3> origin{};std::array<int,3> rotation{};float fov=0;long long begin=0,end=0;bool sceneRead=false;std::array<float,96> matrices{};};
std::array<Record,240> records;thread_local Record* current=nullptr;
bool Read(std::uint32_t at,void* data,std::size_t bytes){SIZE_T got=0;return at>=0x10000&&bytes<=4096&&ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(at),data,bytes,&got)&&got==bytes;}
std::uint32_t U32(std::uint32_t at){std::uint32_t value=0;Read(at,&value,4);return value;}
bool LocalWorld(){unsigned char mode=255;return U32(config.controller+4)==config.controllerIndex&&U32(config.level+4)==config.levelIndex&&U32(config.controller+config.levelOffset)==config.level&&U32(config.controller+config.playerOffset)==config.viewport&&Read(config.level+config.netModeOffset,&mode,1)&&mode==0;}
long long Qpc(){LARGE_INTEGER q;QueryPerformanceCounter(&q);return q.QuadPart;}
void __fastcall DrawHook(void* self,void*,void* viewport,int blit,unsigned char* hit,int* size){
 Record* previous=current;Record* row=nullptr;const bool previousValid=nativeDrawValid;nativeDrawValid=false;
 if(sampling.load(std::memory_order_relaxed)&&reinterpret_cast<std::uint32_t>(viewport)==config.viewport&&LocalWorld()){
  nativeDrawValid=true;const auto index=count.fetch_add(1);if(index<config.maxFrames){row=&records[index];row->frame=index;row->thread=GetCurrentThreadId();row->tickBefore=ticks.load();row->presentBefore=presents.load();row->begin=Qpc();current=row;}
 }
 drawOriginal(self,viewport,blit,hit,size);
 if(row){row->end=Qpc();row->tickAfter=ticks.load();row->presentAfter=presents.load();row->ready.store(true,std::memory_order_release);}current=previous;nativeDrawValid=previousValid;
}
void __fastcall TickHook(void* self,void*,float dt){if(sampling.load(std::memory_order_relaxed))ticks.fetch_add(1);tickOriginal(self,dt);}
void __fastcall RenderHook(void* self,void*,void* ri){
 if(current){auto& row=*current;++row.renders;row.scene=reinterpret_cast<std::uint32_t>(self);row.renderInterface=reinterpret_cast<std::uint32_t>(ri);
  // These fields are backed by this build's FCameraSceneNode constructor,
  // retained in the private binary audit. This probe NEVER writes them.
  Read(row.scene+0x14,row.matrices.data(),sizeof(row.matrices));
  row.sceneRead=Read(row.scene+0x194,row.origin.data(),12)&&Read(row.scene+0x1d4,row.rotation.data(),12)&&Read(row.scene+0x1e0,&row.fov,4);
 }
 ++playerDepth;renderOriginal(self,ri);--playerDepth;
}

void __fastcall LevelHook(void* self,void*,void* ri){const bool root=nativeDrawValid&&playerDepth&&!levelDepth&&!privateEye;if(current){++current->levelRenders;if(config.targetTrial&&root)ProbeTarget(self,ri);}++levelDepth;levelOriginal(self,ri);--levelDepth;if(config.stereoTrial&&root)ProbeStereo(self,ri);if(config.bridgeTrial&&root)BridgeLevel(self,ri);}
void __fastcall ActorAnimationHook(void* self,void*,float dt){if(current)++current->actorAnimation;actorAnimationOriginal(self,dt);}
int __fastcall SkeletalAnimationHook(void* self,void*,float dt){if(current)++current->skeletalAnimation;return skeletalAnimationOriginal(self,dt);}
void __fastcall GetFrameHook(void* self,void*,void* actor,void* scene,void* vectors,int stride,int* countOut,unsigned long flags){if(current)++current->skeletalFrames;getFrameOriginal(self,actor,scene,vectors,stride,countOut,flags);}
void __fastcall ProcessEventHook(void* self,void*,void* function,void* params,void* result){
 if(current){auto& row=*current;++row.scriptEvents;const auto at=reinterpret_cast<std::uint32_t>(function);const auto name=U32(at+0x24),owner=U32(U32(at+0x1c)+0x24);const unsigned phase=privateEye?3:levelDepth?2:playerDepth?1:0;bool recorded=false;
  for(auto& entry:row.scripts){if(!entry.calls||(entry.name==name&&entry.ownerName==owner&&entry.phase==phase)){entry.name=name;entry.ownerName=owner;entry.phase=phase;++entry.calls;recorded=true;break;}}
  if(!recorded)++row.scriptOverflow;
 }if(config.inputTrial)kfvr::native::InputAuditEvent(self,function,false);processEventOriginal(self,function,params,result);if(config.inputTrial)kfvr::native::InputAuditEvent(self,function,true);
}

struct ColorCapture {std::atomic<bool> ready=false;bool attempted=false,isEx=false,exKnown=false;unsigned width=0,height=0,format=0,samples=0;long result=0;std::vector<unsigned char> pixels;};
ColorCapture capture;
void CaptureColor(void* renderer){
 if(capture.attempted)return;capture.attempted=true;
 using Microsoft::WRL::ComPtr;
 const auto deviceAddress=U32(reinterpret_cast<std::uint32_t>(renderer)+0x495c);
 const auto vtable=U32(deviceAddress);
 // Windows apphelp legitimately keeps a copied COM vtable in private memory.
 // Validate the actual methods we will call, not ownership of the table storage.
 auto methodOwned=[&](unsigned slot,HMODULE owner){const auto method=U32(vtable+slot*4);MEMORY_BASIC_INFORMATION info{};return method&&VirtualQuery(reinterpret_cast<void*>(method),&info,sizeof(info))&&info.AllocationBase==owner&&(info.Protect&(PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE));};
 const auto d3d9=GetModuleHandleW(L"d3d9.dll");
 if(!methodOwned(18,d3d9)||!methodOwned(28,d3d9)||!methodOwned(32,d3d9)||!methodOwned(34,d3d9)||!methodOwned(36,d3d9)){capture.result=E_NOINTERFACE;capture.ready.store(true,std::memory_order_release);return;}
 auto* device=reinterpret_cast<IDirect3DDevice9*>(deviceAddress);
 ComPtr<IDirect3DDevice9Ex> extended;if(methodOwned(0,d3d9)||methodOwned(0,GetModuleHandleW(L"apphelp.dll"))){capture.exKnown=true;capture.isEx=SUCCEEDED(device->QueryInterface(IID_PPV_ARGS(&extended)));}
 ComPtr<IDirect3DSurface9> back,staging,resolved;HRESULT hr=device->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,&back);D3DSURFACE_DESC desc{};
 if(SUCCEEDED(hr))hr=back->GetDesc(&desc);
 capture.width=desc.Width;capture.height=desc.Height;capture.format=unsigned(desc.Format);capture.samples=unsigned(desc.MultiSampleType);
 if(SUCCEEDED(hr)&&(desc.Width>4096||desc.Height>4096||!desc.Width||!desc.Height||(desc.Format!=D3DFMT_A8R8G8B8&&desc.Format!=D3DFMT_X8R8G8B8)))hr=E_INVALIDARG;
 if(SUCCEEDED(hr)&&desc.MultiSampleType!=D3DMULTISAMPLE_NONE){hr=device->CreateRenderTarget(desc.Width,desc.Height,desc.Format,D3DMULTISAMPLE_NONE,0,FALSE,&resolved,nullptr);if(SUCCEEDED(hr))hr=device->StretchRect(back.Get(),nullptr,resolved.Get(),nullptr,D3DTEXF_NONE);}
 if(SUCCEEDED(hr))hr=device->CreateOffscreenPlainSurface(desc.Width,desc.Height,desc.Format,D3DPOOL_SYSTEMMEM,&staging,nullptr);
 if(SUCCEEDED(hr))hr=device->GetRenderTargetData(resolved?resolved.Get():back.Get(),staging.Get());
 D3DLOCKED_RECT locked{};if(SUCCEEDED(hr))hr=staging->LockRect(&locked,nullptr,D3DLOCK_READONLY);
 if(SUCCEEDED(hr)){try{capture.pixels.resize(std::size_t(desc.Width)*desc.Height*4);for(unsigned y=0;y<desc.Height;++y)std::memcpy(capture.pixels.data()+std::size_t(y)*desc.Width*4,static_cast<const char*>(locked.pBits)+std::size_t(y)*locked.Pitch,desc.Width*4);}catch(...){hr=E_OUTOFMEMORY;}staging->UnlockRect();}
 capture.result=hr;capture.ready.store(true,std::memory_order_release);
}
void __fastcall PresentHook(void* self,void*,void* viewport){if(sampling.load(std::memory_order_relaxed)&&reinterpret_cast<std::uint32_t>(viewport)==config.viewport){presents.fetch_add(1);if(LocalWorld()){CaptureColor(self);if(config.targetTrial)ReadTarget();if(config.stereoTrial)ReadStereo();}}if(config.bridgeTrial)kfvr::native::EyeBridgePresent(reinterpret_cast<std::uint32_t>(viewport)==config.viewport&&LocalWorld());presentOriginal(self,viewport);if(config.bridgeTrial)kfvr::native::EyeBridgePresent(reinterpret_cast<std::uint32_t>(viewport)==config.viewport&&LocalWorld());}
bool Executable(void* at,HMODULE owner){MEMORY_BASIC_INFORMATION m{};if(!VirtualQuery(at,&m,sizeof(m))||m.AllocationBase!=owner||m.State!=MEM_COMMIT||m.Protect&(PAGE_GUARD|PAGE_NOACCESS))return false;return (m.Protect&(PAGE_EXECUTE|PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY))!=0;}
struct TargetTrial {
 bool attempted=false,targetsRestored=false,depthRestored=false,viewportRestored=false,cacheRestored=false,stackRestored=false,canary=true,distinct=false;
 std::atomic<bool> ready=false;HRESULT result=E_PENDING;unsigned width=0,height=0,format=0,samples=0,pixelsVerified=0;
 Microsoft::WRL::ComPtr<IDirect3DSurface9> surface;
} targetTrial;
void ProbeTarget(void* scene,void* ri){
 if(targetTrial.attempted)return;targetTrial.attempted=true;
 using Microsoft::WRL::ComPtr;
 const auto address=reinterpret_cast<std::uint32_t>(ri),table=U32(address),renderer=U32(address+4),deviceAddress=U32(renderer+0x495c);
 for(unsigned slot=0;slot<5;++slot)if(U32(table+slot*4)!=reinterpret_cast<std::uint32_t>(targetMethods[slot])){targetTrial.result=E_NOINTERFACE;targetTrial.ready=true;return;}
 const auto d3d=GetModuleHandleW(L"d3d9.dll");const auto deviceTable=U32(deviceAddress);
 for(unsigned slot:{38u,40u,48u})if(!Executable(reinterpret_cast<void*>(U32(deviceTable+slot*4)),d3d)){targetTrial.result=E_NOINTERFACE;targetTrial.ready=true;return;}
 const auto stack=U32(address+0x59870),cache=U32(address+0x5940c);std::array<unsigned char,0x594> before{},after{};
 if(stack>240||!Read(cache,before.data(),before.size())){targetTrial.result=E_UNEXPECTED;targetTrial.ready=true;return;}
 auto* device=reinterpret_cast<IDirect3DDevice9*>(deviceAddress);ComPtr<IDirect3DSurface9> previous,depth;D3DVIEWPORT9 viewport{};
 if(FAILED(device->GetRenderTarget(0,&previous))||FAILED(device->GetDepthStencilSurface(&depth))||FAILED(device->GetViewport(&viewport))){targetTrial.result=E_FAIL;targetTrial.ready=true;return;}
 alignas(16) std::array<unsigned char,64> aux;aux.fill(0xcd);auxCtor(aux.data(),512,384,5);
 const auto push=reinterpret_cast<SimpleCall>(targetMethods[0]),pop=reinterpret_cast<SimpleCall>(targetMethods[1]);
 const auto setTarget=reinterpret_cast<int(__thiscall*)(void*,void*)>(targetMethods[2]);
 const auto setViewport=reinterpret_cast<void(__thiscall*)(void*,int,int,int,int)>(targetMethods[3]);
 const auto clear=reinterpret_cast<void(__thiscall*)(void*,int,unsigned,int,float,int,unsigned)>(targetMethods[4]);
 if(config.cameraTrial)kfvr::native::ProbeCameras(scene,aux.data());
 push(ri);
 const int bound=setTarget(ri,aux.data());
 if(bound){setViewport(ri,0,0,512,384);clear(ri,1,0xff3040c0,1,1.f,1,0);targetTrial.result=device->GetRenderTarget(0,&targetTrial.surface);
  if(SUCCEEDED(targetTrial.result)){D3DSURFACE_DESC desc{};targetTrial.result=targetTrial.surface->GetDesc(&desc);targetTrial.width=desc.Width;targetTrial.height=desc.Height;targetTrial.format=unsigned(desc.Format);targetTrial.samples=unsigned(desc.MultiSampleType);targetTrial.distinct=targetTrial.surface.Get()!=previous.Get();}
 }else targetTrial.result=E_FAIL;
 pop(ri);
 auxDtor(aux.data());
 ComPtr<IDirect3DSurface9> restored,restoredDepth;D3DVIEWPORT9 restoredViewport{};
 targetTrial.targetsRestored=SUCCEEDED(device->GetRenderTarget(0,&restored))&&restored.Get()==previous.Get();
 targetTrial.depthRestored=SUCCEEDED(device->GetDepthStencilSurface(&restoredDepth))&&restoredDepth.Get()==depth.Get();
 targetTrial.viewportRestored=SUCCEEDED(device->GetViewport(&restoredViewport))&&std::memcmp(&viewport,&restoredViewport,sizeof(viewport))==0;
 targetTrial.stackRestored=U32(address+0x59870)==stack&&U32(address+0x5940c)==cache;
 targetTrial.cacheRestored=Read(cache,after.data(),after.size())&&before==after;
 for(unsigned i=28;i<aux.size();++i)targetTrial.canary&=aux[i]==0xcd;
 if(FAILED(targetTrial.result)){targetTrial.surface.Reset();targetTrial.ready.store(true,std::memory_order_release);}
}
void ReadTarget(){
 if(!targetTrial.attempted||targetTrial.ready.load(std::memory_order_acquire))return;
 using Microsoft::WRL::ComPtr;
 if(!targetTrial.surface||targetTrial.width!=512||targetTrial.height!=384||targetTrial.samples!=0||(targetTrial.format!=D3DFMT_A8R8G8B8&&targetTrial.format!=D3DFMT_X8R8G8B8)){targetTrial.result=E_INVALIDARG;targetTrial.surface.Reset();targetTrial.ready=true;return;}
 ComPtr<IDirect3DDevice9> device;ComPtr<IDirect3DSurface9> staging;HRESULT hr=targetTrial.surface->GetDevice(&device);
 if(SUCCEEDED(hr))hr=device->CreateOffscreenPlainSurface(512,384,D3DFORMAT(targetTrial.format),D3DPOOL_SYSTEMMEM,&staging,nullptr);
 if(SUCCEEDED(hr))hr=device->GetRenderTargetData(targetTrial.surface.Get(),staging.Get());
 D3DLOCKED_RECT lock{};if(SUCCEEDED(hr))hr=staging->LockRect(&lock,nullptr,D3DLOCK_READONLY);
 if(SUCCEEDED(hr)){if(!lock.pBits||lock.Pitch<512*4)hr=E_INVALIDARG;else for(unsigned y=0;y<384;++y){const auto* row=reinterpret_cast<const unsigned*>(static_cast<const char*>(lock.pBits)+std::size_t(y)*lock.Pitch);for(unsigned x=0;x<512;++x)targetTrial.pixelsVerified+=(row[x]|0xff000000)==0xff3040c0;}staging->UnlockRect();}
 targetTrial.result=hr;targetTrial.surface.Reset();targetTrial.ready.store(true,std::memory_order_release);
}


bool RenderBridgePair(void* scene,void* ri,const kfvr::runtime::PresentationRequirements& requirements,const kfvr::runtime::TrackingFrame& tracking,kfvr::native::NativeSurfaces& surfaces){
 using Microsoft::WRL::ComPtr;
 const auto address=reinterpret_cast<unsigned>(ri),renderer=U32(address+4);auto* device=reinterpret_cast<IDirect3DDevice9*>(U32(renderer+0x495c));
 const auto stack=U32(address+0x59870),cache=U32(address+0x5940c);std::array<unsigned char,0x594> before{},after{};
 ComPtr<IDirect3DSurface9> original,depth;D3DVIEWPORT9 viewport{};
 if(stack>240||!Read(cache,before.data(),before.size())||FAILED(device->GetRenderTarget(0,&original))||FAILED(device->GetDepthStencilSurface(&depth))||FAILED(device->GetViewport(&viewport)))return false;
 const auto push=reinterpret_cast<SimpleCall>(targetMethods[0]),pop=reinterpret_cast<SimpleCall>(targetMethods[1]);
 const auto bind=reinterpret_cast<int(__thiscall*)(void*,void*)>(targetMethods[2]);
 const auto setViewport=reinterpret_cast<void(__thiscall*)(void*,int,int,int,int)>(targetMethods[3]);
 const auto clear=reinterpret_cast<void(__thiscall*)(void*,int,unsigned,int,float,int,unsigned)>(targetMethods[4]);
 bool okay=true;
 for(unsigned eye=0;eye<2&&okay;++eye){
  alignas(16) std::array<unsigned char,64> aux;aux.fill(0xcd);auxCtor(aux.data(),int(requirements.width),int(requirements.height),5);push(ri);
  if(!bind(ri,aux.data()))okay=false;
  else {
   setViewport(ri,0,0,int(requirements.width),int(requirements.height));clear(ri,1,0xff000000,1,1.f,1,0);
   privateEye=true;replayed=true;okay=kfvr::native::RenderTrackedCamera(scene,aux.data(),ri,tracking,eye,requirements.width,requirements.height,config.unitsPerMetre);privateEye=false;
   if(okay)okay=SUCCEEDED(device->GetRenderTarget(0,&surfaces[eye]));
   if(okay){D3DSURFACE_DESC desc{};okay=SUCCEEDED(surfaces[eye]->GetDesc(&desc))&&desc.Width==requirements.width&&desc.Height==requirements.height&&desc.MultiSampleType==D3DMULTISAMPLE_NONE&&desc.Format==D3DFMT_A8R8G8B8&&surfaces[eye].Get()!=original.Get();}
  }
  pop(ri);auxDtor(aux.data());
  for(unsigned i=28;i<aux.size();++i)okay&=aux[i]==0xcd;
  ComPtr<IDirect3DSurface9> restored,restoredDepth;D3DVIEWPORT9 restoredViewport{};
  okay&=SUCCEEDED(device->GetRenderTarget(0,&restored))&&restored.Get()==original.Get()&&SUCCEEDED(device->GetDepthStencilSurface(&restoredDepth))&&restoredDepth.Get()==depth.Get()&&SUCCEEDED(device->GetViewport(&restoredViewport))&&std::memcmp(&viewport,&restoredViewport,sizeof(viewport))==0&&U32(address+0x59870)==stack&&U32(address+0x5940c)==cache&&Read(cache,after.data(),after.size())&&before==after;
 }
 return okay&&surfaces[0]&&surfaces[1]&&surfaces[0].Get()!=surfaces[1].Get();
}
void BridgeLevel(void* scene,void* ri){
 if(!targetTrial.ready.load(std::memory_order_acquire))return;
 if(FAILED(targetTrial.result)||!targetTrial.targetsRestored||!targetTrial.depthRestored||!targetTrial.viewportRestored||!targetTrial.cacheRestored||!targetTrial.stackRestored||!targetTrial.canary){kfvr::native::StopEyeBridge();return;}
 const auto address=reinterpret_cast<unsigned>(ri),table=U32(address);
 for(unsigned slot=0;slot<5;++slot)if(U32(table+slot*4)!=reinterpret_cast<unsigned>(targetMethods[slot])){kfvr::native::StopEyeBridge();return;}
 auto* device=reinterpret_cast<IDirect3DDevice9*>(U32(U32(address+4)+0x495c));
 kfvr::native::EyeBridgeLevel(scene,ri,device,RenderBridgePair);
}

struct StereoTrial {
 bool attempted=false,restored=true,canaries=true,cameras=true,distinct=false;std::atomic<bool> ready=false;
 HRESULT result=E_PENDING;unsigned renders=0,frames=0,scripts=0,animation=0,tickBefore=0,tickAfter=0;long long renderQpc=0;
 std::array<Microsoft::WRL::ComPtr<IDirect3DSurface9>,2> surfaces;
 std::array<std::vector<unsigned char>,2> pixels;
 std::array<kfvr::ue2::SceneViewData,2> camerasData;
} stereo;
void ProbeStereo(void* scene,void* ri){
 if(stereo.attempted)return;stereo.attempted=true;
 using Microsoft::WRL::ComPtr;
 if(FAILED(targetTrial.result)||!targetTrial.targetsRestored||!targetTrial.depthRestored||!targetTrial.viewportRestored||!targetTrial.cacheRestored||!targetTrial.stackRestored||!targetTrial.canary){stereo.result=E_UNEXPECTED;stereo.ready=true;return;}
 const auto address=reinterpret_cast<unsigned>(ri),renderer=U32(address+4);auto* device=reinterpret_cast<IDirect3DDevice9*>(U32(renderer+0x495c));
 const auto stack=U32(address+0x59870),cache=U32(address+0x5940c);std::array<unsigned char,0x594> before{},after{};
 ComPtr<IDirect3DSurface9> original,depth;D3DVIEWPORT9 viewport{};
 if(!Read(cache,before.data(),before.size())||FAILED(device->GetRenderTarget(0,&original))||FAILED(device->GetDepthStencilSurface(&depth))||FAILED(device->GetViewport(&viewport))){stereo.result=E_FAIL;stereo.ready=true;return;}
 const auto push=reinterpret_cast<SimpleCall>(targetMethods[0]),pop=reinterpret_cast<SimpleCall>(targetMethods[1]);
 const auto bind=reinterpret_cast<int(__thiscall*)(void*,void*)>(targetMethods[2]);
 const auto setViewport=reinterpret_cast<void(__thiscall*)(void*,int,int,int,int)>(targetMethods[3]);
 const auto clear=reinterpret_cast<void(__thiscall*)(void*,int,unsigned,int,float,int,unsigned)>(targetMethods[4]);
 const unsigned oldFrames=current->skeletalFrames,oldScripts=current->scriptEvents,oldAnimation=current->actorAnimation+current->skeletalAnimation;
 stereo.tickBefore=ticks.load();const auto start=Qpc();stereo.result=S_OK;
 for(unsigned eye=0;eye<2&&SUCCEEDED(stereo.result);++eye){
  alignas(16) std::array<unsigned char,64> aux;aux.fill(0xcd);auxCtor(aux.data(),512,384,5);push(ri);
  if(!bind(ri,aux.data()))stereo.result=E_FAIL;
  else {
   setViewport(ri,0,0,512,384);clear(ri,1,0xff000000,1,1.f,1,0);
   privateEye=true;replayed=true;stereo.cameras&=kfvr::native::RenderDiagnosticCamera(scene,aux.data(),ri,eye?1.6f:-1.6f,config.stereoTrial,stereo.camerasData[eye]);privateEye=false;++stereo.renders;
   stereo.result=device->GetRenderTarget(0,&stereo.surfaces[eye]);
   if(SUCCEEDED(stereo.result)){D3DSURFACE_DESC desc{};stereo.result=stereo.surfaces[eye]->GetDesc(&desc);if(desc.Width!=512||desc.Height!=384||desc.MultiSampleType!=D3DMULTISAMPLE_NONE||desc.Format!=D3DFMT_A8R8G8B8||stereo.surfaces[eye].Get()==original.Get())stereo.result=E_UNEXPECTED;}
  }
  pop(ri);auxDtor(aux.data());
  for(unsigned i=28;i<aux.size();++i)stereo.canaries&=aux[i]==0xcd;
  ComPtr<IDirect3DSurface9> restored,restoredDepth;D3DVIEWPORT9 restoredViewport{};
  stereo.restored&=SUCCEEDED(device->GetRenderTarget(0,&restored))&&restored.Get()==original.Get()&&SUCCEEDED(device->GetDepthStencilSurface(&restoredDepth))&&restoredDepth.Get()==depth.Get()&&SUCCEEDED(device->GetViewport(&restoredViewport))&&std::memcmp(&viewport,&restoredViewport,sizeof(viewport))==0&&U32(address+0x59870)==stack&&U32(address+0x5940c)==cache&&Read(cache,after.data(),after.size())&&before==after;
  if(!stereo.restored||!stereo.canaries||!stereo.cameras)stereo.result=E_UNEXPECTED;
 }
 stereo.renderQpc=Qpc()-start;stereo.tickAfter=ticks.load();stereo.frames=current->skeletalFrames-oldFrames;stereo.scripts=current->scriptEvents-oldScripts;stereo.animation=current->actorAnimation+current->skeletalAnimation-oldAnimation;
 stereo.distinct=stereo.surfaces[0]&&stereo.surfaces[1]&&stereo.surfaces[0].Get()!=stereo.surfaces[1].Get();
 if(FAILED(stereo.result)){for(auto& surface:stereo.surfaces)surface.Reset();stereo.ready.store(true,std::memory_order_release);}
}
void ReadStereo(){
 if(!stereo.attempted||stereo.ready.load(std::memory_order_acquire))return;
 using Microsoft::WRL::ComPtr;
 HRESULT hr=S_OK;
 for(unsigned eye=0;eye<2&&SUCCEEDED(hr);++eye){
  ComPtr<IDirect3DDevice9> device;ComPtr<IDirect3DSurface9> staging;
  if(!stereo.surfaces[eye]){hr=E_POINTER;break;}
  hr=stereo.surfaces[eye]->GetDevice(&device);
  if(SUCCEEDED(hr))hr=device->CreateOffscreenPlainSurface(512,384,D3DFMT_A8R8G8B8,D3DPOOL_SYSTEMMEM,&staging,nullptr);
  if(SUCCEEDED(hr))hr=device->GetRenderTargetData(stereo.surfaces[eye].Get(),staging.Get());
  D3DLOCKED_RECT lock{};if(SUCCEEDED(hr))hr=staging->LockRect(&lock,nullptr,D3DLOCK_READONLY);
  if(SUCCEEDED(hr)){try{if(!lock.pBits||lock.Pitch<2048)hr=E_INVALIDARG;else{stereo.pixels[eye].resize(512*384*4);for(unsigned y=0;y<384;++y)std::memcpy(stereo.pixels[eye].data()+y*2048,static_cast<const char*>(lock.pBits)+std::size_t(y)*lock.Pitch,2048);}}catch(...){hr=E_OUTOFMEMORY;}staging->UnlockRect();}
 }
 stereo.result=hr;for(auto& surface:stereo.surfaces)surface.Reset();stereo.ready.store(true,std::memory_order_release);
}
void WriteStereo(const std::filesystem::path& folder){
 std::ofstream out(folder/L"native-stereo.json");if(!stereo.ready.load(std::memory_order_acquire)){out<<"{\"ready\":false}\n";return;}
 LARGE_INTEGER frequency;QueryPerformanceFrequency(&frequency);
 out<<std::boolalpha<<"{\"ready\":true,\"hresult\":"<<stereo.result<<",\"renders\":"<<stereo.renders<<",\"state_restored\":"<<stereo.restored<<",\"canaries_intact\":"<<stereo.canaries<<",\"cameras_preserved\":"<<stereo.cameras<<",\"distinct_surfaces\":"<<stereo.distinct<<",\"engine_ticks_during_trial\":"<<stereo.tickAfter-stereo.tickBefore<<",\"skeletal_getframe_calls\":"<<stereo.frames<<",\"script_calls\":"<<stereo.scripts<<",\"selected_animation_calls\":"<<stereo.animation<<",\"render_and_target_ms\":"<<(double(stereo.renderQpc)*1000/frequency.QuadPart)<<",\"asymmetric_diagnostic_fov\":"<<(config.stereoTrial>=2)<<",\"synthetic_head_pose\":"<<(config.stereoTrial==3)<<",\"world_scale_calibrated\":false,\"headset_presented\":false}\n";
 if(FAILED(stereo.result))return;
 std::ofstream cameraOut(folder/L"native-eye-cameras.json");cameraOut.precision(9);cameraOut<<"{\"eyes\":[";
 for(unsigned eye=0;eye<2;++eye){if(eye)cameraOut<<',';cameraOut<<"[";bool first=true;for(const auto& matrix:stereo.camerasData[eye].matrices)for(const auto& row:matrix.values)for(float value:row){if(!first)cameraOut<<',';first=false;cameraOut<<value;}cameraOut<<"]";}cameraOut<<"]}\n";
 for(unsigned eye=0;eye<2;++eye){if(stereo.pixels[eye].size()!=512*384*4)continue;BITMAPFILEHEADER header{};BITMAPINFOHEADER info{};header.bfType=0x4d42;header.bfOffBits=sizeof(header)+sizeof(info);header.bfSize=header.bfOffBits+DWORD(stereo.pixels[eye].size());info.biSize=sizeof(info);info.biWidth=512;info.biHeight=-384;info.biPlanes=1;info.biBitCount=32;info.biCompression=BI_RGB;std::ofstream image(folder/(eye?L"native-right.bmp":L"native-left.bmp"),std::ios::binary);image.write(reinterpret_cast<const char*>(&header),sizeof(header));image.write(reinterpret_cast<const char*>(&info),sizeof(info));image.write(reinterpret_cast<const char*>(stereo.pixels[eye].data()),stereo.pixels[eye].size());}
}

}
extern "C" __declspec(dllexport) DWORD WINAPI KfvrRunNativeProbe(void* request){
 if(InterlockedCompareExchange(&started,1,0))return 10;
 if(!Read(reinterpret_cast<std::uint32_t>(request),&config,sizeof(config))||config.bytes!=sizeof(config)||config.version!=7||config.inputTrial>1||config.bridgeTrial>1||(config.bridgeTrial&&(!config.cameraTrial||config.stereoTrial||config.channelToken[32]!=0||!config.channelToken[0]||!(config.unitsPerMetre>=1&&config.unitsPerMetre<=200)))||config.targetTrial>1||config.cameraTrial>config.targetTrial||(config.stereoTrial>3||(config.stereoTrial&&!config.cameraTrial))||config.durationMs>15000||config.durationMs<100||!config.maxFrames||config.maxFrames>records.size()||config.reportPath[259]!=0||!config.reportPath[0]||!LocalWorld())return 11;
 const auto engine=GetModuleHandleW(L"Engine.dll"),renderer=GetModuleHandleW(L"D3D9Drv.dll"),core=GetModuleHandleW(L"Core.dll");if(!engine||!renderer||!core)return 12;
 const char* names[]={"?Draw@UGameEngine@@UAEXPAVUViewport@@HPAEPAH@Z","?Tick@UGameEngine@@UAEXM@Z","?Render@FPlayerSceneNode@@UAEXPAVFRenderInterface@@@Z","?Present@UD3D9RenderDevice@@UAEXPAVUViewport@@@Z","?Render@FLevelSceneNode@@UAEXPAVFRenderInterface@@@Z","?UpdateAnimation@AActor@@UAEXM@Z","?UpdateAnimation@USkeletalMeshInstance@@UAEHM@Z","?GetFrame@USkeletalMeshInstance@@UAEXPAVAActor@@PAVFLevelSceneNode@@PAVFVector@@HAAHK@Z","?ProcessEvent@UObject@@UAEXPAVUFunction@@PAX1@Z"};
 for(unsigned i=0;i<targets.size();++i){auto owner=i==3?renderer:i==8?core:engine;targets[i]=reinterpret_cast<void*>(GetProcAddress(owner,names[i]));std::array<unsigned char,16> bytes{};if(!targets[i]||!Executable(targets[i],owner)||!Read(reinterpret_cast<std::uint32_t>(targets[i]),bytes.data(),bytes.size())||std::memcmp(bytes.data(),config.prologues[i],16))return 13;}

 if(config.targetTrial){
  constexpr unsigned rvas[]={0x18860,0x1b240,0x189d0,0x17150,0x19100};
  auxCtor=reinterpret_cast<AuxCtor>(GetProcAddress(engine,"??0FAuxRenderTarget@@QAE@HHW4ETextureFormat@@@Z"));auxDtor=reinterpret_cast<SimpleCall>(GetProcAddress(engine,"??1FAuxRenderTarget@@QAE@XZ"));
  for(unsigned i=0;i<7;++i){void* method=i<5?reinterpret_cast<unsigned char*>(renderer)+rvas[i]:i==5?reinterpret_cast<void*>(auxCtor):reinterpret_cast<void*>(auxDtor);auto owner=i<5?renderer:engine;std::array<unsigned char,16> bytes{};
   if(!method||!Executable(method,owner)||!Read(reinterpret_cast<std::uint32_t>(method),bytes.data(),16)||std::memcmp(bytes.data(),config.targetPrologues[i],16))return 21;if(i<5)targetMethods[i]=method;
  }
 }
 if(config.cameraTrial){
  const char* cameraNames[]={"??0FCameraSceneNode@@QAE@PAVUViewport@@PAVFRenderTarget@@PAVAActor@@VFVector@@VFRotator@@M@Z","??1FCameraSceneNode@@UAE@XZ","?Render@FCameraSceneNode@@UAEXPAVFRenderInterface@@@Z"};void* methods[3]{};
  for(unsigned i=0;i<3;++i){methods[i]=reinterpret_cast<void*>(GetProcAddress(engine,cameraNames[i]));std::array<unsigned char,16> bytes{};if(!methods[i]||!Executable(methods[i],engine)||!Read(reinterpret_cast<unsigned>(methods[i]),bytes.data(),16)||std::memcmp(bytes.data(),config.cameraPrologues[i],16))return 22;}
  kfvr::native::SetCameraMethods(methods[0],methods[1],methods[2]);
 }
 if(MH_Initialize()!=MH_OK)return 14;
 if(config.bridgeTrial&&!kfvr::native::StartEyeBridge(config.channelToken))return 23;
 if(config.inputTrial){std::array<unsigned,10> fields{};std::copy(std::begin(config.inputOffsets),std::end(config.inputOffsets),fields.begin());if(!kfvr::native::StartInputAudit(config.viewport,config.controller,fields,config.playerTickName,config.inputPrologue,LocalWorld))return 24;}
 void* detours[]={reinterpret_cast<void*>(&DrawHook),reinterpret_cast<void*>(&TickHook),reinterpret_cast<void*>(&RenderHook),reinterpret_cast<void*>(&PresentHook),reinterpret_cast<void*>(&LevelHook),reinterpret_cast<void*>(&ActorAnimationHook),reinterpret_cast<void*>(&SkeletalAnimationHook),reinterpret_cast<void*>(&GetFrameHook),reinterpret_cast<void*>(&ProcessEventHook)};
 void** originals[]={reinterpret_cast<void**>(&drawOriginal),reinterpret_cast<void**>(&tickOriginal),reinterpret_cast<void**>(&renderOriginal),reinterpret_cast<void**>(&presentOriginal),reinterpret_cast<void**>(&levelOriginal),reinterpret_cast<void**>(&actorAnimationOriginal),reinterpret_cast<void**>(&skeletalAnimationOriginal),reinterpret_cast<void**>(&getFrameOriginal),reinterpret_cast<void**>(&processEventOriginal)};
 unsigned installed=0;bool enabled=false;DWORD status=0;
 for(unsigned i=0;i<targets.size();++i){if(MH_CreateHook(targets[i],detours[i],originals[i])!=MH_OK){status=15;break;}++installed;if(MH_QueueEnableHook(targets[i])!=MH_OK){status=16;break;}}
 if(!status){if(MH_ApplyQueued()!=MH_OK)status=17;else enabled=true;}
 if(enabled){sampling.store(true,std::memory_order_release);const auto start=GetTickCount64();while((config.bridgeTrial||count.load()<config.maxFrames)&&GetTickCount64()-start<config.durationMs&&LocalWorld()&&(!config.bridgeTrial||!kfvr::native::EyeBridgeClosed()))Sleep(10);}
 if(config.bridgeTrial){kfvr::native::StopEyeBridge();const auto deadline=GetTickCount64()+5000;while(!kfvr::native::EyeBridgeClosed()&&GetTickCount64()<deadline)Sleep(10);}
 sampling.store(false,std::memory_order_release);
 const bool mayDisable=!config.bridgeTrial||kfvr::native::EyeBridgeClosed();bool disabled=mayDisable;if(config.inputTrial)disabled&=kfvr::native::StopInputAudit();for(unsigned i=0;mayDisable&&i<installed;++i){const auto result=MH_DisableHook(targets[i]);disabled&=result==MH_OK||result==MH_ERROR_DISABLED;}
 // Keep trampolines and this DLL resident: an original callback may be returning.
 Sleep(50);
 try{

 if(config.inputTrial)kfvr::native::WriteInputAudit(std::filesystem::path(config.reportPath).parent_path());
 if(config.bridgeTrial)kfvr::native::WriteEyeBridgeReport(std::filesystem::path(config.reportPath).parent_path());
 if(config.stereoTrial)WriteStereo(std::filesystem::path(config.reportPath).parent_path());
 if(config.cameraTrial)kfvr::native::WriteCameraReport(std::filesystem::path(config.reportPath).parent_path());
 if(config.targetTrial){std::ofstream target(std::filesystem::path(config.reportPath).parent_path()/L"native-target.json");if(!targetTrial.ready.load(std::memory_order_acquire)){target<<"{\"ready\":false,\"render_replayed\":false,\"camera_written\":false}\n";}else target<<std::boolalpha<<"{\"attempted\":"<<targetTrial.attempted<<",\"ready\":"<<targetTrial.ready.load(std::memory_order_acquire)<<",\"hresult\":"<<targetTrial.result<<",\"width\":"<<targetTrial.width<<",\"height\":"<<targetTrial.height<<",\"format\":"<<targetTrial.format<<",\"samples\":"<<targetTrial.samples<<",\"distinct_surface\":"<<targetTrial.distinct<<",\"color_target_restored\":"<<targetTrial.targetsRestored<<",\"depth_target_restored\":"<<targetTrial.depthRestored<<",\"viewport_restored\":"<<targetTrial.viewportRestored<<",\"state_stack_restored\":"<<targetTrial.stackRestored<<",\"cached_state_restored\":"<<targetTrial.cacheRestored<<",\"constructor_canary_intact\":"<<targetTrial.canary<<",\"verified_pixels\":"<<targetTrial.pixelsVerified<<",\"render_replayed\":false,\"camera_written\":false}\n";}
 if(capture.ready.load(std::memory_order_acquire)){
  const auto folder=std::filesystem::path(config.reportPath).parent_path();std::ofstream meta(folder/L"native-color.json");meta<<"{\"width\":"<<capture.width<<",\"height\":"<<capture.height<<",\"format\":"<<capture.format<<",\"samples\":"<<capture.samples<<",\"d3d9ex_known\":"<<(capture.exKnown?"true":"false")<<",\"d3d9ex\":"<<(capture.isEx?"true":"false")<<",\"hresult\":"<<capture.result<<"}\n";
  if(SUCCEEDED(capture.result)&&!capture.pixels.empty()){BITMAPFILEHEADER file{};BITMAPINFOHEADER info{};file.bfType=0x4d42;file.bfOffBits=sizeof(file)+sizeof(info);file.bfSize=file.bfOffBits+DWORD(capture.pixels.size());info.biSize=sizeof(info);info.biWidth=LONG(capture.width);info.biHeight=-LONG(capture.height);info.biPlanes=1;info.biBitCount=32;info.biCompression=BI_RGB;std::ofstream image(folder/L"native-color.bmp",std::ios::binary);image.write(reinterpret_cast<const char*>(&file),sizeof(file));image.write(reinterpret_cast<const char*>(&info),sizeof(info));image.write(reinterpret_cast<const char*>(capture.pixels.data()),capture.pixels.size());}
 }
 if(records[0].ready.load(std::memory_order_acquire)){std::ofstream camera(std::filesystem::path(config.reportPath).parent_path()/L"native-camera.json");camera<<"{\"matrices\":[";bool first=true;for(float value:records[0].matrices){if(!first)camera<<',';first=false;camera.precision(9);camera<<value;}camera<<"]}\n";}
 std::ofstream out(std::filesystem::path(config.reportPath));if(!out)return 18;out<<"{\n\"status\":"<<status<<",\"hooks_disabled\":"<<(disabled?"true":"false")<<",\"tick_calls\":"<<ticks.load()<<",\"present_calls\":"<<presents.load()<<",\"camera_written\":false,\"render_replayed\":"<<(replayed.load()?"true":"false")<<",\"frames\":[";bool first=true;
  for(auto& row:records){if(!row.ready.load(std::memory_order_acquire))continue;if(!first)out<<',';first=false;out<<"\n{\"frame\":"<<row.frame<<",\"thread\":"<<row.thread<<",\"ticks_before\":"<<row.tickBefore<<",\"ticks_after\":"<<row.tickAfter<<",\"renders\":"<<row.renders<<",\"presents_before\":"<<row.presentBefore<<",\"presents_after\":"<<row.presentAfter<<",\"scene\":"<<row.scene<<",\"render_interface\":"<<row.renderInterface<<",\"origin\":["<<row.origin[0]<<','<<row.origin[1]<<','<<row.origin[2]<<"],\"rotation\":["<<row.rotation[0]<<','<<row.rotation[1]<<','<<row.rotation[2]<<"],\"fov\":"<<row.fov<<",\"scene_read\":"<<(row.sceneRead?"true":"false")<<",\"begin_qpc\":"<<row.begin<<",\"end_qpc\":"<<row.end<<",\"level_renders\":"<<row.levelRenders<<",\"actor_animation_calls_inside_draw\":"<<row.actorAnimation<<",\"skeletal_animation_calls_inside_draw\":"<<row.skeletalAnimation<<",\"skeletal_getframe_calls\":"<<row.skeletalFrames<<",\"script_calls\":"<<row.scriptEvents<<",\"script_overflow\":"<<row.scriptOverflow<<",\"scripts\":[";
   bool firstScript=true;for(const auto& script:row.scripts){if(!script.calls)continue;if(!firstScript)out<<',';firstScript=false;out<<"{\"name_id\":"<<script.name<<",\"owner_name_id\":"<<script.ownerName<<",\"phase\":"<<script.phase<<",\"calls\":"<<script.calls<<'}';}out<<"]}";
  }out<<"\n]}\n";
 }catch(...){return 19;}return !disabled?20:status;
}
BOOL WINAPI DllMain(HINSTANCE module,DWORD reason,void*){if(reason==DLL_PROCESS_ATTACH)DisableThreadLibraryCalls(module);return TRUE;}
