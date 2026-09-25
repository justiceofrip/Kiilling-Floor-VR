#include "ComfortControls.h"
#include <cmath>
namespace kfvr::interaction {
float SnapTurn::Update(bool enabled,float axis,std::int64_t time,float degrees) noexcept {
 const bool continuous=previous>0 && time>previous && time-previous<=250000000;
 previous=time;
 if(!enabled || !std::isfinite(axis)||!std::isfinite(degrees)||degrees<15||degrees>90){armed=false;return 0;}
 if(!continuous){armed=std::abs(axis)<.25f;return 0;}
 if(std::abs(axis)<.25f)armed=true;
 if(armed && std::abs(axis)>.7f){armed=false;return std::copysign(degrees,axis);}return 0;
}
bool NativeTurnPulse::Queue(std::uint64_t identity,float turn,std::int64_t sample,std::uint64_t now) noexcept {
 if(!identity || !std::isfinite(turn) || std::abs(turn)<15 || std::abs(turn)>90 || sample<=0)return false;
 if(owner!=identity){*this={};owner=identity;}
 if(sample<=lastSample)return false;
 lastSample=sample;degrees=turn;queuedAt=now;return true;
}
float NativeTurnPulse::Consume(std::uint64_t identity,bool focused,std::uint64_t now) noexcept {
 const float turn=degrees;degrees=0;
 return identity==owner && identity && focused && now>=queuedAt && now-queuedAt<=150 ? turn:0.f;
}
void StandingHeightReference::Ensure(float current,float configured) noexcept {
 if(!ready)Recenter(std::isfinite(configured)&&configured!=0?configured:current);
}
bool StandingHeightReference::Recenter(float current) noexcept {
 if(!std::isfinite(current))return false;
 height=current;ready=true;return true;
}
Posture PhysicalStance::Update(bool active,float drop,bool crouch,bool prone,std::int64_t time) noexcept {
 if(!active||!std::isfinite(drop)||drop<-.8f||drop>2.5f||time<=0){Reset();return Posture::Standing;}
 if((target==Posture::Prone&&!prone)||(target==Posture::Crouched&&!crouch))Reset();
 const bool gap=last==0||time<=last||time-last>250000000;last=time;
 Posture wanted=Posture::Standing;
 if(prone&&drop>(target==Posture::Prone?.72f:.88f))wanted=Posture::Prone;
 else if(crouch&&drop>(target==Posture::Crouched?.20f:.30f))wanted=Posture::Crouched;
 if(gap){candidate=wanted;since=time;target=Posture::Standing;}
 if(wanted!=candidate){candidate=wanted;since=time;}
 if(time-since>=180000000)target=candidate;
 return target;
}
}