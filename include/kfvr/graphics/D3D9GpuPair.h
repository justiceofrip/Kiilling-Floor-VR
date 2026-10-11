#pragma once
#include "kfvr/graphics/D3D9CpuPair.h"
namespace kfvr::graphics {
// Optional transport for a device created through Direct3DCreate9On12. Uses
// GPU copies/fences only; the public keyed-mutex pair protocol is unchanged.
// The D3D11 immediate context must belong exclusively to this transfer object.
class D3D9GpuPair {
public:
 D3D9GpuPair();~D3D9GpuPair();
 D3D9GpuPair(const D3D9GpuPair&)=delete;D3D9GpuPair& operator=(const D3D9GpuPair&)=delete;
 static bool Supported(IDirect3DDevice9*) noexcept;
 TransferResult Create(IDirect3DDevice9*,ID3D11Device*,UINT,UINT,D3DFORMAT,DXGI_FORMAT,std::uint64_t) noexcept;
 TransferResult Publish(const std::array<IDirect3DSurface9*,2>&,const FrameMetadata&,PairTicket&) noexcept;
 bool PairConsumed(const PairTicket&,bool) noexcept;
 TextureDescriptor Descriptor()const noexcept;
 HRESULT LastError()const noexcept{return error_;}
 double LastTransferMs()const noexcept{return lastMs_;}
 D3D9TransferTiming LastTiming()const noexcept{return timing_;}
 // False indicates resources retained until process exit after a GPU timeout.
 bool Reset() noexcept;
private:
 struct State;std::unique_ptr<State> state_;HRESULT error_=S_OK;
 double lastMs_=0;D3D9TransferTiming timing_{};
};
}
