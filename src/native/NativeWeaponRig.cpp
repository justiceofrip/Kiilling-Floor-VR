#include "NativeWeaponRig.h"
#include "NativeCameraTrial.h"
#include "NativeEyeBridge.h"
#include "kfvr/interaction/TrackedWeapon.h"
#include "kfvr/interaction/FingerPose.h"
#include "kfvr/engine/Ue2Camera.h"
#include "kfvr/interaction/TrackingMath.h"
#include <Windows.h>
#include <cmath>
#include <numbers>
#include <cstring>
#include <fstream>
#include <atomic>
namespace kfvr::native {
namespace {
struct Packet {
 math::Vec3 leftPosition{};ue2::Rotator leftRotation{};
 math::Vec3 rightPosition{};ue2::Rotator rightRotation{};
 float leftTrigger=0,leftGrip=0,rightTrigger=0,rightGrip=0;
 float leftThumb=0,leftIndex=0,leftCurl=0,rightThumb=0,rightIndex=0,rightCurl=0;
 math::Vec3 headPosition{};ue2::Rotator bodyRotation{};float units=0;
 int flags=0,serial=0,inputSerial=0;
};
static_assert(sizeof(Packet)==128&&offsetof(Packet,headPosition)==88&&offsetof(Packet,serial)==120&&offsetof(Packet,inputSerial)==124);
std::atomic<bool> stopped=false;std::atomic<unsigned> users=0;
struct Use {Use(){users.fetch_add(1);}~Use(){users.fetch_sub(1);}};
NativeProbeConfig config{};runtime::TrackingFrame tracking{};
unsigned feedbackSerial=0,leftFeedbackSerial=0,rightFeedbackSerial=0,haptics=0,spaceResets=0;
std::uint64_t controlSpace=0,lastInputSequence=0;int inputSerial=0;
DWORD thread=0;bool anchor=false;unsigned updates=0,valid=0,invalid=0,failed=0;int serial=0;
struct Row {Packet packet;};std::array<Row,1000> rows{};unsigned count=0;
bool Read(unsigned at,void* p,unsigned bytes){SIZE_T n=0;return at>=0x10000&&ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(at),p,bytes,&n)&&n==bytes;}
unsigned U32(unsigned at){unsigned n=0;Read(at,&n,4);return n;}
std::optional<ue2::Rotator> Rotation(const math::Matrix4& m){
 if(!interaction::InverseRigid(m))return {};const auto& f=m.values[2];const double horizontal=std::hypot(f[0],f[1]);
 const double pitch=std::atan2(f[2],horizontal),yaw=horizontal<.0001?std::atan2(-m.values[0][0],m.values[0][1]):std::atan2(f[1],f[0]);
 const double roll=horizontal<.0001?0:std::atan2(-m.values[0][2],m.values[1][2]);
 auto units=[](double x){return int(std::lround(x*65536/(2*std::numbers::pi)));};return ue2::Rotator{units(pitch),units(yaw),units(roll)};
}
}
void ConfigureWeaponRig(const NativeProbeConfig& c){config=c;if(c.weaponTrial){feedbackSerial=U32(c.rig+c.rigFeedbackOffset);leftFeedbackSerial=U32(c.rig+c.rigFeedbackOffset+20);rightFeedbackSerial=U32(c.rig+c.rigFeedbackOffset+24);}}
void CaptureWeaponAnchor(const runtime::TrackingFrame& frame){
 if(!config.weaponTrial)return;
 tracking=frame;thread=GetCurrentThreadId();anchor=frame.headValid&&frame.focused;
}
void UpdateWeaponRig(void* actor,void* function,bool active)noexcept {
 if(!config.weaponTrial||reinterpret_cast<unsigned>(actor)!=config.rig||U32(reinterpret_cast<unsigned>(function)+0x24)!=config.rigTickName)return;
 Use use;if(stopped.load())return;
 if(U32(config.rig+4)!=config.rigIndex||U32(config.rig+0x28)!=config.rigClass||U32(config.rig+config.ownerOffset)!=config.controller||U32(config.rig+config.levelOffset)!=config.level)return;
 Packet packet{};packet.units=config.unitsPerMetre;packet.serial=++serial;++updates;
 const auto pawn=U32(config.controller+config.pawnOffset);int health=0;math::Vec3 location{};ue2::Rotator heading{};
 xr::ControllerFrame controls{};std::uint64_t sequence=0;
 const bool playing=active&&anchor&&thread==GetCurrentThreadId()&&pawn&&U32(pawn+config.pawnControllerOffset)==config.controller&&Read(pawn+config.healthOffset,&health,4)&&health>0&&!U32(config.level+config.pauserOffset)&&Read(pawn+config.locationOffset,&location,12)&&Read(config.controller+config.rotationOffset,&heading,12);
 if(playing&&ReadEyeBridgeControls(controls,sequence)&&controls.focused&&controls.spaceGeneration==tracking.spaceGeneration){
  // Keep game/presentation ticks distinct from fresh controller samples.
  // Re-reading a mailbox pose must not shorten the gesture travel interval.
  if(sequence!=lastInputSequence){lastInputSequence=sequence;++inputSerial;}
  // Even if both new-space eyes/controllers arrive together, issue one
  // neutral game Tick so every pending gesture observes the recenter.
  const bool changedSpace=controlSpace&&controlSpace!=controls.spaceGeneration;
  controlSpace=controls.spaceGeneration;if(changedSpace)++spaceResets;
  const auto currentBody=ReadTrackedBody();
  if(currentBody&&!changedSpace){
   packet.bodyRotation=currentBody->heading;const auto* body=&currentBody->camera;
   const auto head=interaction::SolveTrackedWeapon(*body,tracking.referenceHead,tracking.head,{},config.unitsPerMetre);if(head)packet.headPosition=head->fireOrigin;
   for(unsigned i=0;i<2;++i){const auto& hand=controls.hands[i];if(!hand.gripValid||!hand.aimValid)continue;
    const auto presentation=interaction::ControllerPresentationPose(hand.gripPose,hand.aimPose);if(!presentation)continue;
    const auto pose=interaction::SolveTrackedWeapon(*body,tracking.referenceHead,*presentation,{},config.unitsPerMetre);if(!pose)continue;const auto rotation=Rotation(pose->weaponWorld);if(!rotation)continue;
    const auto relative=math::MakeRelativePose(tracking.referenceHead,hand.gripPose);if(!relative||std::hypot(relative->position.x,relative->position.y,relative->position.z)>3)continue;
    const auto curl=interaction::ControllerFingerCurls(hand);packet.flags|=1<<i;if(hand.primary)packet.flags|=4<<i;if(hand.secondary)packet.flags|=16<<i;
    if(i){packet.rightPosition=pose->fireOrigin;packet.rightRotation=*rotation;packet.rightTrigger=hand.trigger;packet.rightGrip=hand.squeeze;packet.rightThumb=curl[0];packet.rightIndex=curl[1];packet.rightCurl=curl[2];}
    else {packet.leftPosition=pose->fireOrigin;packet.leftRotation=*rotation;packet.leftTrigger=hand.trigger;packet.leftGrip=hand.squeeze;packet.leftThumb=curl[0];packet.leftIndex=curl[1];packet.leftCurl=curl[2];}
   }
  }
 }
 packet.inputSerial=inputSerial;
 struct Feedback{unsigned serial;float left,right;int leftMs,rightMs;unsigned leftSerial,rightSerial;};static_assert(sizeof(Feedback)==28);
 Feedback feedback{};
 if(Read(config.rig+config.rigFeedbackOffset,&feedback,sizeof(feedback))&&feedback.serial!=feedbackSerial){
  const bool leftChanged=feedback.leftSerial!=leftFeedbackSerial,rightChanged=feedback.rightSerial!=rightFeedbackSerial;
  feedbackSerial=feedback.serial;leftFeedbackSerial=feedback.leftSerial;rightFeedbackSerial=feedback.rightSerial;
  if(playing&&controls.focused&&sequence&&(packet.flags&3)&&std::isfinite(feedback.left)&&std::isfinite(feedback.right)&&feedback.left>=0&&feedback.left<=1&&feedback.right>=0&&feedback.right<=1&&feedback.leftMs>=0&&feedback.leftMs<=100&&feedback.rightMs>=0&&feedback.rightMs<=100){
   if(SendEyeBridgeHaptics(sequence,{leftChanged?xr::HapticPulse{feedback.left,unsigned(feedback.leftMs)}:xr::HapticPulse{},rightChanged?xr::HapticPulse{feedback.right,unsigned(feedback.rightMs)}:xr::HapticPulse{}}))++haptics;
  }
 }
 SIZE_T wrote=0;const bool ok=WriteProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(config.rig+config.rigTrackingOffset),&packet,sizeof(packet),&wrote)&&wrote==sizeof(packet);
 if(!ok)++failed;else if(packet.flags&3)++valid;else ++invalid;
 if(count<rows.size())rows[count++].packet=packet;
}
bool StopWeaponRig()noexcept {stopped=true;const auto until=GetTickCount64()+1000;while(users.load()&&GetTickCount64()<until)Sleep(1);return users.load()==0;}
void WriteWeaponRigReport(const std::filesystem::path& folder){
 if(!config.weaponTrial)return;std::ofstream out(folder/L"native-weapon.json");out<<"{\"updates\":"<<updates<<",\"valid\":"<<valid<<",\"invalid\":"<<invalid<<",\"failed\":"<<failed<<",\"haptic_events\":"<<haptics<<",\"reference_space_resets\":"<<spaceResets<<",\"input_samples\":"<<inputSerial<<",\"packets\":[";
 for(unsigned i=0;i<count;++i){const auto& p=rows[i].packet;if(i)out<<',';out<<"{\"serial\":"<<p.serial<<",\"input_serial\":"<<p.inputSerial<<",\"flags\":"<<p.flags<<",\"right\":["<<p.rightPosition.x<<','<<p.rightPosition.y<<','<<p.rightPosition.z<<"],\"rotation\":["<<p.rightRotation.pitch<<','<<p.rightRotation.yaw<<','<<p.rightRotation.roll<<"],\"trigger\":"<<p.rightTrigger<<",\"grip\":"<<p.rightGrip<<",\"fingers\":["<<p.rightThumb<<','<<p.rightIndex<<','<<p.rightCurl<<"]}";}
 out<<"]}\n";
}
}
