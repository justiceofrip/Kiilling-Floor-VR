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

// Capacity changes must be metadata-only, including for discarded identities and
// weapons paused partway through a mechanical action.
void CheckCapacityChange(PhysicalReload& m,unsigned capacity){
    const auto before=m.State();const auto config=m.Config();
    const auto available=m.AvailableRounds(),discarded=m.DiscardedRounds();
    const bool accepted=capacity>=1&&capacity<=1000;
    CHECK(m.SetCapacity(capacity)==accepted);
    const auto& after=m.State();const auto& updated=m.Config();
    CHECK(updated.capacity==(accepted?capacity:config.capacity));
    CHECK(updated.guns==config.guns&&updated.maximumRounds==config.maximumRounds);
    CHECK(updated.maximumMagazines==config.maximumMagazines&&updated.fireMs==config.fireMs);
    CHECK(updated.dualSlideAssist==config.dualSlideAssist&&updated.extractTravel==config.extractTravel);
    CHECK(updated.fullTravel==config.fullTravel&&updated.closedTravel==config.closedTravel);
    CHECK(after.revision==before.revision+unsigned(accepted&&capacity!=config.capacity));
    CHECK(after.reserve==before.reserve&&after.budget==before.budget);
    CHECK(after.spent==before.spent&&after.ejected==before.ejected);
    CHECK(after.magazines.size()==before.magazines.size());
    for(std::size_t i=0;i<after.magazines.size();++i){
        const auto& a=after.magazines[i];const auto& b=before.magazines[i];
        CHECK(a.id==b.id&&a.rounds==b.rounds&&a.capacity==b.capacity);
        CHECK(a.place==b.place&&a.slot==b.slot);
    }
    for(unsigned i=0;i<after.guns.size();++i){
        const auto& a=after.guns[i];const auto& b=before.guns[i];
        CHECK(a.magazine==b.magazine&&a.chamber==b.chamber&&a.slideLocked==b.slideLocked);
        CHECK(a.beltLatched==b.beltLatched&&a.feedArmed==b.feedArmed);
        CHECK(a.actionTravel==b.actionTravel&&a.nextFire==b.nextFire);
    }
    CHECK(m.AvailableRounds()==available&&m.DiscardedRounds()==discarded&&m.Valid());
}

void CapacityBindingTests(){
    PhysicalReloadConfig c;c.capacity=40;c.maximumRounds=5000;
    for(unsigned loaded:{0u,1u,40u,41u,51u,1001u}){
        PhysicalReload m(c,{loaded,0},17);
        const unsigned rounds=loaded?loaded-1:0;
        const auto* mag=m.Magazine(1);
        CHECK(mag&&mag->rounds==rounds&&mag->capacity==(rounds>40?rounds:40));
        CHECK(m.Loaded(0)==loaded&&m.State().guns[0].chamber==(loaded>0));
        CHECK(m.State().budget==loaded+17&&m.AvailableRounds()==loaded+17&&m.Valid());
        CheckCapacityChange(m,1);CheckCapacityChange(m,1000);
    }
    c.guns=2;PhysicalReload dual(c,{51,49},20);
    CHECK(dual.Magazine(1)->capacity==50&&dual.Magazine(2)->capacity==48);
    CHECK(dual.Loaded(0)==51&&dual.Loaded(1)==49&&dual.State().budget==120&&dual.Valid());
    auto reject=[](PhysicalReloadConfig config,std::array<unsigned,2> loaded,unsigned reserve){
        bool rejected=false;
        try{PhysicalReload m(config,loaded,reserve);}
        catch(const std::invalid_argument&){rejected=true;}
        CHECK(rejected);
    };
    reject(c,{1002,0},0);reject(c,{0,1002},0);
    reject(c,{std::numeric_limits<unsigned>::max(),0},0);
    c.guns=1;reject(c,{0,1},0);
    c.capacity=0;reject(c,{0,0},0);
    c.capacity=1001;reject(c,{0,0},0);
    c.capacity=40;c.maximumRounds=50;reject(c,{51,0},0);reject(c,{50,0},1);
    PhysicalReload exactBudget(c,{50,0},0);
    CHECK(exactBudget.Loaded(0)==50&&exactBudget.Magazine(1)->capacity==49&&exactBudget.Valid());
}

