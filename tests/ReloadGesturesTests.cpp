#include "kfvr/interaction/ReloadGestures.h"
#include "kfvr/gameplay/PhysicalReload.h"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace kfvr::interaction;using namespace kfvr::gameplay;
#define CHECK(x) do{if(!(x))throw std::runtime_error(#x);}while(false)
struct InsertionDriver {InsertionGesture gate;InsertionSample s;InsertionDriver(){s.owner=s.space=1;s.focused=s.tracked=true;}
 InsertionOutput At(float y,float x=0){s.magazineTip.position={x,y,0};++s.sequence;s.timeNs+=20000000;return gate.Update(s);}};
struct StrokeDriver {ActionStrokeGesture gate;StrokeSample s;float current=0;StrokeDriver(){s.owner=s.space=1;s.focused=s.tracked=true;}
 StrokeOutput At(float z,bool grip){s.hand.position={0,0,z};s.grip=grip;++s.sequence;s.timeNs+=20000000;auto out=gate.Update(s,current);if(out.changed)current=out.travel;return out;}};
int main(){try{
    {InsertionDriver d;CHECK(!d.At(.035f).seated);CHECK(!d.At(.035f).seated);CHECK(d.At(-.03f).captured);CHECK(d.At(.01f).progress>0);CHECK(d.At(.036f).seated);CHECK(!d.At(.036f).seated);CHECK(!d.gate.Update(d.s).seated);CHECK(!d.At(-.03f).cleared);CHECK(!d.At(-.09f).cleared);CHECK(d.At(-.15f).cleared);}
    {InsertionDriver d;d.At(-.04f);CHECK(!d.At(.036f,.15f).seated);CHECK(!d.At(.036f).seated);d.At(-.04f);d.s.magazineTip.orientation={0,0,1,0};CHECK(!d.At(.036f).seated);}
    {InsertionDriver d;d.At(-.04f);d.s.focused=false;CHECK(!d.At(.036f).seated);d.s.focused=true;CHECK(!d.At(.036f).seated);d.At(-.04f);CHECK(d.At(.036f).seated);}
    {InsertionDriver d;d.At(-.08f);CHECK(!d.At(.065f).seated);CHECK(!d.At(.036f).seated);d.At(-.04f);d.s.space=2;CHECK(!d.At(.036f).seated);}
    {InsertionDriver d;d.At(-.04f);d.s.timeNs+=500000000;CHECK(!d.At(.036f).seated);d.At(-.04f);d.s.magazineTip.orientation.w=std::numeric_limits<float>::quiet_NaN();CHECK(!d.At(.036f).seated);}
    {StrokeDriver d;CHECK(!d.At(0,true).held);d.At(0,false);CHECK(d.At(0,true).held);CHECK(d.At(.03f,true).travel>.49f);CHECK(d.At(.06f,true).travel==1);CHECK(d.At(.03f,true).travel>.49f);CHECK(d.At(.03f,false).travel==0);}
    {StrokeDriver d;d.At(0,false);d.At(0,true);d.At(.03f,true);d.s.tracked=false;CHECK(!d.At(.06f,true).changed);d.s.tracked=true;CHECK(!d.At(.06f,true).held);CHECK(d.current>.49f);d.At(.03f,false);CHECK(d.At(.03f,true).held);CHECK(d.At(.06f,true).travel==1);}
    {StrokeDriver d;d.At(0,false);d.At(0,true);CHECK(!d.At(.5f,true).changed);CHECK(!d.At(.06f,true).held);}
    {InsertionDriver d;d.At(-.04f);auto stale=d.s;stale.timeNs-=1;stale.sequence+=1;stale.magazineTip.position.y=.036f;CHECK(!d.gate.Update(stale).seated);CHECK(d.At(.036f).seated);}
    {StrokeDriver d;d.At(0,false);d.At(0,true);auto stale=d.s;stale.timeNs-=1;stale.sequence+=1;stale.hand.position.z=.06f;CHECK(!d.gate.Update(stale,0).changed);CHECK(d.At(.06f,true).travel==1);}
    {InsertionDriver d;d.At(-.04f);const auto old=d.s;d.s.focused=false;d.At(.036f);d.s.focused=true;CHECK(!d.gate.Update(old).captured);CHECK(!d.At(.036f).seated);}
    // Real geometry gates drive the same conserved magazine state. No elapsed
    // animation timer is involved in seating, extraction or feeding.
    {PhysicalReload m({}, {0,0},15);std::uint32_t seq=0;auto act=[&](PhysicalAction a,float travel=0){CHECK(m.Request({1,++seq,0,0,a,travel},seq*20)==PhysicalResult::Completed);CHECK(m.Valid());};
     act(PhysicalAction::EjectToStorage);act(PhysicalAction::PresentAtBelt);act(PhysicalAction::GrabFromBelt);
     InsertionDriver insert;insert.At(-.04f);insert.At(.01f);CHECK(!m.State().guns[0].magazine);if(insert.At(.036f).seated)act(PhysicalAction::SeatFromHand);CHECK(m.Loaded(0)==15&&!m.State().guns[0].chamber);
     StrokeDriver stroke;stroke.At(0,false);stroke.At(0,true);for(float z:{.02f,.04f,.06f}){auto out=stroke.At(z,true);if(out.changed)act(PhysicalAction::MoveAction,out.travel);}CHECK(!m.State().guns[0].chamber);
     auto release=stroke.At(.06f,false);CHECK(release.changed);act(PhysicalAction::MoveAction,release.travel);CHECK(m.State().guns[0].chamber&&m.AvailableRounds()==15);}
    std::cout<<"Physical approach/alignment/seating/withdrawal, partial action travel, interruption, re-grasp and conserved reload integration passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
