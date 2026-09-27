#include "Test.h"
#include "kfvr/ipc/InputProtocol.h"
#include <limits>
using namespace kfvr;
int main(){
 xr::ControllerFrame frame{};frame.focused=true;frame.predictedNs=0x7123456789LL;frame.spaceGeneration=0x234567891ULL;
 for(auto& h:frame.hands){h.gripValid=h.aimValid=h.menu=true;h.gripPose.position={.25f,1.5f,-.4f};h.aimPose.position={.3f,1.6f,-.5f};h.primary=h.secondary=h.triggerTouchActive=h.triggerTouched=h.thumbTouchActive=true;h.thumbTouched=false;h.trigger=.7f;h.squeeze=.2f;h.stickX=.6f;h.stickY=.8f;}
 ipc::ControlWire wire;CHECK(ipc::EncodeControls(frame,0x312345678ULL,1000,wire));xr::ControllerFrame received;std::uint64_t id=0;
 CHECK(ipc::DecodeControls(wire,999,received,id));CHECK(id==0x312345678ULL&&received.predictedNs==frame.predictedNs&&received.spaceGeneration==frame.spaceGeneration&&received.hands[1].menu);CHECK(Near(received.hands[0].trigger,.7f)&&Near(received.hands[1].aimPose.position.z,-.5f));
 CHECK(received.hands[0].primary&&received.hands[1].secondary&&received.hands[0].triggerTouchActive&&received.hands[0].triggerTouched&&received.hands[0].thumbTouchActive&&!received.hands[0].thumbTouched);
 auto malformed=wire;malformed.version=1;CHECK(!ipc::DecodeControls(malformed,500,received,id));malformed=wire;malformed.hands[0].flags&=~8u;CHECK(!ipc::DecodeControls(malformed,500,received,id));malformed=wire;malformed.hands[0].flags|=512;CHECK(!ipc::DecodeControls(malformed,500,received,id));
 // A stopped host, renderer stall or stale packet must release all held input.
 CHECK(!ipc::DecodeControls(wire,1000,received,id));CHECK(!received.focused&&!received.hands[0].gripValid&&received.hands[0].trigger==0&&id==0);
 auto invalid=wire;invalid.hands[0].trigger=std::numeric_limits<float>::quiet_NaN();CHECK(!ipc::DecodeControls(invalid,500,received,id));invalid=wire;invalid.hands[1].grip.values[6]=0;CHECK(!ipc::DecodeControls(invalid,500,received,id));
 invalid=wire;invalid.hands[1].reserved=1;CHECK(!ipc::DecodeControls(invalid,500,received,id));invalid=wire;invalid.flags=0;CHECK(!ipc::DecodeControls(invalid,500,received,id));invalid=wire;invalid.hands[0].stickY=1;CHECK(!ipc::DecodeControls(invalid,500,received,id));
 frame.focused=false;ipc::ControlWire neutral;CHECK(ipc::EncodeControls(frame,2,1000,neutral));CHECK(ipc::DecodeControls(neutral,500,received,id));CHECK(!received.focused&&received.hands[0].trigger==0&&!received.hands[0].menu&&!received.hands[0].primary&&!received.hands[0].triggerTouchActive&&!received.hands[0].thumbTouchActive);
 frame.focused=true;frame.hands[0].gripValid=false;CHECK(ipc::EncodeControls(frame,3,1000,neutral));CHECK(ipc::DecodeControls(neutral,500,received,id));CHECK(!received.hands[0].gripValid&&received.hands[0].aimValid&&received.hands[0].trigger==0&&received.hands[1].trigger==.7f);
 ipc::HapticWire feedback;CHECK(ipc::EncodeHaptics(wire,1,500,{xr::HapticPulse{2,900},xr::HapticPulse{.3f,12}},feedback));std::uint64_t last=0;std::array<xr::HapticPulse,2> pulses;
 CHECK(ipc::DecodeHaptics(feedback,wire,500,last,pulses)&&last==1);CHECK(pulses[0].amplitude==1&&pulses[0].milliseconds==250&&Near(pulses[1].amplitude,.3f));
 CHECK(!ipc::DecodeHaptics(feedback,wire,500,last,pulses)&&pulses[0].milliseconds==0);last=0;CHECK(!ipc::DecodeHaptics(feedback,wire,1000,last,pulses)&&last==0);
 auto recentered=wire;++recentered.spaceGeneration;CHECK(!ipc::DecodeHaptics(feedback,recentered,500,last,pulses));auto earlier=wire;--earlier.sequence;CHECK(!ipc::DecodeHaptics(feedback,earlier,500,last,pulses));
 auto later=wire;++later.sequence;CHECK(ipc::DecodeHaptics(feedback,later,500,last,pulses));last=0;feedback.hands[0].milliseconds=251;CHECK(!ipc::DecodeHaptics(feedback,wire,500,last,pulses));
 CHECK(!ipc::EncodeHaptics(wire,2,1000,{},feedback));CHECK(ipc::EncodeHaptics(neutral,2,500,{xr::HapticPulse{1,30},xr::HapticPulse{1,30}},feedback)&&feedback.hands[0].amplitude==0&&feedback.hands[1].amplitude==1);
 std::puts("Controller wire: 64-bit identity, focus/tracking loss, stale release, recenter and one-shot feedback verified");return 0;
}
