#include "Test.h"
#include "kfvr/interaction/FingerPose.h"
#include <limits>
using namespace kfvr;
int main(){
 xr::HandInput hand{};hand.gripValid=true;
 auto curls=interaction::ControllerFingerCurls(hand);for(float v:curls)CHECK(v==0);
 hand.squeeze=.8f;hand.trigger=.2f;curls=interaction::ControllerFingerCurls(hand);CHECK(Near(curls[0],.56f)&&Near(curls[1],.2f)&&Near(curls[4],.8f));
 hand.triggerTouchActive=true;hand.triggerTouched=true;hand.thumbTouchActive=true;
 curls=interaction::ControllerFingerCurls(hand);CHECK(curls[0]==0&&Near(curls[1],.55f));
 hand.thumbTouched=true;hand.trigger=.9f;curls=interaction::ControllerFingerCurls(hand);CHECK(curls[0]==1&&Near(curls[1],.9f));
 hand.triggerTouched=false;hand.trigger=0;hand.squeeze=0;curls=interaction::ControllerFingerCurls(hand);CHECK(curls[0]==1&&curls[1]==0&&curls[2]==0);
 hand.gripValid=false;curls=interaction::ControllerFingerCurls(hand);for(float v:curls)CHECK(v==0);
 hand.gripValid=true;hand.thumbTouchActive=false;hand.trigger=std::numeric_limits<float>::quiet_NaN();hand.squeeze=2;curls=interaction::ControllerFingerCurls(hand);CHECK(curls[1]==0&&curls[2]==1&&Near(curls[0],.7f));
 std::puts("Finger pose: independent capacitive contact, analog fallback, tracking loss and invalid input verified");return 0;
}
