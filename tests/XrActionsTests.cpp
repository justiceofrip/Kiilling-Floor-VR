#include "Test.h"
#include "Actions.h"
#include <limits>
#include <map>
#include <set>
#include <string>
#include <vector>
using namespace kfvr::xr;
namespace {
template<class T> T Handle(unsigned value){return (T)(std::uintptr_t)value;}
struct Action {std::string name;XrActionType type;};
struct Binding {std::string action,path;};
struct Space {XrAction action;XrPath hand;};
struct Mock {
 std::map<std::string,XrPath> paths;
 std::map<XrAction,Action> actions;
 std::map<XrSpace,Space> spaces;
 std::map<std::string,std::vector<Binding>> profiles;
 unsigned next=1,createdSets=0,destroyedSets=0,createdSpaces=0,destroyedSpaces=0,attached=0;
 unsigned poseReads=0,locates=0,applied=0,stopped=0,failSpace=0;
 bool valid=true,active=true,gripActive=true;
 XrResult sync=XR_SUCCESS;
 std::string unsupported,missing;
 float trigger=.4f,squeeze=.7f;bool select=false,grab=false,menu=true;
 XrVector2f stick{3,4};
 XrPosef grip{{0,0,0,1},{.25f,1.2f,-.3f}},aim{{0,0,0,1},{.3f,1.25f,-.4f}};
 XrSpaceLocationFlags gripFlags=XR_SPACE_LOCATION_POSITION_VALID_BIT|XR_SPACE_LOCATION_ORIENTATION_VALID_BIT,aimFlags=gripFlags;
 XrTime time=0;XrSpace base=XR_NULL_HANDLE;
 std::vector<XrHapticVibration> vibrations;
 std::string Path(XrPath p)const{for(const auto& [name,id]:paths)if(id==p)return name;return {};}
} m;
#define VALID(x) do {m.valid&=bool(x);} while(false)
XrResult XRAPI_CALL StringToPath(XrInstance,const char* s,XrPath* p){auto [it,added]=m.paths.emplace(s,m.next);if(added)++m.next;*p=it->second;return XR_SUCCESS;}
XrResult XRAPI_CALL CreateActionSet(XrInstance,const XrActionSetCreateInfo* i,XrActionSet* p){VALID(i->type==XR_TYPE_ACTION_SET_CREATE_INFO);*p=Handle<XrActionSet>(m.next++);++m.createdSets;return XR_SUCCESS;}
XrResult XRAPI_CALL DestroyActionSet(XrActionSet){VALID(m.spaces.empty());++m.destroyedSets;m.actions.clear();return XR_SUCCESS;}
XrResult XRAPI_CALL CreateAction(XrActionSet,const XrActionCreateInfo* i,XrAction* p){VALID(i->type==XR_TYPE_ACTION_CREATE_INFO&&i->countSubactionPaths==2);VALID(m.Path(i->subactionPaths[0])=="/user/hand/left"&&m.Path(i->subactionPaths[1])=="/user/hand/right");*p=Handle<XrAction>(m.next++);m.actions.emplace(*p,Action{i->actionName,i->actionType});return XR_SUCCESS;}
XrResult XRAPI_CALL SuggestInteractionProfileBindings(XrInstance,const XrInteractionProfileSuggestedBinding* i){VALID(i->type==XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING);const auto profile=m.Path(i->interactionProfile);auto& bindings=m.profiles[profile];for(unsigned n=0;n<i->countSuggestedBindings;++n){const auto& b=i->suggestedBindings[n];VALID(m.actions.contains(b.action));bindings.push_back({m.actions.at(b.action).name,m.Path(b.binding)});}return profile==m.unsupported?XR_ERROR_PATH_UNSUPPORTED:XR_SUCCESS;}
XrResult XRAPI_CALL CreateActionSpace(XrSession,const XrActionSpaceCreateInfo* i,XrSpace* p){if(m.failSpace&&m.createdSpaces+1==m.failSpace)return XR_ERROR_OUT_OF_MEMORY;VALID(i->type==XR_TYPE_ACTION_SPACE_CREATE_INFO&&i->poseInActionSpace.orientation.w==1);*p=Handle<XrSpace>(m.next++);m.spaces.emplace(*p,Space{i->action,i->subactionPath});++m.createdSpaces;return XR_SUCCESS;}
XrResult XRAPI_CALL DestroySpace(XrSpace p){VALID(m.spaces.erase(p)==1);++m.destroyedSpaces;return XR_SUCCESS;}
XrResult XRAPI_CALL AttachSessionActionSets(XrSession,const XrSessionActionSetsAttachInfo* i){VALID(i->type==XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO&&i->countActionSets==1&&m.spaces.size()==4);++m.attached;return XR_SUCCESS;}
XrResult XRAPI_CALL SyncActions(XrSession,const XrActionsSyncInfo* i){VALID(i->type==XR_TYPE_ACTIONS_SYNC_INFO&&i->countActiveActionSets==1);return m.sync;}
XrResult XRAPI_CALL GetActionStatePose(XrSession,const XrActionStateGetInfo* i,XrActionStatePose* s){VALID(s->type==XR_TYPE_ACTION_STATE_POSE);++m.poseReads;s->isActive=m.active&&(m.actions.at(i->action).name!="grip_pose"||m.gripActive);return XR_SUCCESS;}
XrResult XRAPI_CALL GetActionStateFloat(XrSession,const XrActionStateGetInfo* i,XrActionStateFloat* s){VALID(s->type==XR_TYPE_ACTION_STATE_FLOAT);s->isActive=m.active;s->currentState=m.actions.at(i->action).name=="trigger"?m.trigger:m.squeeze;return XR_SUCCESS;}
XrResult XRAPI_CALL GetActionStateBoolean(XrSession,const XrActionStateGetInfo* i,XrActionStateBoolean* s){VALID(s->type==XR_TYPE_ACTION_STATE_BOOLEAN);s->isActive=m.active;const auto& name=m.actions.at(i->action).name;s->currentState=name=="select"?m.select:name=="grab"?m.grab:m.menu;return XR_SUCCESS;}
XrResult XRAPI_CALL GetActionStateVector2f(XrSession,const XrActionStateGetInfo*,XrActionStateVector2f* s){VALID(s->type==XR_TYPE_ACTION_STATE_VECTOR2F);s->isActive=m.active;s->currentState=m.stick;return XR_SUCCESS;}
XrResult XRAPI_CALL LocateSpace(XrSpace p,XrSpace base,XrTime time,XrSpaceLocation* s){VALID(s->type==XR_TYPE_SPACE_LOCATION);++m.locates;m.time=time;m.base=base;const bool grip=m.actions.at(m.spaces.at(p).action).name=="grip_pose";s->pose=grip?m.grip:m.aim;s->locationFlags=grip?m.gripFlags:m.aimFlags;return XR_SUCCESS;}
XrResult XRAPI_CALL ApplyHapticFeedback(XrSession,const XrHapticActionInfo* i,const XrHapticBaseHeader* header){VALID(i->type==XR_TYPE_HAPTIC_ACTION_INFO&&header->type==XR_TYPE_HAPTIC_VIBRATION);++m.applied;m.vibrations.push_back(*reinterpret_cast<const XrHapticVibration*>(header));return XR_SUCCESS;}
XrResult XRAPI_CALL StopHapticFeedback(XrSession,const XrHapticActionInfo*){++m.stopped;return XR_SUCCESS;}
XrResult XRAPI_CALL Resolve(XrInstance,const char* name,PFN_xrVoidFunction* fn){*fn=nullptr;if(name==m.missing)return XR_ERROR_FUNCTION_UNSUPPORTED;
#define API(n) if(std::strcmp(name,"xr" #n)==0){*fn=reinterpret_cast<PFN_xrVoidFunction>(n);return XR_SUCCESS;}
 API(StringToPath) API(CreateActionSet) API(DestroyActionSet) API(CreateAction) API(SuggestInteractionProfileBindings) API(CreateActionSpace) API(DestroySpace) API(AttachSessionActionSets) API(SyncActions) API(GetActionStatePose) API(GetActionStateFloat) API(GetActionStateBoolean) API(GetActionStateVector2f) API(LocateSpace) API(ApplyHapticFeedback) API(StopHapticFeedback)
#undef API
 return XR_ERROR_FUNCTION_UNSUPPORTED;
}
void Start(Actions& actions){actions.Start(Resolve,Handle<XrInstance>(1000),Handle<XrSession>(1001));}
bool Has(const std::vector<Binding>& bindings,const char* action,const char* path){for(const auto& b:bindings)if(b.action==action&&b.path==path)return true;return false;}
bool Neutral(const ControllerFrame& f){for(const auto& h:f.hands)if(h.gripValid||h.aimValid||h.trigger||h.squeeze||h.stickX||h.stickY||h.menu)return false;return !f.focused;}
}
int main(){
 m={};
 {
  Actions actions;Start(actions);CHECK(m.valid&&m.attached==1&&m.actions.size()==9&&actions.suggestedProfiles==5);
  CHECK(m.profiles.size()==5);
  for(const auto& [profile,bindings]:m.profiles){(void)profile;CHECK(Has(bindings,"grip_pose","/user/hand/left/input/grip/pose"));CHECK(Has(bindings,"aim_pose","/user/hand/right/input/aim/pose"));CHECK(Has(bindings,"feedback","/user/hand/right/output/haptic"));for(const auto& b:bindings)if(b.action=="menu")CHECK(b.path.starts_with("/user/hand/left/"));}
  CHECK(Has(m.profiles.at("/interaction_profiles/khr/simple_controller"),"select","/user/hand/left/input/select/click"));
  CHECK(Has(m.profiles.at("/interaction_profiles/oculus/touch_controller"),"squeeze","/user/hand/left/input/squeeze/value"));
  CHECK(Has(m.profiles.at("/interaction_profiles/valve/index_controller"),"menu","/user/hand/left/input/b/click"));
  CHECK(Has(m.profiles.at("/interaction_profiles/microsoft/motion_controller"),"grab","/user/hand/right/input/squeeze/click"));
  CHECK(Has(m.profiles.at("/interaction_profiles/htc/vive_controller"),"stick","/user/hand/right/input/trackpad"));
  constexpr XrTime predicted=9223372000000000LL;const auto base=Handle<XrSpace>(1002);
  auto frame=actions.Sample(base,predicted,22,true);CHECK(frame.focused&&frame.predictedNs==predicted&&frame.spaceGeneration==22);CHECK(m.time==predicted&&m.base==base&&m.locates==4);
  for(const auto& hand:frame.hands){CHECK(hand.gripValid&&hand.aimValid&&hand.menu);CHECK(Near(hand.gripPose.position.x,.25f)&&Near(hand.aimPose.position.x,.3f));CHECK(Near(hand.trigger,.4f)&&Near(hand.squeeze,.7f));CHECK(Near(hand.stickX,.6f)&&Near(hand.stickY,.8f));}
  actions.Feedback(frame,{HapticPulse{2,500},HapticPulse{.3f,12}});CHECK(m.applied==2&&m.vibrations[0].amplitude==1&&m.vibrations[0].duration==250000000);CHECK(m.vibrations[1].duration==12000000&&Near(m.vibrations[1].amplitude,.3f));
  const float nan=std::numeric_limits<float>::quiet_NaN();
  actions.Feedback(frame,{HapticPulse{nan,20},HapticPulse{-1,20}});CHECK(m.applied==2);
  m.trigger=nan;m.squeeze=5;m.stick.x=nan;frame=actions.Sample(base,predicted,22,true);CHECK(frame.hands[0].trigger==0&&frame.hands[0].squeeze==1&&frame.hands[0].stickX==0&&frame.hands[0].stickY==0);
  m.select=m.grab=true;frame=actions.Sample(base,predicted,22,true);CHECK(frame.hands[0].trigger==1&&frame.hands[0].squeeze==1);
  m.grip.position.x=nan;frame=actions.Sample(base,predicted,22,true);CHECK(!frame.hands[0].gripValid&&frame.hands[0].trigger==0&&!frame.hands[0].menu&&std::isfinite(frame.hands[0].gripPose.position.x));actions.Feedback(frame,{HapticPulse{1,20},HapticPulse{1,20}});CHECK(m.stopped==2&&m.applied==2);
  m.grip.position.x=.25f;m.grip.orientation={0,0,0,0};frame=actions.Sample(base,predicted,22,true);CHECK(!frame.hands[1].gripValid&&frame.hands[1].squeeze==0);
  m.grip.orientation.w=1;m.aimFlags=0;frame=actions.Sample(base,predicted,22,true);CHECK(frame.hands[0].gripValid&&!frame.hands[0].aimValid);
  m.gripActive=false;const auto reads=m.locates;frame=actions.Sample(base,predicted,22,true);CHECK(!frame.hands[0].gripValid&&m.locates==reads+2&&frame.hands[0].trigger==0);
  const auto poses=m.poseReads;frame=actions.Sample(base,predicted,23,false);CHECK(Neutral(frame)&&frame.spaceGeneration==23&&m.poseReads==poses);actions.Feedback(frame,{HapticPulse{1,20},HapticPulse{1,20}});CHECK(m.stopped==4);
  m.sync=XR_SESSION_NOT_FOCUSED;CHECK(Neutral(actions.Sample(base,predicted,23,true)));
  m.sync=XR_ERROR_SESSION_LOST;bool failed=false;try{actions.Sample(base,predicted,23,false);}catch(const std::exception&){failed=true;}CHECK(failed);
  actions.Reset();actions.Reset();CHECK(m.destroyedSpaces==4&&m.destroyedSets==1&&m.valid);
  m.sync=XR_SUCCESS;Start(actions);CHECK(actions.suggestedProfiles==5&&m.createdSets==2);
 }
 CHECK(m.destroyedSpaces==8&&m.destroyedSets==2&&m.valid);
 m={};m.unsupported="/interaction_profiles/valve/index_controller";{Actions a;Start(a);CHECK(a.suggestedProfiles==4);}CHECK(m.valid&&m.destroyedSets==1);
 m={};m.failSpace=3;bool failed=false;{Actions a;try{Start(a);}catch(const std::exception&){failed=true;}}CHECK(failed&&m.createdSpaces==2&&m.destroyedSpaces==2&&m.destroyedSets==1&&m.valid);
 m={};m.missing="xrCreateActionSpace";failed=false;{Actions a;try{Start(a);}catch(const std::exception&){failed=true;}}CHECK(failed&&m.createdSets==0&&m.destroyedSets==0);
 std::puts("XR actions: profiles, prediction, focus, invalid tracking, haptics, errors and partial cleanup verified (mock runtime)");
 return 0;
}