void CapacityPlaceTests(){
    for(auto place:{MagazinePlace::Weapon,MagazinePlace::Hand,MagazinePlace::Belt,
                    MagazinePlace::Stored,MagazinePlace::Discarded}){
        PhysicalReloadConfig c;c.capacity=40;Driver d({41,0},0,c);
        if(place==MagazinePlace::Hand)d.Do(PhysicalAction::EjectToHand);
        if(place==MagazinePlace::Stored||place==MagazinePlace::Belt)d.Do(PhysicalAction::EjectToStorage);
        if(place==MagazinePlace::Belt)d.Do(PhysicalAction::PresentAtBelt);
        if(place==MagazinePlace::Discarded)d.Do(PhysicalAction::EjectToGround);
        CHECK(d.m.Magazine(1)->place==place);
        for(unsigned capacity:{44u,48u,50u,40u,1u,1000u,1000u,0u,1001u,
                               std::numeric_limits<unsigned>::max()}){
            CheckCapacityChange(d.m,capacity);
            CHECK(d.m.Magazine(1)->rounds==40&&d.m.Magazine(1)->capacity==40);
            CHECK(d.m.State().guns[0].chamber&&d.m.State().budget==41);
        }
    }
    // Valid() must use each identity's limit, including discarded magazines.
    Driver d({41,0},0,[]{PhysicalReloadConfig c;c.capacity=40;return c;}());
    d.Do(PhysicalAction::EjectToGround);CheckCapacityChange(d.m,1);
    auto& mag=const_cast<PhysicalMagazine&>(*d.m.Magazine(1));
    mag.capacity=0;CHECK(!d.m.Valid());
    mag.capacity=1001;CHECK(!d.m.Valid());
    mag.capacity=39;CHECK(!d.m.Valid());
    mag.capacity=40;CHECK(d.m.Valid());
}

void CapacitySupplyTests(){
    PhysicalReloadConfig c;c.capacity=40;
    Driver d({11,0},200,c);CheckCapacityChange(d.m,44);
    d.Do(PhysicalAction::EjectToStorage);d.Do(PhysicalAction::PresentAtBelt);
    const auto supplied=d.m.At(MagazinePlace::Belt,0)->id;
    CHECK(supplied!=1&&d.m.Magazine(supplied)->rounds==44&&d.m.Magazine(supplied)->capacity==44);
    CHECK(d.m.Magazine(1)->rounds==10&&d.m.Magazine(1)->capacity==40&&d.m.State().reserve==156);
    CheckCapacityChange(d.m,40);
    d.Do(PhysicalAction::GrabFromBelt);d.Do(PhysicalAction::SeatFromHand);
    CHECK(d.m.Loaded(0)==45); // A retained 44-round magazine plus its existing chamber.
    d.Do(PhysicalAction::EjectToStorage);d.Do(PhysicalAction::PresentAtBelt);
    CHECK(d.m.At(MagazinePlace::Belt,0)->id==supplied&&d.m.State().reserve==156);
    d.Do(PhysicalAction::GrabFromBelt);d.Do(PhysicalAction::DropFromHand);
    CheckCapacityChange(d.m,50);d.Do(PhysicalAction::PresentAtBelt);
    CHECK(d.m.At(MagazinePlace::Belt,0)->capacity==50&&d.m.At(MagazinePlace::Belt,0)->rounds==50);
    CHECK(d.m.State().reserve==106&&d.m.DiscardedRounds()==44);
    CHECK(d.m.Magazine(supplied)->capacity==44&&d.m.Magazine(1)->capacity==40);
    d.Do(PhysicalAction::GrabFromBelt);d.Do(PhysicalAction::DropFromHand);
    CheckCapacityChange(d.m,40);d.Do(PhysicalAction::PresentAtBelt);
    CHECK(d.m.At(MagazinePlace::Belt,0)->capacity==40&&d.m.At(MagazinePlace::Belt,0)->rounds==40);
    CHECK(d.m.State().reserve==66&&d.m.DiscardedRounds()==94);


    // A full identity table still offers a retained partial, without relabeling it.
    PhysicalReloadConfig limited=c;limited.maximumMagazines=1;
    Driver retained({11,0},200,limited);retained.Do(PhysicalAction::EjectToStorage);
    CheckCapacityChange(retained.m,50);retained.Do(PhysicalAction::PresentAtBelt);
    CHECK(retained.m.At(MagazinePlace::Belt,0)->id==1&&retained.m.Magazine(1)->capacity==40);
    CHECK(retained.m.Magazine(1)->rounds==10&&retained.m.State().reserve==200);

    // Limited supply creates a partially filled magazine with the full current limit.
    Driver partial({0,0},7,c);partial.Do(PhysicalAction::EjectToHand);
    CheckCapacityChange(partial.m,48);partial.Do(PhysicalAction::PresentAtBelt);
    CHECK(partial.m.At(MagazinePlace::Belt,0)->rounds==7&&partial.m.At(MagazinePlace::Belt,0)->capacity==48);
    CHECK(partial.m.At(MagazinePlace::Hand,0)->capacity==40&&partial.m.State().reserve==0);
    CheckCapacityChange(partial.m,50);partial.Do(PhysicalAction::DropFromHand);
    partial.Do(PhysicalAction::GrabFromBelt);partial.Do(PhysicalAction::StowFromHand);
    partial.Do(PhysicalAction::PresentAtBelt);
    CHECK(partial.m.At(MagazinePlace::Belt,0)->capacity==48&&partial.m.At(MagazinePlace::Belt,0)->rounds==7);
}

