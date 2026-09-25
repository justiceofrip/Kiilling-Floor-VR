#include "kfvr/runtime/FrameCoordinator.h"
#include "kfvr/interaction/TrackingMath.h"
#include <cmath>

namespace kfvr::runtime {
namespace {
bool TrackingValid(const WorldFrame& world,const TrackingFrame& tracking) noexcept {
    if(!tracking.generation||!tracking.spaceGeneration||!tracking.focused||!tracking.headValid||
       tracking.spaceGeneration!=world.spaceGeneration||world.predictedNs<=0||tracking.predictedNs<=0)return false;
    // Both timestamps use the same OpenXR clock. No wall-clock/QPC mixing.
    const auto delta=world.predictedNs>=tracking.predictedNs?
        world.predictedNs-tracking.predictedNs:tracking.predictedNs-world.predictedNs;
    if(delta>150000000)return false;
    if(!math::MakeRelativePose(tracking.referenceHead,tracking.head))return false;
    for(const auto& eye:tracking.eyes)if(!math::MakeRelativePose(tracking.referenceHead,eye))return false;
    return true;
}
bool ImagesValid(const WorldFrame& w,const std::array<EyeImage,2>& images) noexcept {
    for(const auto& i:images)if(!i.resource||i.frameId!=w.frameId||i.deviceEpoch!=w.deviceEpoch||
        !i.width||!i.height||!i.format)return false;
    return images[0].format==images[1].format&&
        (images[0].resource!=images[1].resource||images[0].arraySlice!=images[1].arraySlice);
}
}
FrameResult FrameCoordinator::Render(IRenderAdapter& adapter,IStereoSink& sink,
    const WorldFrame& world,const TrackingFrame& tracking) noexcept {
    if(!adapter.GetCapabilities().StereoReady())return FrameResult::Unsupported;
    if(world.scene!=Scene::Playing||!world.owner||!world.frameId||!world.deviceEpoch)return FrameResult::Inactive;
    if(!TrackingValid(world,tracking))return FrameResult::InvalidTracking;
    if(world.owner==lastOwner_&&world.deviceEpoch==lastEpoch_&&world.frameId<=lastFrame_)return FrameResult::Duplicate;
    if(!interaction::InverseRigid(world.camera)||!std::isfinite(world.worldUnitsPerMeter)||world.worldUnitsPerMeter<=0)
        return FrameResult::InvalidCamera;
    StereoViewSet views{};
    for(unsigned eye=0;eye<2;++eye){
        const auto camera=math::ComposeRuntimeHeadWithLhCamera(world.camera,tracking.referenceHead,tracking.eyes[eye],world.worldUnitsPerMeter);
        const auto projection=math::MakeLhProjectionFromFovTangents(tracking.fov[eye],world.nearPlane,world.farPlane);
        if(!camera||!projection)return FrameResult::InvalidCamera;
        views.eyes[eye]={*camera,*projection};
    }
    const auto center=math::ComposeRuntimeHeadWithLhCamera(world.camera,tracking.referenceHead,tracking.head,world.worldUnitsPerMeter);
    if(!center)return FrameResult::InvalidCamera;
    const auto envelope=math::EncloseStereoFrusta(*center,{views.eyes[0].world,views.eyes[1].world},tracking.fov,world.nearPlane,world.farPlane);
    // Per-eye culling adapters need not accept the limits of one symmetric cone.
    views.culling=envelope;
    if(!adapter.StillOwns(world))return FrameResult::OwnerChanged;
    if(!adapter.SaveState(world))return FrameResult::CaptureFailed;
    // Once native rendering begins the same frame is never replayed on retry.
    lastOwner_=world.owner;lastEpoch_=world.deviceEpoch;lastFrame_=world.frameId;
    std::array<EyeImage,2> images{};
    {
        struct Restore {IRenderAdapter& adapter;~Restore(){adapter.RestoreState();}} restore{adapter};
        if(!adapter.StillOwns(world))return FrameResult::OwnerChanged;
        if(!adapter.PrepareViews(world,views))return FrameResult::VisibilityFailed;
        for(unsigned eye=0;eye<2;++eye){
            if(!adapter.StillOwns(world))return FrameResult::OwnerChanged;
            if(!adapter.RenderEye(world,eye,views.eyes[eye],images[eye]))return FrameResult::EyeFailed;
        }
    } // Restore before submitting to the external XR presenter.
    if(!adapter.StillOwns(world))return FrameResult::OwnerChanged;
    if(!ImagesValid(world,images))return FrameResult::InvalidImages;
    return sink.Submit(world,tracking,images)?FrameResult::Submitted:FrameResult::SubmissionFailed;
}
}