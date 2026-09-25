#include "kfvr/graphics/D3D11SharedPair.h"
#include <d3d11_1.h>
#include <dxgi1_2.h>
#include <wrl/client.h>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <new>
#include <string>

namespace kfvr::graphics {
using Microsoft::WRL::ComPtr;
namespace {
struct SharedEye {
    ComPtr<ID3D11Texture2D> texture;
    ComPtr<IDXGIKeyedMutex> mutex;
    HANDLE handle=nullptr;
    ~SharedEye(){if(handle)CloseHandle(handle);}
};
struct PairState {
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    TextureDescriptor descriptor{};
    std::array<SharedEye,2> eyes{};
    std::uint64_t sequence=0,frameId=0;
    bool faulted=false;
};
std::wstring ResourceName(const TextureDescriptor& d,unsigned eye) {
    std::wstring name=L"Local\\KFVR.Texture.v1.";
    constexpr wchar_t hex[]=L"0123456789abcdef";
    for(auto b:d.session){name+=hex[b>>4];name+=hex[b&15];}
    name+=L"."+std::to_wstring(d.resourceEpoch)+L"."+std::to_wstring(eye);
    return name;
}
HRESULT AdapterIdentity(ID3D11Device* device,TextureDescriptor& descriptor) noexcept {
    ComPtr<IDXGIDevice> dxgi;HRESULT hr=device->QueryInterface(IID_PPV_ARGS(&dxgi));if(FAILED(hr))return hr;
    ComPtr<IDXGIAdapter> adapter;hr=dxgi->GetAdapter(&adapter);if(FAILED(hr))return hr;
    DXGI_ADAPTER_DESC desc{};hr=adapter->GetDesc(&desc);if(FAILED(hr))return hr;
    descriptor.adapterLow=desc.AdapterLuid.LowPart;descriptor.adapterHigh=desc.AdapterLuid.HighPart;return S_OK;
}
bool SlicesValid(const PairState& s,const std::array<TextureSlice,2>& slices) noexcept {
    if(!slices[0].texture||!slices[1].texture)return false;
    ComPtr<IUnknown> identities[2];
    for(unsigned eye=0;eye<2;++eye){
        const auto& slice=slices[eye];D3D11_TEXTURE2D_DESC d{};slice.texture->GetDesc(&d);
        if(!CopyCompatibleFormats(std::uint32_t(d.Format),s.descriptor.format) || d.SampleDesc.Count!=1 || d.SampleDesc.Quality ||
            !d.MipLevels || d.MipLevels>15 || !d.ArraySize ||
            slice.subresource>=std::uint64_t(d.MipLevels)*d.ArraySize || d.Usage!=D3D11_USAGE_DEFAULT)return false;
        const auto mip=slice.subresource%d.MipLevels;
        if((std::max)(1u,d.Width>>mip)!=s.descriptor.width ||
           (std::max)(1u,d.Height>>mip)!=s.descriptor.height)return false;
        ComPtr<ID3D11Device> owner;slice.texture->GetDevice(&owner);if(owner.Get()!=s.device.Get())return false;
        if(FAILED(slice.texture->QueryInterface(IID_PPV_ARGS(&identities[eye]))))return false;
        for(const auto& shared:s.eyes){
            ComPtr<IUnknown> identity;if(FAILED(shared.texture.As(&identity)) || identity==identities[eye])return false;
        }
    }
    return identities[0]!=identities[1] || slices[0].subresource!=slices[1].subresource;
}
TransferResult LockPair(PairState& s,std::uint64_t key,HRESULT& error) noexcept {
    unsigned held=0;
    for(;held<2;++held){
        error=s.eyes[held].mutex->AcquireSync(key,0);
        // WAIT_TIMEOUT/WAIT_ABANDONED are positive HRESULTs. SUCCEEDED is wrong.
        if(error!=S_OK){
            const auto failure=error;
            for(unsigned i=0;i<held;++i){const auto release=s.eyes[i].mutex->ReleaseSync(key);if(release!=S_OK)s.faulted=true;}
            if(failure!=HRESULT(WAIT_TIMEOUT))s.faulted=true;
            return s.faulted?TransferResult::Faulted:TransferResult::Busy;
        }
    }
    error=S_OK;return TransferResult::Ok;
}
TransferResult UnlockPair(PairState& s,std::uint64_t key,HRESULT& error) noexcept {
    HRESULT first=S_OK;
    for(auto& eye:s.eyes){const auto hr=eye.mutex->ReleaseSync(key);if(first==S_OK && hr!=S_OK)first=hr;}
    if(first!=S_OK){s.faulted=true;error=first;return TransferResult::Faulted;}
    return TransferResult::Ok;
}
TransferResult DeviceStatus(PairState& s,HRESULT& error) noexcept {
    error=s.device->GetDeviceRemovedReason();
    if(error!=S_OK){s.faulted=true;return TransferResult::Faulted;}
    return TransferResult::Ok;
}
}
struct D3D11PairProducer::State:PairState{};
struct D3D11PairConsumer::State:PairState{};
D3D11PairProducer::D3D11PairProducer()=default;
D3D11PairProducer::~D3D11PairProducer()=default;
void D3D11PairProducer::Reset() noexcept{state_.reset();error_=S_OK;}
HRESULT D3D11PairProducer::LastError()const noexcept{return error_;}
TextureDescriptor D3D11PairProducer::Descriptor()const noexcept{return state_?state_->descriptor:TextureDescriptor{};}
TransferResult D3D11PairProducer::Create(ID3D11Device* device,UINT width,UINT height,DXGI_FORMAT format,std::uint64_t epoch) noexcept {
    Reset();if(!device){error_=E_INVALIDARG;return TransferResult::Invalid;}
    try {
        auto next=std::make_unique<State>();auto& d=next->descriptor;
        d.width=width;d.height=height;d.format=format;d.resourceEpoch=epoch;
        GUID id{};error_=CoCreateGuid(&id);if(FAILED(error_))return TransferResult::Unavailable;
        std::memcpy(d.session.data(),&id,sizeof(id));
        if(!Valid(d)){error_=E_INVALIDARG;return TransferResult::Invalid;}
        next->device=device;device->GetImmediateContext(&next->context);
        error_=AdapterIdentity(device,d);if(FAILED(error_))return TransferResult::Unavailable;
        D3D11_TEXTURE2D_DESC desc{};desc.Width=width;desc.Height=height;desc.MipLevels=desc.ArraySize=1;
        desc.Format=format;desc.SampleDesc.Count=1;desc.Usage=D3D11_USAGE_DEFAULT;
        desc.BindFlags=D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE;
        desc.MiscFlags=D3D11_RESOURCE_MISC_SHARED_NTHANDLE|D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX;
        for(unsigned eye=0;eye<2;++eye){
            auto& target=next->eyes[eye];error_=device->CreateTexture2D(&desc,nullptr,&target.texture);
            if(FAILED(error_))return TransferResult::Unavailable;
            error_=target.texture.As(&target.mutex);if(FAILED(error_))return TransferResult::Unavailable;
            ComPtr<IDXGIResource1> resource;error_=target.texture.As(&resource);if(FAILED(error_))return TransferResult::Unavailable;
            const auto name=ResourceName(d,eye);
            error_=resource->CreateSharedHandle(nullptr,DXGI_SHARED_RESOURCE_READ|DXGI_SHARED_RESOURCE_WRITE,name.c_str(),&target.handle);
            if(FAILED(error_))return TransferResult::Unavailable;
        }
        state_=std::move(next);return TransferResult::Ok;
    }catch(...){error_=E_OUTOFMEMORY;return TransferResult::Unavailable;}
}
TransferResult D3D11PairProducer::Publish(const std::array<TextureSlice,2>& sources,const FrameMetadata& frame,PairTicket& ticket) noexcept {
    ticket={};
    if(!state_){error_=E_UNEXPECTED;return TransferResult::Unavailable;}
    auto& s=*state_;if(s.faulted)return TransferResult::Faulted;
    if(!frame.frameId||frame.frameId<=s.frameId||!frame.spaceGeneration||!frame.trackingGeneration||
        frame.predictedNs<=0||s.sequence>=0x7fffffffffffffffULL||!SlicesValid(s,sources)){
        error_=E_INVALIDARG;return TransferResult::Invalid;
    }
    if(DeviceStatus(s,error_)!=TransferResult::Ok)return TransferResult::Faulted;
    const auto locked=LockPair(s,s.sequence*2,error_);if(locked!=TransferResult::Ok)return locked;
    for(unsigned eye=0;eye<2;++eye)s.context->CopySubresourceRegion(s.eyes[eye].texture.Get(),0,0,0,0,sources[eye].texture,sources[eye].subresource,nullptr);
    s.context->Flush();
    const auto deviceResult=DeviceStatus(s,error_);
    const auto next=s.sequence+1;
    const auto unlocked=UnlockPair(s,deviceResult==TransferResult::Ok?next*2-1:s.sequence*2,error_);
    if(deviceResult!=TransferResult::Ok||unlocked!=TransferResult::Ok)return TransferResult::Faulted;
    s.sequence=next;s.frameId=frame.frameId;
    ticket.resourceEpoch=s.descriptor.resourceEpoch;ticket.sequence=next;ticket.frameId=frame.frameId;
    ticket.spaceGeneration=frame.spaceGeneration;ticket.trackingGeneration=frame.trackingGeneration;
    ticket.predictedNs=frame.predictedNs;ticket.session=s.descriptor.session;
    return TransferResult::Ok;
}
D3D11PairConsumer::D3D11PairConsumer()=default;
D3D11PairConsumer::~D3D11PairConsumer()=default;
void D3D11PairConsumer::Reset()noexcept{state_.reset();error_=S_OK;}
HRESULT D3D11PairConsumer::LastError()const noexcept{return error_;}
TransferResult D3D11PairConsumer::Open(ID3D11Device* device,const TextureDescriptor& descriptor) noexcept {
    Reset();if(!device||!Valid(descriptor)){error_=E_INVALIDARG;return TransferResult::Invalid;}
    try {
        auto next=std::make_unique<State>();next->device=device;next->descriptor=descriptor;device->GetImmediateContext(&next->context);
        TextureDescriptor actual{};error_=AdapterIdentity(device,actual);if(FAILED(error_))return TransferResult::Unavailable;
        if(actual.adapterLow!=descriptor.adapterLow||actual.adapterHigh!=descriptor.adapterHigh){error_=E_INVALIDARG;return TransferResult::Invalid;}
        ComPtr<ID3D11Device1> device1;error_=device->QueryInterface(IID_PPV_ARGS(&device1));if(FAILED(error_))return TransferResult::Unavailable;
        for(unsigned eye=0;eye<2;++eye){
            auto& target=next->eyes[eye];const auto name=ResourceName(descriptor,eye);
            error_=device1->OpenSharedResourceByName(name.c_str(),DXGI_SHARED_RESOURCE_READ|DXGI_SHARED_RESOURCE_WRITE,IID_PPV_ARGS(&target.texture));
            if(FAILED(error_))return TransferResult::Unavailable;
            D3D11_TEXTURE2D_DESC d{};target.texture->GetDesc(&d);
            if(d.Width!=descriptor.width||d.Height!=descriptor.height||d.Format!=DXGI_FORMAT(descriptor.format)||d.MipLevels!=1||
                d.ArraySize!=1||d.SampleDesc.Count!=1||d.SampleDesc.Quality||d.Usage!=D3D11_USAGE_DEFAULT||
                !(d.MiscFlags&D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX)||!(d.MiscFlags&D3D11_RESOURCE_MISC_SHARED_NTHANDLE)){
                error_=E_INVALIDARG;return TransferResult::Invalid;
            }
            error_=target.texture.As(&target.mutex);if(FAILED(error_))return TransferResult::Unavailable;
        }
        state_=std::move(next);return TransferResult::Ok;
    }catch(...){error_=E_OUTOFMEMORY;return TransferResult::Unavailable;}
}
TransferResult D3D11PairConsumer::Copy(const PairTicket& ticket,const std::array<TextureSlice,2>& destinations) noexcept {
    if(!state_){error_=E_UNEXPECTED;return TransferResult::Unavailable;}
    auto& s=*state_;if(s.faulted)return TransferResult::Faulted;
    if(!Valid(ticket,s.descriptor)||ticket.sequence<=s.sequence||ticket.frameId<=s.frameId||!SlicesValid(s,destinations)){
        error_=E_INVALIDARG;return TransferResult::Invalid;
    }
    if(DeviceStatus(s,error_)!=TransferResult::Ok)return TransferResult::Faulted;
    const auto locked=LockPair(s,ticket.sequence*2-1,error_);if(locked!=TransferResult::Ok)return locked;
    for(unsigned eye=0;eye<2;++eye)s.context->CopySubresourceRegion(destinations[eye].texture,destinations[eye].subresource,0,0,0,s.eyes[eye].texture.Get(),0,nullptr);
    s.context->Flush();const auto deviceResult=DeviceStatus(s,error_);
    const auto unlocked=UnlockPair(s,ticket.sequence*2,error_);
    if(deviceResult!=TransferResult::Ok||unlocked!=TransferResult::Ok)return TransferResult::Faulted;
    s.sequence=ticket.sequence;s.frameId=ticket.frameId;return TransferResult::Ok;
}
}