void CapacityRefillTests(){
    PhysicalReloadConfig c;c.capacity=40;c.maximumMagazines=1;
    Driver d({1,0},120,c);CheckCapacityChange(d.m,50);
    CHECK(d.m.Magazine(1)->rounds==0&&d.m.Magazine(1)->capacity==40);
    d.Do(PhysicalAction::EjectToStorage);
    CHECK(d.m.Magazine(1)->capacity==40);
    d.Do(PhysicalAction::PresentAtBelt);
    CHECK(d.m.At(MagazinePlace::Belt,0)->id==1&&d.m.Magazine(1)->capacity==50);
    CHECK(d.m.Magazine(1)->rounds==50&&d.m.State().reserve==70);
    d.Do(PhysicalAction::GrabFromBelt);d.Do(PhysicalAction::SeatFromHand);
    CHECK(d.m.Loaded(0)==51);
    for(unsigned i=0;i<51;++i){d.Do(PhysicalAction::Fire);d.now+=c.fireMs;}
    CHECK(d.m.Magazine(1)->rounds==0&&d.m.State().guns[0].slideLocked);
    CheckCapacityChange(d.m,44);d.Do(PhysicalAction::EjectToStorage);
    CHECK(d.m.Magazine(1)->capacity==50);
    d.Do(PhysicalAction::PresentAtBelt);
    CHECK(d.m.At(MagazinePlace::Belt,0)->id==1&&d.m.Magazine(1)->capacity==44);
    CHECK(d.m.Magazine(1)->rounds==44&&d.m.State().reserve==26);
    d.Do(PhysicalAction::SeatFromBelt);d.Do(PhysicalAction::ClearBelt);d.Do(PhysicalAction::ReleaseSlide);
    for(unsigned i=0;i<44;++i){d.Do(PhysicalAction::Fire);d.now+=c.fireMs;}
    CheckCapacityChange(d.m,40);d.Do(PhysicalAction::EjectToStorage);d.Do(PhysicalAction::PresentAtBelt);
    CHECK(d.m.Magazine(1)->capacity==40&&d.m.Magazine(1)->rounds==26&&d.m.State().reserve==0);
    CHECK(d.m.State().magazines.size()==1&&d.m.State().spent==95&&d.m.State().budget==121);
}

