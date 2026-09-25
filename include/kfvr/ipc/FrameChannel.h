#pragma once
#include "kfvr/ipc/FrameProtocol.h"
#include "kfvr/ipc/InputProtocol.h"
#include <memory>
#include <string>
namespace kfvr::ipc {
enum class ChannelResult {Ok,Busy,Invalid,Timeout,Closed};
// One host thread and one producer thread, in independent x64/x86 processes.
// A fresh random token identifies each session. No game pointers cross this API.
class FrameChannel {
public:
    FrameChannel();~FrameChannel();
    FrameChannel(const FrameChannel&)=delete;FrameChannel& operator=(const FrameChannel&)=delete;
    bool CreateHost() noexcept;
    bool ConnectProducer(const std::wstring& token) noexcept;
    const std::wstring& Token()const noexcept;
    bool Connected() noexcept;
    void Close() noexcept;
    // Host: bounded rendezvous for this exact predicted-time sample. Game methods
    // never wait for a CPU mutex. A busy slot drops the current host request.
    ChannelResult RequestPair(const runtime::PresentationRequirements&,const runtime::TrackingFrame&,
        unsigned budgetMs,graphics::TextureDescriptor&,graphics::PairTicket&) noexcept;
    void Feedback(const graphics::PairTicket&,bool consumed) noexcept;
    // Independent of eye-pair availability. All methods take the mutex with zero
    // wait. Missing/stale samples clear output; live samples expire after 100 ms.
    ChannelResult SendInput(const xr::ControllerFrame&) noexcept;
    ChannelResult ReadInput(xr::ControllerFrame&,std::uint64_t& sequence) noexcept;
    ChannelResult SendHaptics(std::uint64_t controlSequence,const std::array<xr::HapticPulse,2>&) noexcept;
    std::array<xr::HapticPulse,2> ReceiveHaptics() noexcept;
    // Producer: call at the verified render boundary, on the graphics thread.
    ChannelResult TryTake(FrameRequest&) noexcept;
    ChannelResult Publish(const graphics::TextureDescriptor&,const graphics::PairTicket&) noexcept;
    ChannelResult Skip() noexcept;
    Outcome PollOutcome() noexcept;
private:struct State;std::unique_ptr<State> state_;std::wstring token_;
};
}
