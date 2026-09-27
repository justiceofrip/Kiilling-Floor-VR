#include "Test.h"
#include "kfvr/ipc/FrameProtocol.h"
#include <cstring>
#include <limits>
using namespace kfvr;
int main(){
    runtime::PresentationRequirements requirements{100,120,29,0x87654321,-3};runtime::TrackingFrame tracking{};
    tracking.generation=0x123456789abcdef0ULL;tracking.spaceGeneration=0x100000007ULL;tracking.predictedNs=10000000000;
    tracking.focused=tracking.headValid=true;tracking.referenceHead.position.y=1.6f;tracking.head=tracking.referenceHead;tracking.head.position.x=.2f;
    tracking.eyes={tracking.head,tracking.head};tracking.eyes[0].position.x-=.032f;tracking.eyes[1].position.x+=.032f;
    tracking.fov={math::FovTangents{-1.2f,.9f,1.1f,-1.f},math::FovTangents{-.9f,1.2f,1.1f,-1.f}};
    ipc::FrameRequest request{};CHECK(ipc::Encode(requirements,tracking,0x100000001ULL,1000,request));
    runtime::PresentationRequirements decoded{};runtime::TrackingFrame sample{};CHECK(ipc::Decode(request,decoded,sample));
    CHECK(decoded.adapterHigh==-3&&decoded.adapterLow==requirements.adapterLow&&sample.generation==tracking.generation&&sample.predictedNs==tracking.predictedNs);
    CHECK(sample.eyes[0].position.x==tracking.eyes[0].position.x&&sample.referenceHead.position.y==1.6f&&sample.fov[1].right==1.2f);
    auto corrupt=request;corrupt.bytes--;CHECK(!ipc::Decode(corrupt,decoded,sample));corrupt=request;corrupt.flags=1;CHECK(!ipc::Decode(corrupt,decoded,sample));
    corrupt=request;corrupt.referenceHead.values[6]=0;CHECK(!ipc::Decode(corrupt,decoded,sample));corrupt=request;corrupt.eyes[1].values[0]=std::numeric_limits<float>::quiet_NaN();CHECK(!ipc::Decode(corrupt,decoded,sample));
    corrupt=request;corrupt.fov[0][1]=corrupt.fov[0][0];CHECK(!ipc::Decode(corrupt,decoded,sample));corrupt=request;corrupt.reserved[1]=1;CHECK(!ipc::Decode(corrupt,decoded,sample));
    tracking.focused=false;CHECK(!ipc::Encode(requirements,tracking,1,1000,corrupt));tracking.focused=true;
    graphics::TextureDescriptor d{};d.width=requirements.width;d.height=requirements.height;d.format=requirements.format;d.adapterLow=requirements.adapterLow;d.adapterHigh=requirements.adapterHigh;d.resourceEpoch=4;d.session[0]=7;
    graphics::PairTicket ticket{};ticket.session=d.session;ticket.resourceEpoch=d.resourceEpoch;ticket.sequence=1;ticket.frameId=0x100000009ULL;
    ticket.trackingGeneration=tracking.generation;ticket.spaceGeneration=tracking.spaceGeneration;ticket.predictedNs=tracking.predictedNs;
    ipc::FrameSlot slot{};ipc::FrameRequest taken{};graphics::TextureDescriptor got{};graphics::PairTicket pair{};
    CHECK(ipc::Begin(slot,request,1));CHECK(!ipc::Begin(slot,request,1));CHECK(ipc::Take(slot,2,taken));CHECK(!ipc::Take(slot,3,taken));
    auto badTicket=ticket;--badTicket.predictedNs;CHECK(!ipc::Publish(slot,request.requestId,5,d,badTicket));CHECK(!ipc::Publish(slot,1,5,d,ticket));
    CHECK(ipc::Publish(slot,request.requestId,5,d,ticket));CHECK(!ipc::Publish(slot,request.requestId,6,d,ticket));
    CHECK(ipc::Deliver(slot,request.requestId,6,got,pair));CHECK(!ipc::Deliver(slot,request.requestId,6,got,pair));
    // A timeout cannot reclaim a texture that the GPU consumer already owns.
    ipc::Cancel(slot,request.requestId);CHECK(ipc::Reap(slot,request.requestId,2000)==ipc::Outcome::Pending);
    CHECK(!ipc::Acknowledge(slot,2,true));CHECK(ipc::Acknowledge(slot,request.requestId,true));CHECK(ipc::Reap(slot,request.requestId,2000)==ipc::Outcome::Consumed);CHECK(slot.phase==ipc::Phase::Idle);
    // Timeout before take; timeout during render; timeout after publish.
    CHECK(ipc::Begin(slot,request,1));CHECK(!ipc::Take(slot,1000,taken));CHECK(slot.phase==ipc::Phase::Idle);
    CHECK(ipc::Begin(slot,request,1));CHECK(ipc::Take(slot,2,taken));ipc::Cancel(slot,request.requestId);
    CHECK(slot.phase==ipc::Phase::Cancelled);CHECK(!ipc::Begin(slot,request,3));CHECK(!ipc::Publish(slot,request.requestId,4,d,ticket));CHECK(slot.phase==ipc::Phase::Idle);
    CHECK(ipc::Begin(slot,request,1));CHECK(ipc::Take(slot,2,taken));CHECK(ipc::Publish(slot,request.requestId,3,d,ticket));
    CHECK(!ipc::Deliver(slot,request.requestId,1000,got,pair));CHECK(ipc::Reap(slot,request.requestId,1000)==ipc::Outcome::Discarded);
    CHECK(ipc::Begin(slot,request,1));CHECK(ipc::Take(slot,2,taken));CHECK(ipc::Publish(slot,request.requestId,3,d,ticket));
    CHECK(ipc::Reap(slot,request.requestId,1000)==ipc::Outcome::Discarded);
    // Skip a frame without a GPU pair, then reject the previous frame's late write.
    CHECK(ipc::Begin(slot,request,1));CHECK(ipc::Take(slot,2,taken));ipc::Skip(slot,request.requestId);
    auto next=request;++next.requestId;CHECK(ipc::Begin(slot,next,3));CHECK(!ipc::Publish(slot,request.requestId,4,d,ticket));CHECK(slot.phase==ipc::Phase::Requested);
    slot.phase=ipc::Phase::Closed;CHECK(!ipc::Begin(slot,request,1));CHECK(ipc::Reap(slot,next.requestId,2)==ipc::Outcome::Discarded);
    // Queue the next request before the render thread reaps the prior pair.
    // Delayed feedback must neither release its textures early nor erase the
    // queued request. Exercise consumed/discarded pairs and both thread orders.
    for(bool consumed:{false,true})for(bool reapFirst:{false,true})for(bool expire:{false,true}){
        slot={};CHECK(ipc::Begin(slot,request,1));CHECK(ipc::Take(slot,2,taken));
        CHECK(ipc::Publish(slot,request.requestId,3,d,ticket));CHECK(ipc::Deliver(slot,request.requestId,4,got,pair));
        CHECK(ipc::Acknowledge(slot,request.requestId,consumed));
        const auto expected=consumed?ipc::Outcome::Consumed:ipc::Outcome::Discarded;
        if(reapFirst)CHECK(ipc::Reap(slot,request.requestId,5)==expected);
        next=request;++next.requestId;CHECK(ipc::Begin(slot,next,6));
        CHECK(!ipc::Acknowledge(slot,request.requestId,true));ipc::Cancel(slot,request.requestId);ipc::Skip(slot,request.requestId);
        CHECK(!ipc::Publish(slot,request.requestId,7,d,ticket));CHECK(slot.phase==ipc::Phase::Requested);
        if(!reapFirst){
            CHECK(!ipc::Take(slot,8,taken));CHECK(ipc::Reap(slot,next.requestId+1,8)==ipc::Outcome::Discarded);
            CHECK(slot.completedId==request.requestId);CHECK(ipc::Reap(slot,request.requestId,9)==expected);
        }
        CHECK(slot.phase==ipc::Phase::Requested&&slot.completedId==0);
        CHECK(ipc::Take(slot,expire?1000:10,taken)==!expire);
        if(!expire){
            CHECK(taken.requestId==next.requestId);CHECK(ipc::Publish(slot,next.requestId,11,d,ticket));
            CHECK(ipc::Deliver(slot,next.requestId,12,got,pair));CHECK(ipc::Acknowledge(slot,next.requestId,false));
            auto third=next;++third.requestId;CHECK(ipc::Begin(slot,third,13));ipc::Cancel(slot,third.requestId);
            CHECK(slot.phase==ipc::Phase::Idle&&slot.completedId==next.requestId);
            CHECK(ipc::Reap(slot,next.requestId,14)==ipc::Outcome::Discarded);
        }
        CHECK(slot.phase==ipc::Phase::Idle&&slot.completedId==0&&slot.consumed==0);
    }
    // Expired published frames can likewise hand off to a new queued request.
    slot={};CHECK(ipc::Begin(slot,request,1));CHECK(ipc::Take(slot,2,taken));CHECK(ipc::Publish(slot,request.requestId,3,d,ticket));
    CHECK(!ipc::Deliver(slot,request.requestId,1000,got,pair));next=request;++next.requestId;next.deadlineQpc=3000;
    CHECK(ipc::Begin(slot,next,1001));CHECK(ipc::Reap(slot,request.requestId,1002)==ipc::Outcome::Discarded);
    CHECK(ipc::Take(slot,1003,taken)&&taken.requestId==next.requestId);
    return 0;
}
