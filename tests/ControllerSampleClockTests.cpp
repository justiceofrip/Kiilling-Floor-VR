#include "Test.h"
#include "kfvr/interaction/ControllerSampleClock.h"
#include <array>
#include <limits>
using namespace kfvr::interaction;
int main(){
    constexpr std::int64_t Base=8000000000000000000LL,Step=20000000;
    {
        // Absolute nanoseconds cannot pass through a float before subtraction.
        ControllerSampleClock clock;
        const auto first=clock.Update(1,Base);
        CHECK(first.valid&&first.fresh&&!first.neutralReset&&first.seconds==0);
        const auto tiny=clock.Update(2,Base+1);
        CHECK(tiny.valid&&tiny.fresh&&Near(tiny.seconds,1e-9f,1e-12f));
        const auto next=clock.Update(3,Base+Step);
        CHECK(next.valid&&Near(next.seconds,.02f,1e-7f));
        for(unsigned i=0;i<100;++i){
            const auto same=clock.Update(3,Base+Step);
            CHECK(same.valid&&!same.fresh&&!same.neutralReset&&same.seconds==next.seconds);
        }
        const auto final=clock.Update(4,Base+2*Step);
        CHECK(final.valid&&Near(final.seconds-next.seconds,.02f,1e-7f));
    }
    {
        ControllerSampleClock clock;
        const auto base=std::numeric_limits<std::int64_t>::max()-Step;
        CHECK(clock.Update(1,base).valid);
        const auto end=clock.Update(2,std::numeric_limits<std::int64_t>::max());
        CHECK(end.valid&&Near(end.seconds,.02f,1e-7f));
    }
    {
        // A producer cannot create motion by publishing the same source instant
        // under new mailbox IDs; repeats of the fault cannot revive the gesture.
        ControllerSampleClock clock;clock.Update(10,Base);
        auto sample=clock.Update(11,Base);
        CHECK(!sample.valid&&!sample.fresh&&sample.neutralReset&&sample.fault==ControllerClockFault::TimestampStalled);
        sample=clock.Update(11,Base);
        CHECK(!sample.valid&&sample.neutralReset&&sample.seconds==0);
        sample=clock.Update(12,Base+Step);
        CHECK(sample.valid&&sample.fresh&&!sample.neutralReset&&Near(sample.seconds,.02f));
    }
    {
        // Mutating an existing mailbox ID is never a fresh sample, even if its
        // timestamp moves forward. An original-payload repeat remains invalid.
        ControllerSampleClock clock;clock.Update(10,Base);
        auto sample=clock.Update(10,Base+Step);
        CHECK(!sample.valid&&sample.neutralReset&&sample.fault==ControllerClockFault::TimestampChanged);
        CHECK(!clock.Update(10,Base).valid);
        sample=clock.Update(11,Base+2*Step);
        CHECK(sample.valid&&Near(sample.seconds,.04f));
    }
    {
        ControllerSampleClock clock;clock.Update(10,Base);clock.Update(11,Base+2*Step);
        auto sample=clock.Update(12,Base+Step);
        CHECK(!sample.valid&&sample.neutralReset&&sample.fault==ControllerClockFault::TimestampRollback);
        CHECK(!clock.Update(12,Base+Step).valid);
        sample=clock.Update(13,Base+2*Step);
        CHECK(sample.valid&&Near(sample.seconds,.02f));
        // Replayed IDs cannot lower the sequence watermark or rebase time.
        sample=clock.Update(9,Base+3*Step);
        CHECK(!sample.valid&&sample.neutralReset&&sample.fault==ControllerClockFault::SequenceRollback);
        CHECK(!clock.Update(10,Base+3*Step).valid);
        CHECK(!clock.Update(13,Base+2*Step).valid);
        sample=clock.Update(14,Base+3*Step);
        CHECK(sample.valid&&Near(sample.seconds,.04f));
    }
    {
        for(const auto bad:std::array<std::int64_t,3>{0,-1,std::numeric_limits<std::int64_t>::min()}){
            ControllerSampleClock clock;clock.Update(10,Base);
            auto sample=clock.Update(11,bad);
            CHECK(!sample.valid&&sample.neutralReset&&sample.fault==ControllerClockFault::InvalidTimestamp);
            CHECK(!clock.Update(11,bad).valid);
            CHECK(!clock.Update(11,Base+Step).valid);
            sample=clock.Update(12,Base+Step);
            CHECK(sample.valid&&sample.fresh&&sample.seconds==0);
        }
        ControllerSampleClock clock;clock.Update(10,Base);
        const auto missing=clock.Update(0,Base+Step);
        CHECK(!missing.valid&&missing.neutralReset&&missing.fault==ControllerClockFault::InvalidSequence);
        CHECK(!clock.Update(10,Base).valid);
        CHECK(clock.Update(11,Base+Step).valid);
    }
    {
        ControllerSampleClock clock;clock.Update(1,Base);
        auto sample=clock.Update(2,Base+ControllerSampleClock::MaximumStepNs);
        CHECK(sample.valid&&Near(sample.seconds,1));
        const auto jump=Base+2*ControllerSampleClock::MaximumStepNs+1;
        sample=clock.Update(3,jump);
        CHECK(!sample.valid&&sample.neutralReset&&sample.fault==ControllerClockFault::SourceJump);
        CHECK(!clock.Update(3,jump).valid);
        sample=clock.Update(4,jump+Step);
        CHECK(sample.valid&&Near(sample.seconds,.02f));
        // Preserve ordinary long gaps for the insertion gate's own 150 ms check.
        sample=clock.Update(5,jump+Step+200000000);
        CHECK(sample.valid&&Near(sample.seconds,.22f));
    }
    {
        ControllerSampleClock clock;clock.Update(1,Base);
        for(std::int64_t second=1;second<600;++second){
            const auto sample=clock.Update(static_cast<std::uint64_t>(second+1),Base+second*1000000000);
            CHECK(sample.valid&&sample.fresh&&sample.seconds==static_cast<float>(second));
        }
        const auto edge=Base+ControllerSampleClock::MaximumElapsedNs;
        auto sample=clock.Update(601,edge);
        CHECK(!sample.valid&&sample.neutralReset&&sample.fault==ControllerClockFault::EpochRebase);
        CHECK(!clock.Update(601,edge).valid);
        sample=clock.Update(602,edge+Step);
        CHECK(sample.valid&&Near(sample.seconds,.02f));
        // Reset is only for a caller that has already reset/suppressed gestures.
        clock.Reset();sample=clock.Update(1,100);
        CHECK(sample.valid&&sample.fresh&&sample.seconds==0);
    }
    {
        // Sequence values are not converted through a signed script int here.
        ControllerSampleClock clock;const auto last=std::numeric_limits<std::uint64_t>::max();
        CHECK(clock.Update(last-1,Base).valid);
        CHECK(clock.Update(last,Base+Step).valid);
        const auto wrap=clock.Update(1,Base+2*Step);
        CHECK(!wrap.valid&&wrap.neutralReset&&wrap.fault==ControllerClockFault::SequenceRollback);
    }
    {
        // Identical physical 3 cm / 20 ms insertion segments retain their timing
        // through normal speed, ZED time and its changing recovery dilation.
        // The speed-envelope comparison exercises the script gate's contract;
        // this is not a substitute for the live slow-motion integration trial.
        constexpr std::array<std::array<float,3>,3> dilations{{{1.1f,1.1f,1.1f},{.22f,.22f,.22f},{.22f,.55f,1.1f}}};
        std::array<float,4> normalTimes{};bool scaledClockRejected=false;
        for(unsigned trial=0;trial<dilations.size();++trial){
            ControllerSampleClock clock;clock.Update(1,Base);float previous=0,gameTime=0,previousGame=0;
            for(unsigned segment=0;segment<3;++segment){
                const auto sample=clock.Update(segment+2,Base+std::int64_t(segment+1)*Step);
                CHECK(sample.valid&&sample.fresh&&!sample.neutralReset);
                const float delta=sample.seconds-previous;
                CHECK(Near(delta,.02f,1e-7f)&&delta<=.15f&&.03f<=4*delta+.005f);
                if(!trial)normalTimes[segment+1]=sample.seconds;
                else CHECK(sample.seconds==normalTimes[segment+1]);
                gameTime+=.02f*dilations[trial][segment];
                if(.03f>4*(gameTime-previousGame)+.005f)scaledClockRejected=true;
                previous=sample.seconds;previousGame=gameTime;
                const auto duplicate=clock.Update(segment+2,Base+std::int64_t(segment+1)*Step);
                CHECK(duplicate.valid&&!duplicate.fresh&&duplicate.seconds==previous);
            }
        }
        CHECK(scaledClockRejected);
    }
    std::puts("Controller sample clock: source precision, duplicates, faults, rebase and dilation independence verified");
    return 0;
}
