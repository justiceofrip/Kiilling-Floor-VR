#pragma once
#include "kfvr/math/StereoMath.h"
#include <cstdint>
namespace kfvr::interaction {
struct GestureSample {
    std::uint64_t owner=0,space=0,sequence=0;
    std::int64_t timeNs=0;
    bool focused=false,tracked=false;
};
struct InsertionProfile {
    float radius=.035f,angleDegrees=25,approach=.08f,seat=.035f,clearance=.12f,maxSpeed=4;
};
struct InsertionSample : GestureSample {math::Pose socket{},magazineTip{};};
struct InsertionOutput {bool captured=false,seated=false,cleared=false;float progress=0;};
// Socket-local +Y is the insertion direction; the magazine must approach from -Y.
// Use the stationary belt pose after seating until the weapon has cleared its dock.
class InsertionGesture {
public:
    explicit InsertionGesture(InsertionProfile={});
    InsertionOutput Update(const InsertionSample&) noexcept;
    void Reset() noexcept;
private:
    InsertionProfile profile_;GestureSample previous_{};math::Vec3 position_{};
    bool observed_=false,captured_=false,latched_=false;
};
struct StrokeProfile {math::Vec3 axis{0,0,1};float length=.06f,grabRadius=.04f,maxSpeed=4;};
struct StrokeSample : GestureSample {math::Pose actionRest{},hand{};bool grip=false;};
struct StrokeOutput {bool held=false,changed=false;float travel=0;};
// The rest pose marks the handle's grasp point at zero travel. Uses authoritative
// current travel when grabbing so a partial action can be re-grasped in place.
class ActionStrokeGesture {
public:
    explicit ActionStrokeGesture(StrokeProfile={});
    StrokeOutput Update(const StrokeSample&,float currentTravel) noexcept;
    void Reset() noexcept;
private:
    StrokeProfile profile_;GestureSample previous_{};math::Vec3 previousPosition_{},anchor_{};
    float initialTravel_=0;bool observed_=false,armed_=false,held_=false,previousGrip_=false;
};
}
