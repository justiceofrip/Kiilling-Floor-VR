#include "kfvr/engine/Ue2Camera.h"
#include "kfvr/interaction/TrackedWeapon.h"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace kfvr;
#define CHECK(x) do{if(!(x))throw std::runtime_error(#x);}while(false)
bool Near(float a,float b){return std::abs(a-b)<.001f;}
math::Quaternion Product(math::Quaternion a,math::Quaternion b){return {
 a.w*b.x+a.x*b.w+a.y*b.z-a.z*b.y,
 a.w*b.y-a.x*b.z+a.y*b.w+a.z*b.x,
 a.w*b.z+a.x*b.y-a.y*b.x+a.z*b.w,
 a.w*b.w-a.x*b.x-a.y*b.y-a.z*b.z};}
int main(){try{
 const auto body=ue2::CameraWorld({100,200,300},{0,7500,0});CHECK(body);
 unsigned cases=0;
 for(float yaw:{-2.4f,0.f,.8f,2.5f})for(float pitch:{-1.1f,-.4f,0.f,.9f})for(float roll:{-.6f,0.f,.5f}){
  const math::Quaternion qYaw{0,std::sin(yaw/2),0,std::cos(yaw/2)},qPitch{std::sin(pitch/2),0,0,std::cos(pitch/2)},qRoll{0,0,std::sin(roll/2),std::cos(roll/2)};
  math::Pose head{{1.2f,1.65f,-.7f},Product(Product(qYaw,qPitch),qRoll)};
  const auto reference=math::MakeYawReferencePose(head);CHECK(reference);
  CHECK(reference->position.x==head.position.x&&reference->position.y==head.position.y&&reference->position.z==head.position.z);
  CHECK(reference->orientation.x==0&&reference->orientation.z==0);
  // Returning the head to level must produce a level camera, regardless of
  // the head pitch/roll captured when the application started.
  math::Pose levelHead{head.position,qYaw};
  const auto camera=ue2::BuildEye(*body,*reference,levelHead,{-1,1,1,-1},50,5,65536);CHECK(camera);
  CHECK(Near(camera->camera.values[1][2],1)&&Near(camera->camera.values[2][2],0));
  CHECK(Near(camera->camera.values[3][0],100)&&Near(camera->camera.values[3][1],200)&&Near(camera->camera.values[3][2],300));
  // Physical vertical movement must stay vertical in game; startup tilt must
  // not make a crouch slide sideways or move hands toward/away from the body.
  auto raised=levelHead;raised.position.y+=.25f;
  const auto upper=ue2::BuildEye(*body,*reference,raised,{-1,1,1,-1},50,5,65536);CHECK(upper);
  CHECK(Near(upper->camera.values[3][0],100)&&Near(upper->camera.values[3][1],200)&&Near(upper->camera.values[3][2],312.5f));
  math::Pose grip{head.position,qYaw};grip.position.x+=.2f;grip.position.y-=.3f;grip.position.z-=.5f;
  const auto expected=interaction::SolveTrackedWeapon(*body,levelHead,grip,{},50);
  const auto actual=interaction::SolveTrackedWeapon(*body,*reference,grip,{},50);CHECK(expected&&actual);
  for(unsigned row=0;row<4;++row)for(unsigned col=0;col<4;++col)CHECK(Near(expected->weaponWorld.values[row][col],actual->weaponWorld.values[row][col]));
  ++cases;
 }
 math::Pose invalid;invalid.orientation={0,0,0,0};CHECK(!math::MakeYawReferencePose(invalid));
 invalid.orientation={.70710678f,0,0,.70710678f};CHECK(!math::MakeYawReferencePose(invalid));
 invalid={};invalid.position.y=std::numeric_limits<float>::quiet_NaN();CHECK(!math::MakeYawReferencePose(invalid));
 std::cout<<cases<<" tilted-startup camera/controller cases passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
