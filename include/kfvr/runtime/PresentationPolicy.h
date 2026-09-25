#pragma once
#include "kfvr/runtime/FrameCoordinator.h"
#include "kfvr/graphics/SharedTextureProtocol.h"
namespace kfvr::runtime {
struct PresentationRequirements {
    std::uint32_t width=0,height=0,format=0,adapterLow=0;
    std::int32_t adapterHigh=0;
};
// Runtime views used for projection must belong to the exact tracking sample
// used to render the pair. Never submit old eye images with new view poses.
inline bool PairMatchesFrame(const PresentationRequirements& requirements,
    const TrackingFrame& tracking,const graphics::TextureDescriptor& descriptor,
    const graphics::PairTicket& ticket) noexcept {
    return graphics::Valid(ticket,descriptor) && tracking.headValid && tracking.focused &&
        descriptor.width==requirements.width && descriptor.height==requirements.height &&
        descriptor.format==requirements.format && descriptor.adapterLow==requirements.adapterLow &&
        descriptor.adapterHigh==requirements.adapterHigh &&
        ticket.trackingGeneration==tracking.generation && ticket.spaceGeneration==tracking.spaceGeneration &&
        ticket.predictedNs==tracking.predictedNs;
}
}
