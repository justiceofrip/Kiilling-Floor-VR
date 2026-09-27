#include "kfvr/gameplay/PhysicalReload.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
namespace kfvr::gameplay {
PhysicalReload::PhysicalReload(PhysicalReloadConfig c,std::array<unsigned,2> loaded,unsigned reserve,std::uint32_t epoch):config_(c),epoch_(epoch){
    const auto budget=std::uint64_t(loaded[0])+loaded[1]+reserve;
    if(!epoch||c.guns<1||c.guns>2||!c.capacity||c.capacity>1000||!c.maximumRounds||c.maximumRounds>1000000||c.maximumMagazines<c.guns||c.maximumMagazines>4096||!c.fireMs||c.fireMs>60000||loaded[0]>c.capacity||loaded[1]>c.capacity||(c.guns==1&&loaded[1])||budget>c.maximumRounds||!std::isfinite(c.closedTravel)||!std::isfinite(c.extractTravel)||!std::isfinite(c.fullTravel)||c.closedTravel<0||c.closedTravel>=c.extractTravel||c.extractTravel>=c.fullTravel||c.fullTravel>1)throw std::invalid_argument("Invalid physical reload profile");
    state_.reserve=reserve;state_.budget=budget;state_.magazines.reserve(c.maximumMagazines);
    for(unsigned i=0;i<c.guns;++i){auto& g=state_.guns[i];g.magazine=nextMagazine_++;g.chamber=loaded[i]>0;state_.magazines.push_back({g.magazine,loaded[i]?loaded[i]-1:0,MagazinePlace::Weapon,i});}
}
const PhysicalMagazine* PhysicalReload::Magazine(std::uint32_t id)const noexcept {for(const auto& m:state_.magazines)if(m.id==id)return &m;return nullptr;}
PhysicalMagazine* PhysicalReload::Find(std::uint32_t id)noexcept {for(auto& m:state_.magazines)if(m.id==id)return &m;return nullptr;}
const PhysicalMagazine* PhysicalReload::At(MagazinePlace p,unsigned slot)const noexcept {for(const auto& m:state_.magazines)if(m.place==p&&m.slot==slot)return &m;return nullptr;}
std::uint64_t PhysicalReload::AvailableRounds()const noexcept {
    std::uint64_t n=state_.reserve;for(unsigned i=0;i<config_.guns;++i)n+=unsigned(state_.guns[i].chamber);
    for(const auto& m:state_.magazines)if(m.place!=MagazinePlace::Discarded)n+=m.rounds;return n;
}
std::uint64_t PhysicalReload::DiscardedRounds()const noexcept {std::uint64_t n=state_.ejected;for(const auto& m:state_.magazines)if(m.place==MagazinePlace::Discarded)n+=m.rounds;return n;}
unsigned PhysicalReload::Loaded(unsigned gun)const noexcept {if(gun>=config_.guns)return 0;const auto& g=state_.guns[gun];const auto* m=Magazine(g.magazine);return unsigned(g.chamber)+(m?m->rounds:0);}
bool PhysicalReload::Valid()const noexcept {
    if(state_.budget>config_.maximumRounds||state_.reserve>state_.budget||state_.spent>state_.budget||state_.ejected>state_.budget||state_.magazines.size()>config_.maximumMagazines)return false;
    std::array<std::array<bool,2>,3> occupied{};
    for(std::size_t i=0;i<state_.magazines.size();++i){const auto& m=state_.magazines[i];
        if(!m.id||m.rounds>config_.capacity||m.place<MagazinePlace::Weapon||m.place>MagazinePlace::Discarded)return false;
        for(std::size_t j=0;j<i;++j)if(state_.magazines[j].id==m.id)return false;
        if(m.place<=MagazinePlace::Belt){const unsigned p=unsigned(m.place);if(m.slot>=2||occupied[p][m.slot])return false;occupied[p][m.slot]=true;}
        if(m.place==MagazinePlace::Weapon&&(m.slot>=config_.guns||state_.guns[m.slot].magazine!=m.id))return false;
    }
    for(unsigned i=0;i<config_.guns;++i){const auto& g=state_.guns[i];const auto* m=Magazine(g.magazine);
        if((g.magazine&&(!m||m->place!=MagazinePlace::Weapon||m->slot!=i))||!std::isfinite(g.actionTravel)||g.actionTravel<0||g.actionTravel>1||(g.slideLocked&&(g.chamber||g.actionTravel<config_.fullTravel))||(g.beltLatched&&!g.magazine))return false;
    }
    return AvailableRounds()+DiscardedRounds()+state_.spent==state_.budget;
}
void PhysicalReload::Feed(PhysicalGun& g){if(!g.chamber){if(auto* m=Find(g.magazine);m&&m->rounds){--m->rounds;g.chamber=true;}}g.feedArmed=false;g.slideLocked=false;g.actionTravel=0;}
bool PhysicalReload::Apply(const PhysicalRequest& r){
    auto& g=state_.guns[r.gun];auto* mag=Find(g.magazine);
    switch(r.action){
    case PhysicalAction::EjectToHand:case PhysicalAction::EjectToStorage:case PhysicalAction::EjectToGround:
        if(!mag||g.beltLatched||(r.action==PhysicalAction::EjectToHand&&At(MagazinePlace::Hand,r.slot)))return false;
        mag->place=r.action==PhysicalAction::EjectToHand?MagazinePlace::Hand:r.action==PhysicalAction::EjectToStorage?MagazinePlace::Stored:MagazinePlace::Discarded;mag->slot=r.slot;g.magazine=0;return true;
    case PhysicalAction::PresentAtBelt:{
        if(g.beltLatched||At(MagazinePlace::Belt,r.slot))return false;
        PhysicalMagazine* best=nullptr;for(auto& m:state_.magazines)if(m.place==MagazinePlace::Stored&&m.rounds&&(!best||m.rounds>best->rounds))best=&m;
        const auto fresh=(std::min)(state_.reserve,config_.capacity);
        if(best&&best->rounds>=fresh){best->place=MagazinePlace::Belt;best->slot=r.slot;return true;}
        if(!fresh)return false;
        // Reuse an empty retained shell; never erase the identity/content of a partial magazine.
        PhysicalMagazine* shell=nullptr;for(auto& m:state_.magazines)if(m.place==MagazinePlace::Stored&&!m.rounds){shell=&m;break;}
        if(!shell){if(state_.magazines.size()>=config_.maximumMagazines){if(!best)return false;best->place=MagazinePlace::Belt;best->slot=r.slot;return true;}state_.magazines.push_back({nextMagazine_++,0,MagazinePlace::Stored,0});shell=&state_.magazines.back();}
        shell->rounds=fresh;shell->place=MagazinePlace::Belt;shell->slot=r.slot;state_.reserve-=fresh;return true;
    }
    case PhysicalAction::GrabFromBelt:{
        // Each physical hand accesses the matching belt slot; crossed grabs are an adapter extension.
        const auto* found=At(MagazinePlace::Belt,r.slot);if(!found||At(MagazinePlace::Hand,r.slot))return false;
        auto* m=Find(found->id);m->place=MagazinePlace::Hand;return true;
    }
    case PhysicalAction::StowFromHand:case PhysicalAction::DropFromHand:{
        const auto* found=At(MagazinePlace::Hand,r.slot);if(!found)return false;
        auto* m=Find(found->id);m->place=r.action==PhysicalAction::StowFromHand?MagazinePlace::Stored:MagazinePlace::Discarded;return true;
    }
    case PhysicalAction::SeatFromHand:case PhysicalAction::SeatFromBelt:{
        if(mag||g.beltLatched)return false;
        const bool belt=r.action==PhysicalAction::SeatFromBelt;
        const auto* found=At(belt?MagazinePlace::Belt:MagazinePlace::Hand,r.slot);if(!found)return false;
        auto* m=Find(found->id);m->place=MagazinePlace::Weapon;m->slot=r.gun;g.magazine=m->id;g.beltLatched=belt;
        if(belt&&config_.guns==2&&config_.dualSlideAssist&&g.slideLocked)Feed(g);
        return true;
    }
    case PhysicalAction::ClearBelt:if(!g.beltLatched)return false;g.beltLatched=false;return true;
    case PhysicalAction::MoveAction:{
        if(g.beltLatched||(g.slideLocked&&r.travel<config_.fullTravel))return false;
        const float before=g.actionTravel;g.actionTravel=r.travel;
        if(before<config_.extractTravel&&r.travel>=config_.extractTravel&&g.chamber){g.chamber=false;++state_.ejected;}
        if(r.travel>=config_.fullTravel){g.feedArmed=true;g.slideLocked=false;}
        if(r.travel<=config_.closedTravel){if(g.feedArmed)Feed(g);else g.slideLocked=false;}
        return true;
    }
    case PhysicalAction::ReleaseSlide:
        if(!g.slideLocked||g.beltLatched)return false;Feed(g);return true;
    case PhysicalAction::Fire:
        if(!g.chamber||g.beltLatched||g.slideLocked||g.actionTravel>config_.closedTravel||clock_<g.nextFire)return false;
        g.chamber=false;++state_.spent;g.nextFire=clock_+config_.fireMs;Feed(g);
        if(!g.chamber&&mag){g.slideLocked=true;g.feedArmed=true;g.actionTravel=1;}
        return true;
    }return false;
}
PhysicalResult PhysicalReload::Request(const PhysicalRequest& r,std::uint64_t now){
    if(now<clock_||now>std::numeric_limits<std::uint64_t>::max()-60000)return PhysicalResult::BadClock;
    if(!active_)return PhysicalResult::Inactive;
    const auto distance=r.sequence-lastSequence_;
    if(r.epoch!=epoch_||(hasSequence_&&(!distance||distance>=0x80000000u)))return PhysicalResult::Stale;
    hasSequence_=true;lastSequence_=r.sequence;clock_=now;
    if(r.gun>=config_.guns||r.slot>=2||r.action<PhysicalAction::EjectToHand||r.action>PhysicalAction::Fire||!std::isfinite(r.travel)||r.travel<0||r.travel>1||!Apply(r))return PhysicalResult::Ineligible;
    ++state_.revision;return PhysicalResult::Completed;
}
bool PhysicalReload::Activate(std::uint32_t epoch)noexcept {if(!epoch||epoch==epoch_)return false;epoch_=epoch;hasSequence_=false;active_=true;return true;}
}
