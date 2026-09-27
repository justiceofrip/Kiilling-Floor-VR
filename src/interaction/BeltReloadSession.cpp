#include "kfvr/interaction/BeltReloadSession.h"
#include "kfvr/interaction/TrackingMath.h"
#include <cmath>
#include <stdexcept>
namespace kfvr::interaction {
using namespace gameplay;
namespace {
bool Down(const math::Pose& barrel){const auto view=math::MakeLhViewFromOpenXRPose(barrel);if(!view)return false;const auto world=InverseRigid(*view);return world&&world->values[2][1]<=-.65f;}
bool Beyond(const math::Pose& socket,const math::Pose& tip,float distance){const auto p=math::MakeRelativePose(socket,tip);return p&&std::hypot(p->position.x,p->position.y,p->position.z)>distance;}
}
BeltReloadSession::BeltReloadSession(PhysicalReloadConfig c,std::array<unsigned,2> loaded,unsigned reserve,std::uint64_t owner,std::uint32_t epoch,InsertionProfile p):ammo_(c,loaded,reserve,epoch),gates_{InsertionGesture(p),InsertionGesture(p)},insertion_(p),owner_(owner),epoch_(epoch){if(!owner)throw std::invalid_argument("Physical weapon owner required");}
void BeltReloadSession::ResetGestures()noexcept {for(auto& gate:gates_)gate.Reset();armed_={};pressed_={};handObserved_={};observed_=false;previous_={};}
void BeltReloadSession::Suspend()noexcept {active_=false;ammo_.Suspend();ResetGestures();}
bool BeltReloadSession::Activate(std::uint64_t owner,std::uint32_t epoch)noexcept {if(!owner||!ammo_.Activate(epoch))return false;owner_=owner;epoch_=epoch;sequence_=0;active_=true;ResetGestures();return true;}
PhysicalResult BeltReloadSession::Command(PhysicalAction a,unsigned gun,unsigned slot,float travel,std::uint64_t now){return ammo_.Request({epoch_,++sequence_,gun,slot,a,travel},now);}
std::array<BeltReloadFeedback,2> BeltReloadSession::Update(const BeltReloadFrame& s,std::uint64_t now){
    std::array<BeltReloadFeedback,2> out{};
    if(!active_||ammo_.Config().guns!=2||s.owner!=owner_||!s.space||!s.sequence||s.timeNs<=0||!s.focused||!s.tracked){for(auto& gate:gates_)gate.Reset();armed_={};pressed_={};handObserved_={};return out;}
    if(observed_&&s.space==previous_.space&&(s.sequence<=previous_.sequence||s.timeNs<=previous_.timeNs))return out;
    const bool continuous=observed_&&s.space==previous_.space&&s.timeNs>previous_.timeNs&&s.timeNs-previous_.timeNs<=150000000;
    if(!continuous){for(auto& gate:gates_)gate.Reset();armed_={};pressed_={};handObserved_={};}
    observed_=true;previous_=s;
    for(unsigned i=0;i<2;++i){const auto& hand=s.guns[i];auto& feedback=out[i];
        if(!hand.tracked||!math::MakeLhViewFromOpenXRPose(hand.barrel)||!math::MakeRelativePose(hand.magwell,hand.beltMagazine)){gates_[i].Reset();armed_[i]=pressed_[i]=handObserved_[i]=false;continue;}
        if(!handObserved_[i]){gates_[i].Reset();armed_[i]=pressed_[i]=false;handObserved_[i]=true;}
        if(!hand.releaseMagazine)armed_[i]=true;
        const bool release=armed_[i]&&hand.releaseMagazine&&!pressed_[i]&&Down(hand.barrel);pressed_[i]=hand.releaseMagazine;
        if(release){
            feedback.ejected=Command(PhysicalAction::EjectToStorage,i,i,0,now)==PhysicalResult::Completed;
            if(!ammo_.State().guns[i].magazine&&!ammo_.At(MagazinePlace::Belt,i))feedback.presented=Command(PhysicalAction::PresentAtBelt,i,i,0,now)==PhysicalResult::Completed;
        }
        if(ammo_.State().guns[i].beltLatched){
            if(Beyond(hand.magwell,hand.beltMagazine,insertion_.clearance)){feedback.cleared=Command(PhysicalAction::ClearBelt,i,i,0,now)==PhysicalResult::Completed;if(feedback.cleared)gates_[i].Reset();}
            continue;
        }
        if(ammo_.State().guns[i].magazine||!ammo_.At(MagazinePlace::Belt,i)){gates_[i].Reset();continue;}
        InsertionSample sample;static_cast<GestureSample&>(sample)=s;sample.socket=hand.magwell;sample.magazineTip=hand.beltMagazine;
        const auto insertion=gates_[i].Update(sample);feedback.insertion=insertion.progress;
        if(insertion.seated){feedback.seated=Command(PhysicalAction::SeatFromBelt,i,i,0,now)==PhysicalResult::Completed;gates_[i].Reset();}
    }
    return out;
}
}
