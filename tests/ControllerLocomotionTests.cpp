#include "Test.h"
#include "kfvr/interaction/ControllerLocomotion.h"
#include <limits>
using namespace kfvr;
struct Fixture {
 interaction::ControllerLocomotion policy;xr::ControllerFrame frame{};
 std::uint64_t sequence=0,owner=1;bool playing=true;
 Fixture(){frame.spaceGeneration=1;frame.focused=true;frame.hands[0].gripValid=true;frame.predictedNs=1000000000;}
 interaction::LocomotionOutput Next(){++sequence;frame.predictedNs+=10000000;return policy.Update(&frame,sequence,owner,playing);}
 void Neutral(){frame.hands[0].stickX=frame.hands[0].stickY=0;Next();Next();}
 void Held(){frame.hands[0].stickY=1;}
};
int main(){
 Fixture f;f.Held();CHECK(!f.Next().forward);CHECK(!f.Next().forward);f.Neutral();f.Held();CHECK(Near(f.Next().forward,1));
 // Repeated game ticks may consume the same fresh mailbox without losing a hold.
 CHECK(Near(f.policy.Update(&f.frame,f.sequence,f.owner,true).forward,1));
 f.frame.hands[0].stickY=.6f;CHECK(Near(f.Next().forward,.5f));f.frame.hands[0].stickX=1;f.Held();auto out=f.Next();CHECK(Near(std::hypot(out.forward,out.strafe),1));CHECK(Near(out.forward,out.strafe));
 f.frame.hands[0].stickX=-1;f.frame.hands[0].stickY=0;CHECK(Near(f.Next().strafe,-1));
 // Every ownership/tracking/lifecycle interruption requires actual raw neutral.
 for(int failure=0;failure<7;++failure){
  f.Neutral();f.Held();CHECK(Near(f.Next().forward,1));
  if(failure==0){f.frame.focused=false;CHECK(!f.Next().valid);f.frame.focused=true;}
  if(failure==1){f.frame.hands[0].gripValid=false;CHECK(!f.Next().valid);f.frame.hands[0].gripValid=true;}
  if(failure==2){f.playing=false;CHECK(!f.Next().valid);f.playing=true;}
  if(failure==3){CHECK(!f.policy.Update(nullptr,0,f.owner,true).valid);}
  if(failure==4){++f.owner;}
  if(failure==5){++f.frame.spaceGeneration;}
  if(failure==6){f.frame.predictedNs+=160000000;}
  CHECK(!f.Next().forward);CHECK(!f.Next().forward);
  f.frame.hands[0].stickY=.3f;CHECK(!f.Next().forward);CHECK(!f.Next().forward);
  // A diagonal outside the radial deadzone also cannot rearm the stick.
  f.frame.hands[0].stickX=f.frame.hands[0].stickY=.18f;CHECK(!f.Next().forward);CHECK(!f.Next().strafe);
  f.Neutral();f.Held();CHECK(Near(f.Next().forward,1));
 }
 f.frame.hands[0].stickY=std::numeric_limits<float>::quiet_NaN();CHECK(!f.Next().valid);f.Neutral();f.frame.hands[0].stickY=1.01f;CHECK(!f.Next().valid);
 f.Neutral();f.Held();CHECK(f.Next().forward>0);f.sequence-=2;CHECK(!f.Next().forward);CHECK(!f.Next().forward);
 f.Neutral();f.Held();CHECK(f.Next().forward>0);f.frame.predictedNs-=100000000;CHECK(!f.Next().forward);CHECK(!f.Next().forward);

 // Right-stick turns fire once per deflection and remain independent of movement.
 Fixture t;t.frame.hands[1].gripValid=true;t.Neutral();t.Held();
 t.frame.hands[1].stickX=1;out=t.Next();CHECK(Near(out.forward,1)&&Near(out.turnDegrees,30));
 CHECK(!t.Next().turnDegrees);CHECK(!t.policy.Update(&t.frame,t.sequence,t.owner,true).turnDegrees);
 t.frame.hands[1].stickX=0;t.Next();t.frame.hands[1].stickX=-1;CHECK(Near(t.Next().turnDegrees,-30));CHECK(!t.Next().turnDegrees);
 // Losing right-hand tracking does not stop a tracked left hand.
 t.frame.hands[1].gripValid=false;out=t.Next();CHECK(Near(out.forward,1)&&!out.turnValid&&!out.turnDegrees);
 t.frame.hands[1].gripValid=true;CHECK(!t.Next().turnDegrees);CHECK(!t.Next().turnDegrees);
 t.frame.hands[1].stickX=0;t.Next();t.frame.hands[1].stickX=1;CHECK(Near(t.Next().turnDegrees,30));
 t.frame.hands[0].gripValid=false;t.frame.hands[1].stickX=0;t.Next();t.frame.hands[1].stickX=-1;out=t.Next();CHECK(!out.valid&&out.turnValid&&Near(out.turnDegrees,-30));
 t.frame.focused=false;CHECK(!t.Next().turnDegrees);t.frame.focused=true;CHECK(!t.Next().turnDegrees);CHECK(!t.Next().turnDegrees);
 t.frame.hands[1].stickX=0;t.Next();t.frame.hands[1].stickX=1;CHECK(Near(t.Next().turnDegrees,30));
 ++t.frame.spaceGeneration;CHECK(!t.Next().turnDegrees);CHECK(!t.Next().turnDegrees);
 t.frame.hands[1].stickX=0;t.Next();t.Next();t.frame.hands[1].stickX=-1;CHECK(Near(t.Next().turnDegrees,-30));
 t.frame.hands[1].stickX=0;t.Next();t.frame.hands[1].stickX=1;++t.sequence;t.frame.predictedNs+=10000000;CHECK(Near(t.policy.Update(&t.frame,t.sequence,t.owner,true,45).turnDegrees,45));
 return 0;
}
