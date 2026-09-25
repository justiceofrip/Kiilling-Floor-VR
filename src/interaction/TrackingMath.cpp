#include "TrackingMath.h"
#include "kfvr/math/WeaponPoseMath.h"
#include <cmath>
namespace kfvr::interaction {
math::Matrix4 Multiply(const math::Matrix4& a,const math::Matrix4& b) noexcept {
    math::Matrix4 c{};for(int i=0;i<4;++i)for(int j=0;j<4;++j)for(int k=0;k<4;++k)c.values[i][j]+=a.values[i][k]*b.values[k][j];return c;
}
std::optional<math::Matrix4> InverseRigid(const math::Matrix4& m) noexcept {
    for(const auto& row:m.values)for(float f:row)if(!std::isfinite(f))return {};
    if(std::abs(m.values[3][3]-1)>.001f)return {};
    for(int i=0;i<3;++i) {
        if(std::abs(m.values[i][3])>.001f)return {};
        for(int j=0;j<3;++j) {
            float dot=0;for(int k=0;k<3;++k)dot+=m.values[i][k]*m.values[j][k];
            if(std::abs(dot-(i==j?1.f:0.f))>.01f)return {};
        }
    }
    math::Matrix4 out{};out.values[3][3]=1;
    for(int i=0;i<3;++i)for(int j=0;j<3;++j)out.values[i][j]=m.values[j][i];
    for(int j=0;j<3;++j)for(int k=0;k<3;++k)out.values[3][j]-=m.values[3][k]*out.values[k][j];
    return out;
}
std::optional<math::Matrix4> InverseAnimatedTransform(const math::Matrix4& m) noexcept {
 // Do not loosen pose validation. The native blended basis is near-rigid, but
 // transpose is not its exact inverse; repeated retargeting magnifies the drift.
 if(!InverseRigid(m))return {};
 const auto& a=m.values;
 const float det=a[0][0]*(a[1][1]*a[2][2]-a[1][2]*a[2][1])-a[0][1]*(a[1][0]*a[2][2]-a[1][2]*a[2][0])+a[0][2]*(a[1][0]*a[2][1]-a[1][1]*a[2][0]);
 if(!std::isfinite(det)||std::abs(det)<.9f)return {};
 math::Matrix4 out{};for(int i=0;i<4;++i)out.values[i][i]=1;auto& b=out.values;
 b[0][0]=(a[1][1]*a[2][2]-a[1][2]*a[2][1])/det;
 b[0][1]=(a[0][2]*a[2][1]-a[0][1]*a[2][2])/det;
 b[0][2]=(a[0][1]*a[1][2]-a[0][2]*a[1][1])/det;
 b[1][0]=(a[1][2]*a[2][0]-a[1][0]*a[2][2])/det;
 b[1][1]=(a[0][0]*a[2][2]-a[0][2]*a[2][0])/det;
 b[1][2]=(a[0][2]*a[1][0]-a[0][0]*a[1][2])/det;
 b[2][0]=(a[1][0]*a[2][1]-a[1][1]*a[2][0])/det;
 b[2][1]=(a[0][1]*a[2][0]-a[0][0]*a[2][1])/det;
 b[2][2]=(a[0][0]*a[1][1]-a[0][1]*a[1][0])/det;
 for(int j=0;j<3;++j)for(int k=0;k<3;++k)b[3][j]-=a[3][k]*b[k][j];
 return out;
}
std::optional<math::Matrix4> TrackedWeaponCamera(const math::Matrix4& sourceCamera,
    const math::Matrix4& eyeCamera,const math::Pose& calibrationHead,
    const math::Pose& referenceGrip,const math::Pose& currentGrip,float scale) noexcept {
    const auto sourceView=InverseRigid(sourceCamera);
    const auto hand=math::MakeLhCalibrationSpaceWeaponDelta(calibrationHead,referenceGrip,currentGrip,scale,1.5f);
    if(!sourceView || !hand)return {};
    const auto delta=math::MakeLhWorldSpaceWeaponDelta(*sourceView,*hand);
    if(!delta)return {};const auto inverse=InverseRigid(*delta);if(!inverse)return {};
    // Moving the view inversely moves native weapon geometry about the grip.
    return Multiply(eyeCamera,*inverse);
}
std::optional<math::Matrix4> MapTrackedFire(const math::Matrix4& nativeFire,
    const math::Matrix4& nativeCamera,const math::Matrix4& gun) noexcept {
    const auto inverse=InverseRigid(nativeCamera);
    if(!inverse || !InverseRigid(nativeFire) || !InverseRigid(gun))return {};
    float distance=0;for(int i=0;i<3;++i){const float d=nativeFire.values[3][i]-nativeCamera.values[3][i];distance+=d*d;}
    if(distance>4)return {};
    // Retain the native barrel offset and angular deviation in the camera's
    // local frame, then attach that frame to the physically held gun.
    return Multiply(Multiply(nativeFire,*inverse),gun);
}
float PoseYaw(const math::Pose& p) noexcept {
    const auto& q=p.orientation;
    const float x=-2.f*(q.x*q.z+q.w*q.y),z=-(1.f-2.f*(q.x*q.x+q.y*q.y));
    return std::isfinite(x) && std::isfinite(z)?std::atan2(x,-z):0.f;
}
}
