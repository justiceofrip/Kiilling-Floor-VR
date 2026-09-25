#include "kfvr/gameplay/MeleeGate.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace kfvr::gameplay {
namespace {
Point Sub(Point a,Point b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
float Dot(Point a,Point b){return a.x*b.x+a.y*b.y+a.z*b.z;}
float Length(Point a){return std::sqrt(Dot(a,a));}
bool Finite(Point a){return std::isfinite(a.x)&&std::isfinite(a.y)&&std::isfinite(a.z);}
}
MeleeGate::MeleeGate(MeleeProfile p):p_(p){if(!std::isfinite(p.minimumSpeed)||!std::isfinite(p.maximumSpeed)||!std::isfinite(p.minimumTravel)||!std::isfinite(p.retraction)||!std::isfinite(p.maximumReach)||!std::isfinite(p.cooldown)||!std::isfinite(p.maximumGap)||p.minimumSpeed<=0||p.maximumSpeed<=p.minimumSpeed||p.minimumTravel<=0||p.retraction<=0||p.maximumReach<=0||p.cooldown<=0||p.maximumGap<=0)throw std::invalid_argument("Invalid melee profile");}
void MeleeGate::Reset(){valid_=false;locked_=false;/* cooldown survives tracking loss/equip changes */}
MeleeStrike MeleeGate::Sample(Point tip,Point shoulder,double now,bool tracked,bool equipped){
    if(!tracked||!equipped||!Finite(tip)||!Finite(shoulder)||!std::isfinite(now)||now<0||Length(Sub(tip,shoulder))>p_.maximumReach){Reset();return {};}
    if(valid_&&now<=lastTime_)return {};
    if(!valid_||now-lastTime_>p_.maximumGap){last_=origin_=tip;lastTime_=now;valid_=true;return {};}
    const double dt=now-lastTime_;const Point previous=last_;last_=tip;lastTime_=now;
    const float speed=Length(Sub(tip,previous))/float(dt);
    if(speed>p_.maximumSpeed){Reset();return {};}
    if(locked_){if(Dot(Sub(strikeTip_,tip),direction_)>=p_.retraction){locked_=false;origin_=tip;}return {};}
    const auto stroke=Sub(tip,origin_);const float distance=Length(stroke);
    if(speed<p_.minimumSpeed){origin_=tip;return {};}
    if(distance<p_.minimumTravel||now<nextStrike_)return {};
    direction_={stroke.x/distance,stroke.y/distance,stroke.z/distance};strikeTip_=tip;locked_=true;nextStrike_=now+p_.cooldown;
    return {true,previous,tip,std::clamp(speed/p_.maximumSpeed,.35f,1.0f)};
}
}
