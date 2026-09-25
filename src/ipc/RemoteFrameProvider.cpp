#include "kfvr/ipc/RemoteFrameProvider.h"
namespace kfvr::ipc {
void RemoteFrameProducer::Close()noexcept {
    channel_.Close();if(step_==Step::Publishing||step_==Step::Waiting)provider_.PairConsumed(ticket_,false);step_=Step::Idle;
}
void RemoteFrameProducer::Pump()noexcept {
    if(step_==Step::Waiting){const auto outcome=channel_.PollOutcome();if(outcome==Outcome::Pending)return;
        provider_.PairConsumed(ticket_,outcome==Outcome::Consumed);step_=Step::Idle;}
    if(step_==Step::Skipping){if(channel_.Skip()==ChannelResult::Busy)return;step_=Step::Idle;}
    if(step_==Step::Idle){
        FrameRequest request{};if(channel_.TryTake(request)!=ChannelResult::Ok)return;
        runtime::PresentationRequirements requirements{};runtime::TrackingFrame tracking{};
        if(!Decode(request,requirements,tracking)||!provider_.TryGetPair(requirements,tracking,descriptor_,ticket_)){
            step_=Step::Skipping;if(channel_.Skip()!=ChannelResult::Busy)step_=Step::Idle;return;}
        step_=Step::Publishing;
    }
    if(step_==Step::Publishing){const auto result=channel_.Publish(descriptor_,ticket_);if(result==ChannelResult::Busy)return;
        if(result==ChannelResult::Ok)step_=Step::Waiting;
        else{provider_.PairConsumed(ticket_,false);step_=Step::Idle;}
    }
}
}
