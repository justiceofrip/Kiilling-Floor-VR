#pragma once
#include "kfvr/math/StereoMath.h"
#include <array>
#include <cstdint>
namespace kfvr::xr {
struct HandInput {
 math::Pose gripPose{},aimPose{};
 bool gripValid=false,aimValid=false;
 float trigger=0,squeeze=0,stickX=0,stickY=0;
 bool menu=false,primary=false,secondary=false;
 bool triggerTouchActive=false,triggerTouched=false,thumbTouchActive=false,thumbTouched=false;
};
struct ControllerFrame {std::uint64_t spaceGeneration=0;std::int64_t predictedNs=0;bool focused=false;std::array<HandInput,2> hands{};};
struct HapticPulse {float amplitude=0;std::uint32_t milliseconds=0;};
class IInputConsumer {
public:
 virtual ~IInputConsumer()=default;
 // Same predicted time as head/eye poses. A neutral frame releases all actions.
 // Requests returned here are bounded by the host and suppressed without focus.
 virtual std::array<HapticPulse,2> Input(const ControllerFrame&) noexcept=0;
};
}
