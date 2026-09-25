#pragma once
#include "kfvr/ipc/FrameChannel.h"
#include "kfvr/runtime/FrameProvider.h"
namespace kfvr::ipc {
class RemoteFrameProvider final:public runtime::IFrameProvider,public xr::IInputConsumer {
public:
    bool Create()noexcept{return channel_.CreateHost();}
    const std::wstring& Token()const noexcept{return channel_.Token();}
    bool Connected()noexcept{return channel_.Connected();}
    void Close()noexcept{channel_.Close();}
    std::array<xr::HapticPulse,2> Input(const xr::ControllerFrame& frame)noexcept override{
        channel_.SendInput(frame);return channel_.ReceiveHaptics();
    }
    // This is a CPU rendezvous budget, not an OpenXR predicted-time offset.
    bool SetBudget(unsigned ms)noexcept{if(!ms||ms>50)return false;budgetMs_=ms;return true;}
    bool TryGetPair(const runtime::PresentationRequirements& r,const runtime::TrackingFrame& t,
        graphics::TextureDescriptor& d,graphics::PairTicket& p)noexcept override {
        return channel_.RequestPair(r,t,budgetMs_,d,p)==ChannelResult::Ok;
    }
    void PairConsumed(const graphics::PairTicket& t,bool consumed)noexcept override{channel_.Feedback(t,consumed);}
private:FrameChannel channel_;unsigned budgetMs_=8;
};
// The local provider must outlive this object. Pump only on its graphics thread,
// at a verified render boundary. Never advances native simulation by itself.
class RemoteFrameProducer {
public:
    explicit RemoteFrameProducer(runtime::IFrameProvider& provider):provider_(provider){}
    ~RemoteFrameProducer(){Close();}
    bool Connect(const std::wstring& token)noexcept{Close();return channel_.ConnectProducer(token);}
    bool Connected()noexcept{return channel_.Connected();}
    ChannelResult ReadInput(xr::ControllerFrame& frame,std::uint64_t& sequence)noexcept{return channel_.ReadInput(frame,sequence);}
    ChannelResult SendHaptics(std::uint64_t sequence,const std::array<xr::HapticPulse,2>& pulses)noexcept{return channel_.SendHaptics(sequence,pulses);}
    void Pump()noexcept;
    void Close()noexcept;
private:
    enum class Step{Idle,Skipping,Publishing,Waiting};
    FrameChannel channel_;runtime::IFrameProvider& provider_;Step step_=Step::Idle;
    graphics::TextureDescriptor descriptor_{};graphics::PairTicket ticket_{};
};
}
