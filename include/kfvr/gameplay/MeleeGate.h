#pragma once
#include <cstdint>
namespace kfvr::gameplay {
struct Point { float x=0,y=0,z=0; };
struct MeleeProfile { float minimumSpeed=1.2f,maximumSpeed=15,minimumTravel=.20f,retraction=.16f,maximumReach=2.0f;double cooldown=.65,maximumGap=.15; };
struct MeleeStrike { bool eligible=false;Point from{},to{};float strength=0; };
class MeleeGate {
public:
    explicit MeleeGate(MeleeProfile profile={});
    MeleeStrike Sample(Point tip,Point shoulder,double serverSeconds,bool trackingValid,bool equipped);
    void Reset();
private:
    MeleeProfile p_;Point last_{},origin_{},strikeTip_{},direction_{};
    double lastTime_=0,nextStrike_=0;bool valid_=false,locked_=false;
};
}
