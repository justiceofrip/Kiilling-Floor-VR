#pragma once
#include "kfvr/interaction/ActionPolicy.h"
#include "kfvr/xr/ControllerInput.h"
namespace kfvr::interaction {
struct LocomotionOutput {bool valid=false;float forward=0,strafe=0;};
// Consume only successfully decoded, unexpired mailbox samples. Passing a null
// frame on a missed/expired read releases state before accepting future input.
class ControllerLocomotion {
public:
 LocomotionOutput Update(const xr::ControllerFrame*,std::uint64_t sequence,std::uint64_t owner,bool playing) noexcept;
private:
 ActionPolicy policy_;std::uint64_t space_=0;
};
}
