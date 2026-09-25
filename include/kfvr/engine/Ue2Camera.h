#pragma once
#include "kfvr/math/StereoMath.h"
#include <cstdint>
namespace kfvr::ue2 {
struct Rotator {std::int32_t pitch=0,yaw=0,roll=0;};
// UE2 world: X forward, Y right, Z up. Camera rows remain right/up/forward.
// This adapter follows the audited FCameraSceneNode matrices, not UE4 APIs.
std::optional<math::Matrix4> CameraWorld(math::Vec3 location,Rotator rotation) noexcept;
std::optional<Rotator> CameraRotation(const math::Matrix4&) noexcept;
struct EyeMatrices {math::Matrix4 camera{},view{},projection{},viewProjection{};};
std::optional<EyeMatrices> BuildEye(const math::Matrix4& bodyCamera,const math::Pose& referenceHead,
 const math::Pose& eye,const math::FovTangents&,float unitsPerMetre,float nearUnits,float farUnits) noexcept;
}
