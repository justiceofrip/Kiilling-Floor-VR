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
// A completed pair can be reprojected briefly using ITS original view poses.
// This is separate from accepting a fresh pair for the current tracking sample.
class RetainedProjection {
    PresentationRequirements requirements_{};TrackingFrame rendered_{};
    bool valid_=false;
public:
    void Clear() noexcept {valid_=false;}
    bool Remember(const PresentationRequirements& r,const TrackingFrame& tracking,
        const graphics::TextureDescriptor& descriptor,const graphics::PairTicket& ticket) noexcept {
        valid_=PairMatchesFrame(r,tracking,descriptor,ticket);
        if(valid_){requirements_=r;rendered_=tracking;}return valid_;
    }
    bool CanPresent(const PresentationRequirements& r,const TrackingFrame& current) const noexcept {
        return valid_&&current.headValid&&current.focused&&current.spaceGeneration==rendered_.spaceGeneration&&
            current.predictedNs>=rendered_.predictedNs&&current.predictedNs-rendered_.predictedNs<=100000000&&
            r.width==requirements_.width&&r.height==requirements_.height&&r.format==requirements_.format&&
            r.adapterLow==requirements_.adapterLow&&r.adapterHigh==requirements_.adapterHigh;
    }
    const TrackingFrame& RenderedTracking() const noexcept {return rendered_;}
};

}
