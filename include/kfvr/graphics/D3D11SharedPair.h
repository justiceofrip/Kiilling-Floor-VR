#pragma once
#include "kfvr/graphics/SharedTextureProtocol.h"
#include <d3d11.h>
#include <memory>

namespace kfvr::graphics {
enum class TransferResult { Ok, Busy, Invalid, Unavailable, Faulted };
struct TextureSlice { ID3D11Texture2D* texture=nullptr; UINT subresource=0; };
// One producer and one consumer, each called from its device's immediate-context
// thread. Caller serializes all context use. No game offsets or OpenXR dependency.
// Backpressure drops a whole pair with Busy; neither side blocks on mutexes.
// A fault requires recreation with a NEW descriptor/session (never retry its keys).
class D3D11PairProducer {
public:
    D3D11PairProducer();~D3D11PairProducer();
    D3D11PairProducer(const D3D11PairProducer&)=delete;
    D3D11PairProducer& operator=(const D3D11PairProducer&)=delete;
    TransferResult Create(ID3D11Device*,UINT width,UINT height,DXGI_FORMAT,std::uint64_t epoch) noexcept;
    TransferResult Publish(const std::array<TextureSlice,2>&,const FrameMetadata&,PairTicket&) noexcept;
    TextureDescriptor Descriptor() const noexcept;
    HRESULT LastError() const noexcept;
    void Reset() noexcept;
private: struct State;std::unique_ptr<State> state_;HRESULT error_=S_OK;
};
class D3D11PairConsumer {
public:
    D3D11PairConsumer();~D3D11PairConsumer();
    D3D11PairConsumer(const D3D11PairConsumer&)=delete;
    D3D11PairConsumer& operator=(const D3D11PairConsumer&)=delete;
    TransferResult Open(ID3D11Device*,const TextureDescriptor&) noexcept;
    // Copies both eyes into caller-owned resources (e.g. acquired XR slices).
    // On failure their contents must not be submitted. No resource is exposed
    // while another process owns it. Ticket is supplied over reliable IPC.
    TransferResult Copy(const PairTicket&,const std::array<TextureSlice,2>&) noexcept;
    HRESULT LastError() const noexcept;
    void Reset() noexcept;
private: struct State;std::unique_ptr<State> state_;HRESULT error_=S_OK;
};
}
