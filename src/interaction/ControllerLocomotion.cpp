#include "kfvr/interaction/ControllerLocomotion.h"
#include <algorithm>
#include <cmath>
namespace kfvr::interaction {
LocomotionOutput ControllerLocomotion::Update(const xr::ControllerFrame* frame,std::uint64_t sequence,std::uint64_t owner,bool playing) noexcept {
 ActionSample sample{};bool valid=false;
 if(frame){
  if(space_!=frame->spaceGeneration){space_=frame->spaceGeneration;policy_={};}
  const auto& hand=frame->hands[0];
  valid=owner&&sequence&&frame->spaceGeneration&&frame->predictedNs>0&&playing&&frame->focused&&hand.gripValid&&std::isfinite(hand.stickX)&&std::isfinite(hand.stickY)&&std::abs(hand.stickX)<=1&&std::abs(hand.stickY)<=1;
  if(valid){
   sample.owner=owner;sample.generation=sequence;sample.timeNs=sample.frameNs=frame->predictedNs;
   sample.focused=sample.tracked=sample.playing=sample.bindingsVerified=true;
   const float length=std::hypot(hand.stickX,hand.stickY);sample.neutralAllowed=length<=.2f;
   if(length>.2f){const float amount=std::min(1.f,(length-.2f)/.8f);sample.forward=hand.stickY*amount/length;sample.strafe=hand.stickX*amount/length;}
  }
 }
 const auto output=policy_.Update(sample);return {valid,output.forward,output.strafe};
}
}
