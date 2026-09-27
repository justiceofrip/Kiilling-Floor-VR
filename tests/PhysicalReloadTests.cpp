#include "kfvr/gameplay/PhysicalReload.h"
#include <cmath>
#include <iostream>
#include <limits>
#include <random>
#include <stdexcept>
using namespace kfvr::gameplay;
#define CHECK(x) do{if(!(x))throw std::runtime_error(#x);}while(false)
struct Driver {
    PhysicalReload m;std::uint32_t seq=0,epoch=1;std::uint64_t now=0;
    Driver(std::array<unsigned,2> loaded,unsigned reserve,PhysicalReloadConfig c={}):m(c,loaded,reserve){}
    PhysicalResult Try(PhysicalAction a,unsigned gun=0,unsigned slot=0,float travel=0){auto r=m.Request({epoch,++seq,gun,slot,a,travel},now);CHECK(m.Valid());return r;}
    void Do(PhysicalAction a,unsigned gun=0,unsigned slot=0,float travel=0){CHECK(Try(a,gun,slot,travel)==PhysicalResult::Completed);}
};
int main(){try{
    {Driver d({6,0},30);const auto original=d.m.State().guns[0].magazine;
     d.Do(PhysicalAction::EjectToHand);CHECK(d.m.State().guns[0].chamber);CHECK(d.m.At(MagazinePlace::Hand,0)->id==original);
     d.Do(PhysicalAction::StowFromHand);d.Do(PhysicalAction::PresentAtBelt);const auto replacement=d.m.At(MagazinePlace::Belt,0)->id;
     CHECK(replacement!=original&&d.m.Magazine(original)->rounds==5&&d.m.State().reserve==15);
     d.Do(PhysicalAction::GrabFromBelt);d.Do(PhysicalAction::SeatFromHand);CHECK(d.m.Loaded(0)==16&&d.m.Magazine(original)->rounds==5);
     d.Do(PhysicalAction::Fire);CHECK(d.m.Loaded(0)==15&&d.m.State().spent==1);CHECK(d.Try(PhysicalAction::Fire)==PhysicalResult::Ineligible);
     d.m.Suspend();CHECK(d.m.Activate(++d.epoch));CHECK(d.Try(PhysicalAction::Fire)==PhysicalResult::Ineligible);d.now=200;d.Do(PhysicalAction::Fire);}
    {Driver d({0,0},15);d.Do(PhysicalAction::EjectToStorage);d.Do(PhysicalAction::PresentAtBelt);d.Do(PhysicalAction::GrabFromBelt);d.Do(PhysicalAction::SeatFromHand);
     CHECK(!d.m.State().guns[0].chamber);d.Do(PhysicalAction::MoveAction,0,0,.8f);d.Do(PhysicalAction::MoveAction,0,0,0);CHECK(!d.m.State().guns[0].chamber);
     d.Do(PhysicalAction::MoveAction,0,0,1);CHECK(!d.m.State().guns[0].chamber);d.m.Suspend();CHECK(d.Try(PhysicalAction::MoveAction)==PhysicalResult::Inactive);CHECK(d.m.State().guns[0].actionTravel==1);
     CHECK(d.m.Activate(++d.epoch));d.Do(PhysicalAction::MoveAction,0,0,.5f);CHECK(!d.m.State().guns[0].chamber);d.Do(PhysicalAction::MoveAction,0,0,0);CHECK(d.m.State().guns[0].chamber&&d.m.Loaded(0)==15);
     d.Do(PhysicalAction::MoveAction,0,0,.5f);d.Do(PhysicalAction::MoveAction,0,0,0);CHECK(d.m.State().ejected==0);
     d.Do(PhysicalAction::MoveAction,0,0,.7f);CHECK(d.m.State().ejected==1&&!d.m.State().guns[0].chamber);d.Do(PhysicalAction::MoveAction,0,0,0);CHECK(!d.m.State().guns[0].chamber);
     d.Do(PhysicalAction::MoveAction,0,0,1);d.Do(PhysicalAction::MoveAction,0,0,0);CHECK(d.m.State().guns[0].chamber&&d.m.Loaded(0)==14);}
    {PhysicalReloadConfig c;c.guns=2;Driver d({1,1},20,c);d.Do(PhysicalAction::Fire,0);d.Do(PhysicalAction::Fire,1);CHECK(d.m.State().guns[0].slideLocked&&d.m.State().guns[1].slideLocked);
     d.Do(PhysicalAction::EjectToStorage,0);d.Do(PhysicalAction::EjectToStorage,1);d.Do(PhysicalAction::PresentAtBelt,0,0);d.Do(PhysicalAction::PresentAtBelt,1,1);
     CHECK(d.m.At(MagazinePlace::Belt,0)->rounds==15&&d.m.At(MagazinePlace::Belt,1)->rounds==5&&d.m.State().reserve==0);
     CHECK(d.Try(PhysicalAction::PresentAtBelt,0,1)==PhysicalResult::Ineligible);
     d.Do(PhysicalAction::SeatFromBelt,0,0);d.Do(PhysicalAction::SeatFromBelt,1,1);CHECK(d.m.Loaded(0)==15&&d.m.Loaded(1)==5);
     CHECK(d.m.State().guns[0].chamber&&d.m.State().guns[1].chamber);d.now=200;CHECK(d.Try(PhysicalAction::Fire,0)==PhysicalResult::Ineligible);
     d.Do(PhysicalAction::ClearBelt,0);d.Do(PhysicalAction::Fire,0);CHECK(d.Try(PhysicalAction::Fire,1)==PhysicalResult::Ineligible);d.Do(PhysicalAction::ClearBelt,1);d.Do(PhysicalAction::Fire,1);
     CHECK(d.m.State().spent==4&&d.m.AvailableRounds()==18);}
    {PhysicalReloadConfig c;c.guns=2;c.dualSlideAssist=false;Driver d({1,1},30,c);d.Do(PhysicalAction::Fire);d.Do(PhysicalAction::EjectToStorage);d.Do(PhysicalAction::PresentAtBelt);d.Do(PhysicalAction::SeatFromBelt);
     CHECK(d.m.State().guns[0].slideLocked&&!d.m.State().guns[0].chamber);d.Do(PhysicalAction::ClearBelt);d.Do(PhysicalAction::ReleaseSlide);CHECK(d.m.State().guns[0].chamber);}
    {PhysicalReloadConfig c;c.guns=2;Driver d({5,6},30,c);d.Do(PhysicalAction::EjectToStorage,0);d.Do(PhysicalAction::EjectToStorage,1);d.Do(PhysicalAction::PresentAtBelt,0,0);d.Do(PhysicalAction::PresentAtBelt,1,1);d.Do(PhysicalAction::SeatFromBelt,0,0);d.Do(PhysicalAction::SeatFromBelt,1,1);
     CHECK(d.m.Loaded(0)==16&&d.m.Loaded(1)==16&&d.m.State().ejected==0);CHECK(d.m.Magazine(1)->rounds==4&&d.m.Magazine(2)->rounds==5);}
    {Driver d({4,0},0);const auto id=d.m.State().guns[0].magazine;d.Do(PhysicalAction::EjectToHand);d.Do(PhysicalAction::StowFromHand);d.Do(PhysicalAction::PresentAtBelt);CHECK(d.m.At(MagazinePlace::Belt,0)->id==id);d.Do(PhysicalAction::GrabFromBelt);d.Do(PhysicalAction::DropFromHand);CHECK(d.m.DiscardedRounds()==3&&d.m.AvailableRounds()==1);}
    {Driver d({2,0},0);d.Do(PhysicalAction::EjectToGround);d.Do(PhysicalAction::Fire);CHECK(!d.m.State().guns[0].slideLocked&&d.m.State().spent==1&&d.m.DiscardedRounds()==1);}
    {Driver d({1,0},0);d.Do(PhysicalAction::Fire);CHECK(d.Try(PhysicalAction::MoveAction,0,0,.5f)==PhysicalResult::Ineligible);d.Do(PhysicalAction::MoveAction,0,0,1);d.Do(PhysicalAction::MoveAction,0,0,0);CHECK(!d.m.State().guns[0].slideLocked);}
    {Driver d({2,0},5);d.Do(PhysicalAction::Fire);CHECK(d.m.Request({1,1,0,0,PhysicalAction::Fire,0},1000)==PhysicalResult::Stale);CHECK(d.m.Request({2,2,0,0,PhysicalAction::Fire,0},1000)==PhysicalResult::Stale);
     const auto rev=d.m.State().revision;CHECK(d.Try(PhysicalAction::MoveAction,0,0,std::numeric_limits<float>::quiet_NaN())==PhysicalResult::Ineligible);CHECK(d.Try(PhysicalAction(999))==PhysicalResult::Ineligible);CHECK(d.Try(PhysicalAction::Fire,2)==PhysicalResult::Ineligible);CHECK(d.m.State().revision==rev);
     d.now=500;d.Do(PhysicalAction::Fire);CHECK(d.m.Request({1,++d.seq,0,0,PhysicalAction::Fire,0},499)==PhysicalResult::BadClock);}
    {Driver d({1,0},0);d.Do(PhysicalAction::Fire);d.Do(PhysicalAction::ReleaseSlide);CHECK(!d.m.State().guns[0].chamber&&!d.m.State().guns[0].slideLocked&&d.m.State().spent==1);}
    {PhysicalReloadConfig c;c.maximumMagazines=1;Driver d({4,0},30,c);d.Do(PhysicalAction::EjectToStorage);d.Do(PhysicalAction::PresentAtBelt);CHECK(d.m.At(MagazinePlace::Belt,0)->id==1&&d.m.At(MagazinePlace::Belt,0)->rounds==3&&d.m.State().reserve==30);}
    {PhysicalReload m({}, {3,0},0);CHECK(m.Request({1,0xfffffffe,0,0,PhysicalAction::Fire,0},0)==PhysicalResult::Completed);CHECK(m.Request({1,1,0,0,PhysicalAction::Fire,0},300)==PhysicalResult::Completed);CHECK(m.Request({1,0xffffffff,0,0,PhysicalAction::Fire,0},600)==PhysicalResult::Stale);}
    {PhysicalReloadConfig c;c.guns=2;Driver d({15,15},100,c);std::mt19937 random(76234);for(unsigned i=0;i<30000;++i){d.now+=random()%300;d.Try(PhysicalAction(random()%13),random()%2,random()%2,float(random()%101)/100);if(random()%71==0){d.m.Suspend();CHECK(d.m.Activate(++d.epoch));}CHECK(d.m.AvailableRounds()+d.m.DiscardedRounds()+d.m.State().spent==130);}}
    std::cout<<"Physical magazines, tactical/partial actions, independent dual chambers, shared belt supply, slide assist, replay/lifecycle and 30000 conservation actions passed\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
