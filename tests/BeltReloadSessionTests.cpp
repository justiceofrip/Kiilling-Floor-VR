#include "kfvr/interaction/BeltReloadSession.h"
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace kfvr::interaction;using namespace kfvr::gameplay;
#define CHECK(x) do{if(!(x))throw std::runtime_error(#x);}while(false)
struct Driver {
 BeltReloadSession session;BeltReloadFrame f;std::uint64_t now=0;
 static PhysicalReloadConfig Config(){PhysicalReloadConfig c;c.guns=2;return c;}
 Driver(std::array<unsigned,2> loaded={1,1},unsigned reserve=20):session(Config(),loaded,reserve,99){f.owner=99;f.space=1;f.focused=f.tracked=true;for(auto& h:f.guns){h.tracked=true;h.barrel.orientation={-.70710678f,0,0,.70710678f};h.beltMagazine.position.y=-.04f;}}
 std::array<BeltReloadFeedback,2> Tick(){++f.sequence;f.timeNs+=20000000;now+=20;auto out=session.Update(f,now);CHECK(session.Ammo().Valid());return out;}
 void FireBoth(){for(unsigned i=0;i<2;++i)CHECK(session.Command(PhysicalAction::Fire,i,i,0,now)==PhysicalResult::Completed);}
};
int main(){try{
 {Driver d;d.FireBoth();d.Tick();for(auto& h:d.f.guns)h.releaseMagazine=true;auto out=d.Tick();CHECK(out[0].ejected&&out[1].ejected&&out[0].presented&&out[1].presented);CHECK(d.session.Ammo().State().reserve==0);
  for(auto& h:d.f.guns)h.beltMagazine.position.y=.01f;out=d.Tick();CHECK(!out[0].seated&&out[0].insertion>0);
  for(auto& h:d.f.guns)h.beltMagazine.position.y=.036f;out=d.Tick();CHECK(out[0].seated&&out[1].seated);CHECK(d.session.Ammo().Loaded(0)==15&&d.session.Ammo().Loaded(1)==5);CHECK(!d.Tick()[0].seated);
  for(float y:{-.04f,-.1f,-.16f}){for(auto& h:d.f.guns)h.beltMagazine.position.y=y;out=d.Tick();}CHECK(out[0].cleared&&out[1].cleared);d.now+=200;d.FireBoth();CHECK(d.session.Ammo().State().spent==4);}
 {Driver d({5,6},30);for(auto& h:d.f.guns)h.releaseMagazine=true;CHECK(!d.Tick()[0].ejected);for(auto& h:d.f.guns)h.releaseMagazine=false;d.Tick();d.f.guns[0].barrel.orientation={};for(auto& h:d.f.guns)h.releaseMagazine=true;auto out=d.Tick();CHECK(!out[0].ejected&&out[1].ejected);CHECK(d.session.Ammo().State().guns[0].magazine&&d.session.Ammo().State().guns[0].chamber);}
 {Driver d;d.Tick();d.f.focused=false;for(auto& h:d.f.guns)h.releaseMagazine=true;d.Tick();d.f.focused=true;CHECK(!d.Tick()[0].ejected);for(auto& h:d.f.guns)h.releaseMagazine=false;d.Tick();d.f.guns[0].tracked=false;for(auto& h:d.f.guns)h.releaseMagazine=true;auto out=d.Tick();CHECK(!out[0].ejected&&out[1].ejected);d.f.guns[0].tracked=true;CHECK(!d.Tick()[0].ejected);}
 {Driver d;d.Tick();d.f.owner=123;d.f.guns[0].releaseMagazine=true;CHECK(!d.Tick()[0].ejected);d.session.Suspend();CHECK(d.session.Command(PhysicalAction::Fire,0,0,0,d.now)==PhysicalResult::Inactive);CHECK(d.session.Activate(123,2));CHECK(!d.Tick()[0].ejected);d.f.guns[0].releaseMagazine=false;d.Tick();d.f.guns[0].releaseMagazine=true;CHECK(d.Tick()[0].ejected);}
 {Driver d;d.Tick();const auto neutral=d.f;d.f.focused=false;d.Tick();d.f.focused=true;CHECK(!d.session.Update(neutral,d.now)[0].ejected);d.f.guns[0].releaseMagazine=true;CHECK(!d.Tick()[0].ejected);}
 std::cout<<"Simultaneous dual belt pipeline, shared reserve, downward release, seat/lift feedback, independent tracking, held recovery and ownership passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
