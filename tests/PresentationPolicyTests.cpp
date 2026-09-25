#include "Test.h"
#include "kfvr/runtime/PresentationPolicy.h"
using namespace kfvr;
int main(){
    runtime::PresentationRequirements r{100,120,28,19,4};runtime::TrackingFrame tracking{};
    tracking.generation=5;tracking.spaceGeneration=7;tracking.predictedNs=100000000;tracking.focused=tracking.headValid=true;
    graphics::TextureDescriptor d{};d.width=r.width;d.height=r.height;d.format=r.format;d.adapterLow=r.adapterLow;d.adapterHigh=r.adapterHigh;d.resourceEpoch=6;d.session[0]=1;
    graphics::PairTicket t{};t.resourceEpoch=d.resourceEpoch;t.session=d.session;t.sequence=1;t.frameId=4;t.spaceGeneration=tracking.spaceGeneration;t.trackingGeneration=tracking.generation;t.predictedNs=tracking.predictedNs;
    CHECK(runtime::PairMatchesFrame(r,tracking,d,t));
    // Even one older predicted-time nanosecond cannot use the new view poses.
    auto old=t;--old.predictedNs;CHECK(!runtime::PairMatchesFrame(r,tracking,d,old));old=t;--old.trackingGeneration;CHECK(!runtime::PairMatchesFrame(r,tracking,d,old));
    old=t;++old.spaceGeneration;CHECK(!runtime::PairMatchesFrame(r,tracking,d,old));old=t;++old.resourceEpoch;CHECK(!runtime::PairMatchesFrame(r,tracking,d,old));
    auto bad=d;++bad.adapterLow;CHECK(!runtime::PairMatchesFrame(r,tracking,bad,t));bad=d;++bad.width;CHECK(!runtime::PairMatchesFrame(r,tracking,bad,t));
    tracking.focused=false;CHECK(!runtime::PairMatchesFrame(r,tracking,d,t));tracking.focused=true;tracking.headValid=false;CHECK(!runtime::PairMatchesFrame(r,tracking,d,t));
    tracking.headValid=true;runtime::RetainedProjection held;CHECK(held.Remember(r,tracking,d,t));
    auto next=tracking;++next.generation;next.predictedNs+=11111111;next.head.position.x=1;
    CHECK(!runtime::PairMatchesFrame(r,next,d,t));CHECK(held.CanPresent(r,next));
    CHECK(held.RenderedTracking().head.position.x==tracking.head.position.x&&held.RenderedTracking().predictedNs==t.predictedNs);
    auto stale=next;stale.predictedNs=tracking.predictedNs+100000001;CHECK(!held.CanPresent(r,stale));
    stale=next;stale.predictedNs=tracking.predictedNs-1;CHECK(!held.CanPresent(r,stale));
    stale=next;++stale.spaceGeneration;CHECK(!held.CanPresent(r,stale));
    stale=next;stale.focused=false;CHECK(!held.CanPresent(r,stale));stale=next;stale.headValid=false;CHECK(!held.CanPresent(r,stale));
    auto changed=r;++changed.width;CHECK(!held.CanPresent(changed,next));
    held.Clear();CHECK(!held.CanPresent(r,next));CHECK(!held.Remember(r,next,d,t));CHECK(!held.CanPresent(r,next));
    return 0;
}
