#pragma once
#include "kfvr/runtime/PresentationPolicy.h"
namespace kfvr::runtime {
// Local C++ contract shared by native producers, IPC and XR presentation.
// Not an exported ABI: serialize explicitly at process/module boundaries.
class IFrameProvider {
public:
    virtual ~IFrameProvider()=default;
    // Called on the frame thread. Use a bounded wait; false submits no layer.
    virtual bool TryGetPair(const PresentationRequirements&,const TrackingFrame&,
        graphics::TextureDescriptor&,graphics::PairTicket&) noexcept=0;
    // False means the pair was not consumed; reset/reclaim that texture stream.
    // True reports GPU consumption, not that xrEndFrame succeeded or was seen.
    virtual void PairConsumed(const graphics::PairTicket&,bool consumed) noexcept=0;
};
}