void CapacityMechanismTests(){
    PhysicalReloadConfig c;c.capacity=40;c.guns=2;Driver d({5,1},100,c);
    d.Do(PhysicalAction::Fire,0);d.Do(PhysicalAction::Fire,1);
    CHECK(d.m.State().guns[1].slideLocked&&d.m.State().guns[1].feedArmed);
    CheckCapacityChange(d.m,44);
    d.Do(PhysicalAction::EjectToStorage,1);d.Do(PhysicalAction::PresentAtBelt,1,1);
    d.Do(PhysicalAction::SeatFromBelt,1,1);CheckCapacityChange(d.m,48);
    CHECK(d.Try(PhysicalAction::Fire,0)==PhysicalResult::Ineligible);
    CHECK(d.Try(PhysicalAction::Fire,1)==PhysicalResult::Ineligible);
    d.Do(PhysicalAction::MoveAction,0,0,.7f);CHECK(d.m.State().ejected==1);
    CheckCapacityChange(d.m,40);
    d.Do(PhysicalAction::MoveAction,0,0,1);CHECK(d.m.State().guns[0].feedArmed);
    CheckCapacityChange(d.m,50);CheckCapacityChange(d.m,0);CheckCapacityChange(d.m,1001);
    d.Do(PhysicalAction::MoveAction,0,0,0);
    d.now=199;CHECK(d.Try(PhysicalAction::Fire)==PhysicalResult::Ineligible);
    d.now=200;d.Do(PhysicalAction::Fire);
    CHECK(d.Try(PhysicalAction::Fire,1)==PhysicalResult::Ineligible);
    d.Do(PhysicalAction::ClearBelt,1);d.Do(PhysicalAction::Fire,1);
}

void CapacityRequestBoundaryTests(){
    PhysicalReload m({}, {3,0},20,7);
    CHECK(m.Request({7,100,0,0,PhysicalAction::Fire,0},500)==PhysicalResult::Completed);
    CheckCapacityChange(m,50);
    CHECK(m.Request({7,100,0,0,PhysicalAction::Fire,0},800)==PhysicalResult::Stale);
    CHECK(m.Request({7,101,0,0,PhysicalAction::Fire,0},499)==PhysicalResult::BadClock);
    CHECK(m.Request({7,101,0,0,PhysicalAction::Fire,0},600)==PhysicalResult::Ineligible);
    CheckCapacityChange(m,0);
    CHECK(m.Request({7,102,0,0,PhysicalAction::Fire,0},700)==PhysicalResult::Completed);
    CheckCapacityChange(m,50);m.Suspend();CheckCapacityChange(m,44);
    CHECK(m.Request({7,103,0,0,PhysicalAction::Fire,0},900)==PhysicalResult::Inactive);
    CHECK(m.Activate(8));
    CHECK(m.Request({8,1,0,0,PhysicalAction::Fire,0},699)==PhysicalResult::BadClock);
    CHECK(m.Request({8,1,0,0,PhysicalAction::Fire,0},900)==PhysicalResult::Completed);
    CHECK(m.State().spent==3&&m.Valid());
}

int main(){try{
    CapacityBindingTests();CapacityPlaceTests();CapacitySupplyTests();CapacityRefillTests();
    CapacityMechanismTests();CapacityRequestBoundaryTests();
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
    {PhysicalReloadConfig c;c.guns=2;Driver d({15,15},100,c);std::mt19937 random(76234);for(unsigned i=0;i<30000;++i){d.now+=random()%300;d.Try(PhysicalAction(random()%13),random()%2,random()%2,float(random()%101)/100);if(random()%71==0){d.m.Suspend();CHECK(d.m.Activate(++d.epoch));}if(random()%7==0){const unsigned capacities[]={0,1,15,40,44,48,50,1000,1001};CheckCapacityChange(d.m,capacities[random()%9]);}CHECK(d.m.AvailableRounds()+d.m.DiscardedRounds()+d.m.State().spent==130);}}
    std::cout<<"Physical magazines, tactical/partial actions, independent dual chambers, shared belt supply, slide assist, replay/lifecycle, per-magazine capacity updates and 30000 randomized conservation actions passed\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
