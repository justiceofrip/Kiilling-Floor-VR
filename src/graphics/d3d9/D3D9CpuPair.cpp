#include "kfvr/graphics/D3D9CpuPair.h"
#include <dxgi.h>
#include <wrl/client.h>
#include <cstring>
#include <emmintrin.h>
#include <vector>
namespace kfvr::graphics {
using Microsoft::WRL::ComPtr;
struct D3D9CpuPair::State {
 ComPtr<IDirect3DDevice9> d9;ComPtr<ID3D11Device> d11;ComPtr<ID3D11DeviceContext> context;
 std::array<ComPtr<IDirect3DSurface9>,2> staging,resolve;
 std::array<ComPtr<ID3D11Texture2D>,2> upload;
 std::array<std::vector<std::uint32_t>,2> pixels;
 D3D11PairProducer producer;PairTicket pendingTicket{};
 UINT width=0,height=0;D3DFORMAT source{};DXGI_FORMAT destination{};std::uint64_t epoch=0,lastFrame=0;
 bool pending=false,faulted=false;
};
D3D9CpuPair::D3D9CpuPair()=default;D3D9CpuPair::~D3D9CpuPair()=default;
void D3D9CpuPair::Reset()noexcept{state_.reset();error_=S_OK;lastMs_=0;timing_={};}
HRESULT D3D9CpuPair::LastError()const noexcept{return error_;}
double D3D9CpuPair::LastTransferMs()const noexcept{return lastMs_;}
D3D9TransferTiming D3D9CpuPair::LastTiming()const noexcept{return timing_;}
TextureDescriptor D3D9CpuPair::Descriptor()const noexcept{return state_?state_->producer.Descriptor():TextureDescriptor{};}
HRESULT D3D9CpuPair::AdapterLuid(IDirect3DDevice9* device,LUID& out)noexcept{
 out={};if(!device)return E_INVALIDARG;D3DDEVICE_CREATION_PARAMETERS creation{};auto hr=device->GetCreationParameters(&creation);if(FAILED(hr))return hr;
 ComPtr<IDirect3D9> original;hr=device->GetDirect3D(&original);if(FAILED(hr))return hr;
 ComPtr<IDirect3D9Ex> enumerator;hr=Direct3DCreate9Ex(D3D_SDK_VERSION,&enumerator);if(FAILED(hr))return hr;
 D3DADAPTER_IDENTIFIER9 a{},b{};hr=original->GetAdapterIdentifier(creation.AdapterOrdinal,0,&a);if(FAILED(hr))return hr;
 hr=enumerator->GetAdapterIdentifier(creation.AdapterOrdinal,0,&b);if(FAILED(hr))return hr;
 if(a.DeviceIdentifier!=b.DeviceIdentifier||a.VendorId!=b.VendorId||a.DeviceId!=b.DeviceId)return E_NOINTERFACE;
 return enumerator->GetAdapterLUID(creation.AdapterOrdinal,&out);
}
TransferResult D3D9CpuPair::Create(IDirect3DDevice9* d9,ID3D11Device* d11,UINT width,UINT height,D3DFORMAT source,DXGI_FORMAT destination,std::uint64_t epoch)noexcept{
 Reset();const bool format=destination==DXGI_FORMAT_R8G8B8A8_UNORM||destination==DXGI_FORMAT_R8G8B8A8_UNORM_SRGB||destination==DXGI_FORMAT_B8G8R8A8_UNORM||destination==DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
 // Conservative aggregate working resource budget: staging, resolve, CPU pixels,
 // upload and shared pair. Driver bookkeeping and caller's source RTs are extra.
 if(!d9||!d11||!width||!height||width>4096||height>4096||std::uint64_t(width)*height*40>256*1024*1024||!epoch||!format||(source!=D3DFMT_X8R8G8B8&&source!=D3DFMT_A8R8G8B8)){error_=E_INVALIDARG;return TransferResult::Invalid;}
 try{
  LUID luid{};error_=AdapterLuid(d9,luid);if(FAILED(error_))return TransferResult::Unavailable;
  ComPtr<IDXGIDevice> dxgi;error_=d11->QueryInterface(IID_PPV_ARGS(&dxgi));if(FAILED(error_))return TransferResult::Unavailable;
  ComPtr<IDXGIAdapter> adapter;error_=dxgi->GetAdapter(&adapter);if(FAILED(error_))return TransferResult::Unavailable;
  DXGI_ADAPTER_DESC desc{};error_=adapter->GetDesc(&desc);if(FAILED(error_))return TransferResult::Unavailable;
  if(luid.LowPart!=desc.AdapterLuid.LowPart||luid.HighPart!=desc.AdapterLuid.HighPart){error_=E_INVALIDARG;return TransferResult::Invalid;}
  auto next=std::make_unique<State>();next->d9=d9;next->d11=d11;d11->GetImmediateContext(&next->context);next->width=width;next->height=height;next->source=source;next->destination=destination;next->epoch=epoch;
  D3D11_TEXTURE2D_DESC texture{};texture.Width=width;texture.Height=height;texture.MipLevels=texture.ArraySize=texture.SampleDesc.Count=1;texture.Format=destination;texture.Usage=D3D11_USAGE_DEFAULT;texture.BindFlags=D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE;
  for(unsigned eye=0;eye<2;++eye){error_=d9->CreateOffscreenPlainSurface(width,height,source,D3DPOOL_SYSTEMMEM,&next->staging[eye],nullptr);if(FAILED(error_))return TransferResult::Unavailable;
   error_=d9->CreateRenderTarget(width,height,source,D3DMULTISAMPLE_NONE,0,FALSE,&next->resolve[eye],nullptr);if(FAILED(error_))return TransferResult::Unavailable;
   error_=d11->CreateTexture2D(&texture,nullptr,&next->upload[eye]);if(FAILED(error_))return TransferResult::Unavailable;next->pixels[eye].resize(std::size_t(width)*height);
  }
  const auto result=next->producer.Create(d11,width,height,destination,epoch);if(result!=TransferResult::Ok){error_=next->producer.LastError();return result;}
  state_=std::move(next);return TransferResult::Ok;
 }catch(...){error_=E_OUTOFMEMORY;return TransferResult::Unavailable;}
}
TransferResult D3D9CpuPair::Publish(const std::array<IDirect3DSurface9*,2>& surfaces,const FrameMetadata& frame,PairTicket& ticket)noexcept{
 ticket={};lastMs_=0;timing_={};if(!state_){error_=E_UNEXPECTED;return TransferResult::Unavailable;}auto& s=*state_;
 if(s.faulted)return TransferResult::Faulted;if(s.pending)return TransferResult::Busy;
 if(!surfaces[0]||!surfaces[1]||surfaces[0]==surfaces[1]||!frame.frameId||frame.frameId<=s.lastFrame||!frame.spaceGeneration||!frame.trackingGeneration||frame.predictedNs<=0){error_=E_INVALIDARG;return TransferResult::Invalid;}
 std::array<D3DSURFACE_DESC,2> descriptions{};
 for(unsigned eye=0;eye<2;++eye){ComPtr<IDirect3DDevice9> owner;error_=surfaces[eye]->GetDevice(&owner);if(FAILED(error_))return TransferResult::Invalid;
  error_=surfaces[eye]->GetDesc(&descriptions[eye]);const auto& d=descriptions[eye];if(FAILED(error_)||owner.Get()!=s.d9.Get()||d.Width!=s.width||d.Height!=s.height||d.Format!=s.source||d.Pool!=D3DPOOL_DEFAULT||!(d.Usage&D3DUSAGE_RENDERTARGET)){error_=E_INVALIDARG;return TransferResult::Invalid;}
 }
 if(!Valid(s.producer.Descriptor())){if(s.epoch==UINT64_MAX){error_=E_UNEXPECTED;return TransferResult::Faulted;}const auto result=s.producer.Create(s.d11.Get(),s.width,s.height,s.destination,++s.epoch);if(result!=TransferResult::Ok){error_=s.producer.LastError();return result;}}
 LARGE_INTEGER begin{},end{},frequency{};QueryPerformanceFrequency(&frequency);QueryPerformanceCounter(&begin);
 auto measure=[&](LARGE_INTEGER& start){LARGE_INTEGER at{};QueryPerformanceCounter(&at);const double ms=double(at.QuadPart-start.QuadPart)*1000/double(frequency.QuadPart);start=at;return ms;};
 LARGE_INTEGER stage=begin;
 for(unsigned eye=0;eye<2;++eye){auto* source=surfaces[eye];if(descriptions[eye].MultiSampleType!=D3DMULTISAMPLE_NONE){error_=s.d9->StretchRect(source,nullptr,s.resolve[eye].Get(),nullptr,D3DTEXF_NONE);if(FAILED(error_)){s.faulted=true;return TransferResult::Faulted;}source=s.resolve[eye].Get();}
  timing_.resolveMs+=measure(stage);
  error_=s.d9->GetRenderTargetData(source,s.staging[eye].Get());if(FAILED(error_)){s.faulted=true;return TransferResult::Faulted;}
  D3DLOCKED_RECT locked{};error_=s.staging[eye]->LockRect(&locked,nullptr,D3DLOCK_READONLY);if(FAILED(error_)){s.faulted=true;return TransferResult::Faulted;}
  timing_.readbackMs+=measure(stage);
  const bool valid=locked.pBits&&locked.Pitch>=int(s.width*4);if(valid){const bool rgba=s.destination==DXGI_FORMAT_R8G8B8A8_UNORM||s.destination==DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
   for(UINT y=0;y<s.height;++y){const auto* row=reinterpret_cast<const std::uint32_t*>(static_cast<const char*>(locked.pBits)+std::size_t(y)*locked.Pitch);auto* target=s.pixels[eye].data()+std::size_t(y)*s.width;
    if(!rgba&&s.source==D3DFMT_A8R8G8B8){std::memcpy(target,row,std::size_t(s.width)*4);continue;}
    // Four packed pixels at a time on the x86 game path. Unaligned loads/stores
    // and a scalar tail preserve arbitrary D3D pitch and non-multiple widths.
    const auto alpha=_mm_set1_epi32(s.source==D3DFMT_X8R8G8B8?int(0xff000000u):0);
    const auto keep=_mm_set1_epi32(int(0xff00ff00u)),blue=_mm_set1_epi32(255);
    UINT x=0;
    for(;x+4<=s.width;x+=4){
     auto pixel=_mm_or_si128(_mm_loadu_si128(reinterpret_cast<const __m128i*>(row+x)),alpha);
     if(rgba)pixel=_mm_or_si128(_mm_and_si128(pixel,keep),_mm_or_si128(_mm_slli_epi32(_mm_and_si128(pixel,blue),16),_mm_and_si128(_mm_srli_epi32(pixel,16),blue)));
     _mm_storeu_si128(reinterpret_cast<__m128i*>(target+x),pixel);
    }
    for(;x<s.width;++x){auto pixel=row[x];if(s.source==D3DFMT_X8R8G8B8)pixel|=0xff000000;if(rgba)pixel=(pixel&0xff00ff00)|((pixel&255)<<16)|((pixel>>16)&255);target[x]=pixel;}
   }
  }
  const auto unlock=s.staging[eye]->UnlockRect();if(!valid||FAILED(unlock)){error_=valid?unlock:E_INVALIDARG;s.faulted=true;return TransferResult::Faulted;}
  timing_.convertMs+=measure(stage);
 }
 // No shared eye is published until BOTH readbacks have succeeded. A failed
 // right eye can never submit a new left eye with an old right eye.
 for(unsigned eye=0;eye<2;++eye)s.context->UpdateSubresource(s.upload[eye].Get(),0,nullptr,s.pixels[eye].data(),s.width*4,0);
 timing_.uploadMs=measure(stage);
 const auto result=s.producer.Publish({TextureSlice{s.upload[0].Get(),0},TextureSlice{s.upload[1].Get(),0}},frame,ticket);error_=s.producer.LastError();
 timing_.publishMs=measure(stage);
 QueryPerformanceCounter(&end);if(frequency.QuadPart>0)lastMs_=double(end.QuadPart-begin.QuadPart)*1000/double(frequency.QuadPart);
 if(result==TransferResult::Ok){s.pending=true;s.pendingTicket=ticket;s.lastFrame=frame.frameId;}else if(result==TransferResult::Faulted)s.producer.Reset();return result;
}
bool D3D9CpuPair::PairConsumed(const PairTicket& ticket,bool consumed)noexcept{
 if(!state_||!state_->pending||std::memcmp(&ticket,&state_->pendingTicket,sizeof(ticket)))return false;auto& s=*state_;s.pending=false;if(!consumed)s.producer.Reset();return true;
}
}
