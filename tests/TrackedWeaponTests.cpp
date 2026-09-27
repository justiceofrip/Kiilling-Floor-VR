#include "kfvr/interaction/TrackedWeapon.h"
#include "kfvr/engine/Ue2Camera.h"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace kfvr;
#define CHECK(x) do{if(!(x))throw std::runtime_error(#x);}while(false)
bool Near(float a,float b){return std::abs(a-b)<.001f;}
int main(){try{
    const auto body=ue2::CameraWorld({100,200,300},{0,0,0});CHECK(body);math::Pose reference{},grip{};grip.position={.2f,-.3f,-.4f};
    interaction::TrackedWeaponProfile profile;profile.weaponToMuzzle.position.z=-.2f;
    const auto gun=interaction::SolveTrackedWeapon(*body,reference,grip,profile,50);CHECK(gun);CHECK(Near(gun->fireOrigin.x,130)&&Near(gun->fireOrigin.y,210)&&Near(gun->fireOrigin.z,285));CHECK(Near(gun->fireDirection.x,1)&&Near(gun->fireDirection.y,0));
    // A changed head pose moves the view, never the fixed controller weapon.
    math::Pose looking;looking.orientation={0,.70710678f,0,.70710678f};const auto eye=ue2::BuildEye(*body,reference,looking,{-1,1,1,-1},50,5,65536);CHECK(eye);const auto still=interaction::SolveTrackedWeapon(*body,reference,grip,profile,50);CHECK(still&&still->weaponWorld.values==gun->weaponWorld.values);
    auto turnedBody=ue2::CameraWorld({100,200,300},{0,16384,0});const auto turned=interaction::SolveTrackedWeapon(*turnedBody,reference,grip,profile,50);CHECK(turned);CHECK(Near(turned->fireOrigin.x,90)&&Near(turned->fireOrigin.y,230)&&Near(turned->fireDirection.y,1));
    profile.gripToWeapon.orientation={0,.70710678f,0,.70710678f};const auto sideways=interaction::SolveTrackedWeapon(*body,reference,grip,profile,50);CHECK(sideways&&Near(sideways->fireDirection.y,-1));
    profile.gripToWeapon.orientation.w=std::numeric_limits<float>::quiet_NaN();CHECK(!interaction::SolveTrackedWeapon(*body,reference,grip,profile,50));CHECK(!interaction::SolveTrackedWeapon(*body,reference,grip,{},-1));
    math::Pose head;head.position={0,1.7f,0};const auto belt=interaction::BeltMagazinePoses(head,0);CHECK(belt);head.orientation={.70710678f,0,0,.70710678f};const auto down=interaction::BeltMagazinePoses(head,0);CHECK(down);for(unsigned i=0;i<2;++i){CHECK(Near((*belt)[i].position.y,1.05f));CHECK((*belt)[i].position.x==(*down)[i].position.x&&(*belt)[i].position.z==(*down)[i].position.z&&(*down)[i].orientation.w==1);}
    std::cout<<"Controller-owned KF weapon/muzzle mapping, body turns, model calibration, head independence and upright belt poses passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
