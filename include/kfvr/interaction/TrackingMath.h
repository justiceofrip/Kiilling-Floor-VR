#pragma once
#include "kfvr/math/StereoMath.h"
namespace kfvr::interaction {
math::Matrix4 Multiply(const math::Matrix4& a,const math::Matrix4& b) noexcept;
std::optional<math::Matrix4> InverseRigid(const math::Matrix4& matrix) noexcept;
// Exact affine inverse within the existing near-rigid acceptance bound.
std::optional<math::Matrix4> InverseAnimatedTransform(const math::Matrix4& matrix) noexcept;
std::optional<math::Matrix4> TrackedWeaponCamera(const math::Matrix4& sourceCamera,
    const math::Matrix4& eyeCamera,const math::Pose& calibrationHead,
    const math::Pose& referenceGrip,const math::Pose& currentGrip,float scale) noexcept;
std::optional<math::Matrix4> MapTrackedFire(const math::Matrix4& nativeFire,
    const math::Matrix4& nativeCamera,const math::Matrix4& gun) noexcept;
float PoseYaw(const math::Pose& pose) noexcept;
}
