#pragma once
#include "kfvr/gameplay/PhysicalReload.h"
#include "kfvr/interaction/ReloadGestures.h"
#include <array>
namespace kfvr::interaction {
struct BeltHandSample {
    bool tracked=false,releaseMagazine=false;
    math::Pose barrel{},magwell{},beltMagazine{};
};
struct BeltReloadFrame : GestureSample {std::array<BeltHandSample,2> guns{};};
struct BeltReloadFeedback {
    bool ejected=false,presented=false,seated=false,cleared=false;
    float insertion=0;
};
// Coordinates validated local geometry with ONE authoritative command sequence
// and ammo supply. Feedback is an event for presentation/haptics, never ammo.
// The two gun/slot indices correspond to left and right, respectively.
class BeltReloadSession {
public:
    BeltReloadSession(gameplay::PhysicalReloadConfig,std::array<unsigned,2> loaded,
        unsigned reserve,std::uint64_t owner,std::uint32_t epoch=1,InsertionProfile={});
    std::array<BeltReloadFeedback,2> Update(const BeltReloadFrame&,std::uint64_t serverMs);
    // Other trusted weapon operations (fire, single-hand/action gestures) use
    // this same sequencer. Do not expose it directly as a client RPC.
    gameplay::PhysicalResult Command(gameplay::PhysicalAction,unsigned gun,unsigned slot,
        float travel,std::uint64_t serverMs);
    void Suspend() noexcept;
    bool Activate(std::uint64_t owner,std::uint32_t newEpoch) noexcept;
    const gameplay::PhysicalReload& Ammo() const noexcept {return ammo_;}
private:
    gameplay::PhysicalReload ammo_;std::array<InsertionGesture,2> gates_;
    InsertionProfile insertion_;std::uint64_t owner_=0;std::uint32_t epoch_=1,sequence_=0;
    GestureSample previous_{};bool active_=true,observed_=false;
    std::array<bool,2> armed_{},pressed_{},handObserved_{};
    void ResetGestures() noexcept;
};
}
