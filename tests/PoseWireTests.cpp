#include "kfvr/gameplay/PoseWire.h"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace kfvr::gameplay;
#define CHECK(x) do{if(!(x))throw std::runtime_error(#x);}while(false)
int main(){try{
 PosePacket p;p.actorEpoch=12;p.sequence=1;p.targets[0].position={-.1234f,1.7f,-.7f};p.targets[1].position={.2f,1.2f,-.4f};p.targets[2].rotation={0,.70710678f,0,.70710678f};
 PoseBuffer buffer;CHECK(EncodePose(p,buffer));CHECK(buffer.size()==58);PosePacket decoded;CHECK(DecodePose(buffer,decoded));CHECK(std::abs(decoded.targets[0].position[0]+.1234f)<.00051f);CHECK(decoded.actorEpoch==12);
 for(std::size_t i=0;i<buffer.size();++i)CHECK(!DecodePose(std::span(buffer).first(i),decoded));
 auto bad=buffer;bad[0]=std::byte(0);CHECK(!DecodePose(bad,decoded));bad=buffer;bad[6]=std::byte(255);CHECK(!DecodePose(bad,decoded));
 PoseGate gate(12);CHECK(gate.Accept(p,1));CHECK(!gate.Accept(p,1.1));++p.sequence;CHECK(!gate.Accept(p,1.001));CHECK(gate.Accept(p,1.05));
 ++p.sequence;p.targets[0].position[0]=2;CHECK(!gate.Accept(p,1.1));p.targets[0].position[0]=-.1f;CHECK(gate.Accept(p,1.1));p.actorEpoch=13;++p.sequence;CHECK(!gate.Accept(p,1.2));p.actorEpoch=12;p.targets[1].rotation[0]=std::numeric_limits<float>::quiet_NaN();CHECK(!EncodePose(p,buffer));
 p.targets[1].rotation={0,0,0,0};CHECK(!EncodePose(p,buffer));p.targets[1].rotation={0,0,0,1};p.targets[0].position[0]=3.1f;CHECK(!EncodePose(p,buffer));
 PosePacket nearUnit;nearUnit.actorEpoch=1;nearUnit.targets[0].rotation={0,0,0,1.001f};CHECK(EncodePose(nearUnit,buffer));CHECK(DecodePose(buffer,decoded));CHECK(decoded.targets[0].rotation[3]==1);
 PoseGate wrap(1);p={};p.actorEpoch=1;p.sequence=0xffffffff;CHECK(wrap.Accept(p,1));p.sequence=0;CHECK(wrap.Accept(p,1.04));CHECK(!wrap.Accept(p,1.08));
 std::cout<<"58-byte pose roundtrip, millimetre precision, malformed/truncated input, epoch, sequence/wrap, server rate/speed and quaternion checks passed\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
