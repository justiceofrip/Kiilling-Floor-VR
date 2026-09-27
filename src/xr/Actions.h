#pragma once
#include "kfvr/xr/ControllerInput.h"
#include <openxr/openxr.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>
namespace kfvr::xr {
// Private runtime binding; the portable consumer and IPC do not expose Xr handles.
class Actions {
 XrInstance instance_=XR_NULL_HANDLE;XrSession session_=XR_NULL_HANDLE;XrActionSet set_=XR_NULL_HANDLE;
 XrAction triggerTouch_=XR_NULL_HANDLE,thumbTouch_=XR_NULL_HANDLE,primary_=XR_NULL_HANDLE,secondary_=XR_NULL_HANDLE;
 XrAction grip_=XR_NULL_HANDLE,aim_=XR_NULL_HANDLE,trigger_=XR_NULL_HANDLE,select_=XR_NULL_HANDLE,squeeze_=XR_NULL_HANDLE,grab_=XR_NULL_HANDLE,stick_=XR_NULL_HANDLE,menu_=XR_NULL_HANDLE,haptic_=XR_NULL_HANDLE;
 std::array<XrPath,2> paths_{};std::array<XrSpace,2> gripSpaces_{},aimSpaces_{};
#define KFVR_ACTION_APIS(F) F(StringToPath) F(CreateActionSet) F(DestroyActionSet) F(CreateAction) F(SuggestInteractionProfileBindings) F(CreateActionSpace) F(DestroySpace) F(AttachSessionActionSets) F(SyncActions) F(GetActionStatePose) F(GetActionStateFloat) F(GetActionStateBoolean) F(GetActionStateVector2f) F(LocateSpace) F(ApplyHapticFeedback) F(StopHapticFeedback)
#define FIELD(n) PFN_xr##n n=nullptr;
 KFVR_ACTION_APIS(FIELD)
#undef FIELD
 static void Check(XrResult r,const char* op){if(XR_FAILED(r))throw std::runtime_error(std::string(op)+" XrResult="+std::to_string(r));}
 XrPath Path(const std::string& s){XrPath p=XR_NULL_PATH;Check(StringToPath(instance_,s.c_str(),&p),"Controller path");return p;}
 void Create(XrAction& a,const char* name,const char* label,XrActionType type){XrActionCreateInfo i{XR_TYPE_ACTION_CREATE_INFO};strcpy_s(i.actionName,name);strcpy_s(i.localizedActionName,label);i.actionType=type;i.countSubactionPaths=2;i.subactionPaths=paths_.data();Check(CreateAction(set_,&i,&a),"Create controller action");}
 void Suggest(const char* profile,const char* trigger,const char* squeeze,const char* stick,const char* menu,bool digitalGrab=false,bool simple=false,bool touch=false){
  std::vector<XrActionSuggestedBinding> b;
  for(unsigned hand=0;hand<2;++hand){std::string base=hand?"/user/hand/right":"/user/hand/left";auto add=[&](XrAction a,const char* suffix){if(suffix)b.push_back({a,Path(base+suffix)});};
   add(grip_,"/input/grip/pose");add(aim_,"/input/aim/pose");add(haptic_,"/output/haptic");add(simple?select_:trigger_,trigger);add(digitalGrab?grab_:squeeze_,squeeze);add(stick_,stick);if(hand==0)add(menu_,menu);
  }
  const auto legacy=b;
  if(touch)for(unsigned hand=0;hand<2;++hand){const std::string base=hand?"/user/hand/right/input/":"/user/hand/left/input/";
   b.push_back({triggerTouch_,Path(base+"trigger/touch")});
   for(const char* part:{"thumbrest/touch","thumbstick/touch"})b.push_back({thumbTouch_,Path(base+part)});
   for(const char* part:hand?std::array<const char*,2>{"a","b"}:std::array<const char*,2>{"x","y"})b.push_back({thumbTouch_,Path(base+part+"/touch")});
   b.push_back({primary_,Path(base+(hand?"a/click":"x/click"))});b.push_back({secondary_,Path(base+(hand?"b/click":"y/click"))});
  }
  XrInteractionProfileSuggestedBinding i{XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};i.interactionProfile=Path(profile);i.countSuggestedBindings=std::uint32_t(b.size());i.suggestedBindings=b.data();auto result=SuggestInteractionProfileBindings(instance_,&i);if(result==XR_ERROR_PATH_UNSUPPORTED&&touch){i.countSuggestedBindings=std::uint32_t(legacy.size());i.suggestedBindings=legacy.data();result=SuggestInteractionProfileBindings(instance_,&i);}if(result==XR_ERROR_PATH_UNSUPPORTED)return;Check(result,"Suggest controller profile");++suggestedProfiles;
 }
 bool PoseActive(XrAction a,unsigned h){XrActionStateGetInfo i{XR_TYPE_ACTION_STATE_GET_INFO};i.action=a;i.subactionPath=paths_[h];XrActionStatePose state{XR_TYPE_ACTION_STATE_POSE};Check(GetActionStatePose(session_,&i,&state),"Read pose action");return state.isActive==XR_TRUE;}
 float Float(XrAction a,unsigned h){XrActionStateGetInfo i{XR_TYPE_ACTION_STATE_GET_INFO};i.action=a;i.subactionPath=paths_[h];XrActionStateFloat s{XR_TYPE_ACTION_STATE_FLOAT};Check(GetActionStateFloat(session_,&i,&s),"Read analog action");return s.isActive&&std::isfinite(s.currentState)?std::clamp(s.currentState,0.f,1.f):0;}
 bool Bool(XrAction a,unsigned h){XrActionStateGetInfo i{XR_TYPE_ACTION_STATE_GET_INFO};i.action=a;i.subactionPath=paths_[h];XrActionStateBoolean s{XR_TYPE_ACTION_STATE_BOOLEAN};Check(GetActionStateBoolean(session_,&i,&s),"Read button action");return s.isActive&&s.currentState;}
 void Touch(XrAction a,unsigned h,bool& active,bool& touched){XrActionStateGetInfo i{XR_TYPE_ACTION_STATE_GET_INFO};i.action=a;i.subactionPath=paths_[h];XrActionStateBoolean state{XR_TYPE_ACTION_STATE_BOOLEAN};Check(GetActionStateBoolean(session_,&i,&state),"Read capacitive action");active=state.isActive==XR_TRUE;touched=active&&state.currentState;}
 bool Locate(XrAction a,XrSpace actionSpace,XrSpace local,XrTime time,unsigned hand,math::Pose& result){
  if(!PoseActive(a,hand))return false;XrSpaceLocation p{XR_TYPE_SPACE_LOCATION};Check(LocateSpace(actionSpace,local,time,&p),"Locate controller");constexpr auto valid=XR_SPACE_LOCATION_POSITION_VALID_BIT|XR_SPACE_LOCATION_ORIENTATION_VALID_BIT;
  if((p.locationFlags&valid)!=valid)return false;const math::Pose candidate{{p.pose.position.x,p.pose.position.y,p.pose.position.z},{p.pose.orientation.x,p.pose.orientation.y,p.pose.orientation.z,p.pose.orientation.w}};if(!math::MakeRelativePose(candidate,candidate))return false;result=candidate;return true;
 }
public:
 unsigned suggestedProfiles=0;
 Actions()=default;Actions(const Actions&)=delete;Actions& operator=(const Actions&)=delete;
 ~Actions(){Reset();}
 void Reset()noexcept{for(auto& s:gripSpaces_)if(s&&DestroySpace){DestroySpace(s);s=XR_NULL_HANDLE;}for(auto& s:aimSpaces_)if(s&&DestroySpace){DestroySpace(s);s=XR_NULL_HANDLE;}if(set_&&DestroyActionSet)DestroyActionSet(set_);set_=XR_NULL_HANDLE;session_=XR_NULL_HANDLE;instance_=XR_NULL_HANDLE;}
 void Start(PFN_xrGetInstanceProcAddr get,XrInstance instance,XrSession session){
  Reset();suggestedProfiles=0;instance_=instance;session_=session;
#define LOAD(n) {PFN_xrVoidFunction fn=nullptr;Check(get(instance,"xr" #n,&fn),"Resolve controller API");if(!fn)throw std::runtime_error("Missing controller API");n=reinterpret_cast<PFN_xr##n>(fn);}
  KFVR_ACTION_APIS(LOAD)
#undef LOAD
  paths_={Path("/user/hand/left"),Path("/user/hand/right")};XrActionSetCreateInfo set{XR_TYPE_ACTION_SET_CREATE_INFO};strcpy_s(set.actionSetName,"kfvr_gameplay");strcpy_s(set.localizedActionSetName,"Killing Floor VR");Check(CreateActionSet(instance_,&set,&set_),"Create action set");
  Create(grip_,"grip_pose","Hand pose",XR_ACTION_TYPE_POSE_INPUT);Create(aim_,"aim_pose","Pointer pose",XR_ACTION_TYPE_POSE_INPUT);Create(trigger_,"trigger","Trigger",XR_ACTION_TYPE_FLOAT_INPUT);Create(select_,"select","Select",XR_ACTION_TYPE_BOOLEAN_INPUT);Create(squeeze_,"squeeze","Grip",XR_ACTION_TYPE_FLOAT_INPUT);Create(grab_,"grab","Grip button",XR_ACTION_TYPE_BOOLEAN_INPUT);Create(stick_,"stick","Move and turn",XR_ACTION_TYPE_VECTOR2F_INPUT);Create(menu_,"menu","Menu",XR_ACTION_TYPE_BOOLEAN_INPUT);Create(haptic_,"feedback","Weapon feedback",XR_ACTION_TYPE_VIBRATION_OUTPUT);
  Create(triggerTouch_,"trigger_touch","Index touch",XR_ACTION_TYPE_BOOLEAN_INPUT);Create(thumbTouch_,"thumb_touch","Thumb touch",XR_ACTION_TYPE_BOOLEAN_INPUT);Create(primary_,"primary_button","Magazine release",XR_ACTION_TYPE_BOOLEAN_INPUT);Create(secondary_,"secondary_button","Weapon alternate",XR_ACTION_TYPE_BOOLEAN_INPUT);
  Suggest("/interaction_profiles/khr/simple_controller","/input/select/click",nullptr,nullptr,"/input/menu/click",false,true);
  Suggest("/interaction_profiles/oculus/touch_controller","/input/trigger/value","/input/squeeze/value","/input/thumbstick","/input/menu/click",false,false,true);
  Suggest("/interaction_profiles/valve/index_controller","/input/trigger/value","/input/squeeze/value","/input/thumbstick","/input/b/click");
  Suggest("/interaction_profiles/microsoft/motion_controller","/input/trigger/value","/input/squeeze/click","/input/thumbstick","/input/menu/click",true);
  Suggest("/interaction_profiles/htc/vive_controller","/input/trigger/value","/input/squeeze/click","/input/trackpad","/input/menu/click",true);
  for(unsigned hand=0;hand<2;++hand){XrActionSpaceCreateInfo s{XR_TYPE_ACTION_SPACE_CREATE_INFO};s.subactionPath=paths_[hand];s.poseInActionSpace.orientation.w=1;s.action=grip_;Check(CreateActionSpace(session_,&s,&gripSpaces_[hand]),"Create grip space");s.action=aim_;Check(CreateActionSpace(session_,&s,&aimSpaces_[hand]),"Create aim space");}
  XrSessionActionSetsAttachInfo attach{XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO};attach.countActionSets=1;attach.actionSets=&set_;Check(AttachSessionActionSets(session_,&attach),"Attach controller actions");
 }
 ControllerFrame Sample(XrSpace local,XrTime time,std::uint64_t generation,bool focused){
  ControllerFrame result;result.predictedNs=time;result.spaceGeneration=generation;
  XrActiveActionSet active{set_,XR_NULL_PATH};XrActionsSyncInfo sync{XR_TYPE_ACTIONS_SYNC_INFO};sync.countActiveActionSets=1;sync.activeActionSets=&active;auto status=SyncActions(session_,&sync);if(status==XR_SESSION_NOT_FOCUSED)return result;Check(status,"Sync controller actions");if(!focused)return result;result.focused=true;
  for(unsigned h=0;h<2;++h){auto& hand=result.hands[h];hand.gripValid=Locate(grip_,gripSpaces_[h],local,time,h,hand.gripPose);hand.aimValid=Locate(aim_,aimSpaces_[h],local,time,h,hand.aimPose);
   if(!hand.gripValid)continue;hand.trigger=std::max(Float(trigger_,h),Bool(select_,h)?1.f:0.f);hand.squeeze=std::max(Float(squeeze_,h),Bool(grab_,h)?1.f:0.f);hand.menu=Bool(menu_,h);hand.primary=Bool(primary_,h);hand.secondary=Bool(secondary_,h);Touch(triggerTouch_,h,hand.triggerTouchActive,hand.triggerTouched);Touch(thumbTouch_,h,hand.thumbTouchActive,hand.thumbTouched);
   XrActionStateGetInfo info{XR_TYPE_ACTION_STATE_GET_INFO};info.action=stick_;info.subactionPath=paths_[h];XrActionStateVector2f state{XR_TYPE_ACTION_STATE_VECTOR2F};Check(GetActionStateVector2f(session_,&info,&state),"Read movement action");if(state.isActive&&std::isfinite(state.currentState.x)&&std::isfinite(state.currentState.y)){float length=std::hypot(state.currentState.x,state.currentState.y);float divisor=std::max(1.f,length);hand.stickX=state.currentState.x/divisor;hand.stickY=state.currentState.y/divisor;}
  }return result;
 }
 void Feedback(const ControllerFrame& frame,const std::array<HapticPulse,2>& requests)noexcept{
  for(unsigned h=0;h<2;++h){XrHapticActionInfo info{XR_TYPE_HAPTIC_ACTION_INFO};info.action=haptic_;info.subactionPath=paths_[h];const auto& r=requests[h];
   if(!frame.focused||!frame.hands[h].gripValid){StopHapticFeedback(session_,&info);continue;}
   if(!r.milliseconds||!std::isfinite(r.amplitude)||r.amplitude<=0)continue;
   XrHapticVibration v{XR_TYPE_HAPTIC_VIBRATION};v.amplitude=std::clamp(r.amplitude,0.f,1.f);v.frequency=XR_FREQUENCY_UNSPECIFIED;v.duration=XrDuration(std::min(r.milliseconds,250u))*1000000;ApplyHapticFeedback(session_,&info,reinterpret_cast<const XrHapticBaseHeader*>(&v));
  }
 }
#undef KFVR_ACTION_APIS
};
}
