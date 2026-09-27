#pragma once
#include "kfvr/graphics/D3D11SharedPair.h"
#include <d3d9.h>
namespace kfvr::graphics {
// Prototype transport for an ordinary (non-Ex) D3D9 device. CPU readback stalls
// are measured, not assumed suitable for shipping VR. All calls belong on the
// render thread, outside BeginScene/EndScene. Does not bind render targets or
// replay drawing. Caller must Reset BEFORE a native device reset/destruction.
struct D3D9TransferTiming { double resolveMs=0,readbackMs=0,convertMs=0,uploadMs=0,publishMs=0; };
class D3D9CpuPair {
public:
 D3D9CpuPair();~D3D9CpuPair();
 D3D9CpuPair(const D3D9CpuPair&)=delete;D3D9CpuPair& operator=(const D3D9CpuPair&)=delete;
 static HRESULT AdapterLuid(IDirect3DDevice9*,LUID&) noexcept;
 TransferResult Create(IDirect3DDevice9*,ID3D11Device*,UINT width,UINT height,D3DFORMAT source,DXGI_FORMAT destination,std::uint64_t epoch) noexcept;
 TransferResult Publish(const std::array<IDirect3DSurface9*,2>&,const FrameMetadata&,PairTicket&) noexcept;
 bool PairConsumed(const PairTicket&,bool consumed) noexcept;
 TextureDescriptor Descriptor()const noexcept;
 HRESULT LastError()const noexcept;
 double LastTransferMs()const noexcept;
 D3D9TransferTiming LastTiming()const noexcept;
 void Reset()noexcept;
private:struct State;std::unique_ptr<State> state_;HRESULT error_=S_OK;double lastMs_=0;D3D9TransferTiming timing_{};
};
}
