#pragma once
#include "kfvr/xr/ControllerInput.h"
#include <algorithm>
#include <cmath>
#include <ostream>
namespace kfvr::xr {
// Opt-in hardware evidence only. No game actions or persistent bindings.
class InputDiagnostic final:public IInputConsumer {
 struct Hand {
  std::uint64_t gripSamples=0,aimSamples=0,triggerPresses=0,triggerReleases=0,squeezePresses=0,squeezeReleases=0,hapticRequests=0;
  float maxTrigger=0,maxSqueeze=0,minX=0,maxX=0,minY=0,maxY=0;
  std::array<float,3> low{},high{};bool havePosition=false,triggerHeld=false,squeezeHeld=false;
  std::int64_t lastPulse=0;
 };
 std::array<Hand,2> hands_{};std::uint64_t focused_=0,neutral_=0;
public:
 std::array<HapticPulse,2> Input(const ControllerFrame& frame)noexcept override {
  std::array<HapticPulse,2> feedback{};
  if(!frame.focused){++neutral_;for(auto& h:hands_){h.triggerHeld=false;h.squeezeHeld=false;}return feedback;}++focused_;
  for(unsigned i=0;i<2;++i){const auto& value=frame.hands[i];auto& h=hands_[i];if(value.aimValid)++h.aimSamples;
   if(!value.gripValid){h.triggerHeld=false;h.squeezeHeld=false;continue;}++h.gripSamples;
   const std::array<float,3> position={value.gripPose.position.x,value.gripPose.position.y,value.gripPose.position.z};
   if(!h.havePosition){h.low=h.high=position;h.havePosition=true;}else for(unsigned n=0;n<3;++n){h.low[n]=std::min(h.low[n],position[n]);h.high[n]=std::max(h.high[n],position[n]);}
   h.maxTrigger=std::max(h.maxTrigger,value.trigger);h.maxSqueeze=std::max(h.maxSqueeze,value.squeeze);
   h.minX=std::min(h.minX,value.stickX);h.maxX=std::max(h.maxX,value.stickX);h.minY=std::min(h.minY,value.stickY);h.maxY=std::max(h.maxY,value.stickY);
   if(!h.triggerHeld&&value.trigger>=.75f){h.triggerHeld=true;++h.triggerPresses;if(!h.lastPulse||(frame.predictedNs>h.lastPulse&&frame.predictedNs-h.lastPulse>=300000000)){feedback[i]={.2f,50};h.lastPulse=frame.predictedNs;++h.hapticRequests;}}
   else if(h.triggerHeld&&value.trigger<=.25f){h.triggerHeld=false;++h.triggerReleases;}
   if(!h.squeezeHeld&&value.squeeze>=.75f){h.squeezeHeld=true;++h.squeezePresses;}else if(h.squeezeHeld&&value.squeeze<=.25f){h.squeezeHeld=false;++h.squeezeReleases;}
  }return feedback;
 }
 void WriteJson(std::ostream& out)const {
  out<<"{\"focused_frames\":"<<focused_<<",\"neutral_frames\":"<<neutral_<<",\"hands\":[";
  for(unsigned i=0;i<2;++i){const auto& h=hands_[i];if(i)out<<',';out<<"{\"hand\":\""<<(i?"right":"left")<<"\",\"grip_samples\":"<<h.gripSamples<<",\"aim_samples\":"<<h.aimSamples<<",\"motion_extent_metres\":"<<std::hypot(h.high[0]-h.low[0],h.high[1]-h.low[1],h.high[2]-h.low[2])
    <<",\"trigger_max\":"<<h.maxTrigger<<",\"trigger_presses\":"<<h.triggerPresses<<",\"trigger_releases\":"<<h.triggerReleases<<",\"grip_max\":"<<h.maxSqueeze<<",\"grip_presses\":"<<h.squeezePresses<<",\"grip_releases\":"<<h.squeezeReleases
    <<",\"stick_x_range\":["<<h.minX<<','<<h.maxX<<"],\"stick_y_range\":["<<h.minY<<','<<h.maxY<<"],\"haptic_requests\":"<<h.hapticRequests<<'}';}
  out<<"]}";
 }
};
}
