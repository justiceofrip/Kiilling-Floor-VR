#include "Test.h"
#include "kfvr/interaction/TrackingMath.h"
#include "kfvr/interaction/ComfortCamera.h"
#include "kfvr/interaction/ComfortControls.h"
#include "kfvr/interaction/RecenterPolicy.h"
#include "kfvr/interaction/MovementFrame.h"
#include "kfvr/interaction/AutoAdsPolicy.h"
#include "kfvr/interaction/WeaponGrip.h"
#include "kfvr/math/UiPointerMath.h"
using namespace kfvr;
int main(){
    math::Matrix4 identity{};for(int i=0;i<4;++i)identity.values[i][i]=1;
    math::Pose head{};head.position.y=1.7f;const auto eyes=math::ComputeEyePoses(head,.064f);CHECK(eyes);
    CHECK(Near(eyes->right.position.x-eyes->left.position.x,.064f));CHECK(!math::ComputeEyePoses(head,-1));
    const auto projection=math::MakeLhProjectionFromFovTangents({-.8f,1.2f,1,-1},.04f,300);CHECK(projection);
    auto near=math::TransformRowVector({0,0,.04f,1},*projection);auto far=math::TransformRowVector({0,0,300,1},*projection);
    CHECK(Near(near.z/near.w,0));CHECK(Near(far.z/far.w,1));CHECK(!math::MakeLhProjectionFromFovTangents({0,0,0,0},.1f,30));
    auto animated=identity;animated.values[0][0]=1.002f;animated.values[3]={2,4,8,1};
    for(int iteration=0;iteration<1000;++iteration){auto inverse=interaction::InverseAnimatedTransform(animated);CHECK(inverse);auto result=interaction::Multiply(animated,*inverse);CHECK(Near(result.values[0][0],1));CHECK(Near(result.values[3][0],0));}
    animated.values[0][0]=2;CHECK(!interaction::InverseAnimatedTransform(animated));
    interaction::ComfortYaw yaw;yaw.Bind(1);double native=20;
    for(int i=0;i<20000;++i){const float recoil=i%3==0?.125f:-.0625f;native=std::remainder(native+recoil,360.);yaw.AddRecoil(recoil);const auto heading=yaw.Heading(float(native),0);CHECK(heading);CHECK(Near(float(*heading),20));}
    auto movement=interaction::MakeMovementCamera(identity,90);CHECK(movement);CHECK(Near(movement->values[2][0],1));
    CHECK(!interaction::MakeMovementCamera(identity,181));
    interaction::StandingHeightReference height;CHECK(height.Recenter(0));CHECK(Near(height.Drop(-.5f),.5f));
    interaction::PhysicalStance stance;CHECK(stance.Update(true,1.1f,true,false,1000000000)==interaction::Posture::Standing);
    CHECK(stance.Update(true,1.1f,true,false,1200000000)==interaction::Posture::Crouched); // no inherited prone in KFVR
    CHECK(stance.Update(true,1.1f,false,false,1210000000)==interaction::Posture::Standing);
    interaction::RecenterPolicy recenter;CHECK(!recenter.Update(true,1));CHECK(!recenter.Update(false,2));CHECK(!recenter.Update(true,100));CHECK(!recenter.Update(false,101));CHECK(recenter.Update(true,900));CHECK(!recenter.Update(true,1000));
    interaction::AutoAdsPolicy ads;for(std::uint64_t t=1;t<201;t+=20)ads.Update(true,.9f,false,t);CHECK(ads.active);
    CHECK(!ads.Update(true,.9f,true,210));CHECK(!ads.Update(true,.9f,false,220));ads.Update(true,0,false,230);CHECK(!ads.blocked);
    auto anchor=math::MakeYawOnlyUiAnchor(head);CHECK(anchor);CHECK(Near(anchor->position.y,1.7f));
    return 0;
}