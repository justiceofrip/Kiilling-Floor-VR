#pragma once
#include "kfvr/interaction/ActionPolicy.h"
#include "kfvr/xr/ControllerInput.h"
namespace kfvr::interaction {
struct LocomotionOutput {bool valid=false;float forward=0,strafe=0;bool turnValid=false;float turnDegrees=0;};
// Consume only successfully decoded, unexpired mailbox samples. Passing a null
// frame on a missed/expired read releases state before accepting future input.
class ControllerLocomotion {
public:
 LocomotionOutput Update(const xr::ControllerFrame*,std::uint64_t sequence,std::uint64_t owner,bool playing,float snapDegrees=30) noexcept;
private:
 ActionPolicy policy_,turnPolicy_;std::uint64_t space_=0;
};
}
