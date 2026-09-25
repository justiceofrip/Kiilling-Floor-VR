#include "Test.h"
#include "kfvr/interaction/ActionPolicy.h"
using namespace kfvr::interaction;
int main(){
    ActionPolicy p;ActionSample s{};s.owner=1;s.generation=1;s.timeNs=s.frameNs=1000000000;s.focused=s.tracked=s.playing=s.bindingsVerified=true;
    auto next=[&](){++s.generation;s.frameNs=s.timeNs+=10000000;return p.Update(s);};
    CHECK(!p.Update(s).held);CHECK(!next().held);s.held=Fire;s.forward=s.strafe=1;s.turn=1;auto out=next();CHECK(out.pressed==Fire&&out.held==Fire&&Near(out.turnDegrees,30));CHECK(Near(std::hypot(out.forward,out.strafe),1));
    out=p.Update(s);CHECK(out.held==Fire&&!out.pressed&&!out.turnDegrees&&out.forward>0);s.focused=false;out=next();CHECK(out.released==Fire&&!out.held&&out.owner==1);
    s.focused=true;CHECK(!next().pressed);CHECK(!next().pressed);s.held=0;s.forward=s.strafe=s.turn=0;next();s.held=Fire;CHECK(next().pressed==Fire);
    s.owner=2;out=next();CHECK(out.released==Fire&&out.owner==1);CHECK(!next().held);s.held=0;next();s.held=Fire;CHECK(next().pressed==Fire);
    s.frameNs+=160000001;out=p.Update(s);CHECK(out.released==Fire&&!out.held);s.held=0;s.frameNs=s.timeNs;next();next();s.held=Fire;CHECK(next().pressed==Fire);
    s.bindingsVerified=false;CHECK(next().released==Fire);CHECK(!next().held);
    return 0;
}