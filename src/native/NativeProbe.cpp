#include <Windows.h>
#include <MinHook.h>
#include <d3d9.h>
#include <wrl/client.h>
#include <vector>
#include "NativeProbeConfig.h"
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
 Record* previous=current;Record* row=nullptr;
 if(sampling.load(std::memory_order_relaxed)&&reinterpret_cast<std::uint32_t>(viewport)==config.viewport&&LocalWorld()){
  const auto index=count.fetch_add(1);if(index<config.maxFrames){row=&records[index];row->frame=index;row->thread=GetCurrentThreadId();row->tickBefore=ticks.load();row->presentBefore=presents.load();row->begin=Qpc();current=row;}
 }
 drawOriginal(self,viewport,blit,hit,size);
 if(row){row->end=Qpc();row->tickAfter=ticks.load();row->presentAfter=presents.load();row->ready.store(true,std::memory_order_release);}current=previous;
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

void __fastcall LevelHook(void* self,void*,void* ri){if(current)++current->levelRenders;++levelDepth;levelOriginal(self,ri);--levelDepth;}
void __fastcall ActorAnimationHook(void* self,void*,float dt){if(current)++current->actorAnimation;actorAnimationOriginal(self,dt);}
int __fastcall SkeletalAnimationHook(void* self,void*,float dt){if(current)++current->skeletalAnimation;return skeletalAnimationOriginal(self,dt);}
void __fastcall GetFrameHook(void* self,void*,void* actor,void* scene,void* vectors,int stride,int* countOut,unsigned long flags){if(current)++current->skeletalFrames;getFrameOriginal(self,actor,scene,vectors,stride,countOut,flags);}
void __fastcall ProcessEventHook(void* self,void*,void* function,void* params,void* result){
 if(current){auto& row=*current;++row.scriptEvents;const auto at=reinterpret_cast<std::uint32_t>(function);const auto name=U32(at+0x24),owner=U32(U32(at+0x1c)+0x24);const unsigned phase=levelDepth?2:playerDepth?1:0;bool recorded=false;
  for(auto& entry:row.scripts){if(!entry.calls||(entry.name==name&&entry.ownerName==owner&&entry.phase==phase)){entry.name=name;entry.ownerName=owner;entry.phase=phase;++entry.calls;recorded=true;break;}}
  if(!recorded)++row.scriptOverflow;
 }processEventOriginal(self,function,params,result);
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
void __fastcall PresentHook(void* self,void*,void* viewport){if(sampling.load(std::memory_order_relaxed)&&reinterpret_cast<std::uint32_t>(viewport)==config.viewport){presents.fetch_add(1);if(LocalWorld())CaptureColor(self);}presentOriginal(self,viewport);}
bool Executable(void* at,HMODULE owner){MEMORY_BASIC_INFORMATION m{};if(!VirtualQuery(at,&m,sizeof(m))||m.AllocationBase!=owner||m.State!=MEM_COMMIT||m.Protect&(PAGE_GUARD|PAGE_NOACCESS))return false;return (m.Protect&(PAGE_EXECUTE|PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY))!=0;}
}
extern "C" __declspec(dllexport) DWORD WINAPI KfvrRunNativeProbe(void* request){
 if(InterlockedCompareExchange(&started,1,0))return 10;
 if(!Read(reinterpret_cast<std::uint32_t>(request),&config,sizeof(config))||config.bytes!=sizeof(config)||config.version!=2||config.durationMs>15000||config.durationMs<100||!config.maxFrames||config.maxFrames>records.size()||config.reportPath[259]!=0||!config.reportPath[0]||!LocalWorld())return 11;
 const auto engine=GetModuleHandleW(L"Engine.dll"),renderer=GetModuleHandleW(L"D3D9Drv.dll"),core=GetModuleHandleW(L"Core.dll");if(!engine||!renderer||!core)return 12;
 const char* names[]={"?Draw@UGameEngine@@UAEXPAVUViewport@@HPAEPAH@Z","?Tick@UGameEngine@@UAEXM@Z","?Render@FPlayerSceneNode@@UAEXPAVFRenderInterface@@@Z","?Present@UD3D9RenderDevice@@UAEXPAVUViewport@@@Z","?Render@FLevelSceneNode@@UAEXPAVFRenderInterface@@@Z","?UpdateAnimation@AActor@@UAEXM@Z","?UpdateAnimation@USkeletalMeshInstance@@UAEHM@Z","?GetFrame@USkeletalMeshInstance@@UAEXPAVAActor@@PAVFLevelSceneNode@@PAVFVector@@HAAHK@Z","?ProcessEvent@UObject@@UAEXPAVUFunction@@PAX1@Z"};
 for(unsigned i=0;i<targets.size();++i){auto owner=i==3?renderer:i==8?core:engine;targets[i]=reinterpret_cast<void*>(GetProcAddress(owner,names[i]));std::array<unsigned char,16> bytes{};if(!targets[i]||!Executable(targets[i],owner)||!Read(reinterpret_cast<std::uint32_t>(targets[i]),bytes.data(),bytes.size())||std::memcmp(bytes.data(),config.prologues[i],16))return 13;}
 if(MH_Initialize()!=MH_OK)return 14;
 void* detours[]={reinterpret_cast<void*>(&DrawHook),reinterpret_cast<void*>(&TickHook),reinterpret_cast<void*>(&RenderHook),reinterpret_cast<void*>(&PresentHook),reinterpret_cast<void*>(&LevelHook),reinterpret_cast<void*>(&ActorAnimationHook),reinterpret_cast<void*>(&SkeletalAnimationHook),reinterpret_cast<void*>(&GetFrameHook),reinterpret_cast<void*>(&ProcessEventHook)};
 void** originals[]={reinterpret_cast<void**>(&drawOriginal),reinterpret_cast<void**>(&tickOriginal),reinterpret_cast<void**>(&renderOriginal),reinterpret_cast<void**>(&presentOriginal),reinterpret_cast<void**>(&levelOriginal),reinterpret_cast<void**>(&actorAnimationOriginal),reinterpret_cast<void**>(&skeletalAnimationOriginal),reinterpret_cast<void**>(&getFrameOriginal),reinterpret_cast<void**>(&processEventOriginal)};
 unsigned installed=0;bool enabled=false;DWORD status=0;
 for(unsigned i=0;i<targets.size();++i){if(MH_CreateHook(targets[i],detours[i],originals[i])!=MH_OK){status=15;break;}++installed;if(MH_QueueEnableHook(targets[i])!=MH_OK){status=16;break;}}
 if(!status){if(MH_ApplyQueued()!=MH_OK)status=17;else enabled=true;}
 if(enabled){sampling.store(true,std::memory_order_release);const auto start=GetTickCount64();while(count.load()<config.maxFrames&&GetTickCount64()-start<config.durationMs&&LocalWorld())Sleep(10);}
 sampling.store(false,std::memory_order_release);
 bool disabled=true;for(unsigned i=0;i<installed;++i){const auto result=MH_DisableHook(targets[i]);disabled&=result==MH_OK||result==MH_ERROR_DISABLED;}
 // Keep trampolines and this DLL resident: an original callback may be returning.
 Sleep(50);
 try{
 if(capture.ready.load(std::memory_order_acquire)){
  const auto folder=std::filesystem::path(config.reportPath).parent_path();std::ofstream meta(folder/L"native-color.json");meta<<"{\"width\":"<<capture.width<<",\"height\":"<<capture.height<<",\"format\":"<<capture.format<<",\"samples\":"<<capture.samples<<",\"d3d9ex_known\":"<<(capture.exKnown?"true":"false")<<",\"d3d9ex\":"<<(capture.isEx?"true":"false")<<",\"hresult\":"<<capture.result<<"}\n";
  if(SUCCEEDED(capture.result)&&!capture.pixels.empty()){BITMAPFILEHEADER file{};BITMAPINFOHEADER info{};file.bfType=0x4d42;file.bfOffBits=sizeof(file)+sizeof(info);file.bfSize=file.bfOffBits+DWORD(capture.pixels.size());info.biSize=sizeof(info);info.biWidth=LONG(capture.width);info.biHeight=-LONG(capture.height);info.biPlanes=1;info.biBitCount=32;info.biCompression=BI_RGB;std::ofstream image(folder/L"native-color.bmp",std::ios::binary);image.write(reinterpret_cast<const char*>(&file),sizeof(file));image.write(reinterpret_cast<const char*>(&info),sizeof(info));image.write(reinterpret_cast<const char*>(capture.pixels.data()),capture.pixels.size());}
 }
 if(records[0].ready.load(std::memory_order_acquire)){std::ofstream camera(std::filesystem::path(config.reportPath).parent_path()/L"native-camera.json");camera<<"{\"matrices\":[";bool first=true;for(float value:records[0].matrices){if(!first)camera<<',';first=false;camera.precision(9);camera<<value;}camera<<"]}\n";}
 std::ofstream out(std::filesystem::path(config.reportPath));if(!out)return 18;out<<"{\n\"status\":"<<status<<",\"hooks_disabled\":"<<(disabled?"true":"false")<<",\"tick_calls\":"<<ticks.load()<<",\"present_calls\":"<<presents.load()<<",\"camera_written\":false,\"render_replayed\":false,\"frames\":[";bool first=true;
  for(auto& row:records){if(!row.ready.load(std::memory_order_acquire))continue;if(!first)out<<',';first=false;out<<"\n{\"frame\":"<<row.frame<<",\"thread\":"<<row.thread<<",\"ticks_before\":"<<row.tickBefore<<",\"ticks_after\":"<<row.tickAfter<<",\"renders\":"<<row.renders<<",\"presents_before\":"<<row.presentBefore<<",\"presents_after\":"<<row.presentAfter<<",\"scene\":"<<row.scene<<",\"render_interface\":"<<row.renderInterface<<",\"origin\":["<<row.origin[0]<<','<<row.origin[1]<<','<<row.origin[2]<<"],\"rotation\":["<<row.rotation[0]<<','<<row.rotation[1]<<','<<row.rotation[2]<<"],\"fov\":"<<row.fov<<",\"scene_read\":"<<(row.sceneRead?"true":"false")<<",\"begin_qpc\":"<<row.begin<<",\"end_qpc\":"<<row.end<<",\"level_renders\":"<<row.levelRenders<<",\"actor_animation_calls_inside_draw\":"<<row.actorAnimation<<",\"skeletal_animation_calls_inside_draw\":"<<row.skeletalAnimation<<",\"skeletal_getframe_calls\":"<<row.skeletalFrames<<",\"script_calls\":"<<row.scriptEvents<<",\"script_overflow\":"<<row.scriptOverflow<<",\"scripts\":[";
   bool firstScript=true;for(const auto& script:row.scripts){if(!script.calls)continue;if(!firstScript)out<<',';firstScript=false;out<<"{\"name_id\":"<<script.name<<",\"owner_name_id\":"<<script.ownerName<<",\"phase\":"<<script.phase<<",\"calls\":"<<script.calls<<'}';}out<<"]}";
  }out<<"\n]}\n";
 }catch(...){return 19;}return !disabled?20:status;
}
BOOL WINAPI DllMain(HINSTANCE module,DWORD reason,void*){if(reason==DLL_PROCESS_ATTACH)DisableThreadLibraryCalls(module);return TRUE;}
