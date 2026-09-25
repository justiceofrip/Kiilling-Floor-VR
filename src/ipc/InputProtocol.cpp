#include "kfvr/ipc/InputProtocol.h"
#include <algorithm>
#include <cmath>
namespace kfvr::ipc {
namespace {
PoseWire Pack(const math::Pose& p){return {{{p.position.x,p.position.y,p.position.z,p.orientation.x,p.orientation.y,p.orientation.z,p.orientation.w}}};}
math::Pose Unpack(const PoseWire& p){const auto& v=p.values;return {{v[0],v[1],v[2]},{v[3],v[4],v[5],v[6]}};}
bool Unit(float v){return std::isfinite(v)&&v>=0&&v<=1;}
bool Empty(const PoseWire& p){for(float v:p.values)if(v!=0)return false;return true;}
bool ValidPose(const PoseWire& p){for(float v:p.values)if(!std::isfinite(v))return false;return bool(math::MakeRelativePose(Unpack(p),Unpack(p)));}
}
bool DecodeControls(const ControlWire& wire,std::int64_t now,xr::ControllerFrame& out,std::uint64_t& sequence)noexcept{
 out={};sequence=0;
 if(wire.magic!=0x3149464b||wire.version!=1||wire.bytes!=sizeof(wire)||wire.flags>1||!wire.sequence||now<=0||wire.expiresQpc<=now)return false;
 const bool focused=wire.flags==1;if(focused&&(!wire.spaceGeneration||wire.predictedNs<=0))return false;
 xr::ControllerFrame frame{};frame.focused=focused;frame.spaceGeneration=wire.spaceGeneration;frame.predictedNs=wire.predictedNs;
 for(unsigned hand=0;hand<2;++hand){const auto& w=wire.hands[hand];auto& h=frame.hands[hand];
  if(w.flags>7||w.reserved||(!focused&&w.flags)||!Unit(w.trigger)||!Unit(w.squeeze)||!std::isfinite(w.stickX)||!std::isfinite(w.stickY)||std::hypot(w.stickX,w.stickY)>1.0001f)return false;
  h.gripValid=(w.flags&1)!=0;h.aimValid=(w.flags&2)!=0;h.menu=(w.flags&4)!=0;
  if(h.gripValid){if(!ValidPose(w.grip))return false;h.gripPose=Unpack(w.grip);h.trigger=w.trigger;h.squeeze=w.squeeze;h.stickX=w.stickX;h.stickY=w.stickY;}
  else if(!Empty(w.grip)||w.trigger||w.squeeze||w.stickX||w.stickY||h.menu)return false;
  if(h.aimValid){if(!ValidPose(w.aim))return false;h.aimPose=Unpack(w.aim);}else if(!Empty(w.aim))return false;
 }
 out=frame;sequence=wire.sequence;return true;
}
bool EncodeControls(const xr::ControllerFrame& frame,std::uint64_t sequence,std::int64_t expires,ControlWire& out)noexcept{
 ControlWire wire{};wire.sequence=sequence;wire.expiresQpc=expires;wire.spaceGeneration=frame.spaceGeneration;wire.predictedNs=frame.predictedNs;wire.flags=frame.focused?1:0;
 if(frame.focused)for(unsigned hand=0;hand<2;++hand){auto& w=wire.hands[hand];const auto& h=frame.hands[hand];if(h.gripValid){w.flags|=1;w.grip=Pack(h.gripPose);w.trigger=h.trigger;w.squeeze=h.squeeze;w.stickX=h.stickX;w.stickY=h.stickY;if(h.menu)w.flags|=4;}if(h.aimValid){w.flags|=2;w.aim=Pack(h.aimPose);}}
 xr::ControllerFrame check;std::uint64_t id;if(!DecodeControls(wire,1,check,id))return false;out=wire;return true;
}
bool EncodeHaptics(const ControlWire& input,std::uint64_t sequence,std::int64_t now,const std::array<xr::HapticPulse,2>& pulses,HapticWire& out)noexcept{
 xr::ControllerFrame frame;std::uint64_t inputId=0;if(!DecodeControls(input,now,frame,inputId)||!frame.focused||!sequence)return false;
 HapticWire wire{};wire.sequence=sequence;wire.controlSequence=inputId;wire.spaceGeneration=input.spaceGeneration;wire.expiresQpc=input.expiresQpc;
 for(unsigned hand=0;hand<2;++hand){const auto& p=pulses[hand];if(!frame.hands[hand].gripValid||!std::isfinite(p.amplitude)||p.amplitude<=0||!p.milliseconds)continue;wire.hands[hand]={std::min(p.amplitude,1.f),std::min(p.milliseconds,250u)};}
 out=wire;return true;
}
bool DecodeHaptics(const HapticWire& wire,const ControlWire& input,std::int64_t now,std::uint64_t& last,std::array<xr::HapticPulse,2>& out)noexcept{
 out={};xr::ControllerFrame frame;std::uint64_t inputId=0;
 if(!DecodeControls(input,now,frame,inputId)||!frame.focused||wire.magic!=0x3148464b||wire.version!=1||wire.bytes!=sizeof(wire)||wire.reserved||!wire.sequence||wire.sequence<=last||!wire.controlSequence||wire.controlSequence>inputId||wire.spaceGeneration!=input.spaceGeneration||wire.expiresQpc<=now)return false;
 for(const auto& p:wire.hands)if(!Unit(p.amplitude)||p.milliseconds>250||((p.amplitude==0)!=(p.milliseconds==0)))return false;
 for(unsigned hand=0;hand<2;++hand)if(frame.hands[hand].gripValid)out[hand]={wire.hands[hand].amplitude,wire.hands[hand].milliseconds};
 last=wire.sequence;return true;
}
}
