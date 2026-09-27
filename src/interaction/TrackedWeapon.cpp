#include "kfvr/interaction/TrackedWeapon.h"
#include "kfvr/interaction/TrackingMath.h"
#include <cmath>
namespace kfvr::interaction {
namespace {
std::optional<math::Matrix4> Attachment(const math::Pose& p,float scale){const auto view=math::MakeLhViewFromOpenXRPose(p);if(!view)return {};auto world=InverseRigid(*view);if(!world)return {};for(unsigned i=0;i<3;++i)world->values[3][i]*=scale;return world;}
}
std::optional<TrackedWeaponPose> SolveTrackedWeapon(const math::Matrix4& body,const math::Pose& reference,const math::Pose& grip,const TrackedWeaponProfile& profile,float units)noexcept {
    if(!std::isfinite(units)||units<=0||units>10000)return {};
    const auto hand=math::ComposeRuntimeHeadWithLhCamera(body,reference,grip,units);
    const auto weapon=Attachment(profile.gripToWeapon,units),muzzle=Attachment(profile.weaponToMuzzle,units);
    if(!hand||!weapon||!muzzle)return {};
    TrackedWeaponPose out;out.weaponWorld=Multiply(*weapon,*hand);out.muzzleWorld=Multiply(*muzzle,out.weaponWorld);
    if(!InverseRigid(out.weaponWorld)||!InverseRigid(out.muzzleWorld))return {};
    const auto& m=out.muzzleWorld.values;out.fireOrigin={m[3][0],m[3][1],m[3][2]};out.fireDirection={m[2][0],m[2][1],m[2][2]};return out;
}
std::optional<std::array<math::Pose,2>> BeltMagazinePoses(const math::Pose& head,float yaw,float drop,float width,float forward)noexcept {
    if(!math::MakeLhViewFromOpenXRPose(head)||!std::isfinite(yaw)||!std::isfinite(drop)||drop<.2f||drop>1.2f||!std::isfinite(width)||width<.05f||width>.6f||!std::isfinite(forward)||std::abs(forward)>.6f)return {};
    const float s=std::sin(yaw),c=std::cos(yaw);std::array<math::Pose,2> out{};
    for(unsigned i=0;i<2;++i){const float x=i?width:-width;out[i].position={head.position.x+c*x-s*forward,head.position.y-drop,head.position.z-s*x-c*forward};out[i].orientation={0,std::sin(yaw/2),0,std::cos(yaw/2)};}return out;
}
}
