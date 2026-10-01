#pragma once
#include <array>
#include <cstdint>
#include <vector>
namespace kfvr::gameplay {
// Detachable, closed-bolt self-loading weapons. One ledger owns both dual guns.
// This boundary is called by authoritative gesture/weapon adapters, not raw RPCs.
enum class MagazinePlace { Weapon, Hand, Belt, Stored, Discarded };
struct PhysicalMagazine {
    std::uint32_t id=0,rounds=0,capacity=0;
    MagazinePlace place=MagazinePlace::Stored;
    unsigned slot=0;
};
struct PhysicalGun {
    std::uint32_t magazine=0;
    bool chamber=false,slideLocked=false,beltLatched=false,feedArmed=false;
    float actionTravel=0;
    std::uint64_t nextFire=0;
};
struct PhysicalReloadConfig {
    unsigned guns=1,capacity=15,maximumRounds=1000,maximumMagazines=256;
    std::uint64_t fireMs=200;
    bool dualSlideAssist=true;
    float extractTravel=.65f,fullTravel=.9f,closedTravel=.1f;
};
struct PhysicalReloadState {
    std::array<PhysicalGun,2> guns{};
    std::vector<PhysicalMagazine> magazines;
    std::uint32_t reserve=0;
    std::uint64_t budget=0,spent=0,ejected=0,revision=0;
};
enum class PhysicalAction {
    EjectToHand, EjectToStorage, EjectToGround, PresentAtBelt, GrabFromBelt,
    StowFromHand, DropFromHand, SeatFromHand, SeatFromBelt, ClearBelt,
    MoveAction, ReleaseSlide, Fire
};
struct PhysicalRequest {
    std::uint32_t epoch=0,sequence=0;
    unsigned gun=0,slot=0; // slot is a hand or belt index, never a round count.
    PhysicalAction action=PhysicalAction::Fire;
    float travel=0; // Validated mechanical travel, normalized 0..1.
};
enum class PhysicalResult { Completed, Ineligible, Stale, Inactive, BadClock };
class PhysicalReload {
public:
    PhysicalReload(PhysicalReloadConfig,std::array<unsigned,2> loaded,unsigned reserve,std::uint32_t epoch=1);
    PhysicalResult Request(const PhysicalRequest&,std::uint64_t serverMs);
    // Trusted supply/profile update: retained magazine identities and contents are unchanged.
    // A valid unchanged capacity succeeds without creating a new revision.
    bool SetCapacity(unsigned newCapacity) noexcept;
    void Suspend() noexcept {active_=false;}
    bool Activate(std::uint32_t newEpoch) noexcept;
    const PhysicalReloadState& State() const noexcept {return state_;}
    const PhysicalReloadConfig& Config() const noexcept {return config_;}
    const PhysicalMagazine* At(MagazinePlace,unsigned slot) const noexcept;
    const PhysicalMagazine* Magazine(std::uint32_t id) const noexcept;
    std::uint64_t AvailableRounds() const noexcept;
    std::uint64_t DiscardedRounds() const noexcept;
    unsigned Loaded(unsigned gun) const noexcept;
    bool Valid() const noexcept;
private:
    PhysicalReloadConfig config_;PhysicalReloadState state_;
    std::uint32_t epoch_=1,lastSequence_=0,nextMagazine_=1;
    std::uint64_t clock_=0;
    bool active_=true,hasSequence_=false;
    PhysicalMagazine* Find(std::uint32_t) noexcept;
    bool Apply(const PhysicalRequest&);
    void Feed(PhysicalGun&);
};
}
