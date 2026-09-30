#pragma once
#include "kfvr/math/StereoMath.h"
namespace kfvr::interaction {
struct TrackedWeaponProfile {
    // OpenXR-local coordinates in metres. Calibrate from the actual weapon's
    // model grip and muzzle; identity is only a diagnostic attachment.
    math::Pose gripToWeapon{},weaponToMuzzle{};
};
struct TrackedWeaponPose {math::Matrix4 weaponWorld{},muzzleWorld{};math::Vec3 fireOrigin{},fireDirection{};};
// Keep the physical grasp centre while aligning the presentation frame with
// the runtime pointer orientation. Grip and aim may differ by a large fixed
// rotation and translation; the aim origin must not move the virtual palm.
std::optional<math::Pose> ControllerPresentationPose(const math::Pose& grip,
    const math::Pose& aim) noexcept;
// Body/reference are identical to the eye-camera anchor, but no current eye/head
// pose is involved. Looking around cannot drag the held gun or its shot ray.
std::optional<TrackedWeaponPose> SolveTrackedWeapon(const math::Matrix4& body,
    const math::Pose& referenceHead,const math::Pose& grip,
    const TrackedWeaponProfile&,float unitsPerMetre) noexcept;
// Explicit body heading keeps belt magazines upright when the player looks down.
// Position is a head-derived waist estimate until waist tracking is available.
std::optional<std::array<math::Pose,2>> BeltMagazinePoses(const math::Pose& head,
    float bodyYaw,float belowHead=.65f,float halfWidth=.18f,float forward=.18f) noexcept;
}
