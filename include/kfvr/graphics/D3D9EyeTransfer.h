#pragma once
#include "kfvr/graphics/D3D9GpuPair.h"
namespace kfvr::graphics {
// A startup-selected On12 device uses only the GPU transport. A failure is
// reported instead of silently returning to the CPU path. Ordinary launches
// retain the established CPU implementation for comparison and compatibility.
class D3D9EyeTransfer {
public:
 TransferResult Create(IDirect3DDevice9* d9,ID3D11Device* d11,UINT w,UINT h,D3DFORMAT source,DXGI_FORMAT target,std::uint64_t epoch) noexcept {
  if(!Reset())return TransferResult::Faulted;
  gpuSelected_=D3D9GpuPair::Supported(d9);
  return gpuSelected_?gpu_.Create(d9,d11,w,h,source,target,epoch):cpu_.Create(d9,d11,w,h,source,target,epoch);
 }
 TransferResult Publish(const std::array<IDirect3DSurface9*,2>& eyes,const FrameMetadata& frame,PairTicket& ticket)noexcept{return gpuSelected_?gpu_.Publish(eyes,frame,ticket):cpu_.Publish(eyes,frame,ticket);}
 bool PairConsumed(const PairTicket& ticket,bool consumed)noexcept{return gpuSelected_?gpu_.PairConsumed(ticket,consumed):cpu_.PairConsumed(ticket,consumed);}
 TextureDescriptor Descriptor()const noexcept{return gpuSelected_?gpu_.Descriptor():cpu_.Descriptor();}
 HRESULT LastError()const noexcept{return gpuSelected_?gpu_.LastError():cpu_.LastError();}
 double LastTransferMs()const noexcept{return gpuSelected_?gpu_.LastTransferMs():cpu_.LastTransferMs();}
 D3D9TransferTiming LastTiming()const noexcept{return gpuSelected_?gpu_.LastTiming():cpu_.LastTiming();}
 bool Reset()noexcept{cpu_.Reset();if(!gpu_.Reset())released_=false;return released_;}
 bool ResourcesReleased()const noexcept{return released_;}
 bool GpuSelected()const noexcept{return gpuSelected_;}
 const char* Backend()const noexcept{return gpuSelected_?"d3d9on12_gpu":"d3d9_cpu";}
private:D3D9CpuPair cpu_;D3D9GpuPair gpu_;bool gpuSelected_=false,released_=true;
};
}
