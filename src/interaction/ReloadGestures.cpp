#include "kfvr/interaction/ReloadGestures.h"
#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>
namespace kfvr::interaction {
namespace {
float Dot(math::Vec3 a,math::Vec3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
math::Vec3 Sub(math::Vec3 a,math::Vec3 b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
math::Vec3 Scale(math::Vec3 a,float s){return {a.x*s,a.y*s,a.z*s};}
float Length(math::Vec3 a){return std::sqrt(Dot(a,a));}
bool Valid(const GestureSample& s){return s.owner&&s.space&&s.sequence&&s.timeNs>0&&s.focused&&s.tracked;}
bool Same(const GestureSample& a,const GestureSample& b){return a.owner==b.owner&&a.space==b.space;}
bool Next(const GestureSample& a,const GestureSample& b){return Same(a,b)&&a.sequence>b.sequence&&a.timeNs>b.timeNs&&a.timeNs-b.timeNs<=150000000;}
float DistanceLimit(const GestureSample& a,const GestureSample& b,float speed){return speed*float(double(a.timeNs-b.timeNs)*1.e-9)+.005f;}
}
InsertionGesture::InsertionGesture(InsertionProfile p):profile_(p){
    for(float v:{p.radius,p.angleDegrees,p.approach,p.seat,p.clearance,p.maxSpeed})if(!std::isfinite(v)||v<=0)throw std::invalid_argument("Invalid insertion profile");
    if(p.radius>.2f||p.angleDegrees>60||p.approach>.3f||p.seat>.2f||p.clearance<p.seat||p.clearance>1||p.maxSpeed>10)throw std::invalid_argument("Unbounded insertion profile");
}
void InsertionGesture::Reset()noexcept {previous_={};position_={};observed_=captured_=latched_=false;}
InsertionOutput InsertionGesture::Update(const InsertionSample& s)noexcept {
    InsertionOutput out{};
    const auto relative=math::MakeRelativePose(s.socket,s.magazineTip);
    if(!Valid(s)||!relative){observed_=false;captured_=false;return out;}
    if(previous_.owner&&Same(s,previous_)&&(s.sequence<=previous_.sequence||s.timeNs<=previous_.timeNs))return out;
    const auto p=relative->position;
    if(previous_.owner&&!Same(s,previous_)){captured_=false;latched_=false;}
    const bool continuous=observed_&&Next(s,previous_)&&Length(Sub(p,position_))<=DistanceLimit(s,previous_,profile_.maxSpeed);
    if(!continuous)captured_=false;
    previous_=s;position_=p;observed_=true;
    if(latched_){if(Length(p)>profile_.clearance){latched_=false;out.cleared=true;}return out;}
    const float cosine=std::cos(profile_.angleDegrees*std::numbers::pi_v<float>/360);
    if(std::hypot(p.x,p.z)>profile_.radius||std::abs(relative->orientation.w)<cosine||p.y < -profile_.approach||p.y>profile_.seat+.03f){captured_=false;return out;}
    // Every new approach starts visibly outside the socket. Resuming while
    // already seated cannot create a new magazine insertion.
    if(p.y<=-.005f)captured_=true;
    if(!captured_)return out;
    out.captured=true;out.progress=std::clamp(p.y/profile_.seat,0.f,1.f);
    if(continuous&&p.y>=profile_.seat){latched_=true;captured_=false;out.seated=true;out.captured=false;}
    return out;
}
ActionStrokeGesture::ActionStrokeGesture(StrokeProfile p):profile_(p){
    const float length=Length(p.axis);
    if(!std::isfinite(length)||length<.001f||!std::isfinite(p.length)||p.length<.005f||p.length>.3f||!std::isfinite(p.grabRadius)||p.grabRadius<=0||p.grabRadius>.15f||!std::isfinite(p.maxSpeed)||p.maxSpeed<=0||p.maxSpeed>10)throw std::invalid_argument("Invalid action stroke profile");
    profile_.axis=Scale(p.axis,1/length);
}
void ActionStrokeGesture::Reset()noexcept {previous_={};previousPosition_={};anchor_={};initialTravel_=0;observed_=armed_=held_=previousGrip_=false;}
StrokeOutput ActionStrokeGesture::Update(const StrokeSample& s,float current)noexcept {
    StrokeOutput out{false,false,current};const auto relative=math::MakeRelativePose(s.actionRest,s.hand);
    if(!Valid(s)||!relative||!std::isfinite(current)||current<0||current>1){observed_=armed_=held_=previousGrip_=false;return out;}
    if(previous_.owner&&Same(s,previous_)&&(s.sequence<=previous_.sequence||s.timeNs<=previous_.timeNs)){out.held=held_;return out;}
    const auto p=relative->position;
    const bool continuous=observed_&&Next(s,previous_)&&Length(Sub(p,previousPosition_))<=DistanceLimit(s,previous_,profile_.maxSpeed);
    if(!continuous){armed_=held_=previousGrip_=false;}
    previous_=s;previousPosition_=p;observed_=true;
    if(!s.grip){if(held_){out.changed=current!=0;out.travel=0;}held_=false;armed_=true;previousGrip_=false;return out;}
    if(armed_&&!previousGrip_&&Length(Sub(p,Scale(profile_.axis,current*profile_.length)))<=profile_.grabRadius){held_=true;anchor_=p;initialTravel_=current;}
    previousGrip_=true;
    if(!held_)return out;
    const auto delta=Sub(p,anchor_);const float along=Dot(delta,profile_.axis);
    if(Length(Sub(delta,Scale(profile_.axis,along)))>profile_.grabRadius*2){held_=armed_=false;return out;}
    out.held=true;out.travel=std::clamp(initialTravel_+along/profile_.length,0.f,1.f);out.changed=std::abs(out.travel-current)>.0001f;return out;
}
}
