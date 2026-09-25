#pragma once
#include "ComfortControls.h"
#include <cstdint>
namespace kfvr::interaction {
enum Action : std::uint32_t { Fire=1, AlternateFire=2, Use=4, Reload=8, Sprint=16, Menu=32 };
struct ActionSample {
    std::uint64_t owner=0,generation=0;
    std::int64_t timeNs=0,frameNs=0;
    bool focused=false,tracked=false,playing=false,bindingsVerified=false;
    // Adapters may require neutral in the original, pre-deadzone control space.
    bool neutralAllowed=true;
    float forward=0,strafe=0,turn=0;
    std::uint32_t held=0;
};
struct ActionOutput {
    float forward=0,strafe=0,turnDegrees=0;
    std::uint32_t held=0,pressed=0,released=0;
    // Releases belong to the previous owner; never dispatch them to a respawn.
    std::uint64_t owner=0;
};
class ActionPolicy {
public:
    ActionOutput Update(const ActionSample&,float snapDegrees=30) noexcept;
private:
    std::uint64_t owner_=0,generation_=0;
    std::int64_t time_=0;
    std::uint32_t held_=0;
    bool armed_=false;float forward_=0,strafe_=0;
    SnapTurn turn_;
};
}