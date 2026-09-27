#pragma once
#include "kfvr/ipc/FrameProtocol.h"
#include "kfvr/xr/ControllerInput.h"
namespace kfvr::ipc {
struct HandWire {
 PoseWire grip{},aim{};
 float trigger=0,squeeze=0,stickX=0,stickY=0;
 std::uint32_t flags=0,reserved=0;
};
struct alignas(8) ControlWire {
 std::uint32_t magic=0x3149464b,version=2,bytes=208,flags=0;
 std::uint64_t sequence=0,spaceGeneration=0;
 std::int64_t predictedNs=0,expiresQpc=0;
 std::array<HandWire,2> hands{};
};
struct PulseWire {float amplitude=0;std::uint32_t milliseconds=0;};
struct alignas(8) HapticWire {
 std::uint32_t magic=0x3148464b,version=1,bytes=64,reserved=0;
 std::uint64_t sequence=0,controlSequence=0,spaceGeneration=0;
 std::int64_t expiresQpc=0;
 std::array<PulseWire,2> hands{};
};
static_assert(sizeof(HandWire)==80&&sizeof(ControlWire)==208&&offsetof(ControlWire,hands)==48);
static_assert(sizeof(HapticWire)==64&&offsetof(HapticWire,hands)==48);
static_assert(std::is_trivially_copyable_v<ControlWire>&&std::is_trivially_copyable_v<HapticWire>);
// Invalid, unfocused or expired input always clears the receiver's output.
bool EncodeControls(const xr::ControllerFrame&,std::uint64_t sequence,std::int64_t expires,ControlWire&) noexcept;
bool DecodeControls(const ControlWire&,std::int64_t now,xr::ControllerFrame&,std::uint64_t& sequence) noexcept;
bool EncodeHaptics(const ControlWire&,std::uint64_t sequence,std::int64_t now,const std::array<xr::HapticPulse,2>&,HapticWire&) noexcept;
bool DecodeHaptics(const HapticWire&,const ControlWire&,std::int64_t now,std::uint64_t& lastSequence,std::array<xr::HapticPulse,2>&) noexcept;
}
