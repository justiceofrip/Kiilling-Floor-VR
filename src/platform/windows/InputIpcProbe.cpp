#include "kfvr/ipc/FrameChannel.h"
#include <Windows.h>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
using namespace kfvr;
namespace {
void Require(bool okay,const char* why){if(!okay)throw std::runtime_error(why);}
struct Handle {HANDLE h=nullptr;~Handle(){if(h)CloseHandle(h);}};
struct Child {
 Handle process;
 Child(const std::filesystem::path& exe,const std::wstring& token){STARTUPINFOW si{};si.cb=sizeof(si);PROCESS_INFORMATION pi{};std::wstring command=L"\""+exe.wstring()+L"\" --child "+token;Require(CreateProcessW(exe.c_str(),command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,nullptr,&si,&pi)!=0,"Launch isolated input probe child");process.h=pi.hProcess;CloseHandle(pi.hThread);}
 ~Child(){if(process.h&&WaitForSingleObject(process.h,0)==WAIT_TIMEOUT){TerminateProcess(process.h,3);WaitForSingleObject(process.h,2000);}}
 void Wait(){Require(WaitForSingleObject(process.h,5000)==WAIT_OBJECT_0,"Input child timeout");DWORD exit=1;Require(GetExitCodeProcess(process.h,&exit)&&exit==0,"Input child failed");}
};
void Wait(HANDLE event){Require(WaitForSingleObject(event,5000)==WAIT_OBJECT_0,"Input verification handshake timeout");}
std::wstring Name(const std::wstring& token,const wchar_t* suffix){return L"Local\\KFVR.InputTest."+token+suffix;}
constexpr std::uint64_t Generation=0x234567891ULL;constexpr std::int64_t Predicted=0x734567891LL;
bool Neutral(const xr::ControllerFrame& f){if(f.focused)return false;for(const auto& h:f.hands)if(h.gripValid||h.aimValid||h.trigger||h.squeeze||h.stickX||h.stickY||h.menu||h.primary||h.secondary||h.triggerTouchActive||h.triggerTouched||h.thumbTouchActive||h.thumbTouched)return false;return true;}
xr::ControllerFrame Sample(){xr::ControllerFrame f{};f.focused=true;f.spaceGeneration=Generation;f.predictedNs=Predicted;for(unsigned i=0;i<2;++i){auto& h=f.hands[i];h.gripValid=h.aimValid=h.menu=true;h.gripPose.position={float(i)*.5f-.25f,1.4f,-.3f};h.aimPose.position={.2f,1.5f,-.4f};h.primary=h.triggerTouchActive=h.triggerTouched=h.thumbTouchActive=true;h.thumbTouched=(i==1);h.trigger=.7f;h.squeeze=.3f;h.stickX=.6f;h.stickY=.8f;}return f;}
int Produce(const std::wstring& token){
 ipc::FrameChannel endpoint;Require(endpoint.ConnectProducer(token),"Connect input producer");
 Handle go{OpenEventW(SYNCHRONIZE,FALSE,Name(token,L".go").c_str())},ack{OpenEventW(EVENT_MODIFY_STATE,FALSE,Name(token,L".ack").c_str())};Require(go.h&&ack.h,"Open input-test events");SetEvent(ack.h);
 xr::ControllerFrame frame;std::uint64_t sequence=0;
 Wait(go.h);Require(endpoint.ReadInput(frame,sequence)==ipc::ChannelResult::Ok&&frame.focused&&sequence,"Read cross-process controls");
 Require(frame.spaceGeneration==Generation&&frame.predictedNs==Predicted,"Controller 64-bit identity truncated");Require(frame.hands[0].gripValid&&frame.hands[1].aimValid&&frame.hands[0].menu&&frame.hands[1].trigger==.7f&&frame.hands[1].gripPose.position.x==.25f,"Controller values changed in transport");
 Require(frame.hands[0].primary&&frame.hands[0].triggerTouchActive&&frame.hands[0].triggerTouched&&frame.hands[0].thumbTouchActive&&!frame.hands[0].thumbTouched&&frame.hands[1].thumbTouched,"Capacitive/face input changed in transport");
 Require(endpoint.SendHaptics(sequence,{xr::HapticPulse{.8f,25},xr::HapticPulse{2,500}})==ipc::ChannelResult::Ok,"Return haptics");SetEvent(ack.h);
 Wait(go.h);Require(endpoint.ReadInput(frame,sequence)==ipc::ChannelResult::Ok,"Read pending-haptic sample");
 Require(endpoint.SendHaptics(sequence,{xr::HapticPulse{.7f,25},{}})==ipc::ChannelResult::Ok,"Queue left haptic");
 Require(endpoint.SendHaptics(sequence,{xr::HapticPulse{},xr::HapticPulse{.6f,30}})==ipc::ChannelResult::Ok,"Queue right haptic");SetEvent(ack.h);
 Wait(go.h);Require(endpoint.ReadInput(frame,sequence)==ipc::ChannelResult::Ok,"Read next haptic sample");
 Require(endpoint.SendHaptics(sequence,{xr::HapticPulse{},xr::HapticPulse{.4f,15}})==ipc::ChannelResult::Ok,"Queue independent right haptic");SetEvent(ack.h);
 Wait(go.h);const auto contentionStart=GetTickCount64();
 Require(endpoint.ReadInput(frame,sequence)==ipc::ChannelResult::Busy&&Neutral(frame)&&sequence==0,"Contended read did not fail neutral");
 Require(GetTickCount64()-contentionStart<100,"Input mutex wait was unbounded");SetEvent(ack.h);
 Wait(go.h);const auto start=GetTickCount64();bool expired=false;
 while(GetTickCount64()-start<2000){const auto result=endpoint.ReadInput(frame,sequence);if(result==ipc::ChannelResult::Invalid){Require(Neutral(frame)&&sequence==0,"Stale input did not release all controls");expired=true;break;}Sleep(1);}Require(expired,"Host stall did not expire controls");SetEvent(ack.h);
 Wait(go.h);Require(endpoint.ReadInput(frame,sequence)==ipc::ChannelResult::Ok&&Neutral(frame),"Focus loss was not transmitted without an eye frame");SetEvent(ack.h);
 Wait(go.h);Require(endpoint.ReadInput(frame,sequence)==ipc::ChannelResult::Ok&&frame.focused&&frame.spaceGeneration==Generation+1,"Recentered controller space not received");SetEvent(ack.h);
 Wait(go.h);Require(endpoint.ReadInput(frame,sequence)==ipc::ChannelResult::Ok&&Neutral(frame),"Invalid host pose did not publish a neutral release");SetEvent(ack.h);
 Wait(go.h);Require(endpoint.ReadInput(frame,sequence)==ipc::ChannelResult::Closed&&Neutral(frame)&&sequence==0,"Host close did not release controls");SetEvent(ack.h);return 0;
}
void Send(ipc::FrameChannel& host,const xr::ControllerFrame& frame,ipc::ChannelResult expected=ipc::ChannelResult::Ok){const auto start=GetTickCount64();ipc::ChannelResult result;do{result=host.SendInput(frame);if(result!=ipc::ChannelResult::Busy)break;Sleep(1);}while(GetTickCount64()-start<2000);Require(result==expected,"Send controller sample");}
}
int wmain(int argc,wchar_t** argv){try{
 if(argc==3&&std::wstring(argv[1])==L"--child")return Produce(argv[2]);
 Require(argc==3&&std::wstring(argv[1])==L"--producer-exe","Usage: KFVRInputProbe --producer-exe path");const auto exe=std::filesystem::absolute(argv[2]);DWORD type=0;Require(GetBinaryTypeW(exe.c_str(),&type),"Read input producer architecture");
 ipc::FrameChannel host;Require(host.CreateHost(),"Create input host");Handle go{CreateEventW(nullptr,FALSE,FALSE,Name(host.Token(),L".go").c_str())},ack{CreateEventW(nullptr,FALSE,FALSE,Name(host.Token(),L".ack").c_str())};Require(go.h&&ack.h,"Create input-test events");Child child(exe,host.Token());Wait(ack.h);Require(host.Connected(),"Input producer handshake");
 auto frame=Sample();Send(host,frame);SetEvent(go.h);Wait(ack.h);auto feedback=host.ReceiveHaptics();Require(feedback[0].amplitude==.8f&&feedback[0].milliseconds==25&&feedback[1].amplitude==1&&feedback[1].milliseconds==250,"Cross-process haptics lost or out of bounds");Require(host.ReceiveHaptics()[0].milliseconds==0,"Haptic pulse replayed");
 Send(host,frame);SetEvent(go.h);Wait(ack.h);feedback=host.ReceiveHaptics();
 Require(feedback[0].amplitude==.7f&&feedback[0].milliseconds==25&&feedback[1].amplitude==.6f&&feedback[1].milliseconds==30,"Pending hands overwrote each other");
 Require(host.ReceiveHaptics()[0].milliseconds==0,"Merged haptic replayed");
 Send(host,frame);SetEvent(go.h);Wait(ack.h);feedback=host.ReceiveHaptics();
 Require(feedback[0].milliseconds==0&&feedback[1].amplitude==.4f&&feedback[1].milliseconds==15,"Consumed left haptic was replayed by right event");
 Handle contested{OpenMutexW(SYNCHRONIZE|MUTEX_MODIFY_STATE,FALSE,(L"Local\\KFVR.Control.v4."+host.Token()+L".mutex").c_str())};
 Require(contested.h&&WaitForSingleObject(contested.h,100)==WAIT_OBJECT_0,"Hold input mutex for contention test");
 SetEvent(go.h);Wait(ack.h);Require(ReleaseMutex(contested.h),"Release contended input mutex");
 SetEvent(go.h);Wait(ack.h);
 frame.focused=false;Send(host,frame);SetEvent(go.h);Wait(ack.h);
 frame.focused=true;++frame.spaceGeneration;Send(host,frame);SetEvent(go.h);Wait(ack.h);Require(host.ReceiveHaptics()[0].milliseconds==0,"Old feedback survived recenter");
 frame.hands[0].gripPose.position.x=std::numeric_limits<float>::quiet_NaN();Send(host,frame,ipc::ChannelResult::Invalid);SetEvent(go.h);Wait(ack.h);
 host.Close();SetEvent(go.h);Wait(ack.h);child.Wait();
 std::cout<<"{\"state\":\"verified\",\"host_bits\":"<<sizeof(void*)*8<<",\"producer_bits\":"<<(type==SCS_32BIT_BINARY?32:64)<<",\"controller_roundtrip\":true,\"capacitive_roundtrip\":true,\"haptic_roundtrip\":true,\"pending_hands_preserved\":true,\"consumed_feedback_not_replayed\":true,\"input_expiration\":true,\"input_contention_bounded\":true,\"focus_loss_without_eye_frame\":true,\"recenter\":true,\"invalid_pose_release\":true,\"host_close_release\":true,\"game_connected\":false,\"headset_used\":false}\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
