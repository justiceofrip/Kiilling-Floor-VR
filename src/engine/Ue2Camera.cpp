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
std::optional<SceneViewData> MakeSceneView(const math::Matrix4& camera,const math::Matrix4& projection,std::uint32_t width,std::uint32_t height) noexcept {
 if(!width||!height||width>8192||height>8192)return {};
 // Use the exact inverse, including the small floating-point basis error.
 const auto view=interaction::InverseAnimatedTransform(camera);if(!view)return {};
 const auto& p=projection.values;
 for(const auto& row:p)for(float value:row)if(!std::isfinite(value))return {};
 if(p[0][0]<=0||p[1][1]<=0||p[2][2]<1||p[3][2]>=0||p[2][3]!=1)return {};
 for(unsigned r=0;r<4;++r)for(unsigned c=0;c<4;++c){const bool allowed=(r==0&&c==0)||(r==1&&c==1)||r==2||(r==3&&c==2);if(!allowed&&p[r][c]!=0)return {};}
 math::Matrix4 inverse{};auto& q=inverse.values;
 q[0][0]=1/p[0][0];q[1][1]=1/p[1][1];q[2][3]=1/p[3][2];
 q[3][0]=-p[2][0]/p[0][0];q[3][1]=-p[2][1]/p[1][1];q[3][2]=1;q[3][3]=-p[2][2]/p[3][2];
 SceneViewData out;out.matrices={*view,camera,projection,inverse,interaction::Multiply(*view,projection),interaction::Multiply(inverse,camera)};
 const auto& a=view->values;
 const double det=a[0][0]*(double(a[1][1])*a[2][2]-double(a[1][2])*a[2][1])-a[0][1]*(double(a[1][0])*a[2][2]-double(a[1][2])*a[2][0])+a[0][2]*(double(a[1][0])*a[2][1]-double(a[1][1])*a[2][0]);
 out.determinant=float(det*(-double(p[0][0])*p[1][1]*p[3][2]));
 out.origin={camera.values[3][0],camera.values[3][1],camera.values[3][2]};
 const auto& world=out.matrices[5].values;
 out.pixelDx={2*world[0][0]/width,2*world[0][1]/width,2*world[0][2]/width};
 out.pixelDy={2*world[1][0]/height,2*world[1][1]/height,2*world[1][2]/height};
 for(const auto& matrix:out.matrices)for(const auto& row:matrix.values)for(float value:row)if(!std::isfinite(value))return {};
 for(float value:{out.pixelDx.x,out.pixelDx.y,out.pixelDx.z,out.pixelDy.x,out.pixelDy.y,out.pixelDy.z,out.determinant})if(!std::isfinite(value))return {};
 if(out.determinant==0)return {};
 return out;
}

}


