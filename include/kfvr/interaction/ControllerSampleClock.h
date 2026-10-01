#pragma once
#include <cstdint>
namespace kfvr::interaction {
enum class ControllerClockFault {
    None,InvalidSequence,InvalidTimestamp,SequenceRollback,TimestampChanged,
    TimestampStalled,TimestampRollback,SourceJump,EpochRebase
};
struct ControllerClockSample {
    float seconds=0;
    bool valid=false,fresh=false,neutralReset=false;
    ControllerClockFault fault=ControllerClockFault::None;
};
// Converts controller pose time, not game Tick time, into bounded script floats.
// Consume only decoded, unexpired input. QPC expiry remains the caller's policy.
// The caller MUST suppress tracked input/reset pending gestures on neutralReset,
// and emit/reset neutral when explicitly calling Reset for a new host/reference.
// Exact mailbox repeats retain the clock. A new sequence with an unchanged
// timestamp is conservatively invalid; it cannot manufacture physical travel.
// Faulting samples stay invalid on repeated reads until a later sequence has a
// usable advancing source stamp. A rollback never lowers the sequence watermark.
class ControllerSampleClock {
public:
    // Gesture gaps (currently 150 ms) remain the gesture gate's responsibility.
    // A larger source step loses continuity outright. Rebase before float time
    // reaches 600 s (resolution remains below 61 us; native sessions cap at 180 s).
    static constexpr std::int64_t MaximumStepNs=1000000000;
    static constexpr std::int64_t MaximumElapsedNs=600000000000;
    void Reset() noexcept {*this=ControllerSampleClock{};}
    ControllerClockSample Update(std::uint64_t sequence,std::int64_t predictedNs) noexcept {
        if(!sequence)return Reject(ControllerClockFault::InvalidSequence);
        if(sequence<sequence_)return Reject(ControllerClockFault::SequenceRollback);
        if(sequence==sequence_){
            if(predictedNs!=stampNs_)return Reject(ControllerClockFault::TimestampChanged);
            return Result();
        }
        sequence_=sequence;
        if(predictedNs<=0){
            stampNs_=predictedNs;epochNs_=0;seconds_=0;
            return Reject(ControllerClockFault::InvalidTimestamp);
        }
        if(!epochNs_){epochNs_=stampNs_=predictedNs;seconds_=0;return Accept();}
        if(predictedNs<=stampNs_)
            return Rebase(predictedNs,predictedNs==stampNs_?ControllerClockFault::TimestampStalled:ControllerClockFault::TimestampRollback);
        // Both stamps are positive and ordered before subtraction, avoiding
        // signed overflow even near INT64_MAX. Subtract before float conversion.
        if(predictedNs-stampNs_>MaximumStepNs)return Rebase(predictedNs,ControllerClockFault::SourceJump);
        if(predictedNs-epochNs_>=MaximumElapsedNs)return Rebase(predictedNs,ControllerClockFault::EpochRebase);
        stampNs_=predictedNs;
        seconds_=static_cast<float>(static_cast<double>(predictedNs-epochNs_)*1e-9);
        // Sub-float-resolution increments may retain seconds despite fresh=true;
        // consumers must also require increasing time before advancing travel.
        return Accept();
    }
private:
    std::uint64_t sequence_=0;
    std::int64_t epochNs_=0,stampNs_=0;
    float seconds_=0;
    bool valid_=false;
    ControllerClockFault fault_=ControllerClockFault::None;
    ControllerClockSample Result(bool fresh=false) const noexcept {
        return {seconds_,valid_,fresh,!valid_,fault_};
    }
    ControllerClockSample Accept() noexcept {valid_=true;fault_=ControllerClockFault::None;return Result(true);}
    ControllerClockSample Reject(ControllerClockFault fault) noexcept {valid_=false;fault_=fault;return Result();}
    ControllerClockSample Rebase(std::int64_t stamp,ControllerClockFault fault) noexcept {
        epochNs_=stampNs_=stamp;seconds_=0;return Reject(fault);
    }
};
}
