#include <Windows.h>
#include <d3d9on12.h>
#include <d3d11_4.h>
#include <dxgi1_2.h>
#include <wrl/client.h>
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

using Microsoft::WRL::ComPtr;
namespace {
constexpr std::uint32_t Magic=0x3231504b,Version=1;
constexpr unsigned PairCount=20;
void Require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
void Hr(HRESULT hr,const char* operation){if(FAILED(hr)){std::ostringstream s;s<<operation<<" HRESULT=0x"<<std::hex<<std::uint32_t(hr);throw std::runtime_error(s.str());}}
struct Handle {
 HANDLE h=nullptr;
 Handle()=default;Handle(const Handle&)=delete;Handle& operator=(const Handle&)=delete;
 ~Handle(){Close();}
 void Close(){if(h&&h!=INVALID_HANDLE_VALUE)CloseHandle(h);h=nullptr;}
};
struct Window {
 HWND h=CreateWindowExW(0,L"STATIC",L"KFVR isolated D3D9On12 proof",WS_POPUP,0,0,64,64,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
 Window(){Require(h!=nullptr,"Create hidden test window");}~Window(){if(h)DestroyWindow(h);}
};
bool Read(HANDLE h,void* data,DWORD bytes){auto* p=static_cast<char*>(data);while(bytes){DWORD n=0;if(!ReadFile(h,p,bytes,&n,nullptr)||!n)return false;p+=n;bytes-=n;}return true;}
bool Write(HANDLE h,const void* data,DWORD bytes){auto* p=static_cast<const char*>(data);while(bytes){DWORD n=0;if(!WriteFile(h,p,bytes,&n,nullptr)||!n)return false;p+=n;bytes-=n;}return true;}
// Fixed-width protocol: handles are inherited by the launched child, never
// dereferenced as addresses. There is no frame-pixel data in either pipe.
struct Bootstrap {
 std::uint32_t magic=Magic,version=Version,width=0,height=0,format=87,producerBits=sizeof(void*)*8;
 std::uint32_t adapterLow=0;std::int32_t adapterHigh=0;
 std::uint64_t firstFrame=1,frames=PairCount;
 std::array<std::uint64_t,2> textures{};
 std::uint64_t readyFence=0,doneFence=0;
};
struct Frame {std::uint64_t id=0;};
struct Reply {std::uint32_t magic=Magic,status=1,bits=sizeof(void*)*8,reserved=0;std::uint64_t frame=0;};
static_assert(sizeof(Bootstrap)==80&&sizeof(Frame)==8&&sizeof(Reply)==24);
HANDLE ToHandle(std::uint64_t h){Require(h&&h<=UINTPTR_MAX,"Valid inherited handle width");return reinterpret_cast<HANDLE>(std::uintptr_t(h));}
std::uint32_t Color(std::uint64_t frame,unsigned eye,unsigned quadrant){return ((80+quadrant*30)<<24)|(((30+unsigned(frame)*17+eye*89)&255)<<16)|((55+quadrant*33)<<8)|(20+eye*41+quadrant*13);}
std::uint32_t ManagedColor(std::uint64_t frame){return 0xbd000000u|(((unsigned(frame)*13)&255)<<16)|0x009700u|((unsigned(frame)*29)&255);}
struct D11 {
 ComPtr<ID3D11Device> device;ComPtr<ID3D11Device1> device1;ComPtr<ID3D11Device5> device5;
 ComPtr<ID3D11DeviceContext> context;ComPtr<ID3D11DeviceContext4> context4;
 explicit D11(LUID luid){
  ComPtr<IDXGIFactory1> factory;Hr(CreateDXGIFactory1(IID_PPV_ARGS(&factory)),"Create DXGI factory");
  ComPtr<IDXGIAdapter1> selected;
  for(UINT i=0;;++i){ComPtr<IDXGIAdapter1> a;const auto hr=factory->EnumAdapters1(i,&a);if(hr==DXGI_ERROR_NOT_FOUND)break;Hr(hr,"Enumerate adapter");DXGI_ADAPTER_DESC1 d{};Hr(a->GetDesc1(&d),"Describe adapter");if(d.AdapterLuid.LowPart==luid.LowPart&&d.AdapterLuid.HighPart==luid.HighPart){selected=a;break;}}
  Require(bool(selected),"Find exact producer adapter");const D3D_FEATURE_LEVEL levels[]={D3D_FEATURE_LEVEL_11_0};D3D_FEATURE_LEVEL level{};
  Hr(D3D11CreateDevice(selected.Get(),D3D_DRIVER_TYPE_UNKNOWN,nullptr,D3D11_CREATE_DEVICE_BGRA_SUPPORT,levels,1,D3D11_SDK_VERSION,&device,&level,&context),"Create native D3D11 consumer");
  Hr(device.As(&device1),"D3D11Device1");Hr(device.As(&device5),"D3D11Device5");Hr(context.As(&context4),"D3D11Context4");
 }
};
int Consume(){
 Reply reply;
 try{
  Bootstrap b;Require(Read(GetStdHandle(STD_INPUT_HANDLE),&b,sizeof(b)),"Read bootstrap");
  Require(b.magic==Magic&&b.version==Version&&b.producerBits==32&&sizeof(void*)==8&&b.width>=8&&b.height>=8&&b.width<=4096&&b.height<=4096&&b.format==87&&b.frames&&b.frames<=PairCount&&b.firstFrame>0,"Validate bootstrap");
  D11 d({b.adapterLow,b.adapterHigh});std::array<ComPtr<ID3D11Texture2D>,2> shared;
  for(unsigned eye=0;eye<2;++eye){
   Hr(d.device1->OpenSharedResource1(ToHandle(b.textures[eye]),IID_PPV_ARGS(&shared[eye])),"Open D3D12 shared eye in D3D11");
   D3D11_TEXTURE2D_DESC desc{};shared[eye]->GetDesc(&desc);
   Require(desc.Width==b.width&&desc.Height==b.height&&desc.Format==DXGI_FORMAT_B8G8R8A8_UNORM&&desc.ArraySize==1&&desc.MipLevels==1&&desc.SampleDesc.Count==1&&!desc.SampleDesc.Quality&&desc.Usage==D3D11_USAGE_DEFAULT,"Validate shared eye descriptor");
  }
  Require(b.textures[0]!=b.textures[1],"Independent eye handles");
  ComPtr<ID3D11Fence> ready,done;Hr(d.device5->OpenSharedFence(ToHandle(b.readyFence),IID_PPV_ARGS(&ready)),"Open producer fence");Hr(d.device5->OpenSharedFence(ToHandle(b.doneFence),IID_PPV_ARGS(&done)),"Open consumer fence");
  D3D11_TEXTURE2D_DESC desc{};desc.Width=b.width;desc.Height=b.height;desc.MipLevels=1;desc.ArraySize=2;desc.Format=DXGI_FORMAT_B8G8R8A8_UNORM;desc.SampleDesc.Count=1;desc.Usage=D3D11_USAGE_DEFAULT;desc.BindFlags=D3D11_BIND_RENDER_TARGET;
  ComPtr<ID3D11Texture2D> destination,staging;Hr(d.device->CreateTexture2D(&desc,nullptr,&destination),"Create simulated eye array");
  desc.ArraySize=1;desc.Usage=D3D11_USAGE_STAGING;desc.BindFlags=0;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;Hr(d.device->CreateTexture2D(&desc,nullptr,&staging),"Create verification-only readback");
  reply.status=0;Require(Write(GetStdHandle(STD_OUTPUT_HANDLE),&reply,sizeof(reply)),"Reply consumer initialized");
  for(std::uint64_t i=0;i<b.frames;++i){
   Frame f;Require(Read(GetStdHandle(STD_INPUT_HANDLE),&f,sizeof(f))&&f.id==b.firstFrame+i,"Read sequential frame identity");
   Hr(d.context4->Wait(ready.Get(),f.id),"GPU wait for complete pair");
   for(unsigned eye=0;eye<2;++eye)d.context->CopySubresourceRegion(destination.Get(),eye,0,0,0,shared[eye].Get(),0,nullptr);
   Hr(d.context4->Signal(done.Get(),f.id),"GPU signal pair copied");d.context->Flush();
   // Readback is solely test verification. The producer's image transfer uses
   // neither CPU image upload nor GetRenderTargetData/Map.
   for(unsigned eye=0;eye<2;++eye){
    d.context->CopySubresourceRegion(staging.Get(),0,0,0,0,destination.Get(),eye,nullptr);
    D3D11_MAPPED_SUBRESOURCE mapped{};Hr(d.context->Map(staging.Get(),0,D3D11_MAP_READ,0,&mapped),"Map verification pixels");
    bool valid=mapped.pData&&mapped.RowPitch>=b.width*4;
    for(UINT y=0;valid&&y<b.height;++y){const auto* row=reinterpret_cast<const std::uint32_t*>(static_cast<const char*>(mapped.pData)+std::size_t(y)*mapped.RowPitch);
     for(UINT x=0;x<b.width;++x){const unsigned q=(x>=b.width/2?1:0)+(y>=b.height/2?2:0);const auto expected=x<4&&y<4?ManagedColor(f.id):Color(f.id,eye,q);
      if(row[x]!=expected){std::cerr<<"pixel_mismatch frame="<<f.id<<" eye="<<eye<<" x="<<x<<" y="<<y<<" expected=0x"<<std::hex<<expected<<" actual=0x"<<row[x]<<std::dec<<std::endl;valid=false;break;}}
    }
    d.context->Unmap(staging.Get(),0);Require(valid,"Full-pixel/alpha/eye/managed-texture verification");
   }
   Hr(d.device->GetDeviceRemovedReason(),"Consumer device state");reply.frame=f.id;Require(Write(GetStdHandle(STD_OUTPUT_HANDLE),&reply,sizeof(reply)),"Reply verified frame");
  }
  return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<std::endl;reply.status=1;Write(GetStdHandle(STD_OUTPUT_HANDLE),&reply,sizeof(reply));return 1;}
}
class Consumer {
 Handle inputRead_,inputWrite_,outputRead_,outputWrite_,process_,thread_;
public:
 Consumer(const std::filesystem::path& exe,const Bootstrap& b){
  SECURITY_ATTRIBUTES sa{sizeof(sa),nullptr,TRUE};Require(CreatePipe(&inputRead_.h,&inputWrite_.h,&sa,0)&&CreatePipe(&outputRead_.h,&outputWrite_.h,&sa,0),"Create consumer pipes");
  Require(SetHandleInformation(inputWrite_.h,HANDLE_FLAG_INHERIT,0)&&SetHandleInformation(outputRead_.h,HANDLE_FLAG_INHERIT,0),"Restrict pipe inheritance");
  STARTUPINFOW si{};si.cb=sizeof(si);si.dwFlags=STARTF_USESTDHANDLES;si.hStdInput=inputRead_.h;si.hStdOutput=outputWrite_.h;si.hStdError=GetStdHandle(STD_ERROR_HANDLE);
  PROCESS_INFORMATION pi{};std::wstring command=L"\""+exe.wstring()+L"\" --consume";
  Require(CreateProcessW(exe.c_str(),command.data(),nullptr,nullptr,TRUE,CREATE_NO_WINDOW,nullptr,nullptr,&si,&pi),"Launch x64 consumer");process_.h=pi.hProcess;thread_.h=pi.hThread;inputRead_.Close();outputWrite_.Close();
  Require(Write(inputWrite_.h,&b,sizeof(b)),"Send inherited resource handles");Receive(0);
 }
 ~Consumer(){inputWrite_.Close();if(process_.h&&WaitForSingleObject(process_.h,1000)==WAIT_TIMEOUT)TerminateProcess(process_.h,3);}
 void Receive(std::uint64_t expected){
  const auto deadline=GetTickCount64()+12000;
  for(;;){DWORD available=0;Require(PeekNamedPipe(outputRead_.h,nullptr,0,nullptr,&available,nullptr),"Poll consumer reply");if(available>=sizeof(Reply))break;Require(WaitForSingleObject(process_.h,0)==WAIT_TIMEOUT,"Consumer exited without reply");Require(GetTickCount64()<deadline,"Consumer/GPU verification timeout");Sleep(1);}
  Reply reply;Require(Read(outputRead_.h,&reply,sizeof(reply))&&reply.magic==Magic&&reply.status==0&&reply.bits==64&&!reply.reserved&&reply.frame==expected,"Consumer verification receipt");
 }
 void Verify(std::uint64_t frame){const Frame f{frame};Require(Write(inputWrite_.h,&f,sizeof(f)),"Send frame metadata");Receive(frame);}
 void Finish(){inputWrite_.Close();Require(WaitForSingleObject(process_.h,12000)==WAIT_OBJECT_0,"Consumer exit timeout");DWORD code=1;Require(GetExitCodeProcess(process_.h,&code)&&code==0,"Consumer exit status");}
};
void ManagedUpdate(IDirect3DTexture9* texture,std::uint64_t frame){
 D3DLOCKED_RECT locked{};Hr(texture->LockRect(0,&locked,nullptr,0),"Lock managed fixture texture");
 for(unsigned y=0;y<2;++y){auto* row=reinterpret_cast<std::uint32_t*>(static_cast<char*>(locked.pBits)+y*locked.Pitch);for(unsigned x=0;x<2;++x)row[x]=ManagedColor(frame);}
 Hr(texture->UnlockRect(0),"Unlock managed fixture texture");texture->PreLoad();
}
void CheckManaged(IDirect3DTexture9* texture,std::uint64_t frame){
 D3DLOCKED_RECT locked{};Hr(texture->LockRect(0,&locked,nullptr,D3DLOCK_READONLY),"Read managed fixture after reset");bool good=true;
 for(unsigned y=0;y<2;++y){const auto* row=reinterpret_cast<const std::uint32_t*>(static_cast<const char*>(locked.pBits)+y*locked.Pitch);for(unsigned x=0;x<2;++x)good=good&&row[x]==ManagedColor(frame);}
 Hr(texture->UnlockRect(0),"Unlock managed reset check");Require(good,"Managed texture contents survive Reset");
}
void DrawFixture(IDirect3DDevice9* device,IDirect3DTexture9* managed,const std::array<ComPtr<IDirect3DSurface9>,2>& surfaces,UINT width,UINT height,std::uint64_t frame){
 ManagedUpdate(managed,frame);
 for(unsigned eye=0;eye<2;++eye)for(unsigned q=0;q<4;++q){RECT rect{LONG(q&1?width/2:0),LONG(q&2?height/2:0),LONG(q&1?width:width/2),LONG(q&2?height:height/2)};Hr(device->ColorFill(surfaces[eye].Get(),&rect,Color(frame,eye,q)),"Fill GPU eye quadrants");}
 ComPtr<IDirect3DSurface9> original,depth;Hr(device->GetRenderTarget(0,&original),"Save native render target");const auto depthResult=device->GetDepthStencilSurface(&depth);Require(depthResult==S_OK||depthResult==D3DERR_NOTFOUND,"Save native depth");D3DVIEWPORT9 viewport{};Hr(device->GetViewport(&viewport),"Save native viewport");
 ComPtr<IDirect3DStateBlock9> state;Hr(device->CreateStateBlock(D3DSBT_ALL,&state),"Capture fixture rendering state");
 Hr(device->SetDepthStencilSurface(nullptr),"Disable fixture depth");Hr(device->SetVertexShader(nullptr),"Fixed-function fixture vertex");Hr(device->SetPixelShader(nullptr),"Fixed-function fixture pixel");Hr(device->SetFVF(D3DFVF_XYZRHW|D3DFVF_TEX1),"Fixture FVF");Hr(device->SetTexture(0,managed),"Bind managed fixture");
 for(auto flag:{D3DRS_LIGHTING,D3DRS_ZENABLE,D3DRS_ZWRITEENABLE,D3DRS_ALPHABLENDENABLE,D3DRS_ALPHATESTENABLE,D3DRS_FOGENABLE,D3DRS_SCISSORTESTENABLE,D3DRS_SRGBWRITEENABLE,D3DRS_DITHERENABLE})Hr(device->SetRenderState(flag,FALSE),"Fixture render flags");
 Hr(device->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE),"Fixture culling");Hr(device->SetRenderState(D3DRS_COLORWRITEENABLE,15),"Fixture RGBA writes");
 Hr(device->SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_SELECTARG1),"Fixture color operation");Hr(device->SetTextureStageState(0,D3DTSS_COLORARG1,D3DTA_TEXTURE),"Fixture color source");Hr(device->SetTextureStageState(0,D3DTSS_ALPHAOP,D3DTOP_SELECTARG1),"Fixture alpha operation");Hr(device->SetTextureStageState(0,D3DTSS_ALPHAARG1,D3DTA_TEXTURE),"Fixture alpha source");Hr(device->SetTextureStageState(1,D3DTSS_COLOROP,D3DTOP_DISABLE),"Disable second texture stage");
 for(auto sampler:{D3DSAMP_MINFILTER,D3DSAMP_MAGFILTER})Hr(device->SetSamplerState(0,sampler,D3DTEXF_POINT),"Fixture point filtering");Hr(device->SetSamplerState(0,D3DSAMP_MIPFILTER,D3DTEXF_NONE),"Fixture mip filter");Hr(device->SetSamplerState(0,D3DSAMP_SRGBTEXTURE,FALSE),"Fixture encoded color bytes");
 struct Vertex{float x,y,z,rhw,u,v;};const Vertex vertices[]={{-.5f,-.5f,0,1,0,0},{3.5f,-.5f,0,1,1,0},{-.5f,3.5f,0,1,0,1},{3.5f,3.5f,0,1,1,1}};
 Hr(device->BeginScene(),"Begin managed fixture draw");
 for(const auto& surface:surfaces){Hr(device->SetRenderTarget(0,surface.Get()),"Bind fixture eye");D3DVIEWPORT9 vp{0,0,width,height,0,1};Hr(device->SetViewport(&vp),"Fixture viewport");Hr(device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,2,vertices,sizeof(Vertex)),"Draw managed fixture patch");}
 Hr(device->EndScene(),"End fixture draw");Hr(state->Apply(),"Restore fixture state");Hr(device->SetRenderTarget(0,original.Get()),"Restore native target");Hr(device->SetDepthStencilSurface(depth.Get()),"Restore native depth");Hr(device->SetViewport(&viewport),"Restore native viewport");
}
D3D12_RESOURCE_BARRIER Transition(ID3D12Resource* r,D3D12_RESOURCE_STATES before,D3D12_RESOURCE_STATES after){D3D12_RESOURCE_BARRIER b{};b.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;b.Transition={r,D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,before,after};return b;}
struct Statistics {unsigned pairs=0,returned=0;std::vector<double> submitMs;double verifiedWallMs=0;};
double Elapsed(LARGE_INTEGER start){LARGE_INTEGER end{},frequency{};QueryPerformanceCounter(&end);QueryPerformanceFrequency(&frequency);return double(end.QuadPart-start.QuadPart)*1000.0/double(frequency.QuadPart);}
void Round(IDirect3DDevice9* device,IDirect3DDevice9On12* interop,ID3D12Device* d12,ID3D12CommandQueue* queue,IDirect3DTexture9* managed,const std::filesystem::path& consumerExe,UINT width,UINT height,std::uint64_t first,unsigned count,Statistics& stats){
 std::array<ComPtr<IDirect3DSurface9>,2> eyes;for(auto& eye:eyes)Hr(device->CreateRenderTarget(width,height,D3DFMT_A8R8G8B8,D3DMULTISAMPLE_NONE,0,FALSE,&eye,nullptr),"Create non-MSAA D3D9 eye");
 Bootstrap b;b.width=width;b.height=height;b.firstFrame=first;b.frames=count;const auto luid=d12->GetAdapterLuid();b.adapterLow=luid.LowPart;b.adapterHigh=luid.HighPart;
 D3D12_HEAP_PROPERTIES heap{};heap.Type=D3D12_HEAP_TYPE_DEFAULT;heap.CreationNodeMask=heap.VisibleNodeMask=1;
 D3D12_RESOURCE_DESC desc{};desc.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;desc.Width=width;desc.Height=height;desc.DepthOrArraySize=desc.MipLevels=1;desc.Format=DXGI_FORMAT_B8G8R8A8_UNORM;desc.SampleDesc.Count=1;desc.Layout=D3D12_TEXTURE_LAYOUT_UNKNOWN;desc.Flags=D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET|D3D12_RESOURCE_FLAG_ALLOW_SIMULTANEOUS_ACCESS;
 std::array<ComPtr<ID3D12Resource>,2> shared;std::array<Handle,2> sharedHandles;Handle readyHandle,doneHandle;SECURITY_ATTRIBUTES sa{sizeof(sa),nullptr,TRUE};
 for(unsigned eye=0;eye<2;++eye){Hr(d12->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_SHARED,&desc,D3D12_RESOURCE_STATE_COMMON,nullptr,IID_PPV_ARGS(&shared[eye])),"Create D3D12 shared GPU eye");Hr(d12->CreateSharedHandle(shared[eye].Get(),&sa,GENERIC_ALL,nullptr,&sharedHandles[eye].h),"Export inheritable GPU eye");b.textures[eye]=reinterpret_cast<std::uintptr_t>(sharedHandles[eye].h);}
 ComPtr<ID3D12Fence> ready,done;Hr(d12->CreateFence(0,D3D12_FENCE_FLAG_SHARED,IID_PPV_ARGS(&ready)),"Create producer fence");Hr(d12->CreateFence(0,D3D12_FENCE_FLAG_SHARED,IID_PPV_ARGS(&done)),"Create consumer fence");Hr(d12->CreateSharedHandle(ready.Get(),&sa,GENERIC_ALL,nullptr,&readyHandle.h),"Share producer fence");Hr(d12->CreateSharedHandle(done.Get(),&sa,GENERIC_ALL,nullptr,&doneHandle.h),"Share consumer fence");b.readyFence=reinterpret_cast<std::uintptr_t>(readyHandle.h);b.doneFence=reinterpret_cast<std::uintptr_t>(doneHandle.h);
 Consumer consumer(consumerExe,b);
 // Keep allocators alive through the round; no CPU fence wait or allocator
 // reset is needed to submit any pair. Consumer readback only paces the test.
 std::vector<ComPtr<ID3D12CommandAllocator>> allocators;std::vector<ComPtr<ID3D12GraphicsCommandList>> lists;
 LARGE_INTEGER wallStart{};QueryPerformanceCounter(&wallStart);
 for(unsigned i=0;i<count;++i){const std::uint64_t frame=first+i;
  DrawFixture(device,managed,eyes,width,height,frame);
  ComPtr<IDirect3DSurface9> original;Hr(device->GetRenderTarget(0,&original),"Save transfer native target");D3DVIEWPORT9 viewport{};Hr(device->GetViewport(&viewport),"Save transfer native viewport");
  ComPtr<ID3D12CommandAllocator> allocator;ComPtr<ID3D12GraphicsCommandList> list;Hr(d12->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&allocator)),"Create copy allocator");Hr(d12->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,allocator.Get(),nullptr,IID_PPV_ARGS(&list)),"Create copy list");
  LARGE_INTEGER start{};QueryPerformanceCounter(&start);if(i)Hr(queue->Wait(done.Get(),frame-1),"GPU wait before shared-pair reuse");
  std::array<ComPtr<ID3D12Resource>,2> source;
  for(unsigned eye=0;eye<2;++eye){Hr(interop->UnwrapUnderlyingResource(eyes[eye].Get(),queue,IID_PPV_ARGS(&source[eye])),"Unwrap D3D9 eye on copy queue");const auto actual=source[eye]->GetDesc();Require(actual.Dimension==D3D12_RESOURCE_DIMENSION_TEXTURE2D&&actual.Width==width&&actual.Height==height&&actual.DepthOrArraySize==1&&actual.MipLevels==1&&actual.SampleDesc.Count==1&&(actual.Format==DXGI_FORMAT_B8G8R8A8_UNORM||actual.Format==DXGI_FORMAT_B8G8R8A8_TYPELESS),"Validate underlying native BGRA eye");}
  // No D3D9 API access occurs while either native resource is checked out.
  for(unsigned eye=0;eye<2;++eye){auto src=Transition(source[eye].Get(),D3D12_RESOURCE_STATE_COMMON,D3D12_RESOURCE_STATE_COPY_SOURCE);auto dst=Transition(shared[eye].Get(),D3D12_RESOURCE_STATE_COMMON,D3D12_RESOURCE_STATE_COPY_DEST);const D3D12_RESOURCE_BARRIER begin[]={src,dst};list->ResourceBarrier(2,begin);list->CopyResource(shared[eye].Get(),source[eye].Get());std::swap(src.Transition.StateBefore,src.Transition.StateAfter);std::swap(dst.Transition.StateBefore,dst.Transition.StateAfter);const D3D12_RESOURCE_BARRIER end[]={src,dst};list->ResourceBarrier(2,end);}
  Hr(list->Close(),"Close GPU copy list");ID3D12CommandList* command=list.Get();queue->ExecuteCommandLists(1,&command);Hr(queue->Signal(ready.Get(),frame),"Signal both GPU eyes ready");
  UINT64 signal=frame;ID3D12Fence* fence=ready.Get();for(unsigned eye=0;eye<2;++eye){Hr(interop->ReturnUnderlyingResource(eyes[eye].Get(),1,&signal,&fence),"Return native eye with complete GPU fence");++stats.returned;}
  stats.submitMs.push_back(Elapsed(start));allocators.push_back(std::move(allocator));lists.push_back(std::move(list));
  ComPtr<IDirect3DSurface9> after;Hr(device->GetRenderTarget(0,&after),"Check transfer native target");D3DVIEWPORT9 afterViewport{};Hr(device->GetViewport(&afterViewport),"Check transfer native viewport");Require(original.Get()==after.Get()&&std::memcmp(&viewport,&afterViewport,sizeof(viewport))==0,"GPU transfer preserved native target and viewport");
  consumer.Verify(frame);Hr(d12->GetDeviceRemovedReason(),"Producer device state");Require(done->GetCompletedValue()>=frame,"Consumer fence completes after verified readback");++stats.pairs;
 }
 stats.verifiedWallMs+=Elapsed(wallStart);consumer.Finish();
}
int Produce(const std::filesystem::path& consumer,UINT width,UINT height){
 Require(sizeof(void*)==4,"Producer must be x86");DWORD binary=0;Require(GetBinaryTypeW(consumer.c_str(),&binary)&&binary==SCS_64BIT_BINARY,"Consumer must be x64");
 Window window;Handle module; // d3d9 remains loaded for process lifetime.
 const auto dll=LoadLibraryExW(L"d3d9.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);Require(dll!=nullptr,"Load system D3D9");
 const auto create=reinterpret_cast<PFN_Direct3DCreate9On12>(GetProcAddress(dll,"Direct3DCreate9On12"));Require(create!=nullptr,"System non-Ex D3D9On12 export");
 D3D9ON12_ARGS override{};override.Enable9On12=TRUE;ComPtr<IDirect3D9> api;api.Attach(create(D3D_SDK_VERSION,&override,1));Require(bool(api),"Create non-Ex D3D9On12 enumerator");
 D3DPRESENT_PARAMETERS pp{};pp.BackBufferWidth=pp.BackBufferHeight=64;pp.BackBufferFormat=D3DFMT_X8R8G8B8;pp.BackBufferCount=1;pp.SwapEffect=D3DSWAPEFFECT_DISCARD;pp.hDeviceWindow=window.h;pp.Windowed=TRUE;pp.PresentationInterval=D3DPRESENT_INTERVAL_IMMEDIATE;
 ComPtr<IDirect3DDevice9> device;Hr(api->CreateDevice(0,D3DDEVTYPE_HAL,window.h,D3DCREATE_HARDWARE_VERTEXPROCESSING|D3DCREATE_FPU_PRESERVE,&pp,&device),"Create ordinary D3D9 device on D3D12");
 ComPtr<IDirect3DDevice9Ex> ex;Require(FAILED(device.As(&ex)),"Preserve non-Ex device semantics");ComPtr<IDirect3DDevice9On12> interop;Hr(device.As(&interop),"Query actual D3D9On12 device");
 ComPtr<ID3D12Device> d12;Hr(interop->GetD3D12Device(IID_PPV_ARGS(&d12)),"Get underlying D3D12 device");ComPtr<ID3D12CommandQueue> queue;D3D12_COMMAND_QUEUE_DESC q{};q.Type=D3D12_COMMAND_LIST_TYPE_DIRECT;Hr(d12->CreateCommandQueue(&q,IID_PPV_ARGS(&queue)),"Create dedicated interop queue");
 ComPtr<IDirect3DTexture9> managed;Hr(device->CreateTexture(2,2,1,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,&managed,nullptr),"Create original-API managed texture");
 Statistics stats;Round(device.Get(),interop.Get(),d12.Get(),queue.Get(),managed.Get(),consumer,width,height,1,PairCount,stats);
 Hr(device->Reset(&pp),"Reset after GPU resource return and release");CheckManaged(managed.Get(),PairCount);
 Round(device.Get(),interop.Get(),d12.Get(),queue.Get(),managed.Get(),consumer,width,height,PairCount+1,1,stats);
 auto sorted=stats.submitMs;std::sort(sorted.begin(),sorted.end());double sum=0;for(auto t:sorted)sum+=t;const auto luid=d12->GetAdapterLuid();
 std::cout<<"{\"state\":\"verified\",\"producer_bits\":32,\"consumer_bits\":64,\"d3d9on12\":true,\"d3d9ex\":false,\"width\":"<<width<<",\"height\":"<<height<<",\"format\":87,\"adapter_low\":"<<luid.LowPart<<",\"adapter_high\":"<<luid.HighPart<<",\"pixel_verified_pairs\":"<<stats.pairs<<",\"returned_resources\":"<<stats.returned<<",\"managed_texture_creation_update_and_reset\":true,\"native_bindings_unchanged\":true,\"device_reset\":true,\"producer_cpu_image_readback\":false,\"producer_cpu_eye_upload\":false,\"consumer_readback_verification_only\":true,\"rgba_conversion_implemented\":false,\"game_connected\":false,\"headset_used\":false,\"timing_scope\":\"CPU_unwrap_copy_submission_signal_return_only\",\"submit_ms\":{\"min\":"<<sorted.front()<<",\"median\":"<<sorted[sorted.size()/2]<<",\"mean\":"<<sum/sorted.size()<<",\"max\":"<<sorted.back()<<"},\"verification_paced_wall_ms\":"<<stats.verifiedWallMs<<",\"raw_submit_ms\":[";
 for(std::size_t i=0;i<stats.submitMs.size();++i){if(i)std::cout<<',';std::cout<<stats.submitMs[i];}std::cout<<"]}"<<std::endl;return 0;
}
int Supervise(const std::filesystem::path& consumer,UINT width,UINT height){
 std::wstring self(32768,L'\0');const auto length=GetModuleFileNameW(nullptr,self.data(),DWORD(self.size()));Require(length&&length<self.size(),"Resolve probe executable");self.resize(length);
 Handle job,process,thread;job.h=CreateJobObjectW(nullptr,nullptr);Require(job.h!=nullptr,"Create bounded probe job");JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};limits.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;Require(SetInformationJobObject(job.h,JobObjectExtendedLimitInformation,&limits,sizeof(limits)),"Set probe tree lifetime bound");
 std::wstring command=L"\""+self+L"\" --produce --consumer-exe \""+consumer.wstring()+L"\" --width "+std::to_wstring(width)+L" --height "+std::to_wstring(height);
 STARTUPINFOW si{};si.cb=sizeof(si);si.dwFlags=STARTF_USESTDHANDLES;si.hStdInput=GetStdHandle(STD_INPUT_HANDLE);si.hStdOutput=GetStdHandle(STD_OUTPUT_HANDLE);si.hStdError=GetStdHandle(STD_ERROR_HANDLE);PROCESS_INFORMATION pi{};
 Require(CreateProcessW(self.c_str(),command.data(),nullptr,nullptr,TRUE,CREATE_NO_WINDOW|CREATE_SUSPENDED,nullptr,nullptr,&si,&pi),"Launch isolated GPU producer");process.h=pi.hProcess;thread.h=pi.hThread;
 if(!AssignProcessToJobObject(job.h,process.h)){TerminateProcess(process.h,3);throw std::runtime_error("Assign bounded producer/consumer job");}Require(ResumeThread(thread.h)!=DWORD(-1),"Resume GPU producer");Require(WaitForSingleObject(process.h,60000)==WAIT_OBJECT_0,"Whole GPU proof exceeded 60 seconds");DWORD code=1;Require(GetExitCodeProcess(process.h,&code),"Read GPU proof exit status");return int(code);
}
unsigned Dimension(const wchar_t* value){const std::wstring s(value);Require(!s.empty()&&s.size()<=4&&s.find_first_not_of(L"0123456789")==std::wstring::npos,"Numeric dimension");const auto n=std::stoul(s);Require(n>=8&&n<=4096,"Dimensions in 8..4096");return unsigned(n);}
}
int wmain(int argc,wchar_t** argv){try{
 if(argc==2&&std::wstring(argv[1])==L"--consume")return Consume();
 bool worker=false;unsigned width=137,height=93;std::filesystem::path consumer;
 for(int i=1;i<argc;++i){const std::wstring flag(argv[i]);if(flag==L"--produce"){worker=true;continue;}Require(i+1<argc,"Missing argument value");if(flag==L"--consumer-exe")consumer=std::filesystem::absolute(argv[++i]);else if(flag==L"--width")width=Dimension(argv[++i]);else if(flag==L"--height")height=Dimension(argv[++i]);else throw std::runtime_error("Unknown probe argument");}
 Require(!consumer.empty(),"Usage: KFVRD3D9On12Probe --consumer-exe x64-path [--width 2500 --height 2676]");
 return worker?Produce(consumer,width,height):Supervise(consumer,width,height);
 }catch(const std::exception& e){std::cerr<<e.what()<<std::endl;return 1;}}
