#include "kfvr/gameplay/AmmoMachine.h"
#include <algorithm>
#include <limits>
#include <stdexcept>
namespace kfvr::gameplay {
namespace { bool Newer(std::uint32_t a,std::uint32_t b){const auto d=a-b;return d && d<0x80000000u;} }
AmmoMachine::AmmoMachine(AmmoConfig c,std::uint32_t loaded,std::uint32_t reserve,std::uint32_t epoch):config_(c),epoch_(epoch){
    if((c.feed!=Feed::Detachable&&c.feed!=Feed::Tube)||(c.cycle!=Cycle::SelfLoading&&c.cycle!=Cycle::Manual)||!epoch||!c.capacity||c.capacity>1000||c.maximumRounds>1000000||loaded>c.capacity||std::uint64_t(loaded)+reserve>c.maximumRounds||!c.fireMs||c.fireMs>60000||c.removeMs>60000||c.prepareMs>60000||c.insertMs>60000||c.chargeMs>60000||c.shellMs>60000)throw std::invalid_argument("Invalid ammo profile");
    state_.chamber=loaded>0;state_.magazine=loaded?loaded-1:0;state_.reserve=reserve;state_.budget=std::uint64_t(loaded)+reserve;
}
std::uint64_t AmmoMachine::KFTotal()const{return std::uint64_t(state_.magazine)+state_.reserve+state_.handMagazine+unsigned(state_.chamber);}
bool AmmoMachine::Valid(const AmmoState& s)const{
    if(s.magazine>config_.capacity||s.handMagazine>config_.capacity||s.reserve>config_.maximumRounds||(!s.inserted&&s.magazine)||(!s.handOccupied&&s.handMagazine)||(config_.feed==Feed::Tube&&(!s.inserted||s.handOccupied))||s.budget>config_.maximumRounds||s.spent>s.budget||s.discarded>s.budget)return false;
    return std::uint64_t(s.magazine)+s.reserve+s.handMagazine+unsigned(s.chamber)+s.spent+s.discarded==s.budget;
}
bool AmmoMachine::Eligible(AmmoAction a)const{
    switch(a){
    case AmmoAction::RemoveMagazine:return config_.feed==Feed::Detachable&&state_.inserted&&!state_.handOccupied;
    case AmmoAction::StowMagazine:case AmmoAction::DiscardMagazine:return state_.handOccupied;
    case AmmoAction::PrepareMagazine:return config_.feed==Feed::Detachable&&!state_.handOccupied&&state_.reserve>0;
    case AmmoAction::InsertMagazine:return config_.feed==Feed::Detachable&&!state_.inserted&&state_.handOccupied;
    case AmmoAction::Charge:return state_.chamber||(state_.inserted&&state_.magazine>0);
    case AmmoAction::InsertShell:return config_.feed==Feed::Tube&&state_.magazine<config_.capacity&&state_.reserve>0;
    case AmmoAction::Fire:return state_.chamber;
    }return false;
}
std::uint64_t AmmoMachine::Duration(AmmoAction a)const{
    switch(a){case AmmoAction::RemoveMagazine:return config_.removeMs;case AmmoAction::PrepareMagazine:case AmmoAction::StowMagazine:case AmmoAction::DiscardMagazine:return config_.prepareMs;case AmmoAction::InsertMagazine:return config_.insertMs;case AmmoAction::Charge:return config_.chargeMs;case AmmoAction::InsertShell:return config_.shellMs;case AmmoAction::Fire:return 0;}return 0;
}
void AmmoMachine::Commit(AmmoAction a){
    switch(a){
    case AmmoAction::RemoveMagazine:state_.handMagazine=state_.magazine;state_.handOccupied=true;state_.magazine=0;state_.inserted=false;break;
    case AmmoAction::StowMagazine:state_.reserve+=state_.handMagazine;state_.handMagazine=0;state_.handOccupied=false;break;
    case AmmoAction::DiscardMagazine:state_.discarded+=state_.handMagazine;state_.handMagazine=0;state_.handOccupied=false;break;
    case AmmoAction::PrepareMagazine:state_.handMagazine=(std::min)(state_.reserve,config_.capacity);state_.reserve-=state_.handMagazine;state_.handOccupied=true;break;
    case AmmoAction::InsertMagazine:state_.magazine=state_.handMagazine;state_.handMagazine=0;state_.handOccupied=false;state_.inserted=true;break;
    case AmmoAction::Charge:state_.discarded+=unsigned(state_.chamber);state_.chamber=false;if(state_.inserted&&state_.magazine){--state_.magazine;state_.chamber=true;}break;
    case AmmoAction::InsertShell:--state_.reserve;++state_.magazine;break;
    case AmmoAction::Fire:++state_.spent;state_.chamber=false;if(config_.cycle==Cycle::SelfLoading&&state_.inserted&&state_.magazine){--state_.magazine;state_.chamber=true;}break;
    }++state_.revision;
}
void AmmoMachine::Advance(std::uint64_t now){
    if(now<clock_)return;clock_=now;
    if(pending_&&now>=pending_->due){auto a=pending_->action;pending_.reset();if(active_&&Eligible(a))Commit(a);}
}
AmmoResult AmmoMachine::Request(AmmoRequest r,std::uint64_t now){
    if(now<clock_||now>std::numeric_limits<std::uint64_t>::max()-60000)return AmmoResult::BadClock;
    if(!active_)return AmmoResult::Inactive;
    if(r.epoch!=epoch_||(hasSequence_&&!Newer(r.sequence,lastSequence_)))return AmmoResult::Stale;
    // Consume even rejected sequence numbers so delayed retries cannot fire later.
    lastSequence_=r.sequence;hasSequence_=true;Advance(now);
    if(r.action==AmmoAction::Fire){
        if(!Eligible(r.action)||now<nextFire_)return AmmoResult::Ineligible;
        if(pending_&&pending_->action==AmmoAction::Charge)return AmmoResult::Busy;
        pending_.reset();Commit(r.action);nextFire_=now+config_.fireMs;return AmmoResult::Completed;
    }
    if(pending_)return AmmoResult::Busy;
    if(!Eligible(r.action))return AmmoResult::Ineligible;
    pending_=PendingAction{r.action,now+Duration(r.action)};Advance(now);return Pending()?AmmoResult::Accepted:AmmoResult::Completed;
}
void AmmoMachine::Suspend(){active_=false;pending_.reset();}
bool AmmoMachine::Activate(std::uint32_t e){if(!e||e==epoch_)return false;epoch_=e;hasSequence_=false;pending_.reset();active_=true;return true;}
bool AmmoMachine::Correct(const AmmoState& s,std::uint32_t e,std::uint64_t now){
    if(e!=epoch_||now<clock_||s.revision<state_.revision||s.budget!=state_.budget||!Valid(s))return false;
    state_=s;pending_.reset();clock_=now;return true;
}
}
