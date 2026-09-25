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
 auto neutral=ue2::CameraWorld({},{});CHECK(neutral);math::Pose left,right;left.position.x=-.032f;right.position.x=.032f;
 auto l=ue2::BuildEye(*neutral,{},left,{-.8f,1.1f,1,-1},50,5,65536);auto r=ue2::BuildEye(*neutral,{},right,{-1.1f,.8f,1,-1},50,5,65536);CHECK(l&&r);CHECK(Near(r->camera.values[3][1]-l->camera.values[3][1],3.2f));CHECK(!Near(l->projection.values[2][0],r->projection.values[2][0]));
 math::Pose forward;forward.position.z=-1;auto moved=ue2::BuildEye(*neutral,{},forward,{-1,1,1,-1},50,5,65536);CHECK(moved&&Near(moved->camera.values[3][0],50));
 CHECK(!ue2::BuildEye(*neutral,{},left,{-1,1,1,-1},0,5,65536));CHECK(!ue2::CameraWorld({std::numeric_limits<float>::quiet_NaN(),0,0},{}));
 return 0;
}

