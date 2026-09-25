#include "kfvr/ipc/FrameProtocol.h"
#include <cmath>
namespace kfvr::ipc {
namespace {
PoseWire Pack(const math::Pose& p){return {{{p.position.x,p.position.y,p.position.z,p.orientation.x,p.orientation.y,p.orientation.z,p.orientation.w}}};}
math::Pose Unpack(const PoseWire& p){const auto& v=p.values;return {{v[0],v[1],v[2]},{v[3],v[4],v[5],v[6]}};}
bool PoseValid(const math::Pose& p){
    const auto& q=p.orientation;const float norm=q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w;
    return std::isfinite(p.position.x)&&std::isfinite(p.position.y)&&std::isfinite(p.position.z)&&std::isfinite(norm)&&std::abs(norm-1.f)<.01f;
}
bool Match(const FrameSlot& s,std::uint64_t id){return id&&s.request.requestId==id;}
bool Expired(const FrameSlot& s,std::int64_t now){return now<=0||now>=s.request.deadlineQpc;}
}
bool Decode(const FrameRequest& r,runtime::PresentationRequirements& requirements,runtime::TrackingFrame& tracking)noexcept {
    if(r.magic!=FrameMagic||r.version!=1||r.bytes!=sizeof(r)||r.flags!=3||!r.requestId||r.deadlineQpc<=0||r.reserved0||r.reserved[0]||r.reserved[1]||
       !r.trackingGeneration||!r.spaceGeneration||r.predictedNs<=0||!r.width||r.width>16384||!r.height||r.height>16384||
       (r.format!=28&&r.format!=29&&r.format!=87&&r.format!=91))return false;
    runtime::TrackingFrame t{};t.generation=r.trackingGeneration;t.spaceGeneration=r.spaceGeneration;t.predictedNs=r.predictedNs;t.focused=t.headValid=true;
    t.referenceHead=Unpack(r.referenceHead);t.head=Unpack(r.head);
    if(!PoseValid(t.referenceHead)||!PoseValid(t.head))return false;
    for(unsigned eye=0;eye<2;++eye){t.eyes[eye]=Unpack(r.eyes[eye]);const auto& f=r.fov[eye];t.fov[eye]={f[0],f[1],f[2],f[3]};
        if(!PoseValid(t.eyes[eye])||!math::MakeLhProjectionFromFovTangents(t.fov[eye],.05f,100.f))return false;}
    requirements={r.width,r.height,r.format,r.adapterLow,r.adapterHigh};tracking=t;return true;
}
bool Encode(const runtime::PresentationRequirements& requirements,const runtime::TrackingFrame& tracking,std::uint64_t id,std::int64_t deadline,FrameRequest& out)noexcept {
    if(!tracking.headValid||!tracking.focused)return false;
    FrameRequest r{};r.requestId=id;r.deadlineQpc=deadline;r.width=requirements.width;r.height=requirements.height;r.format=requirements.format;
    r.adapterLow=requirements.adapterLow;r.adapterHigh=requirements.adapterHigh;r.trackingGeneration=tracking.generation;r.spaceGeneration=tracking.spaceGeneration;r.predictedNs=tracking.predictedNs;
    r.referenceHead=Pack(tracking.referenceHead);r.head=Pack(tracking.head);
    for(unsigned eye=0;eye<2;++eye){r.eyes[eye]=Pack(tracking.eyes[eye]);const auto& f=tracking.fov[eye];r.fov[eye]={f.left,f.right,f.up,f.down};}
    runtime::PresentationRequirements check{};runtime::TrackingFrame t{};if(!Decode(r,check,t))return false;out=r;return true;
}
bool Begin(FrameSlot& s,const FrameRequest& r,std::int64_t now)noexcept {
    runtime::PresentationRequirements requirements{};runtime::TrackingFrame tracking{};
    if(s.phase!=Phase::Idle||now<=0||r.deadlineQpc<=now||!Decode(r,requirements,tracking))return false;
    s={};s.request=r;s.phase=Phase::Requested;return true;
}
bool Take(FrameSlot& s,std::int64_t now,FrameRequest& out)noexcept {
    if(s.phase!=Phase::Requested)return false;
    if(Expired(s,now)){s.phase=Phase::Idle;return false;}
    runtime::PresentationRequirements requirements{};runtime::TrackingFrame tracking{};
    if(!Decode(s.request,requirements,tracking)){s.phase=Phase::Closed;return false;}
    out=s.request;s.phase=Phase::Rendering;return true;
}
bool Publish(FrameSlot& s,std::uint64_t id,std::int64_t now,const graphics::TextureDescriptor& d,const graphics::PairTicket& t)noexcept {
    if(!Match(s,id))return false;
    if(s.phase==Phase::Cancelled||(s.phase==Phase::Rendering&&Expired(s,now))){s.phase=Phase::Idle;return false;}
    runtime::PresentationRequirements requirements{};runtime::TrackingFrame tracking{};
    if(s.phase!=Phase::Rendering||!Decode(s.request,requirements,tracking)||!runtime::PairMatchesFrame(requirements,tracking,d,t))return false;
    s.descriptor=d;s.ticket=t;s.phase=Phase::Ready;return true;
}
bool Deliver(FrameSlot& s,std::uint64_t id,std::int64_t now,graphics::TextureDescriptor& d,graphics::PairTicket& t)noexcept {
    if(!Match(s,id)||s.phase!=Phase::Ready)return false;
    if(Expired(s,now)){Cancel(s,id);return false;}
    runtime::PresentationRequirements requirements{};runtime::TrackingFrame tracking{};
    if(!Decode(s.request,requirements,tracking)||!runtime::PairMatchesFrame(requirements,tracking,s.descriptor,s.ticket)){Cancel(s,id);return false;}
    d=s.descriptor;t=s.ticket;s.phase=Phase::Delivered;return true;
}
void Cancel(FrameSlot& s,std::uint64_t id)noexcept {
    if(!Match(s,id))return;
    if(s.phase==Phase::Requested)s.phase=Phase::Idle;
    else if(s.phase==Phase::Rendering)s.phase=Phase::Cancelled;
    else if(s.phase==Phase::Ready){s.phase=Phase::Complete;s.consumed=0;}
    // Delivered belongs to the GPU consumer until explicit feedback or peer exit.
}
void Skip(FrameSlot& s,std::uint64_t id)noexcept {if(Match(s,id)&&(s.phase==Phase::Rendering||s.phase==Phase::Cancelled))s.phase=Phase::Idle;}
bool Acknowledge(FrameSlot& s,std::uint64_t id,bool consumed)noexcept {
    if(!Match(s,id)||s.phase!=Phase::Delivered)return false;s.consumed=consumed?1u:0u;s.phase=Phase::Complete;return true;
}
Outcome Reap(FrameSlot& s,std::uint64_t id,std::int64_t now)noexcept {
    if(!Match(s,id)||s.phase==Phase::Idle||s.phase==Phase::Closed)return Outcome::Discarded;
    if(s.phase==Phase::Ready&&Expired(s,now))Cancel(s,id);
    if(s.phase!=Phase::Complete)return Outcome::Pending;
    const auto outcome=s.consumed==1?Outcome::Consumed:Outcome::Discarded;s.phase=Phase::Idle;return outcome;
}
}
