#include "kfvr/engine/Ue2Camera.h"
#include "kfvr/interaction/TrackingMath.h"
#include <cmath>
#include <numbers>
namespace kfvr::ue2 {
namespace {
double Angle(std::int32_t n){return -double((0u-std::uint32_t(n))&65532u)*(2*std::numbers::pi/65536.);}
std::int32_t Units(double r){return std::int32_t(std::lround(r*(65536./(2*std::numbers::pi))));}
}
std::optional<math::Matrix4> CameraWorld(math::Vec3 p,Rotator r) noexcept {
 if(!std::isfinite(p.x)||!std::isfinite(p.y)||!std::isfinite(p.z))return {};
 const double sp=std::sin(Angle(r.pitch)),cp=std::cos(Angle(r.pitch)),sy=std::sin(Angle(r.yaw)),cy=std::cos(Angle(r.yaw)),sr=std::sin(Angle(r.roll)),cr=std::cos(Angle(r.roll));
 math::Matrix4 m; m.values[0]={float(sr*sp*cy-cr*sy),float(sr*sp*sy+cr*cy),float(-sr*cp),0};
 m.values[1]={float(-cr*sp*cy-sr*sy),float(sr*cy-cr*sp*sy),float(cr*cp),0};
 m.values[2]={float(cp*cy),float(cp*sy),float(sp),0};m.values[3]={p.x,p.y,p.z,1};return m;
}
std::optional<Rotator> CameraRotation(const math::Matrix4& m) noexcept {
 if(!interaction::InverseRigid(m))return {};
 const auto& f=m.values[2];const double horizontal=std::hypot(f[0],f[1]);
 if(horizontal<.0001)return {}; // Caller retains last yaw at the vertical singularity.
 return Rotator{Units(std::atan2(f[2],horizontal)),Units(std::atan2(f[1],f[0])),Units(std::atan2(-m.values[0][2],m.values[1][2]))};
}
std::optional<EyeMatrices> BuildEye(const math::Matrix4& body,const math::Pose& reference,const math::Pose& eye,const math::FovTangents& fov,float units,float nearZ,float farZ) noexcept {
 if(!std::isfinite(units)||units<=0||units>10000)return {};
 auto camera=math::ComposeRuntimeHeadWithLhCamera(body,reference,eye,units);auto projection=math::MakeLhProjectionFromFovTangents(fov,nearZ,farZ);if(!camera||!projection)return {};
 auto view=interaction::InverseRigid(*camera);if(!view)return {};
 return EyeMatrices{*camera,*view,*projection,interaction::Multiply(*view,*projection)};
}
}


