#include "kfvr/graphics/D3D9GpuPair.h"
#include <d3d12.h>
#include <d3d9on12.h>
#include <d3d11_4.h>
#include <d3dcompiler.h>
#include <dxgi1_2.h>
#include <wrl/client.h>
#include <cstring>
namespace kfvr::graphics {
using Microsoft::WRL::ComPtr;
namespace {
struct Handle {HANDLE value=nullptr;~Handle(){if(value)CloseHandle(value);}};
struct Failure {HRESULT hr;};
void Check(HRESULT hr){if(FAILED(hr))throw Failure{hr};}
D3D12_RESOURCE_BARRIER Barrier(ID3D12Resource* resource,D3D12_RESOURCE_STATES before,D3D12_RESOURCE_STATES after){
 D3D12_RESOURCE_BARRIER b{};b.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
 b.Transition={resource,D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,before,after};return b;
}
double Milliseconds(LARGE_INTEGER start){LARGE_INTEGER now{},freq{};QueryPerformanceCounter(&now);QueryPerformanceFrequency(&freq);return double(now.QuadPart-start.QuadPart)*1000.0/double(freq.QuadPart);}
constexpr char Shader[]=R"(
struct Vertex {float4 position:SV_POSITION;};
Vertex VS(uint id:SV_VertexID){Vertex o;o.position=float4(id==1?3:-1,id==2?-3:1,0,1);return o;}
Texture2D<float4> image:register(t0);
float4 PS(Vertex i):SV_TARGET{return image.Load(int3(int2(i.position.xy),0));}
)";
}
struct D3D9GpuPair::State {
 ComPtr<IDirect3DDevice9> d9;ComPtr<IDirect3DDevice9On12> interop;
 ComPtr<ID3D12Device> d12;ComPtr<ID3D12CommandQueue> queue;
 ComPtr<ID3D12CommandAllocator> allocator;ComPtr<ID3D12GraphicsCommandList> list;
 ComPtr<ID3D12Fence> ready,done;ComPtr<ID3D11Fence> ready11,done11;
 ComPtr<ID3D11Device> d11;ComPtr<ID3D11DeviceContext4> context;
 std::array<ComPtr<ID3D12Resource>,2> shared,source;
 std::array<ComPtr<IDirect3DSurface9>,2> nativeSource;
 std::array<ComPtr<ID3D11Texture2D>,2> opened,output;
 std::array<ComPtr<ID3D11ShaderResourceView>,2> srv;
 std::array<ComPtr<ID3D11RenderTargetView>,2> rtv;
 ComPtr<ID3D11VertexShader> vs;ComPtr<ID3D11PixelShader> ps;
 ComPtr<ID3D11RasterizerState> raster;ComPtr<ID3D11DepthStencilState> depth;
 D3D11PairProducer producer;PairTicket pendingTicket{};
 UINT width=0,height=0;DXGI_FORMAT format{};
 std::uint64_t epoch=0,lastFrame=0,serial=0,readyValue=0,doneValue=0;
 bool pending=false,faulted=false,unsafeSubmission=false;
 std::array<bool,2> checkedOut{};
 bool Drain()noexcept{
  if(!d12||FAILED(d12->GetDeviceRemovedReason()))return true;
  if(unsafeSubmission||checkedOut[0]||checkedOut[1])return false;
  const auto deadline=GetTickCount64()+1000;
  for(const auto& entry:{std::pair{ready.Get(),readyValue},std::pair{done.Get(),doneValue}}){
   if(!entry.first||!entry.second||entry.first->GetCompletedValue()>=entry.second)continue;
   Handle event;event.value=CreateEventW(nullptr,FALSE,FALSE,nullptr);
   if(!event.value||FAILED(entry.first->SetEventOnCompletion(entry.second,event.value)))return false;
   const auto now=GetTickCount64();if(now>=deadline||WaitForSingleObject(event.value,DWORD(deadline-now))!=WAIT_OBJECT_0)return false;
  }
  return true;
 }
};
D3D9GpuPair::D3D9GpuPair()=default;
D3D9GpuPair::~D3D9GpuPair(){Reset();}
bool D3D9GpuPair::Reset() noexcept{
 const bool drained=!state_||state_->Drain();
 // D3D12 does not retain submitted resources for us. On a timeout, preserve
 // their COM ownership in the resident module rather than free in-flight data.
 if(!drained)static_cast<void>(state_.release());else state_.reset();
 error_=drained?S_OK:HRESULT_FROM_WIN32(WAIT_TIMEOUT);lastMs_=0;timing_={};return drained;
}
bool D3D9GpuPair::Supported(IDirect3DDevice9* device)noexcept{ComPtr<IDirect3DDevice9On12> interop;return device&&SUCCEEDED(device->QueryInterface(IID_PPV_ARGS(&interop)));}
TextureDescriptor D3D9GpuPair::Descriptor()const noexcept{return state_?state_->producer.Descriptor():TextureDescriptor{};}
TransferResult D3D9GpuPair::Create(IDirect3DDevice9* d9,ID3D11Device* d11,UINT width,UINT height,D3DFORMAT source,DXGI_FORMAT destination,std::uint64_t epoch)noexcept{
 if(!Reset())return TransferResult::Faulted;
 const bool rgba=destination==DXGI_FORMAT_R8G8B8A8_UNORM||destination==DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
 const bool bgra=destination==DXGI_FORMAT_B8G8R8A8_UNORM||destination==DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
 if(!d9||!d11||!width||!height||width>4096||height>4096||std::uint64_t(width)*height*40>256*1024*1024||!epoch||(!rgba&&!bgra)||source!=D3DFMT_A8R8G8B8){error_=E_INVALIDARG;return TransferResult::Invalid;}
 try{
  auto next=std::make_unique<State>();auto& s=*next;s.d9=d9;s.d11=d11;s.width=width;s.height=height;s.format=destination;s.epoch=epoch;
  Check(d9->QueryInterface(IID_PPV_ARGS(&s.interop)));Check(s.interop->GetD3D12Device(IID_PPV_ARGS(&s.d12)));
  ComPtr<IDXGIDevice> dxgi;Check(d11->QueryInterface(IID_PPV_ARGS(&dxgi)));ComPtr<IDXGIAdapter> adapter;Check(dxgi->GetAdapter(&adapter));DXGI_ADAPTER_DESC description{};Check(adapter->GetDesc(&description));
  const auto luid=s.d12->GetAdapterLuid();if(luid.LowPart!=description.AdapterLuid.LowPart||luid.HighPart!=description.AdapterLuid.HighPart)throw Failure{E_INVALIDARG};
  ComPtr<ID3D11Device5> device5;Check(d11->QueryInterface(IID_PPV_ARGS(&device5)));ComPtr<ID3D11DeviceContext> context;d11->GetImmediateContext(&context);Check(context.As(&s.context));
  D3D12_COMMAND_QUEUE_DESC queue{};queue.Type=D3D12_COMMAND_LIST_TYPE_DIRECT;Check(s.d12->CreateCommandQueue(&queue,IID_PPV_ARGS(&s.queue)));
  Check(s.d12->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&s.allocator)));
  Check(s.d12->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,s.allocator.Get(),nullptr,IID_PPV_ARGS(&s.list)));Check(s.list->Close());
  for(unsigned i=0;i<2;++i){auto& fence=i?s.done:s.ready;auto& fence11=i?s.done11:s.ready11;Handle handle;
   Check(s.d12->CreateFence(0,D3D12_FENCE_FLAG_SHARED,IID_PPV_ARGS(&fence)));Check(s.d12->CreateSharedHandle(fence.Get(),nullptr,GENERIC_ALL,nullptr,&handle.value));Check(device5->OpenSharedFence(handle.value,IID_PPV_ARGS(&fence11)));
  }
  D3D12_HEAP_PROPERTIES heap{};heap.Type=D3D12_HEAP_TYPE_DEFAULT;heap.CreationNodeMask=heap.VisibleNodeMask=1;
  D3D12_RESOURCE_DESC resource{};resource.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;resource.Width=width;resource.Height=height;resource.DepthOrArraySize=resource.MipLevels=1;resource.Format=DXGI_FORMAT_B8G8R8A8_UNORM;resource.SampleDesc.Count=1;resource.Layout=D3D12_TEXTURE_LAYOUT_UNKNOWN;resource.Flags=D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET|D3D12_RESOURCE_FLAG_ALLOW_SIMULTANEOUS_ACCESS;
  D3D11_TEXTURE2D_DESC texture{};texture.Width=width;texture.Height=height;texture.MipLevels=texture.ArraySize=texture.SampleDesc.Count=1;
  // KF eye pixels are already encoded. Linear SRV/RTV views perform only a
  // channel-layout conversion; sRGB transfer metadata must not encode twice.
  texture.Format=rgba?DXGI_FORMAT_R8G8B8A8_UNORM:DXGI_FORMAT_B8G8R8A8_UNORM;texture.Usage=D3D11_USAGE_DEFAULT;texture.BindFlags=D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE;
  for(unsigned eye=0;eye<2;++eye){Handle handle;
   Check(s.d12->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_SHARED,&resource,D3D12_RESOURCE_STATE_COMMON,nullptr,IID_PPV_ARGS(&s.shared[eye])));
   Check(s.d12->CreateSharedHandle(s.shared[eye].Get(),nullptr,GENERIC_ALL,nullptr,&handle.value));Check(device5->OpenSharedResource1(handle.value,IID_PPV_ARGS(&s.opened[eye])));
   Check(d11->CreateTexture2D(&texture,nullptr,&s.output[eye]));
   if(rgba){Check(d11->CreateShaderResourceView(s.opened[eye].Get(),nullptr,&s.srv[eye]));Check(d11->CreateRenderTargetView(s.output[eye].Get(),nullptr,&s.rtv[eye]));}
  }
  if(rgba){ComPtr<ID3DBlob> vertex,pixel,errors;
   Check(D3DCompile(Shader,sizeof(Shader)-1,nullptr,nullptr,nullptr,"VS","vs_5_0",D3DCOMPILE_ENABLE_STRICTNESS,0,&vertex,&errors));
   Check(D3DCompile(Shader,sizeof(Shader)-1,nullptr,nullptr,nullptr,"PS","ps_5_0",D3DCOMPILE_ENABLE_STRICTNESS,0,&pixel,&errors));
   Check(d11->CreateVertexShader(vertex->GetBufferPointer(),vertex->GetBufferSize(),nullptr,&s.vs));Check(d11->CreatePixelShader(pixel->GetBufferPointer(),pixel->GetBufferSize(),nullptr,&s.ps));
   D3D11_RASTERIZER_DESC raster{};raster.FillMode=D3D11_FILL_SOLID;raster.CullMode=D3D11_CULL_NONE;raster.DepthClipEnable=TRUE;Check(d11->CreateRasterizerState(&raster,&s.raster));
   D3D11_DEPTH_STENCIL_DESC depth{};depth.DepthEnable=FALSE;Check(d11->CreateDepthStencilState(&depth,&s.depth));
  }
  const auto result=s.producer.Create(d11,width,height,destination,epoch);if(result!=TransferResult::Ok){error_=s.producer.LastError();return result;}
  state_=std::move(next);return TransferResult::Ok;
 }catch(const Failure& e){error_=e.hr;return TransferResult::Unavailable;}catch(...){error_=E_OUTOFMEMORY;return TransferResult::Unavailable;}
}
TransferResult D3D9GpuPair::Publish(const std::array<IDirect3DSurface9*,2>& surfaces,const FrameMetadata& frame,PairTicket& ticket)noexcept{
 ticket={};lastMs_=0;timing_={};if(!state_){error_=E_UNEXPECTED;return TransferResult::Unavailable;}auto& s=*state_;
 if(s.faulted)return TransferResult::Faulted;if(s.pending)return TransferResult::Busy;
 if(!surfaces[0]||!surfaces[1]||surfaces[0]==surfaces[1]||!frame.frameId||frame.frameId<=s.lastFrame||!frame.spaceGeneration||!frame.trackingGeneration||frame.predictedNs<=0){error_=E_INVALIDARG;return TransferResult::Invalid;}
 for(auto* surface:surfaces){ComPtr<IDirect3DDevice9> owner;D3DSURFACE_DESC d{};error_=surface->GetDevice(&owner);if(FAILED(error_))return TransferResult::Invalid;error_=surface->GetDesc(&d);
  if(FAILED(error_)||owner.Get()!=s.d9.Get()||d.Width!=s.width||d.Height!=s.height||d.Format!=D3DFMT_A8R8G8B8||d.MultiSampleType!=D3DMULTISAMPLE_NONE||d.Pool!=D3DPOOL_DEFAULT||!(d.Usage&D3DUSAGE_RENDERTARGET)){error_=E_INVALIDARG;return TransferResult::Invalid;}
 }
 if(FAILED(s.d12->GetDeviceRemovedReason())||FAILED(s.d11->GetDeviceRemovedReason())){error_=DXGI_ERROR_DEVICE_REMOVED;s.faulted=true;return TransferResult::Faulted;}
 // Never reset an allocator still in use. Drop this entire pair rather than
 // stall the game thread waiting for the GPU.
 if(s.ready->GetCompletedValue()<s.readyValue)return TransferResult::Busy;
 LARGE_INTEGER start{},stage{};QueryPerformanceCounter(&start);stage=start;
 unsigned checkedOut=0;bool executed=false;
 try{
  if(!Valid(s.producer.Descriptor())){if(s.epoch==UINT64_MAX)throw Failure{E_UNEXPECTED};const auto result=s.producer.Create(s.d11.Get(),s.width,s.height,s.format,++s.epoch);if(result!=TransferResult::Ok)throw Failure{s.producer.LastError()};}
  Check(s.allocator->Reset());Check(s.list->Reset(s.allocator.Get(),nullptr));
  for(auto& source:s.source)source.Reset();for(auto& source:s.nativeSource)source.Reset();
  if(s.doneValue)Check(s.queue->Wait(s.done.Get(),s.doneValue));
  for(unsigned eye=0;eye<2;++eye){s.nativeSource[eye]=surfaces[eye];Check(s.interop->UnwrapUnderlyingResource(surfaces[eye],s.queue.Get(),IID_PPV_ARGS(&s.source[eye])));++checkedOut;s.checkedOut[eye]=true;
   const auto d=s.source[eye]->GetDesc();if(d.Dimension!=D3D12_RESOURCE_DIMENSION_TEXTURE2D||d.Width!=s.width||d.Height!=s.height||d.DepthOrArraySize!=1||d.MipLevels!=1||d.SampleDesc.Count!=1||(d.Format!=DXGI_FORMAT_B8G8R8A8_UNORM&&d.Format!=DXGI_FORMAT_B8G8R8A8_TYPELESS))throw Failure{E_INVALIDARG};
  }
  for(unsigned eye=0;eye<2;++eye){auto src=Barrier(s.source[eye].Get(),D3D12_RESOURCE_STATE_COMMON,D3D12_RESOURCE_STATE_COPY_SOURCE);auto dst=Barrier(s.shared[eye].Get(),D3D12_RESOURCE_STATE_COMMON,D3D12_RESOURCE_STATE_COPY_DEST);const D3D12_RESOURCE_BARRIER begin[]={src,dst};s.list->ResourceBarrier(2,begin);s.list->CopyResource(s.shared[eye].Get(),s.source[eye].Get());std::swap(src.Transition.StateBefore,src.Transition.StateAfter);std::swap(dst.Transition.StateBefore,dst.Transition.StateAfter);const D3D12_RESOURCE_BARRIER end[]={src,dst};s.list->ResourceBarrier(2,end);}
  Check(s.list->Close());if(s.serial==UINT64_MAX)throw Failure{E_UNEXPECTED};const auto serial=++s.serial;
  ID3D12CommandList* commands=s.list.Get();s.queue->ExecuteCommandLists(1,&commands);executed=true;s.unsafeSubmission=true;
  Check(s.queue->Signal(s.ready.Get(),serial));s.readyValue=serial;s.unsafeSubmission=false;
  UINT64 signal=serial;ID3D12Fence* fence=s.ready.Get();HRESULT returnError=S_OK;
  for(unsigned eye=0;eye<checkedOut;++eye){const auto hr=s.interop->ReturnUnderlyingResource(surfaces[eye],1,&signal,&fence);if(FAILED(hr))returnError=hr;else s.checkedOut[eye]=false;}checkedOut=0;Check(returnError);
  timing_.resolveMs=Milliseconds(stage);QueryPerformanceCounter(&stage);
  // Any failure after local GPU work is queued must retain that work until
  // a completion fence is known, including failures of Signal itself.
  s.unsafeSubmission=true;
  Check(s.context->Wait(s.ready11.Get(),serial));
  if(s.vs){
   s.context->ClearState();s.context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);s.context->VSSetShader(s.vs.Get(),nullptr,0);s.context->PSSetShader(s.ps.Get(),nullptr,0);s.context->RSSetState(s.raster.Get());s.context->OMSetDepthStencilState(s.depth.Get(),0);
   const D3D11_VIEWPORT viewport{0,0,float(s.width),float(s.height),0,1};s.context->RSSetViewports(1,&viewport);
   for(unsigned eye=0;eye<2;++eye){auto* srv=s.srv[eye].Get();auto* rtv=s.rtv[eye].Get();s.context->PSSetShaderResources(0,1,&srv);s.context->OMSetRenderTargets(1,&rtv,nullptr);s.context->Draw(3,0);}
   s.context->ClearState();
  }else for(unsigned eye=0;eye<2;++eye)s.context->CopyResource(s.output[eye].Get(),s.opened[eye].Get());
  timing_.convertMs=Milliseconds(stage);QueryPerformanceCounter(&stage);
  const auto result=s.producer.Publish({TextureSlice{s.output[0].Get(),0},TextureSlice{s.output[1].Get(),0}},frame,ticket);error_=s.producer.LastError();timing_.publishMs=Milliseconds(stage);
  // Fence the entire local D3D11 path, including publication copies. This
  // protects inputs, outputs and resources released by Reset on every result.
  Check(s.context->Signal(s.done11.Get(),serial));s.doneValue=serial;s.context->Flush();s.unsafeSubmission=false;lastMs_=Milliseconds(start);
  if(result==TransferResult::Ok){s.pending=true;s.pendingTicket=ticket;s.lastFrame=frame.frameId;}else if(result==TransferResult::Faulted)s.producer.Reset();
  return result;
 }catch(const Failure& e){error_=e.hr;}catch(...){error_=E_OUTOFMEMORY;}
 // Before submission, returning an unmodified checked-out resource needs no
 // fence. After submission, every return includes the signaled completion.
 for(unsigned eye=0;eye<checkedOut;++eye){UINT64 value=s.readyValue;ID3D12Fence* fence=s.ready.Get();
  HRESULT returned=E_FAIL;
  if(!executed)returned=s.interop->ReturnUnderlyingResource(surfaces[eye],0,nullptr,nullptr);
  else if(!s.unsafeSubmission)returned=s.interop->ReturnUnderlyingResource(surfaces[eye],1,&value,&fence);
  if(SUCCEEDED(returned))s.checkedOut[eye]=false;
 }
 s.faulted=true;lastMs_=Milliseconds(start);return TransferResult::Faulted;
}
bool D3D9GpuPair::PairConsumed(const PairTicket& ticket,bool consumed)noexcept{
 if(!state_||!state_->pending||std::memcmp(&ticket,&state_->pendingTicket,sizeof(ticket)))return false;
 state_->pending=false;if(!consumed)state_->producer.Reset();return true;
}
}
