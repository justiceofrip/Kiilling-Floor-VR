#pragma once
#include <cstdint>
#include <optional>
namespace kfvr::gameplay {
// This is authoritative, engine-independent logic. Client requests never carry rounds.
enum class Feed { Detachable, Tube };
enum class Cycle { SelfLoading, Manual };
enum class AmmoAction { RemoveMagazine, StowMagazine, PrepareMagazine, InsertMagazine, Charge, InsertShell, Fire, DiscardMagazine };
struct AmmoConfig {
    Feed feed=Feed::Detachable;
    Cycle cycle=Cycle::SelfLoading;
    std::uint32_t capacity=10, maximumRounds=1000;
    std::uint64_t removeMs=150, prepareMs=150, insertMs=250, chargeMs=250, shellMs=300, fireMs=200;
};
struct AmmoState {
    std::uint32_t magazine=0,reserve=0,handMagazine=0;
    bool inserted=true,chamber=false,handOccupied=false;
    std::uint64_t spent=0,discarded=0,budget=0,revision=0;
};
struct AmmoRequest { std::uint32_t epoch=0,sequence=0; AmmoAction action=AmmoAction::Fire; };
enum class AmmoResult { Accepted, Completed, Stale, Inactive, Busy, Ineligible, BadClock };
class AmmoMachine {
public:
    AmmoMachine(AmmoConfig config,std::uint32_t loaded,std::uint32_t reserve,std::uint32_t epoch=1);
    AmmoResult Request(AmmoRequest request,std::uint64_t serverMilliseconds);
    void Advance(std::uint64_t serverMilliseconds);
    // Switching/death/drop cancel incomplete gestures, retaining completed physical state.
    void Suspend();
    bool Activate(std::uint32_t newEpoch);
    const AmmoState& State() const { return state_; }
    bool Pending() const { return pending_.has_value(); }
    bool Valid(const AmmoState&) const;
    // Trusted server snapshot only; never expose as a client RPC. Cancels prediction.
    bool Correct(const AmmoState&,std::uint32_t epoch,std::uint64_t serverMilliseconds);
    std::uint32_t KFLoaded() const { return state_.magazine+unsigned(state_.chamber); }
    std::uint64_t KFTotal() const; // Includes rounds in weapon, reserve and retained hand mag.
private:
    struct PendingAction { AmmoAction action;std::uint64_t due; };
    AmmoConfig config_;AmmoState state_;std::optional<PendingAction> pending_;
    std::uint32_t epoch_=1,lastSequence_=0;
    std::uint64_t clock_=0,nextFire_=0;
    bool active_=true,hasSequence_=false;
    bool Eligible(AmmoAction) const;
    std::uint64_t Duration(AmmoAction) const;
    void Commit(AmmoAction);
};
}
