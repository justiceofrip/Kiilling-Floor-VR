#pragma once
#include "kfvr/math/StereoMath.h"
#include <cstdint>
namespace kfvr::ue2 {
struct Rotator {std::int32_t pitch=0,yaw=0,roll=0;};
// UE2 world: X forward, Y right, Z up. Camera rows remain right/up/forward.
// This adapter follows the audited FCameraSceneNode matrices, not UE4 APIs.
std::optional<math::Matrix4> CameraWorld(math::Vec3 location,Rotator rotation) noexcept;
// Shared living-pawn anchor for eyes/controllers; excludes desktop WalkBob
// and camera shake while retaining the native eye-height adjustment.
std::optional<math::Matrix4> TrackedBodyWorld(math::Vec3 pawnLocation,float eyeHeight,Rotator controllerRotation) noexcept;
std::optional<Rotator> CameraRotation(const math::Matrix4&) noexcept;
struct EyeMatrices {math::Matrix4 camera{},view{},projection{},viewProjection{};};
std::optional<EyeMatrices> BuildEye(const math::Matrix4& bodyCamera,const math::Pose& referenceHead,
 const math::Pose& eye,const math::FovTangents&,float unitsPerMetre,float nearUnits,float farUnits) noexcept;
// Complete derived camera data in the audited UE2 order. Construct a native
// scene first; this payload only replaces its private camera/visibility data.
struct SceneViewData {
 std::array<math::Matrix4,6> matrices{}; // view, camera, projection, inverse projection, VP, inverse VP
 math::Vec3 origin{},pixelDx{},pixelDy{};
 float determinant=0;
};
std::optional<SceneViewData> MakeSceneView(const math::Matrix4& camera,
 const math::Matrix4& projection,std::uint32_t width,std::uint32_t height) noexcept;
}
