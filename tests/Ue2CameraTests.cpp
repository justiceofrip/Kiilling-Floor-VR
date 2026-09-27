#include "Test.h"
#include "kfvr/engine/Ue2Camera.h"
#include "kfvr/interaction/TrackingMath.h"
#include <limits>
using namespace kfvr;
int main(){
 auto camera=ue2::CameraWorld({3066.85986f,-1373.27832f,-4563.15088f},{-72,-1851,0});CHECK(camera);
 // Recorded from FCameraSceneNode+0x54 in the installed game on KF-Manor.
 const float live[3][3]={{.176249295f,.984345555f,0},{.00679479865f,-.00121662405f,.999976218f},{.984322131f,-.176245108f,-.00690285908f}};
 for(int i=0;i<3;++i)for(int j=0;j<3;++j){if(!Near(camera->values[i][j],live[i][j],.000003f))std::fprintf(stderr,"matrix %d,%d actual %.9f native %.9f\n",i,j,camera->values[i][j],live[i][j]);CHECK(Near(camera->values[i][j],live[i][j],.000003f));}
 auto inverse=interaction::InverseRigid(*camera);CHECK(inverse);auto origin=math::TransformRowVector({3066.85986f,-1373.27832f,-4563.15088f,1},*inverse);CHECK(std::abs(origin.x)<.001&&std::abs(origin.y)<.001&&std::abs(origin.z)<.001);
 for(int yaw:{0,4096,-1851,16384,32768})for(int pitch:{-8000,0,6000})for(int roll:{0,3000,-2000}){auto m=ue2::CameraWorld({}, {pitch,yaw,roll});CHECK(m);auto r=ue2::CameraRotation(*m);CHECK(r);auto roundtrip=ue2::CameraWorld({},*r);CHECK(roundtrip);for(int i=0;i<3;++i)for(int j=0;j<3;++j)CHECK(Near(m->values[i][j],roundtrip->values[i][j],.0004f));}
 // Exact and near-vertical gaze must produce a usable native constructor
 // rotation, including wrist/head roll, either side of the Euler singularity.
 for(int pitch:{-16388,-16384,-16380,16380,16384,16388})
  for(int yaw:{-32768,-23457,-8192,0,8192,23457,32767})
   for(int roll:{-24576,-8192,0,8192,24576}){
    auto m=ue2::CameraWorld({}, {pitch,yaw,roll});CHECK(m);
    auto rotation=ue2::CameraRotation(*m);CHECK(rotation);
    auto reconstructed=ue2::CameraWorld({},*rotation);CHECK(reconstructed);
    for(int i=0;i<3;++i)for(int j=0;j<3;++j)CHECK(Near(m->values[i][j],reconstructed->values[i][j],.001f));
   }
 auto neutral=ue2::CameraWorld({},{});CHECK(neutral);math::Pose left,right;left.position.x=-.032f;right.position.x=.032f;
 auto l=ue2::BuildEye(*neutral,{},left,{-.8f,1.1f,1,-1},50,5,65536);auto r=ue2::BuildEye(*neutral,{},right,{-1.1f,.8f,1,-1},50,5,65536);CHECK(l&&r);CHECK(Near(r->camera.values[3][1]-l->camera.values[3][1],3.2f));CHECK(!Near(l->projection.values[2][0],r->projection.values[2][0]));
 math::Pose forward;forward.position.z=-1;auto moved=ue2::BuildEye(*neutral,{},forward,{-1,1,1,-1},50,5,65536);CHECK(moved&&Near(moved->camera.values[3][0],50));
 CHECK(!ue2::BuildEye(*neutral,{},left,{-1,1,1,-1},0,5,65536));CHECK(!ue2::CameraWorld({std::numeric_limits<float>::quiet_NaN(),0,0},{}));

 // Both inverse directions, clip-edge mapping and world-space unprojection
 // independently exercise the complete asymmetric UE2 camera payload.
 for(float tx:{0.f,137.f,-4096.f})for(float yaw:{0.f,.7f})for(int width:{137,512,2048}){
  math::Pose tracked;tracked.position={.032f,.12f,-.3f};tracked.orientation={0,std::sin(yaw/2),0,std::cos(yaw/2)};
  auto body=ue2::CameraWorld({tx,-121,33},{-500,4231,151});CHECK(body);
  math::FovTangents fov{-.65f,1.12f,.83f,-.61f};auto eye=ue2::BuildEye(*body,{},tracked,fov,50,5,65536);CHECK(eye);
  auto scene=ue2::MakeSceneView(eye->camera,eye->projection,width,333);CHECK(scene);
  for(auto pair:{std::pair{0,1},std::pair{2,3},std::pair{4,5}}){
   for(bool reverse:{false,true}){const auto& a=scene->matrices[reverse?pair.second:pair.first];const auto& b=scene->matrices[reverse?pair.first:pair.second];for(int i=0;i<4;++i)for(int j=0;j<4;++j){
    // Large translated VP matrices cancel million-sized float terms. Bound
    // residuals by input precision/conditioning; world roundtrips below also
    // enforce a fixed absolute position bound.
    double sum=0,magnitude=0;for(int k=0;k<4;++k){const double term=double(a.values[i][k])*b.values[k][j];sum+=term;magnitude+=std::abs(term);}
    CHECK(std::abs(sum-(i==j?1.:0.))<=8*std::numeric_limits<float>::epsilon()*magnitude+.000001);
   }}
  }
  for(float horizontal:{fov.left,fov.right})for(float vertical:{fov.down,fov.up}){
   auto world=math::TransformRowVector({horizontal*13,vertical*13,13,1},eye->camera);
   auto clip=math::TransformRowVector(world,scene->matrices[4]);CHECK(Near(clip.x/clip.w,horizontal==fov.left?-1.f:1.f,.0002f));CHECK(Near(clip.y/clip.w,vertical==fov.down?-1.f:1.f,.0002f));
   auto back=math::TransformRowVector(clip,scene->matrices[5]);CHECK(Near(back.x/back.w,world.x,.04f)&&Near(back.y/back.w,world.y,.004f)&&Near(back.z/back.w,world.z,.004f));
  }
  auto base=math::TransformRowVector({0,0,.2f,1},scene->matrices[5]);auto next=math::TransformRowVector({2.f/width,0,.2f,1},scene->matrices[5]);
  CHECK(Near(next.x-base.x,scene->pixelDx.x,.0002f)&&Near(next.y-base.y,scene->pixelDx.y,.0002f)&&Near(next.z-base.z,scene->pixelDx.z,.0002f));
  CHECK(scene->determinant>0);auto half=ue2::MakeSceneView(eye->camera,eye->projection,width*2,666);CHECK(half);CHECK(Near(half->pixelDx.x*2,scene->pixelDx.x));CHECK(Near(half->pixelDy.z*2,scene->pixelDy.z));
 }
 CHECK(!ue2::MakeSceneView(l->camera,l->projection,0,384));CHECK(!ue2::MakeSceneView(l->camera,l->projection,512,8193));
 auto invalid=l->projection;invalid.values[3][2]=0;CHECK(!ue2::MakeSceneView(l->camera,invalid,512,384));
 invalid=l->projection;invalid.values[0][1]=.01f;CHECK(!ue2::MakeSceneView(l->camera,invalid,512,384));
 invalid=l->projection;invalid.values[2][0]=std::numeric_limits<float>::infinity();CHECK(!ue2::MakeSceneView(l->camera,invalid,512,384));
 invalid=l->projection;invalid.values[0][0]=std::numeric_limits<float>::denorm_min();CHECK(!ue2::MakeSceneView(l->camera,invalid,512,384));
 return 0;
}

