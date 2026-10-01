#include "kfvr/ipc/FrameChannel.h"
#include <Windows.h>
#include <objbase.h>
#include <algorithm>
#include <cstring>
#include <limits>
#include <stdexcept>
namespace kfvr::ipc {
namespace {
struct Handle {
    HANDLE h=nullptr;
    ~Handle(){if(h)CloseHandle(h);}
    void Set(HANDLE value){if(h)CloseHandle(h);h=value;}
};
struct alignas(8) Shared {
    std::uint32_t magic=0x3243464b,version=4,bytes=784,reserved0=0;
    std::array<std::uint8_t,16> session{};
    std::uint32_t hostPid=0,producerPid=0;
    std::uint64_t hostStarted=0,producerStarted=0;
    FrameSlot slot{};
    std::array<std::uint64_t,5> reserved{};
    ControlWire input{};HapticWire haptics{};
};
static_assert(sizeof(Shared)==784 && offsetof(Shared,slot)==56);
std::int64_t Now(){LARGE_INTEGER n{};return QueryPerformanceCounter(&n)?n.QuadPart:0;}
std::uint64_t Started(HANDLE process){FILETIME created{},exited{},kernel{},user{};if(!GetProcessTimes(process,&created,&exited,&kernel,&user))return 0;return (std::uint64_t(created.dwHighDateTime)<<32)|created.dwLowDateTime;}
bool TokenBytes(const std::wstring& token,std::array<std::uint8_t,16>& bytes){
    if(token.size()!=32)return false;bool any=false;
    for(unsigned i=0;i<16;++i){unsigned value=0;for(unsigned j=0;j<2;++j){const auto c=token[2*i+j];if(c>=L'0'&&c<=L'9')value=value*16+unsigned(c-L'0');else if(c>=L'a'&&c<=L'f')value=value*16+unsigned(c-L'a'+10);else return false;}bytes[i]=std::uint8_t(value);any|=value!=0;}return any;
}
std::wstring NewToken(){GUID guid{};if(FAILED(CoCreateGuid(&guid)))throw std::runtime_error("Cannot create IPC session");const auto* p=reinterpret_cast<const unsigned char*>(&guid);std::wstring token;for(unsigned i=0;i<16;++i){token+=L"0123456789abcdef"[p[i]>>4];token+=L"0123456789abcdef"[p[i]&15];}return token;}
}
struct FrameChannel::State {
    bool host=false,fault=false,pendingAck=false,ackConsumed=false;
    std::uint64_t active=0,next=0,inputSequence=0,hapticSequence=0,lastHaptic=0;
    graphics::PairTicket delivered{};
    Handle mapping,mutex,response,peer;
    Shared* shared=nullptr;
    std::array<std::uint8_t,16> session{};
    std::int64_t frequency=0;
    ~State(){if(shared)UnmapViewOfFile(shared);}
    bool Header()const {
        if(shared->magic!=0x3243464b||shared->version!=4||shared->bytes!=sizeof(Shared)||shared->session!=session||shared->reserved0||!shared->hostPid||!shared->hostStarted)return false;
        for(auto v:shared->reserved)if(v)return false;
        return std::uint32_t(shared->slot.phase)<=std::uint32_t(Phase::Closed)&&shared->slot.consumed<=1&&(!shared->slot.consumed||shared->slot.completedId);
    }
    struct Lock {
        State& state;bool locked=false;
        explicit Lock(State& s,DWORD timeout=0):state(s){
            if(s.fault||!s.mutex.h||!s.shared)return;
            const auto result=WaitForSingleObject(s.mutex.h,timeout);
            if(result==WAIT_OBJECT_0){locked=true;if(!s.Header()){s.fault=true;s.shared->slot.phase=Phase::Closed;}}
            else if(result==WAIT_ABANDONED){locked=true;s.fault=true;s.shared->slot.phase=Phase::Closed;}
            else if(result!=WAIT_TIMEOUT)s.fault=true;
        }
        explicit operator bool()const{return locked&&!state.fault;}
        ~Lock(){if(locked)ReleaseMutex(state.mutex.h);}
    };
    bool Peer(){ // called under mutex, caches a handle to prevent PID-reuse confusion
        if(fault||shared->slot.phase==Phase::Closed)return false;
        const auto pid=host?shared->producerPid:shared->hostPid;
        const auto started=host?shared->producerStarted:shared->hostStarted;
        if(!pid)return false;
        if(!peer.h){peer.h=OpenProcess(SYNCHRONIZE|PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid);
            if(!peer.h||!started||Started(peer.h)!=started){fault=true;shared->slot.phase=Phase::Closed;return false;}}
        if(WaitForSingleObject(peer.h,0)!=WAIT_TIMEOUT){fault=true;shared->slot.phase=Phase::Closed;return false;}return true;
    }
    void FlushFeedback(){if(pendingAck){ipc::Acknowledge(shared->slot,active,ackConsumed);pendingAck=false;active=0;}}
};
FrameChannel::FrameChannel()=default;
FrameChannel::~FrameChannel(){Close();}
void FrameChannel::Close()noexcept {
    if(state_){auto& s=*state_;{State::Lock lock(s,20);if(lock){s.FlushFeedback();s.shared->slot.phase=Phase::Closed;SetEvent(s.response.h);}}state_.reset();}token_.clear();
}
const std::wstring& FrameChannel::Token()const noexcept{return token_;}
bool FrameChannel::CreateHost()noexcept {
    Close();try {
        auto s=std::make_unique<State>();s->host=true;const auto token=NewToken();if(!TokenBytes(token,s->session))return false;
        const auto prefix=L"Local\\KFVR.Control.v4."+token;
        s->mutex.h=CreateMutexW(nullptr,FALSE,(prefix+L".mutex").c_str());if(!s->mutex.h||GetLastError()==ERROR_ALREADY_EXISTS)return false;
        s->response.h=CreateEventW(nullptr,FALSE,FALSE,(prefix+L".response").c_str());if(!s->response.h||GetLastError()==ERROR_ALREADY_EXISTS)return false;
        s->mapping.h=CreateFileMappingW(INVALID_HANDLE_VALUE,nullptr,PAGE_READWRITE,0,sizeof(Shared),(prefix+L".map").c_str());if(!s->mapping.h||GetLastError()==ERROR_ALREADY_EXISTS)return false;
        s->shared=static_cast<Shared*>(MapViewOfFile(s->mapping.h,FILE_MAP_READ|FILE_MAP_WRITE,0,0,sizeof(Shared)));if(!s->shared)return false;
        LARGE_INTEGER frequency{};if(!QueryPerformanceFrequency(&frequency)||frequency.QuadPart<=0)return false;s->frequency=frequency.QuadPart;
        Shared initial{};initial.session=s->session;initial.hostPid=GetCurrentProcessId();initial.hostStarted=Started(GetCurrentProcess());if(!initial.hostStarted)return false;
        std::memcpy(s->shared,&initial,sizeof(initial));token_=token;state_=std::move(s);return true;
    }catch(...){Close();return false;}
}
bool FrameChannel::ConnectProducer(const std::wstring& token)noexcept {
    Close();try {
        auto s=std::make_unique<State>();if(!TokenBytes(token,s->session))return false;const auto prefix=L"Local\\KFVR.Control.v4."+token;
        s->mutex.h=OpenMutexW(SYNCHRONIZE|MUTEX_MODIFY_STATE,FALSE,(prefix+L".mutex").c_str());if(!s->mutex.h)return false;
        s->response.h=OpenEventW(EVENT_MODIFY_STATE|SYNCHRONIZE,FALSE,(prefix+L".response").c_str());if(!s->response.h)return false;
        s->mapping.h=OpenFileMappingW(FILE_MAP_READ|FILE_MAP_WRITE,FALSE,(prefix+L".map").c_str());if(!s->mapping.h)return false;
        s->shared=static_cast<Shared*>(MapViewOfFile(s->mapping.h,FILE_MAP_READ|FILE_MAP_WRITE,0,0,sizeof(Shared)));if(!s->shared)return false;
        const auto started=Started(GetCurrentProcess());if(!started)return false;
        {State::Lock lock(*s,20);if(!lock||s->shared->producerPid||!s->Peer())return false;
            s->shared->producerStarted=started;s->shared->producerPid=GetCurrentProcessId();}
        token_=token;state_=std::move(s);return true;
    }catch(...){Close();return false;}
}
bool FrameChannel::Connected()noexcept {if(!state_)return false;auto& s=*state_;State::Lock lock(s);return lock&&s.Peer();}
ChannelResult FrameChannel::RequestPair(const runtime::PresentationRequirements& requirements,const runtime::TrackingFrame& tracking,unsigned budgetMs,graphics::TextureDescriptor& d,graphics::PairTicket& t)noexcept {
    if(!state_||!state_->host)return ChannelResult::Closed;
    if(!budgetMs||budgetMs>50)return ChannelResult::Invalid;
    auto& s=*state_;std::int64_t deadline=0;
    {State::Lock lock(s);if(!lock)return s.fault?ChannelResult::Closed:ChannelResult::Busy;
        if(!s.Peer())return s.fault||s.shared->slot.phase==Phase::Closed?ChannelResult::Closed:ChannelResult::Busy;
        s.FlushFeedback();
        if(s.active){if(s.shared->slot.phase==Phase::Delivered)return ChannelResult::Busy;Cancel(s.shared->slot,s.active);s.active=0;}
        if(s.shared->slot.phase!=Phase::Idle)return ChannelResult::Busy;
        const auto now=Now();const auto ticks=(s.frequency/1000)*budgetMs+(s.frequency%1000)*budgetMs/1000;
        if(now<=0||ticks<=0||now>INT64_MAX-ticks||s.next==UINT64_MAX)return ChannelResult::Invalid;
        deadline=now+ticks;FrameRequest request{};
        if(!Encode(requirements,tracking,s.next+1,deadline,request)||!Begin(s.shared->slot,request,now))return ChannelResult::Invalid;
        s.active=++s.next;ResetEvent(s.response.h);
    }
    while(true){
        {State::Lock lock(s);if(lock){
            if(!s.Peer())return ChannelResult::Closed;
            if(Deliver(s.shared->slot,s.active,Now(),d,t)){s.delivered=t;return ChannelResult::Ok;}
            const auto phase=s.shared->slot.phase;
            if(phase==Phase::Idle||phase==Phase::Complete||phase==Phase::Cancelled){s.active=0;return ChannelResult::Timeout;}
        }else if(s.fault)return ChannelResult::Closed;}
        const auto remaining=deadline-Now();if(remaining<=0)break;
        const auto ms=static_cast<DWORD>((std::min)(std::int64_t(budgetMs),(remaining*1000+s.frequency-1)/s.frequency));
        HANDLE waits[]={s.response.h,s.peer.h};const auto result=WaitForMultipleObjects(2,waits,FALSE,ms);
        if(result==WAIT_OBJECT_0+1||result==WAIT_FAILED){s.fault=true;return ChannelResult::Closed;}
    }
    {State::Lock lock(s);if(lock){Cancel(s.shared->slot,s.active);s.active=0;}}
    return ChannelResult::Timeout;
}
void FrameChannel::Feedback(const graphics::PairTicket& ticket,bool consumed)noexcept {
    if(!state_||!state_->host)return;auto& s=*state_;
    if(!s.active||std::memcmp(&s.delivered,&ticket,sizeof(ticket)))return;
    s.pendingAck=true;s.ackConsumed=consumed;State::Lock lock(s);if(lock)s.FlushFeedback();
}
ChannelResult FrameChannel::TryTake(FrameRequest& out)noexcept {
    if(!state_||state_->host)return ChannelResult::Closed;auto& s=*state_;if(s.active)return ChannelResult::Busy;
    State::Lock lock(s);if(!lock)return s.fault?ChannelResult::Closed:ChannelResult::Busy;
    if(!s.Peer())return ChannelResult::Closed;
    if(!Take(s.shared->slot,Now(),out))return ChannelResult::Busy;s.active=out.requestId;return ChannelResult::Ok;
}
ChannelResult FrameChannel::Publish(const graphics::TextureDescriptor& d,const graphics::PairTicket& t)noexcept {
    if(!state_||state_->host)return ChannelResult::Closed;auto& s=*state_;if(!s.active)return ChannelResult::Invalid;
    State::Lock lock(s);if(!lock)return s.fault?ChannelResult::Closed:ChannelResult::Busy;
    if(!s.Peer())return ChannelResult::Closed;
    if(ipc::Publish(s.shared->slot,s.active,Now(),d,t)){SetEvent(s.response.h);return ChannelResult::Ok;}
    const bool expired=s.shared->slot.phase==Phase::Idle;ipc::Skip(s.shared->slot,s.active);s.active=0;SetEvent(s.response.h);
    return expired?ChannelResult::Timeout:ChannelResult::Invalid;
}
ChannelResult FrameChannel::Skip()noexcept {
    if(!state_||state_->host)return ChannelResult::Closed;auto& s=*state_;
    State::Lock lock(s);if(!lock)return s.fault?ChannelResult::Closed:ChannelResult::Busy;
    ipc::Skip(s.shared->slot,s.active);s.active=0;SetEvent(s.response.h);return ChannelResult::Ok;
}
Outcome FrameChannel::PollOutcome()noexcept {
    if(!state_||state_->host)return Outcome::Discarded;auto& s=*state_;
    State::Lock lock(s);if(!lock)return s.fault?Outcome::Discarded:Outcome::Pending;
    if(!s.Peer()){s.active=0;return Outcome::Discarded;}
    const auto outcome=Reap(s.shared->slot,s.active,Now());if(outcome!=Outcome::Pending)s.active=0;return outcome;
}
ChannelResult FrameChannel::SendInput(const xr::ControllerFrame& frame)noexcept{
 if(!state_||!state_->host)return ChannelResult::Closed;auto& s=*state_;State::Lock lock(s);
 if(!lock)return s.fault?ChannelResult::Closed:ChannelResult::Busy;
 if(!s.Peer())return s.fault?ChannelResult::Closed:ChannelResult::Busy;
 const auto now=Now(),lifetime=s.frequency/10;
 if(now<=0||lifetime<=0||now>INT64_MAX-lifetime||s.inputSequence==UINT64_MAX)return ChannelResult::Invalid;
 ControlWire wire{};const auto sequence=++s.inputSequence;const bool valid=EncodeControls(frame,sequence,now+lifetime,wire);
 if(!valid&&!EncodeControls({},sequence,now+lifetime,wire))return ChannelResult::Invalid;
 // A focus or reference-space transition cancels feedback from the old state.
 if(!wire.flags||wire.spaceGeneration!=s.shared->input.spaceGeneration)s.shared->haptics={};
 s.shared->input=wire;return valid?ChannelResult::Ok:ChannelResult::Invalid;
}
ChannelResult FrameChannel::ReadInput(xr::ControllerFrame& frame,std::uint64_t& sequence,InputReadInfo* info)noexcept{
 if(info)*info={};frame={};sequence=0;if(!state_||state_->host)return ChannelResult::Closed;auto& s=*state_;
 // The host holds this mutex briefly while publishing a frame request. A
 // zero-time try-lock can manufacture a tracking-loss edge from that overlap.
 // Bound the input read wait to 1 ms; keep the original wire expiry untouched.
 State::Lock lock(s,1);
 if(!lock)return s.fault?ChannelResult::Closed:ChannelResult::Busy;if(!s.Peer())return ChannelResult::Closed;
 const auto now=Now();if(info)*info={s.shared->input.sequence,now,s.shared->input.expiresQpc};
 return DecodeControls(s.shared->input,now,frame,sequence)?ChannelResult::Ok:ChannelResult::Invalid;
}
ChannelResult FrameChannel::SendHaptics(std::uint64_t sequence,const std::array<xr::HapticPulse,2>& pulses)noexcept{
 if(!state_||state_->host)return ChannelResult::Closed;auto& s=*state_;State::Lock lock(s,1);
 if(!lock)return s.fault?ChannelResult::Closed:ChannelResult::Busy;if(!s.Peer())return ChannelResult::Closed;
 if(sequence!=s.shared->input.sequence||s.hapticSequence==UINT64_MAX)return ChannelResult::Invalid;
 HapticWire wire{};if(!EncodeHaptics(s.shared->input,s.hapticSequence+1,Now(),pulses,wire))return ChannelResult::Invalid;
 CoalesceHaptics(s.shared->input,Now(),s.shared->haptics,wire);
 ++s.hapticSequence;s.shared->haptics=wire;return ChannelResult::Ok;
}
std::array<xr::HapticPulse,2> FrameChannel::ReceiveHaptics()noexcept{
 std::array<xr::HapticPulse,2> pulses{};if(!state_||!state_->host)return pulses;auto& s=*state_;State::Lock lock(s);
 if(lock&&s.Peer()&&DecodeHaptics(s.shared->haptics,s.shared->input,Now(),s.lastHaptic,pulses))s.shared->haptics={};return pulses;
}

}
