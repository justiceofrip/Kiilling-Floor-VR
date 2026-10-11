#include "NativeEyeBridge.h"
#include <MinHook.h>
#include <dxgi1_2.h>
#include <atomic>
#include <cstring>
#include <fstream>
#include <algorithm>
namespace kfvr::native {
namespace {
using Microsoft::WRL::ComPtr;
using Reset=HRESULT(WINAPI*)(IDirect3DDevice9*,D3DPRESENT_PARAMETERS*);
enum class Stage {Idle,Prepare,Rendered,Publish,Feedback,Skip};
struct InputFault {unsigned kind=0,reads=0;std::uint64_t sequence=0,firstMs=0,lastMs=0;};
struct State {
 ipc::FrameChannel channel;graphics::D3D9EyeTransfer transfer;
 ComPtr<IDirect3DDevice9> device;ComPtr<ID3D11Device> d11;NativeSurfaces eyes;
 runtime::PresentationRequirements requirements{},allocated{};runtime::TrackingFrame tracking{};
 ipc::FrameRequest request{};graphics::PairTicket ticket{};Stage stage=Stage::Idle;
 std::atomic<bool> stop=false,closed=true;DWORD thread=0;
 void* resetTarget=nullptr;Reset resetOriginal=nullptr;std::array<unsigned char,16> resetBytes{};
 bool resetEnabled=false,resetRestored=true;const char* reason="not_started";
 bool suppressDesktop=false,mirrorReady=false,desktopReplacedPending=false;
 D3DSURFACE_DESC mirrorDescription{},mirrorObservedTarget{},mirrorObservedSource{};
 D3DTEXTUREFILTERTYPE mirrorFilter=D3DTEXF_NONE;HRESULT mirrorResult=S_FALSE;
 const char* mirrorStage="not_attempted";const char* mirrorFirstFailureStage="none";HRESULT mirrorFirstFailure=S_OK;
 DWORD mirrorDevCaps2=0,mirrorStretchFilterCaps=0,mirrorTextureFilterCaps=0;UINT mirrorRenderTargets=0;unsigned mirrorStretchCalls=0;
 ComPtr<IDirect3DTexture9> mirrorTexture;D3DSURFACE_DESC mirrorTextureDescription{};
 const char* mirrorPath="none";const char* mirrorTextureSource="none";unsigned mirrorQuadAttempts=0,mirrorQuadCopies=0,mirrorTextureCopies=0,mirrorStateRestores=0;
 unsigned mirrorSceneBegins=0,mirrorSceneEnds=0;HRESULT mirrorRestoreResult=S_FALSE;
 HRESULT mirrorBackResult=S_FALSE,mirrorTargetDescResult=S_FALSE,mirrorCapsResult=S_FALSE,mirrorSourceDescResult=S_FALSE;
 HRESULT mirrorColorResult=S_FALSE,mirrorDepthResult=S_FALSE,mirrorViewportResult=S_FALSE;
 unsigned mirrorAttempts=0,mirrorCopies=0,mirroredReplacements=0,mirrorFailures=0,mirrorStateFailures=0,desktopReplacements=0,focusFallbacks=0;
 unsigned restoredRenderFailures=0,unsafeRenderFailures=0;
 unsigned levelCallbacks=0,noRequestCallbacks=0,pendingPairCallbacks=0;long long started=0,finished=0;graphics::D3D9TransferTiming timing{};
 HRESULT transferError=S_OK;unsigned transferBusy=0,transferAttempts=0;
 unsigned requests=0,rendered=0,published=0,consumed=0,discarded=0,expired=0,inputs=0,resets=0,inputBusy=0,inputInvalid=0;
 unsigned inputMissing=0,inputExpired=0,inputMalformed=0,inputFaultCount=0,inputFaultOverflow=0;
 bool previousInputFault=false;std::array<InputFault,128> inputFaults{};
 std::uint64_t frame=0,epoch=0,lastInput=0;double renderMs=0,renderMax=0,transferMs=0,transferMax=0;
} s;
long long Now(){LARGE_INTEGER q;QueryPerformanceCounter(&q);return q.QuadPart;}
double Milliseconds(long long ticks){LARGE_INTEGER f;QueryPerformanceFrequency(&f);return double(ticks)*1000/double(f.QuadPart);}
bool Read(const void* p,void* out,SIZE_T n){SIZE_T got=0;return ReadProcessMemory(GetCurrentProcess(),p,out,n,&got)&&got==n;}
void Close(const char* reason){
 if(s.closed.load(std::memory_order_relaxed))return;
 s.reason=reason;s.stop=true;s.finished=Now();s.mirrorReady=false;
 for(auto& eye:s.eyes)eye.Reset();s.mirrorTexture.Reset();s.transfer.Reset();s.d11.Reset();s.device.Reset();s.channel.Close();
 if(s.resetEnabled){const auto result=MH_DisableHook(s.resetTarget);std::array<unsigned char,16> bytes{};s.resetRestored=(result==MH_OK||result==MH_ERROR_DISABLED)&&Read(s.resetTarget,bytes.data(),bytes.size())&&bytes==s.resetBytes;s.resetEnabled=!s.resetRestored;}
 // Publish statistics only after render-thread resource cleanup. Keep the DLL
 // and MinHook trampolines resident until process exit.
 s.closed.store(true,std::memory_order_release);
}
HRESULT WINAPI ResetHook(IDirect3DDevice9* device,D3DPRESENT_PARAMETERS* parameters){
 if(device==s.device.Get()){
  // D3D9 Reset must run on the device thread. No worker releases default-pool
  // resources; this detour releases them before entering the original Reset.
  if(GetCurrentThreadId()!=s.thread)return D3DERR_INVALIDCALL;
  ++s.resets;Close("device_reset");
 }
 return s.resetOriginal(device,parameters);
}
bool ProtectReset(){
 if(s.resetEnabled)return true;
 void** table=nullptr;if(!Read(s.device.Get(),&table,sizeof(table))||!Read(table+16,&s.resetTarget,sizeof(void*)))return false;
 MEMORY_BASIC_INFORMATION info{};
 if(!VirtualQuery(s.resetTarget,&info,sizeof(info))||info.AllocationBase!=GetModuleHandleW(L"d3d9.dll")||!(info.Protect&(PAGE_EXECUTE|PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE))||!Read(s.resetTarget,s.resetBytes.data(),s.resetBytes.size()))return false;
 if(MH_CreateHook(s.resetTarget,reinterpret_cast<void*>(&ResetHook),reinterpret_cast<void**>(&s.resetOriginal))!=MH_OK)return false;
 if(MH_EnableHook(s.resetTarget)!=MH_OK)return false;s.resetEnabled=true;s.resetRestored=false;return true;
}
bool Prepare(){
 LUID luid{};if(FAILED(graphics::D3D9CpuPair::AdapterLuid(s.device.Get(),luid))||luid.LowPart!=s.requirements.adapterLow||luid.HighPart!=s.requirements.adapterHigh)return false;
 if(!ProtectReset())return false;
 if(!s.d11){
  ComPtr<IDXGIFactory1> factory;if(FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory))))return false;
  ComPtr<IDXGIAdapter1> chosen;
  for(UINT i=0;;++i){ComPtr<IDXGIAdapter1> adapter;if(factory->EnumAdapters1(i,&adapter)!=S_OK)break;DXGI_ADAPTER_DESC1 desc{};if(SUCCEEDED(adapter->GetDesc1(&desc))&&desc.AdapterLuid.LowPart==luid.LowPart&&desc.AdapterLuid.HighPart==luid.HighPart){chosen=adapter;break;}}
  if(!chosen)return false;const D3D_FEATURE_LEVEL levels[]={D3D_FEATURE_LEVEL_11_0};D3D_FEATURE_LEVEL level{};
  if(FAILED(D3D11CreateDevice(chosen.Get(),D3D_DRIVER_TYPE_UNKNOWN,nullptr,D3D11_CREATE_DEVICE_BGRA_SUPPORT,levels,1,D3D11_SDK_VERSION,&s.d11,&level,nullptr)))return false;
 }
 if(s.transfer.Create(s.device.Get(),s.d11.Get(),s.requirements.width,s.requirements.height,D3DFMT_A8R8G8B8,DXGI_FORMAT(s.requirements.format),++s.epoch)!=graphics::TransferResult::Ok){s.transferError=s.transfer.LastError();return false;}
 s.allocated=s.requirements;return true;
}
// The mirror is tested on the actual native backbuffer. Unsupported formats,
// changed extents keep the ordinary desktop world active. MSAA uses a separately
// verified textured quad because StretchRect cannot write a multisample target.
bool MirrorTarget(ComPtr<IDirect3DSurface9>& back,D3DSURFACE_DESC& desc){
 s.mirrorStage="get_backbuffer";
 if(!s.device){s.mirrorResult=E_POINTER;return false;}
 s.mirrorBackResult=s.device->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,&back);
 if(FAILED(s.mirrorBackResult)){s.mirrorResult=s.mirrorBackResult;return false;}
 s.mirrorStage="backbuffer_description";s.mirrorTargetDescResult=back->GetDesc(&desc);
 if(FAILED(s.mirrorTargetDescResult)){s.mirrorResult=s.mirrorTargetDescResult;return false;}
 s.mirrorObservedTarget=desc;s.mirrorStage="backbuffer_policy";
 if(!desc.Width||!desc.Height||desc.Width>4096||desc.Height>4096||desc.Pool!=D3DPOOL_DEFAULT||!(desc.Usage&D3DUSAGE_RENDERTARGET)||
    (desc.Format!=D3DFMT_A8R8G8B8&&desc.Format!=D3DFMT_X8R8G8B8)){s.mirrorResult=D3DERR_INVALIDCALL;return false;}
 D3DCAPS9 caps{};s.mirrorStage="device_caps";s.mirrorCapsResult=s.device->GetDeviceCaps(&caps);
 if(FAILED(s.mirrorCapsResult)){s.mirrorResult=s.mirrorCapsResult;return false;}
 s.mirrorDevCaps2=caps.DevCaps2;s.mirrorStretchFilterCaps=caps.StretchRectFilterCaps;s.mirrorTextureFilterCaps=caps.TextureFilterCaps;s.mirrorRenderTargets=caps.NumSimultaneousRTs;s.mirrorStage="texture_stretch_capability";
 if(!(caps.DevCaps2&D3DDEVCAPS2_CAN_STRETCHRECT_FROM_TEXTURES)){s.mirrorResult=D3DERR_INVALIDCALL;return false;}
 const DWORD point=D3DPTFILTERCAPS_MINFPOINT|D3DPTFILTERCAPS_MAGFPOINT;
 s.mirrorFilter=(caps.StretchRectFilterCaps&point)==point?D3DTEXF_POINT:D3DTEXF_NONE;
 return true;
}
void MirrorFailed(){
 s.mirrorReady=false;++s.mirrorFailures;
 if(std::strcmp(s.mirrorFirstFailureStage,"none")==0){s.mirrorFirstFailureStage=s.mirrorStage;s.mirrorFirstFailure=s.mirrorResult;}
}
bool SameMirrorTarget(const D3DSURFACE_DESC& desc){
 return desc.Width==s.mirrorDescription.Width&&desc.Height==s.mirrorDescription.Height&&desc.Format==s.mirrorDescription.Format&&
        desc.MultiSampleType==s.mirrorDescription.MultiSampleType&&desc.MultiSampleQuality==s.mirrorDescription.MultiSampleQuality&&desc.Pool==s.mirrorDescription.Pool;
}
bool CurrentFocus(){
 xr::ControllerFrame controls{};std::uint64_t sequence=0;
 return s.channel.ReadInput(controls,sequence)==ipc::ChannelResult::Ok&&controls.focused&&controls.spaceGeneration==s.tracking.spaceGeneration;
}
// D3DSBT_ALL restores the native renderer's device state. Output bindings are
// additionally restored explicitly; SetRenderTarget also resets the viewport.
// Read back every state this pass changes, including DrawPrimitiveUP's stream0.
struct MirrorRenderSetting {D3DRENDERSTATETYPE state;DWORD value;};
constexpr MirrorRenderSetting mirrorRenderSettings[]={
 {D3DRS_ZENABLE,FALSE},{D3DRS_ZWRITEENABLE,FALSE},{D3DRS_ALPHATESTENABLE,FALSE},
 {D3DRS_ALPHABLENDENABLE,FALSE},{D3DRS_SEPARATEALPHABLENDENABLE,FALSE},
 {D3DRS_CULLMODE,D3DCULL_NONE},{D3DRS_FILLMODE,D3DFILL_SOLID},{D3DRS_LIGHTING,FALSE},
 {D3DRS_FOGENABLE,FALSE},{D3DRS_SPECULARENABLE,FALSE},{D3DRS_STENCILENABLE,FALSE},{D3DRS_SCISSORTESTENABLE,FALSE},
 {D3DRS_CLIPPLANEENABLE,0},{D3DRS_SRGBWRITEENABLE,FALSE},{D3DRS_DITHERENABLE,FALSE},
 {D3DRS_COLORWRITEENABLE,15},{D3DRS_MULTISAMPLEANTIALIAS,TRUE},{D3DRS_MULTISAMPLEMASK,0xffffffff},
 {D3DRS_VERTEXBLEND,D3DVBF_DISABLE},{D3DRS_INDEXEDVERTEXBLENDENABLE,FALSE},{D3DRS_WRAP0,0}
};
struct MirrorStageSetting {DWORD stage;D3DTEXTURESTAGESTATETYPE state;DWORD value;};
constexpr MirrorStageSetting mirrorStageSettings[]={
 {0,D3DTSS_COLOROP,D3DTOP_SELECTARG1},{0,D3DTSS_COLORARG1,D3DTA_TEXTURE},
 {0,D3DTSS_RESULTARG,D3DTA_CURRENT},{0,D3DTSS_ALPHAOP,D3DTOP_SELECTARG1},{0,D3DTSS_ALPHAARG1,D3DTA_TEXTURE},
 {0,D3DTSS_TEXCOORDINDEX,0},{0,D3DTSS_TEXTURETRANSFORMFLAGS,D3DTTFF_DISABLE},
 {1,D3DTSS_COLOROP,D3DTOP_DISABLE},{1,D3DTSS_ALPHAOP,D3DTOP_DISABLE}
};
struct MirrorSamplerSetting {D3DSAMPLERSTATETYPE state;DWORD value;};
constexpr MirrorSamplerSetting mirrorSamplerSettings[]={
 {D3DSAMP_ADDRESSU,D3DTADDRESS_CLAMP},{D3DSAMP_ADDRESSV,D3DTADDRESS_CLAMP},
 {D3DSAMP_MINFILTER,D3DTEXF_POINT},{D3DSAMP_MAGFILTER,D3DTEXF_POINT},
 {D3DSAMP_MIPFILTER,D3DTEXF_NONE},{D3DSAMP_MAXMIPLEVEL,0},{D3DSAMP_MIPMAPLODBIAS,0},{D3DSAMP_SRGBTEXTURE,FALSE}
};
bool MirrorCall(HRESULT result){if(FAILED(result)){s.mirrorResult=result;return false;}return true;}
bool OptionalSurface(HRESULT result){return SUCCEEDED(result)||result==D3DERR_NOTFOUND;}
struct MirrorDeviceState {
 ComPtr<IDirect3DStateBlock9> block;std::array<ComPtr<IDirect3DSurface9>,4> colors;ComPtr<IDirect3DSurface9> depth;
 ComPtr<IDirect3DVertexShader9> vertexShader;ComPtr<IDirect3DPixelShader9> pixelShader;
 ComPtr<IDirect3DVertexDeclaration9> declaration;ComPtr<IDirect3DVertexBuffer9> stream;
 ComPtr<IDirect3DBaseTexture9> texture;D3DVIEWPORT9 viewport{};UINT count=0,offset=0,stride=0;DWORD fvf=0;
 std::array<DWORD,sizeof(mirrorRenderSettings)/sizeof(mirrorRenderSettings[0])> render{};
 std::array<DWORD,sizeof(mirrorStageSettings)/sizeof(mirrorStageSettings[0])> stages{};
 std::array<DWORD,sizeof(mirrorSamplerSettings)/sizeof(mirrorSamplerSettings[0])> sampler{};
 bool Capture(){
  auto* d=s.device.Get();count=s.mirrorRenderTargets;s.mirrorStage="quad_capture_outputs";
  if(count<1||count>colors.size()){s.mirrorResult=D3DERR_INVALIDCALL;return false;}
  for(UINT i=0;i<count;++i){const auto hr=d->GetRenderTarget(i,&colors[i]);if(!(i?OptionalSurface(hr):SUCCEEDED(hr)))return MirrorCall(hr);}
  const auto hr=d->GetDepthStencilSurface(&depth);if(!OptionalSurface(hr))return MirrorCall(hr);
  if(!MirrorCall(d->GetViewport(&viewport)))return false;
  s.mirrorStage="quad_capture_pipeline";
  if(!MirrorCall(d->GetVertexShader(&vertexShader))||!MirrorCall(d->GetPixelShader(&pixelShader))||
     !MirrorCall(d->GetVertexDeclaration(&declaration))||!MirrorCall(d->GetFVF(&fvf))||
     !MirrorCall(d->GetStreamSource(0,&stream,&offset,&stride))||!MirrorCall(d->GetTexture(0,&texture)))return false;
  for(std::size_t i=0;i<render.size();++i)if(!MirrorCall(d->GetRenderState(mirrorRenderSettings[i].state,&render[i])))return false;
  for(std::size_t i=0;i<stages.size();++i)if(!MirrorCall(d->GetTextureStageState(mirrorStageSettings[i].stage,mirrorStageSettings[i].state,&stages[i])))return false;
  for(std::size_t i=0;i<sampler.size();++i)if(!MirrorCall(d->GetSamplerState(0,mirrorSamplerSettings[i].state,&sampler[i])))return false;
  s.mirrorStage="quad_capture_stateblock";
  return MirrorCall(d->CreateStateBlock(D3DSBT_ALL,&block))&&MirrorCall(block->Capture());
 }
 bool Restore(){
  auto* d=s.device.Get();s.mirrorRestoreResult=block->Apply();bool okay=SUCCEEDED(s.mirrorRestoreResult);
  auto restored=[&](HRESULT hr){if(FAILED(hr)){okay=false;s.mirrorRestoreResult=hr;}};
  restored(d->SetDepthStencilSurface(nullptr));
  for(UINT i=1;i<count;++i)restored(d->SetRenderTarget(i,nullptr));
  restored(d->SetRenderTarget(0,colors[0].Get()));
  for(UINT i=1;i<count;++i)restored(d->SetRenderTarget(i,colors[i].Get()));
  restored(d->SetDepthStencilSurface(depth.Get()));restored(d->SetViewport(&viewport));
  for(UINT i=0;i<count;++i){ComPtr<IDirect3DSurface9> current;const auto hr=d->GetRenderTarget(i,&current);okay=(i?OptionalSurface(hr):SUCCEEDED(hr))&&current.Get()==colors[i].Get()&&okay;}
  ComPtr<IDirect3DSurface9> currentDepth;D3DVIEWPORT9 currentViewport{};
  okay=OptionalSurface(d->GetDepthStencilSurface(&currentDepth))&&currentDepth.Get()==depth.Get()&&okay;
  okay=SUCCEEDED(d->GetViewport(&currentViewport))&&std::memcmp(&viewport,&currentViewport,sizeof(viewport))==0&&okay;
  ComPtr<IDirect3DVertexShader9> vs;ComPtr<IDirect3DPixelShader9> ps;ComPtr<IDirect3DVertexDeclaration9> vd;
  ComPtr<IDirect3DVertexBuffer9> vb;ComPtr<IDirect3DBaseTexture9> tx;UINT at=0,step=0;DWORD value=0;
  okay=SUCCEEDED(d->GetVertexShader(&vs))&&vs.Get()==vertexShader.Get()&&okay;
  okay=SUCCEEDED(d->GetPixelShader(&ps))&&ps.Get()==pixelShader.Get()&&okay;
  okay=SUCCEEDED(d->GetVertexDeclaration(&vd))&&vd.Get()==declaration.Get()&&okay;
  okay=SUCCEEDED(d->GetFVF(&value))&&value==fvf&&okay;
  okay=SUCCEEDED(d->GetStreamSource(0,&vb,&at,&step))&&vb.Get()==stream.Get()&&at==offset&&step==stride&&okay;
  okay=SUCCEEDED(d->GetTexture(0,&tx))&&tx.Get()==texture.Get()&&okay;
  for(std::size_t i=0;i<render.size();++i)okay=SUCCEEDED(d->GetRenderState(mirrorRenderSettings[i].state,&value))&&value==render[i]&&okay;
  for(std::size_t i=0;i<stages.size();++i)okay=SUCCEEDED(d->GetTextureStageState(mirrorStageSettings[i].stage,mirrorStageSettings[i].state,&value))&&value==stages[i]&&okay;
  for(std::size_t i=0;i<sampler.size();++i)okay=SUCCEEDED(d->GetSamplerState(0,mirrorSamplerSettings[i].state,&value))&&value==sampler[i]&&okay;
  if(!okay&&SUCCEEDED(s.mirrorRestoreResult))s.mirrorRestoreResult=E_UNEXPECTED;
  return okay;
 }
};
bool MirrorTexture(const D3DSURFACE_DESC& source,ComPtr<IDirect3DTexture9>& texture){
 s.mirrorStage="quad_source_texture";
 ComPtr<IDirect3DTexture9> container;ComPtr<IDirect3DSurface9> first;
 if(SUCCEEDED(s.eyes[0]->GetContainer(IID_PPV_ARGS(&container)))&&SUCCEEDED(container->GetSurfaceLevel(0,&first))&&first.Get()==s.eyes[0].Get()){
  s.mirrorTextureSource="native_level_zero";texture=container;return true;
 }
 // A surface without a usable texture container is copied to an owned reusable
 // texture. This copy remains outside BeginScene and never binds a native RHI target.
 if(!s.mirrorTexture||s.mirrorTextureDescription.Width!=source.Width||s.mirrorTextureDescription.Height!=source.Height||s.mirrorTextureDescription.Format!=source.Format){
  s.mirrorStage="quad_create_texture";s.mirrorTexture.Reset();
  if(!MirrorCall(s.device->CreateTexture(source.Width,source.Height,1,D3DUSAGE_RENDERTARGET,source.Format,D3DPOOL_DEFAULT,&s.mirrorTexture,nullptr)))return false;
  s.mirrorTextureDescription=source;
 }
 first.Reset();s.mirrorStage="quad_copy_source";
 if(!MirrorCall(s.mirrorTexture->GetSurfaceLevel(0,&first)))return false;
 ++s.mirrorStretchCalls;if(!MirrorCall(s.device->StretchRect(s.eyes[0].Get(),nullptr,first.Get(),nullptr,D3DTEXF_NONE)))return false;
 ++s.mirrorTextureCopies;s.mirrorTextureSource="owned_rt_copy";texture=s.mirrorTexture;return true;
}
bool MirrorQuad(IDirect3DSurface9* back,const D3DSURFACE_DESC& target,const D3DSURFACE_DESC& source,const RECT& crop){
 ++s.mirrorQuadAttempts;s.mirrorPath="msaa_textured_quad";ComPtr<IDirect3DTexture9> texture;
 if(!MirrorTexture(source,texture))return false;
 MirrorDeviceState saved;if(!saved.Capture())return false;
 s.mirrorStage="quad_begin_scene";if(!MirrorCall(s.device->BeginScene()))return false;++s.mirrorSceneBegins;
 // Once BeginScene succeeds, every path below ends our scene and restores state.
 auto* d=s.device.Get();bool okay=true;s.mirrorStage="quad_configure";
 auto set=[&](HRESULT hr){if(FAILED(hr)&&okay){s.mirrorResult=hr;okay=false;}};
 set(d->SetDepthStencilSurface(nullptr));for(UINT i=1;i<saved.count;++i)set(d->SetRenderTarget(i,nullptr));
 set(d->SetRenderTarget(0,back));const D3DVIEWPORT9 viewport{0,0,target.Width,target.Height,0,1};set(d->SetViewport(&viewport));
 set(d->SetVertexShader(nullptr));set(d->SetPixelShader(nullptr));set(d->SetFVF(D3DFVF_XYZRHW|D3DFVF_TEX1));
 set(d->SetTexture(0,texture.Get()));
 for(const auto& setting:mirrorRenderSettings)set(d->SetRenderState(setting.state,setting.value));
 for(const auto& setting:mirrorStageSettings)set(d->SetTextureStageState(setting.stage,setting.state,setting.value));
 const DWORD linear=D3DPTFILTERCAPS_MINFLINEAR|D3DPTFILTERCAPS_MAGFLINEAR;
 s.mirrorFilter=(s.mirrorTextureFilterCaps&linear)==linear?D3DTEXF_LINEAR:D3DTEXF_POINT;
 for(const auto& setting:mirrorSamplerSettings)set(d->SetSamplerState(0,setting.state,
  setting.state==D3DSAMP_MINFILTER||setting.state==D3DSAMP_MAGFILTER?DWORD(s.mirrorFilter):setting.value));
 struct Vertex {float x,y,z,rhw,u,v;};
 const float u0=float(crop.left)/source.Width,u1=float(crop.right)/source.Width,v0=float(crop.top)/source.Height,v1=float(crop.bottom)/source.Height;
 const float right=float(target.Width)-0.5f,bottom=float(target.Height)-0.5f;
 const Vertex vertices[]={{-0.5f,-0.5f,0,1,u0,v0},{right,-0.5f,0,1,u1,v0},{-0.5f,bottom,0,1,u0,v1},{right,bottom,0,1,u1,v1}};
 if(okay){s.mirrorStage="quad_draw";s.mirrorResult=d->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,2,vertices,sizeof(Vertex));okay=SUCCEEDED(s.mirrorResult);}
 const auto end=d->EndScene();if(SUCCEEDED(end))++s.mirrorSceneEnds;
 else {s.mirrorResult=end;s.mirrorStage="quad_end_scene";okay=false;}
 const bool restored=saved.Restore();
 if(!restored||FAILED(end)){++s.mirrorStateFailures;if(!restored){s.mirrorStage="quad_state_verification";s.mirrorResult=s.mirrorRestoreResult;}return false;}
 ++s.mirrorStateRestores;if(okay)++s.mirrorQuadCopies;return okay;
}
void MirrorDesktop(){
 ++s.mirrorAttempts;ComPtr<IDirect3DSurface9> back;D3DSURFACE_DESC desc{},source{};
 s.mirrorResult=D3DERR_INVALIDCALL;s.mirrorStage="source_missing";
 if(!s.eyes[0]){MirrorFailed();return;}
 if(!MirrorTarget(back,desc)){MirrorFailed();return;}
 s.mirrorStage="source_description";s.mirrorSourceDescResult=s.eyes[0]->GetDesc(&source);
 if(FAILED(s.mirrorSourceDescResult)){s.mirrorResult=s.mirrorSourceDescResult;MirrorFailed();return;}
 s.mirrorObservedSource=source;s.mirrorStage="source_policy";
 if(source.Pool!=D3DPOOL_DEFAULT||source.Format!=D3DFMT_A8R8G8B8||source.MultiSampleType!=D3DMULTISAMPLE_NONE||
    source.Width!=s.requirements.width||source.Height!=s.requirements.height){MirrorFailed();return;}
 // Center crop preserves the desktop aspect ratio without changing either eye.
 RECT crop{0,0,LONG(source.Width),LONG(source.Height)};
 if(std::uint64_t(source.Width)*desc.Height>std::uint64_t(source.Height)*desc.Width){
  const LONG width=LONG(std::uint64_t(source.Height)*desc.Width/desc.Height);crop.left=(LONG(source.Width)-width)/2;crop.right=crop.left+width;
 }else{
  const LONG height=LONG(std::uint64_t(source.Width)*desc.Height/desc.Width);crop.top=(LONG(source.Height)-height)/2;crop.bottom=crop.top+height;
 }
 s.mirrorStage="crop_policy";if(crop.right<=crop.left||crop.bottom<=crop.top){MirrorFailed();return;}
 if(desc.MultiSampleType!=D3DMULTISAMPLE_NONE){s.mirrorReady=MirrorQuad(back.Get(),desc,source,crop);}
 else {
  s.mirrorPath="direct_stretch";
  ComPtr<IDirect3DSurface9> color,depth,afterColor,afterDepth;D3DVIEWPORT9 viewport{},afterViewport{};
  s.mirrorStage="snapshot_color";s.mirrorColorResult=s.device->GetRenderTarget(0,&color);
  if(FAILED(s.mirrorColorResult)){s.mirrorResult=s.mirrorColorResult;MirrorFailed();return;}
  s.mirrorStage="snapshot_depth";s.mirrorDepthResult=s.device->GetDepthStencilSurface(&depth);
  if(!OptionalSurface(s.mirrorDepthResult)){s.mirrorResult=s.mirrorDepthResult;MirrorFailed();return;}
  s.mirrorStage="snapshot_viewport";s.mirrorViewportResult=s.device->GetViewport(&viewport);
  if(FAILED(s.mirrorViewportResult)){s.mirrorResult=s.mirrorViewportResult;MirrorFailed();return;}
  s.mirrorStage="stretch_rect";++s.mirrorStretchCalls;
  s.mirrorResult=s.device->StretchRect(s.eyes[0].Get(),&crop,back.Get(),nullptr,s.mirrorFilter);
  const bool unchanged=SUCCEEDED(s.device->GetRenderTarget(0,&afterColor))&&afterColor.Get()==color.Get()&&
   OptionalSurface(s.device->GetDepthStencilSurface(&afterDepth))&&afterDepth.Get()==depth.Get()&&
   SUCCEEDED(s.device->GetViewport(&afterViewport))&&std::memcmp(&viewport,&afterViewport,sizeof(viewport))==0;
  if(!unchanged){++s.mirrorStateFailures;s.mirrorStage="state_verification";}
  s.mirrorReady=SUCCEEDED(s.mirrorResult)&&unchanged;
 }
 if(s.mirrorReady){++s.mirrorCopies;if(s.desktopReplacedPending)++s.mirroredReplacements;s.mirrorDescription=desc;s.mirrorStage="complete";}else MirrorFailed();
}
void Reap(){
 if(s.stage!=Stage::Feedback)return;const auto outcome=s.channel.PollOutcome();if(outcome==ipc::Outcome::Pending)return;
 const bool consumed=outcome==ipc::Outcome::Consumed;
 if(!s.transfer.PairConsumed(s.ticket,consumed)){Close("feedback_failed");return;}
 if(consumed)++s.consumed;else ++s.discarded;s.stage=Stage::Idle;
}
void Skip(){
 const auto result=s.channel.Skip();if(result==ipc::ChannelResult::Busy){s.stage=Stage::Skip;return;}
 if(result==ipc::ChannelResult::Closed){Close("host_closed");return;}s.stage=Stage::Idle;
}
}
bool StartEyeBridge(const wchar_t* token,bool suppressDesktop){
 if(!s.channel.ConnectProducer(token))return false;s.suppressDesktop=suppressDesktop;s.reason="running";s.closed=false;return true;
}
bool EyeBridgeStateUnsafe() noexcept {return s.unsafeRenderFailures!=0||s.mirrorStateFailures!=0;}
bool EyeBridgeDesktopReady() noexcept {return s.suppressDesktop&&s.mirrorReady&&!s.closed.load(std::memory_order_acquire)&&!s.stop.load(std::memory_order_acquire);}
bool ReadEyeBridgeControls(xr::ControllerFrame& controls,std::uint64_t& sequence) noexcept{
 controls={};sequence=0;
 if(s.closed.load(std::memory_order_acquire)||s.stop.load(std::memory_order_acquire)||s.thread!=GetCurrentThreadId())return false;
 ipc::InputReadInfo info{};const auto result=s.channel.ReadInput(controls,sequence,&info);
 if(result==ipc::ChannelResult::Busy)++s.inputBusy;
 if(result==ipc::ChannelResult::Invalid){
  ++s.inputInvalid;const unsigned kind=!info.sequence?0:info.expiresQpc<=info.nowQpc?1:2;
  if(kind==0)++s.inputMissing;else if(kind==1)++s.inputExpired;else ++s.inputMalformed;
  const auto now=GetTickCount64();
  if(s.previousInputFault&&s.inputFaultCount&&s.inputFaults[s.inputFaultCount-1].kind==kind&&s.inputFaults[s.inputFaultCount-1].sequence==info.sequence){auto& event=s.inputFaults[s.inputFaultCount-1];event.lastMs=now;++event.reads;}
  else if(s.inputFaultCount<s.inputFaults.size())s.inputFaults[s.inputFaultCount++]={kind,1,info.sequence,now,now};else ++s.inputFaultOverflow;
 }
 s.previousInputFault=result==ipc::ChannelResult::Invalid;
 return result==ipc::ChannelResult::Ok;
}
bool SendEyeBridgeHaptics(std::uint64_t sequence,const std::array<xr::HapticPulse,2>& pulses) noexcept {
 if(s.closed.load(std::memory_order_acquire)||s.stop.load(std::memory_order_acquire)||s.thread!=GetCurrentThreadId())return false;
 return s.channel.SendHaptics(sequence,pulses)==ipc::ChannelResult::Ok;
}
void StopEyeBridge() noexcept{s.stop.store(true,std::memory_order_release);}
bool EyeBridgeClosed() noexcept{return s.closed.load(std::memory_order_acquire);}
EyeBridgeResult EyeBridgeLevel(void* scene,void* ri,IDirect3DDevice9* device,EyeRenderer render,bool replaceDesktop) noexcept {try{
 if(s.closed.load(std::memory_order_acquire)||s.stop.load(std::memory_order_acquire))return EyeBridgeResult::NoPair;
 if(!s.thread)s.thread=GetCurrentThreadId();if(s.thread!=GetCurrentThreadId()){s.stop=true;return EyeBridgeResult::NoPair;}
 if(s.device&&s.device.Get()!=device){Close("device_changed");return EyeBridgeResult::NoPair;}
 ++s.levelCallbacks;if(!s.started)s.started=Now();
 Reap();if(s.closed.load()||s.stage!=Stage::Idle){++s.pendingPairCallbacks;return EyeBridgeResult::NoPair;}
 const auto taken=s.channel.TryTake(s.request);
 if(taken==ipc::ChannelResult::Closed){Close("host_closed");return EyeBridgeResult::NoPair;}
 if(taken!=ipc::ChannelResult::Ok){++s.noRequestCallbacks;return EyeBridgeResult::NoPair;}
 ++s.requests;
 if(!ipc::Decode(s.request,s.requirements,s.tracking)||s.requirements.width>4096||s.requirements.height>4096||std::uint64_t(s.requirements.width)*s.requirements.height*40>256*1024*1024){Close("unsupported_request");return EyeBridgeResult::NoPair;}
 if(!s.device)s.device=device;
 if(std::memcmp(&s.requirements,&s.allocated,sizeof(s.allocated))){s.mirrorReady=false;s.stage=Stage::Prepare;return EyeBridgeResult::NoPair;}
 if(Now()>=s.request.deadlineQpc){++s.expired;Skip();return EyeBridgeResult::NoPair;}
 if(replaceDesktop&&!CurrentFocus()){++s.focusFallbacks;Skip();return EyeBridgeResult::NoPair;}
 const auto start=Now();const auto outcome=render(scene,ri,s.requirements,s.tracking,s.eyes);const auto ms=Milliseconds(Now()-start);s.renderMs+=ms;s.renderMax=std::max(s.renderMax,ms);
 if(outcome==EyeRenderResult::FailedUnsafe){++s.unsafeRenderFailures;Close("native_restoration_failed");return EyeBridgeResult::FailedUnsafe;}
 if(outcome==EyeRenderResult::FailedRestored){++s.restoredRenderFailures;Close("native_render_failed_restored");return EyeBridgeResult::FailedRestored;}
 ++s.rendered;s.stage=Stage::Rendered;
 // Suppression is per invocation, never inferred from a retained or pending
 // pair. Recheck freshness/focus after both expensive native eye renders.
 if(replaceDesktop&&EyeBridgeDesktopReady()&&Now()<s.request.deadlineQpc&&CurrentFocus()){
  ComPtr<IDirect3DSurface9> back;D3DSURFACE_DESC desc{};
  if(MirrorTarget(back,desc)&&SameMirrorTarget(desc)){
   ++s.desktopReplacements;s.desktopReplacedPending=true;return EyeBridgeResult::DesktopReplaced;
  }
  s.mirrorReady=false;
 }
 return EyeBridgeResult::PairReady;
}catch(...){++s.unsafeRenderFailures;Close("level_exception_state_unknown");return EyeBridgeResult::FailedUnsafe;}}
void EyeBridgePresent(bool worldValid) noexcept {try{
 if(s.closed.load(std::memory_order_acquire))return;
 if(!s.thread)s.thread=GetCurrentThreadId();if(s.thread!=GetCurrentThreadId()){s.stop=true;return;}
 if(!worldValid){Close("world_changed");return;}
 // Complete this draw's mirror before publication releases the eye references,
 // including a stop arriving after the desktop world has already been skipped.
 if(s.suppressDesktop&&s.stage==Stage::Rendered&&(s.desktopReplacedPending||(Now()<s.request.deadlineQpc&&CurrentFocus()))){
  MirrorDesktop();s.desktopReplacedPending=false;
  if(s.mirrorStateFailures){Close("mirror_state_changed");return;}
 }
 if(s.stop.load(std::memory_order_acquire)){Close("duration_finished");return;}
 xr::ControllerFrame controls{};std::uint64_t sequence=0;
 if(s.channel.ReadInput(controls,sequence)==ipc::ChannelResult::Ok&&sequence!=s.lastInput){s.lastInput=sequence;++s.inputs;}
 Reap();if(s.closed.load())return;
 if(s.stage==Stage::Prepare){if(!Prepare()){Close("transport_setup_failed");return;}Skip();return;}
 if(s.stage==Stage::Skip){Skip();return;}
 if(s.stage==Stage::Rendered){
  if(Now()>=s.request.deadlineQpc){for(auto& eye:s.eyes)eye.Reset();++s.expired;Skip();return;}
  const graphics::FrameMetadata metadata{++s.frame,s.tracking.spaceGeneration,s.tracking.generation,s.tracking.predictedNs};
  ++s.transferAttempts;
  const auto result=s.transfer.Publish({s.eyes[0].Get(),s.eyes[1].Get()},metadata,s.ticket);
  for(auto& eye:s.eyes)eye.Reset();
  // Busy publication can still have queued GPU copy/conversion work. Retain
  // its CPU submission cost; early backpressure resets these timings to zero.
  const auto timing=s.transfer.LastTiming();s.timing.resolveMs+=timing.resolveMs;s.timing.readbackMs+=timing.readbackMs;s.timing.convertMs+=timing.convertMs;s.timing.uploadMs+=timing.uploadMs;s.timing.publishMs+=timing.publishMs;
  const auto ms=s.transfer.LastTransferMs();s.transferMs+=ms;s.transferMax=std::max(s.transferMax,ms);
  if(result==graphics::TransferResult::Busy){++s.transferBusy;Skip();return;}
  if(result!=graphics::TransferResult::Ok){s.transferError=s.transfer.LastError();Close("pixel_transfer_failed");return;}
  s.stage=Stage::Publish;
 }
 if(s.stage==Stage::Publish){
  const auto result=s.channel.Publish(s.transfer.Descriptor(),s.ticket);
  if(result==ipc::ChannelResult::Busy)return;
  if(result==ipc::ChannelResult::Ok){++s.published;s.stage=Stage::Feedback;return;}
  s.transfer.PairConsumed(s.ticket,false);++s.discarded;
  if(result==ipc::ChannelResult::Closed){Close("host_closed");return;}Skip();
 }
}catch(...){Close("present_exception");}}
void WriteEyeBridgeReport(const std::filesystem::path& folder){
 std::ofstream out(folder/L"native-bridge.json");if(!EyeBridgeClosed()){out<<"{\"closed\":false,\"cleanup_pending_on_graphics_thread\":true}\n";return;}
 out<<std::boolalpha<<"{\"closed\":true,\"reason\":\""<<s.reason<<"\",\"reset_hook_restored\":"<<s.resetRestored<<",\"resources_released\":"<<s.transfer.ResourcesReleased()<<",\"transfer_backend\":\""<<s.transfer.Backend()<<"\",\"per_frame_cpu_image_readback\":"<<!s.transfer.GpuSelected()<<",\"transfer_hresult\":"<<s.transferError<<",\"transfer_busy_pairs\":"<<s.transferBusy<<",\"transfer_attempts\":"<<s.transferAttempts<<",\"transfer_timing_scope\":\"CPU_transfer_calls_including_queue_submission_not_GPU_duration\",\"requests\":"<<s.requests<<",\"rendered_pairs\":"<<s.rendered<<",\"published_pairs\":"<<s.published<<",\"consumed_pairs\":"<<s.consumed<<",\"discarded_pairs\":"<<s.discarded<<",\"expired_before_transfer\":"<<s.expired<<",\"controller_samples\":"<<s.inputs<<",\"input_busy_reads\":"<<s.inputBusy<<",\"input_invalid_reads\":"<<s.inputInvalid<<",\"device_resets\":"<<s.resets<<",\"render_total_ms\":"<<s.renderMs<<",\"render_max_ms\":"<<s.renderMax<<",\"transfer_total_ms\":"<<s.transferMs<<",\"transfer_max_ms\":"<<s.transferMax<<",\"elapsed_ms\":"<<(s.started?Milliseconds(s.finished-s.started):0.0)<<",\"level_callbacks\":"<<s.levelCallbacks<<",\"no_request_callbacks\":"<<s.noRequestCallbacks<<",\"pending_pair_callbacks\":"<<s.pendingPairCallbacks<<",\"resolve_total_ms\":"<<s.timing.resolveMs<<",\"readback_total_ms\":"<<s.timing.readbackMs<<",\"convert_total_ms\":"<<s.timing.convertMs<<",\"upload_total_ms\":"<<s.timing.uploadMs<<",\"publish_total_ms\":"<<s.timing.publishMs<<",\"desktop_suppression_opt_in\":"<<s.suppressDesktop<<",\"desktop_replacements\":"<<s.desktopReplacements<<",\"mirror_attempts\":"<<s.mirrorAttempts<<",\"mirror_copies\":"<<s.mirrorCopies<<",\"mirrored_replacements\":"<<s.mirroredReplacements<<",\"mirror_failures\":"<<s.mirrorFailures<<",\"mirror_state_failures\":"<<s.mirrorStateFailures<<",\"mirror_hresult\":"<<s.mirrorResult<<",\"mirror_width\":"<<s.mirrorDescription.Width<<",\"mirror_height\":"<<s.mirrorDescription.Height<<",\"mirror_format\":"<<unsigned(s.mirrorDescription.Format)<<",\"mirror_filter\":"<<unsigned(s.mirrorFilter)<<",\"focus_fallbacks\":"<<s.focusFallbacks<<",\"restored_render_failures\":"<<s.restoredRenderFailures<<",\"unsafe_render_failures\":"<<s.unsafeRenderFailures<<",\"world_scale_calibrated\":false,\"input_missing_reads\":"<<s.inputMissing<<",\"input_expired_reads\":"<<s.inputExpired<<",\"input_malformed_reads\":"<<s.inputMalformed<<",\"input_fault_overflow\":"<<s.inputFaultOverflow<<",\"input_faults\":[";
 for(unsigned i=0;i<s.inputFaultCount;++i){const auto& event=s.inputFaults[i];if(i)out<<',';out<<"{\"kind\":\""<<(event.kind==0?"missing":event.kind==1?"expired":"malformed")<<"\",\"sequence\":"<<event.sequence<<",\"first_uptime_ms\":"<<event.firstMs<<",\"last_uptime_ms\":"<<event.lastMs<<",\"reads\":"<<event.reads<<'}';}out<<"],\"mirror_diagnostic\":{\"stage\":\""<<s.mirrorStage<<"\",\"first_failure_stage\":\""<<s.mirrorFirstFailureStage<<"\",\"first_failure_hresult\":"<<s.mirrorFirstFailure
  <<",\"stretch_calls\":"<<s.mirrorStretchCalls<<",\"dev_caps2\":"<<s.mirrorDevCaps2<<",\"stretch_filter_caps\":"<<s.mirrorStretchFilterCaps
  <<",\"texture_filter_caps\":"<<s.mirrorTextureFilterCaps<<",\"render_target_slots\":"<<s.mirrorRenderTargets<<",\"texture_source\":\""<<s.mirrorTextureSource<<"\",\"path\":\""<<s.mirrorPath<<"\",\"quad_attempts\":"<<s.mirrorQuadAttempts<<",\"quad_copies\":"<<s.mirrorQuadCopies<<",\"texture_copies\":"<<s.mirrorTextureCopies
  <<",\"verified_state_restores\":"<<s.mirrorStateRestores<<",\"state_restore_hresult\":"<<s.mirrorRestoreResult<<",\"scene_begins\":"<<s.mirrorSceneBegins<<",\"scene_ends\":"<<s.mirrorSceneEnds
  <<",\"get_backbuffer_hresult\":"<<s.mirrorBackResult<<",\"target_description_hresult\":"<<s.mirrorTargetDescResult<<",\"device_caps_hresult\":"<<s.mirrorCapsResult
  <<",\"source_description_hresult\":"<<s.mirrorSourceDescResult<<",\"get_color_hresult\":"<<s.mirrorColorResult<<",\"get_depth_hresult\":"<<s.mirrorDepthResult<<",\"get_viewport_hresult\":"<<s.mirrorViewportResult;
 auto description=[&](const char* name,const D3DSURFACE_DESC& d){out<<",\""<<name<<"\":{\"width\":"<<d.Width<<",\"height\":"<<d.Height<<",\"format\":"<<unsigned(d.Format)<<",\"type\":"<<unsigned(d.Type)<<",\"usage\":"<<d.Usage<<",\"pool\":"<<unsigned(d.Pool)<<",\"samples\":"<<unsigned(d.MultiSampleType)<<",\"quality\":"<<d.MultiSampleQuality<<'}';};
 description("target",s.mirrorObservedTarget);description("source",s.mirrorObservedSource);out<<"}}\n";
}
}
