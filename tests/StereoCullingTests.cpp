#include "Test.h"
#include "kfvr/math/StereoCulling.h"
#include "kfvr/interaction/TrackingMath.h"
#include <limits>
using namespace kfvr;
math::Matrix4 Identity(){math::Matrix4 m{};for(unsigned i=0;i<4;++i)m.values[i][i]=1;return m;}
bool Inside(const math::StereoCullEnvelope& volume,const math::Vec4& world){
    const auto inverse=interaction::InverseAnimatedTransform(volume.world);if(!inverse)return false;
    const auto p=math::TransformRowVector(world,*inverse);
    return p.z>=volume.nearPlane&&p.z<=volume.farPlane&&p.x>=volume.fov.left*p.z&&p.x<=volume.fov.right*p.z&&p.y>=volume.fov.down*p.z&&p.y<=volume.fov.up*p.z;
}
int main(){
    auto center=Identity();std::array<math::Matrix4,2> eyes{center,center};eyes[0].values[3][0]=-.032f;eyes[1].values[3][0]=.032f;
    std::array<math::FovTangents,2> fov{math::FovTangents{-1,1,1,-1},math::FovTangents{-1,1,1,-1}};
    auto envelope=math::EncloseStereoFrusta(center,eyes,fov,.04f,300);CHECK(envelope);
    // Analytic near-edge requirement: tan=1 + half-IPD/near = 1.8. A mono cone
    // would drop close geometry visible only to the outer edge of either eye.
    CHECK(envelope->fov.right>=1.8f&&envelope->fov.right<1.801f);CHECK(envelope->nearPlane<.04f&&envelope->farPlane>300);
    CHECK(Inside(*envelope,{.072f,.04f,.04f,1}));CHECK(Inside(*envelope,{-.072f,-.04f,.04f,1}));
    // Canted eye bases and off-center FOVs under a translated/turned game camera.
    center.values[0][0]=0;center.values[0][2]=-1;center.values[2][0]=1;center.values[2][2]=0;center.values[3]={140,3,-80,1};
    for(unsigned eye=0;eye<2;++eye){auto local=Identity();const float angle=eye?.08f:-.08f;local.values[0][0]=local.values[2][2]=std::cos(angle);local.values[0][2]=-std::sin(angle);local.values[2][0]=std::sin(angle);local.values[3][0]=eye?.032f:-.032f;local.values[3][2]=.008f;eyes[eye]=interaction::Multiply(local,center);}
    fov={math::FovTangents{-1.1f,.8f,.9f,-1.2f},math::FovTangents{-.8f,1.1f,1.2f,-.9f}};envelope=math::EncloseStereoFrusta(center,eyes,fov,.08f,100);CHECK(envelope);
    for(unsigned eye=0;eye<2;++eye)for(float depth:{.08f,.2f,1.f,17.f,100.f})for(float u:{0.f,.25f,.5f,.75f,1.f})for(float v:{0.f,.25f,.5f,.75f,1.f}){
        const auto x=fov[eye].left+(fov[eye].right-fov[eye].left)*u,y=fov[eye].down+(fov[eye].up-fov[eye].down)*v;
        const auto point=math::TransformRowVector({depth*x,depth*y,depth,1},eyes[eye]);CHECK(Inside(*envelope,point));
    }
    auto badEyes=eyes;badEyes[0].values[0][0]=NAN;CHECK(!math::EncloseStereoFrusta(center,badEyes,fov,.08f,100));
    auto badFov=fov;badFov[1].up=badFov[1].down;CHECK(!math::EncloseStereoFrusta(center,eyes,badFov,.08f,100));
    CHECK(!math::EncloseStereoFrusta(center,eyes,fov,100,.08f));
    center=Identity();eyes={center,center};eyes[0].values[3][2]=-1;CHECK(!math::EncloseStereoFrusta(center,eyes,fov,.08f,100));
    return 0;
}
