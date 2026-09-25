#pragma once
#include "kfvr/math/StereoMath.h"
#include "kfvr/math/StereoCulling.h"
#include <array>
#include <cstdint>

namespace kfvr::runtime {
// Tracking remains in OpenXR metres, +Y up, -Z forward. Native adapters must
// explicitly convert their own coordinates; the core never receives native pointers.
struct TrackingFrame {
    std::uint64_t generation=0, spaceGeneration=0;
    std::int64_t predictedNs=0;
    bool focused=false, headValid=false;
    math::Pose referenceHead{}, head{};
    std::array<math::Pose,2> eyes{};
    std::array<math::FovTangents,2> fov{};
};
enum class Scene { Unknown, Loading, Menu, Playing, Cutscene };
struct WorldFrame {
    std::uint64_t frameId=0, owner=0, deviceEpoch=0, spaceGeneration=0;
    std::int64_t predictedNs=0;
    Scene scene=Scene::Unknown;
    // Canonical row-vector, left-handed camera. Absolute far plane, NOT
    // Refractor's far-distance delta. KFVR must verify and convert its layout.
    math::Matrix4 camera{};
    float nearPlane=0, farPlane=0, worldUnitsPerMeter=0;
};
struct EyeView { math::Matrix4 world{}, projection{}; };
struct StereoViewSet {std::array<EyeView,2> eyes{};std::optional<math::StereoCullEnvelope> culling{};};
struct EyeImage {
    std::uint64_t resource=0, frameId=0, deviceEpoch=0;
    std::uint32_t arraySlice=0, width=0, height=0, format=0;
};
struct Capabilities {
    bool camera=false, renderOnly=false, stateRestore=false, eyeTargets=false, stereoVisibility=false;
    bool StereoReady() const noexcept {return camera&&renderOnly&&stateRestore&&eyeTargets&&stereoVisibility;}
};
class IRenderAdapter {
public:
    virtual ~IRenderAdapter()=default;
    virtual Capabilities GetCapabilities() const noexcept=0;
    virtual bool StillOwns(const WorldFrame&) const noexcept=0;
    // SaveState false MUST leave the native renderer unchanged.
    virtual bool SaveState(const WorldFrame&) noexcept=0;
    // Prepare stereo visibility once. KFVR can use a conservative union; other
    // engines may provide per-eye visibility. Never tick simulation here. If
    // visibility was prepared earlier, validate its owner/sample instead.
    virtual bool PrepareViews(const WorldFrame&,const StereoViewSet&) noexcept=0;
    // RenderEye must not tick simulation, advance animation, or call Present.
    virtual bool RenderEye(const WorldFrame&,unsigned,const EyeView&,EyeImage&) noexcept=0;
    virtual void RestoreState() noexcept=0;
};
class IStereoSink {
public:
    virtual ~IStereoSink()=default;
    // Atomic pair submission only. A failed pair never becomes a flat menu.
    virtual bool Submit(const WorldFrame&,const TrackingFrame&,const std::array<EyeImage,2>&) noexcept=0;
};
enum class FrameResult { Submitted, Unsupported, Inactive, InvalidTracking, InvalidCamera,
    Duplicate, OwnerChanged, CaptureFailed, VisibilityFailed, EyeFailed, InvalidImages, SubmissionFailed };
class FrameCoordinator {
public:
    FrameResult Render(IRenderAdapter&,IStereoSink&,const WorldFrame&,const TrackingFrame&) noexcept;
    void Reset() noexcept {*this={};}
private:
    std::uint64_t lastFrame_=0, lastOwner_=0, lastEpoch_=0;
};
}