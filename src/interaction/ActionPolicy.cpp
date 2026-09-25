#include "kfvr/interaction/ActionPolicy.h"
#include <algorithm>
#include <cmath>
namespace kfvr::interaction {
ActionOutput ActionPolicy::Update(const ActionSample& s,float snapDegrees) noexcept {
    ActionOutput out{};out.owner=owner_;
    const bool clockValid=s.timeNs>0&&s.frameNs>0;
    const auto age=clockValid?(s.frameNs>=s.timeNs?s.frameNs-s.timeNs:s.timeNs-s.frameNs):INT64_MAX;
    const bool finite=std::isfinite(s.forward)&&std::isfinite(s.strafe)&&std::isfinite(s.turn)&&std::isfinite(snapDegrees)&&snapDegrees>=0&&snapDegrees<=180;
    const bool valid=s.owner&&s.generation&&s.focused&&s.tracked&&s.playing&&s.bindingsVerified&&clockValid&&age<=150000000&&finite;
    const bool switched=s.owner!=owner_;
    const bool backwards=!switched&&(s.timeNs<time_||s.generation<generation_);
    if(!valid||switched||backwards){
        out.released=held_;held_=0;forward_=strafe_=0;armed_=false;turn_={};generation_=0;time_=0;
        owner_=valid?s.owner:0;
        // Always suppress the transition sample; new ownership needs neutral.
        return out;
    }
    out.owner=owner_;
    if(s.generation==generation_){out.held=held_;out.forward=forward_;out.strafe=strafe_;return out;}
    const bool longGap=time_&&s.timeNs-time_>150000000;
    generation_=s.generation;time_=s.timeNs;
    if(longGap){out.released=held_;held_=0;forward_=strafe_=0;armed_=false;turn_={};return out;}
    constexpr std::uint32_t mask=Fire|AlternateFire|Use|Reload|Sprint|Menu;
    if(!armed_){if(s.neutralAllowed&&!(s.held&mask)&&std::abs(s.turn)<.2f&&std::abs(s.forward)<.2f&&std::abs(s.strafe)<.2f){armed_=true;turn_.Update(true,0,s.timeNs,snapDegrees);}return out;}
    out.held=s.held&mask;out.pressed=out.held&~held_;out.released=held_&~out.held;held_=out.held;
    out.forward=std::clamp(s.forward,-1.f,1.f);out.strafe=std::clamp(s.strafe,-1.f,1.f);
    const auto length=std::hypot(out.forward,out.strafe);if(length>1){out.forward/=length;out.strafe/=length;}
    forward_=out.forward;strafe_=out.strafe;out.turnDegrees=turn_.Update(true,std::clamp(s.turn,-1.f,1.f),s.timeNs,snapDegrees);
    return out;
}
}