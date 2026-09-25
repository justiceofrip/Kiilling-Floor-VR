#include "kfvr/gameplay/PoseWire.h"
#include <cmath>
namespace kfvr::gameplay {
namespace {
void Put(PoseBuffer& b,std::size_t& at,std::uint32_t n,unsigned bytes){for(unsigned i=0;i<bytes;++i)b[at++]=std::byte((n>>(8*i))&255);}
std::uint32_t Get(std::span<const std::byte> b,std::size_t& at,unsigned bytes){std::uint32_t n=0;for(unsigned i=0;i<bytes;++i)n|=std::uint32_t(std::to_integer<unsigned>(b[at++]))<<(8*i);return n;}
bool Valid(const PosePacket& p){
    if(!p.actorEpoch||(p.flags&~std::uint16_t(15)))return false;
    for(const auto& t:p.targets){float n=0;
        for(float x:t.position)if(!std::isfinite(x)||std::abs(x)>3)return false;
        for(float x:t.rotation){if(!std::isfinite(x)||std::abs(x)>1.001f)return false;n+=x*x;}
        if(std::abs(n-1)>.005f)return false;
    }return true;
}
}
bool EncodePose(const PosePacket& p,PoseBuffer& b){
    if(!Valid(p))return false;std::size_t at=0;Put(b,at,0x3150464b,4);Put(b,at,1,2);Put(b,at,p.flags,2);Put(b,at,p.actorEpoch,4);Put(b,at,p.sequence,4);
    for(const auto& t:p.targets){for(float x:t.position)Put(b,at,std::uint16_t(std::int16_t(std::lround(x*1000))),2);float norm=0;for(float x:t.rotation)norm+=x*x;norm=std::sqrt(norm);for(float x:t.rotation)Put(b,at,std::uint16_t(std::int16_t(std::lround(x/norm*32767))),2);}return at==PoseBytes;
}
bool DecodePose(std::span<const std::byte> b,PosePacket& result){
    if(b.size()!=PoseBytes)return false;std::size_t at=0;if(Get(b,at,4)!=0x3150464b||Get(b,at,2)!=1)return false;
    PosePacket p;p.flags=std::uint16_t(Get(b,at,2));p.actorEpoch=Get(b,at,4);p.sequence=Get(b,at,4);
    auto signedValue=[&](){auto v=Get(b,at,2);return v<32768?int(v):int(v)-65536;};
    for(auto& t:p.targets){for(auto& x:t.position)x=float(signedValue())/1000;for(auto& x:t.rotation)x=float(signedValue())/32767;}
    if(!Valid(p))return false;for(auto& t:p.targets){float n=0;for(float x:t.rotation)n+=x*x;n=std::sqrt(n);for(auto& x:t.rotation)x/=n;}result=p;return true;
}
bool PoseGate::Accept(const PosePacket& p,double now){
    if(!Valid(p)||p.actorEpoch!=epoch_||!std::isfinite(now)||now<0)return false;
    if(initialized_){const auto delta=p.sequence-last_.sequence;const auto dt=now-lastTime_;
        if(!delta||delta>=0x80000000u||dt<1.0/30.0)return false;
        // Receive time is server-owned. Client timestamps cannot authorize faster motion.
        if(dt<.25){for(unsigned i=0;i<3;++i){double d=0;for(unsigned j=0;j<3;++j){double x=p.targets[i].position[j]-last_.targets[i].position[j];d+=x*x;}const double limit=(i?12:4)*dt+.03;if(d>limit*limit)return false;}}
    }
    last_=p;lastTime_=now;initialized_=true;return true;
}
}
