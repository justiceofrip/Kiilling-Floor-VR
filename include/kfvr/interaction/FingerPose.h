#pragma once
#include "kfvr/xr/ControllerInput.h"
#include <algorithm>
#include <array>
#include <cmath>
namespace kfvr::interaction {
// Adapted from the project's BF2142VR FingerPose.h; no BF bone indices/ABI.
// Order: thumb, index, middle, ring, little. Touch is controller capacitance,
// not optical hand tracking; unsupported touch uses trigger/grip analog fallback.
inline std::array<float,5> ControllerFingerCurls(const xr::HandInput& hand) noexcept {
 if(!hand.gripValid)return {};
 const auto analog=[](float v){return std::isfinite(v)?std::clamp(v,0.f,1.f):0.f;};
 const float grip=analog(hand.squeeze);float index=analog(hand.trigger);
 if(hand.triggerTouchActive&&hand.triggerTouched)index=std::max(index,.55f);
 const float thumb=hand.thumbTouchActive?(hand.thumbTouched?1.f:0.f):std::max(grip,index)*.7f;
 return {thumb,index,grip,grip,grip};
}
}
