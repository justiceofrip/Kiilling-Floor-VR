#include "kfvr/interaction/ControllerLocomotion.h"
#include <algorithm>
#include <cmath>
namespace kfvr::interaction {
LocomotionOutput ControllerLocomotion::Update(const xr::ControllerFrame* frame,std::uint64_t sequence,std::uint64_t owner,bool playing,float snapDegrees) noexcept {
 ActionSample sample{},turn{};bool valid=false,turnValid=false;
 if(frame){
  if(space_!=frame->spaceGeneration){space_=frame->spaceGeneration;policy_={};turnPolicy_={};}
  const auto& right=frame->hands[1];
  turnValid=owner&&sequence&&frame->spaceGeneration&&frame->predictedNs>0&&playing&&frame->focused&&right.gripValid&&std::isfinite(right.stickX)&&std::isfinite(right.stickY)&&std::abs(right.stickX)<=1&&std::abs(right.stickY)<=1;
  if(turnValid){turn.owner=owner;turn.generation=sequence;turn.timeNs=turn.frameNs=frame->predictedNs;turn.focused=turn.tracked=turn.playing=turn.bindingsVerified=true;turn.turn=right.stickX;turn.neutralAllowed=std::hypot(right.stickX,right.stickY)<=.2f;}
  const auto& hand=frame->hands[0];
  valid=owner&&sequence&&frame->spaceGeneration&&frame->predictedNs>0&&playing&&frame->focused&&hand.gripValid&&std::isfinite(hand.stickX)&&std::isfinite(hand.stickY)&&std::abs(hand.stickX)<=1&&std::abs(hand.stickY)<=1;
  if(valid){
   sample.owner=owner;sample.generation=sequence;sample.timeNs=sample.frameNs=frame->predictedNs;
   sample.focused=sample.tracked=sample.playing=sample.bindingsVerified=true;
   const float length=std::hypot(hand.stickX,hand.stickY);sample.neutralAllowed=length<=.2f;
   if(length>.2f){const float amount=std::min(1.f,(length-.2f)/.8f);sample.forward=hand.stickY*amount/length;sample.strafe=hand.stickX*amount/length;}
  }
 }
 const auto output=policy_.Update(sample);const auto rotation=turnPolicy_.Update(turn,snapDegrees);return {valid,output.forward,output.strafe,turnValid,rotation.turnDegrees};
}
}
