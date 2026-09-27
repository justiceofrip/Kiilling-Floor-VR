#include "NativeEyeBridge.h"
#include <MinHook.h>
#include <dxgi1_2.h>
#include <atomic>
#include <cstring>
#include <fstream>
#include <algorithm>
namespace kfvr::native {
namespace {
using Microsoft::WRL::ComPtr;
using Reset=HRESULT(WINAPI*)(IDirect3DDevice9*,D3DPRESENT_PARAMETERS*);
enum class Stage {Idle,Prepare,Rendered,Publish,Feedback,Skip};
struct State {
 ipc::FrameChannel channel;graphics::D3D9CpuPair transfer;
 ComPtr<IDirect3DDevice9> device;ComPtr<ID3D11Device> d11;NativeSurfaces eyes;
 runtime::PresentationRequirements requirements{},allocated{};runtime::TrackingFrame tracking{};
 ipc::FrameRequest request{};graphics::PairTicket ticket{};Stage stage=Stage::Idle;
 std::atomic<bool> stop=false,closed=true;DWORD thread=0;
 void* resetTarget=nullptr;Reset resetOriginal=nullptr;std::array<unsigned char,16> resetBytes{};
 bool resetEnabled=false,resetRestored=true;const char* reason="not_started";
 unsigned levelCallbacks=0,noRequestCallbacks=0,pendingPairCallbacks=0;long long started=0,finished=0;graphics::D3D9TransferTiming timing{};
 unsigned requests=0,rendered=0,published=0,consumed=0,discarded=0,expired=0,inputs=0,resets=0,inputBusy=0,inputInvalid=0;
 std::uint64_t frame=0,epoch=0,lastInput=0;double renderMs=0,renderMax=0,transferMs=0,transferMax=0;
} s;
long long Now(){LARGE_INTEGER q;QueryPerformanceCounter(&q);return q.QuadPart;}
double Milliseconds(long long ticks){LARGE_INTEGER f;QueryPerformanceFrequency(&f);return double(ticks)*1000/double(f.QuadPart);}
bool Read(const void* p,void* out,SIZE_T n){SIZE_T got=0;return ReadProcessMemory(GetCurrentProcess(),p,out,n,&got)&&got==n;}
void Close(const char* reason){
 if(s.closed.load(std::memory_order_relaxed))return;
 s.reason=reason;s.stop=true;s.finished=Now();
 for(auto& eye:s.eyes)eye.Reset();s.transfer.Reset();s.d11.Reset();s.device.Reset();s.channel.Close();
 if(s.resetEnabled){const auto result=MH_DisableHook(s.resetTarget);std::array<unsigned char,16> bytes{};s.resetRestored=(result==MH_OK||result==MH_ERROR_DISABLED)&&Read(s.resetTarget,bytes.data(),bytes.size())&&bytes==s.resetBytes;s.resetEnabled=!s.resetRestored;}
 // Publish statistics only after render-thread resource cleanup. Keep the DLL
 // and MinHook trampolines resident until process exit.
 s.closed.store(true,std::memory_order_release);
}
HRESULT WINAPI ResetHook(IDirect3DDevice9* device,D3DPRESENT_PARAMETERS* parameters){
 if(device==s.device.Get()){
  // D3D9 Reset must run on the device thread. No worker releases default-pool
  // resources; this detour releases them before entering the original Reset.
  if(GetCurrentThreadId()!=s.thread)return D3DERR_INVALIDCALL;
  ++s.resets;Close("device_reset");
 }
 return s.resetOriginal(device,parameters);
}
bool ProtectReset(){
 if(s.resetEnabled)return true;
 void** table=nullptr;if(!Read(s.device.Get(),&table,sizeof(table))||!Read(table+16,&s.resetTarget,sizeof(void*)))return false;
 MEMORY_BASIC_INFORMATION info{};
 if(!VirtualQuery(s.resetTarget,&info,sizeof(info))||info.AllocationBase!=GetModuleHandleW(L"d3d9.dll")||!(info.Protect&(PAGE_EXECUTE|PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE))||!Read(s.resetTarget,s.resetBytes.data(),s.resetBytes.size()))return false;
 if(MH_CreateHook(s.resetTarget,reinterpret_cast<void*>(&ResetHook),reinterpret_cast<void**>(&s.resetOriginal))!=MH_OK)return false;
 if(MH_EnableHook(s.resetTarget)!=MH_OK)return false;s.resetEnabled=true;s.resetRestored=false;return true;
}
bool Prepare(){
 LUID luid{};if(FAILED(graphics::D3D9CpuPair::AdapterLuid(s.device.Get(),luid))||luid.LowPart!=s.requirements.adapterLow||luid.HighPart!=s.requirements.adapterHigh)return false;
 if(!ProtectReset())return false;
 if(!s.d11){
  ComPtr<IDXGIFactory1> factory;if(FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory))))return false;
  ComPtr<IDXGIAdapter1> chosen;
  for(UINT i=0;;++i){ComPtr<IDXGIAdapter1> adapter;if(factory->EnumAdapters1(i,&adapter)!=S_OK)break;DXGI_ADAPTER_DESC1 desc{};if(SUCCEEDED(adapter->GetDesc1(&desc))&&desc.AdapterLuid.LowPart==luid.LowPart&&desc.AdapterLuid.HighPart==luid.HighPart){chosen=adapter;break;}}
  if(!chosen)return false;const D3D_FEATURE_LEVEL levels[]={D3D_FEATURE_LEVEL_11_0};D3D_FEATURE_LEVEL level{};
  if(FAILED(D3D11CreateDevice(chosen.Get(),D3D_DRIVER_TYPE_UNKNOWN,nullptr,D3D11_CREATE_DEVICE_BGRA_SUPPORT,levels,1,D3D11_SDK_VERSION,&s.d11,&level,nullptr)))return false;
 }
 s.transfer.Reset();if(s.transfer.Create(s.device.Get(),s.d11.Get(),s.requirements.width,s.requirements.height,D3DFMT_A8R8G8B8,DXGI_FORMAT(s.requirements.format),++s.epoch)!=graphics::TransferResult::Ok)return false;
 s.allocated=s.requirements;return true;
}
void Reap(){
 if(s.stage!=Stage::Feedback)return;const auto outcome=s.channel.PollOutcome();if(outcome==ipc::Outcome::Pending)return;
 const bool consumed=outcome==ipc::Outcome::Consumed;
 if(!s.transfer.PairConsumed(s.ticket,consumed)){Close("feedback_failed");return;}
 if(consumed)++s.consumed;else ++s.discarded;s.stage=Stage::Idle;
}
void Skip(){
 const auto result=s.channel.Skip();if(result==ipc::ChannelResult::Busy){s.stage=Stage::Skip;return;}
 if(result==ipc::ChannelResult::Closed){Close("host_closed");return;}s.stage=Stage::Idle;
}
}
bool StartEyeBridge(const wchar_t* token){
 if(!s.channel.ConnectProducer(token))return false;s.reason="running";s.closed=false;return true;
}
bool ReadEyeBridgeControls(xr::ControllerFrame& controls,std::uint64_t& sequence) noexcept{
 controls={};sequence=0;
 if(s.closed.load(std::memory_order_acquire)||s.stop.load(std::memory_order_acquire)||s.thread!=GetCurrentThreadId())return false;
 const auto result=s.channel.ReadInput(controls,sequence);
 if(result==ipc::ChannelResult::Busy)++s.inputBusy;
 if(result==ipc::ChannelResult::Invalid)++s.inputInvalid;
 return result==ipc::ChannelResult::Ok;
}
bool SendEyeBridgeHaptics(std::uint64_t sequence,const std::array<xr::HapticPulse,2>& pulses) noexcept {
 if(s.closed.load(std::memory_order_acquire)||s.stop.load(std::memory_order_acquire)||s.thread!=GetCurrentThreadId())return false;
 return s.channel.SendHaptics(sequence,pulses)==ipc::ChannelResult::Ok;
}
void StopEyeBridge() noexcept{s.stop.store(true,std::memory_order_release);}
bool EyeBridgeClosed() noexcept{return s.closed.load(std::memory_order_acquire);}
void EyeBridgeLevel(void* scene,void* ri,IDirect3DDevice9* device,EyeRenderer render) noexcept {try{
 if(s.closed.load(std::memory_order_acquire)||s.stop.load(std::memory_order_acquire))return;
 if(!s.thread)s.thread=GetCurrentThreadId();if(s.thread!=GetCurrentThreadId()){s.stop=true;return;}
 if(s.device&&s.device.Get()!=device){Close("device_changed");return;}
 ++s.levelCallbacks;if(!s.started)s.started=Now();
 Reap();if(s.closed.load()||s.stage!=Stage::Idle){++s.pendingPairCallbacks;return;}
 const auto taken=s.channel.TryTake(s.request);
 if(taken==ipc::ChannelResult::Closed){Close("host_closed");return;}if(taken!=ipc::ChannelResult::Ok){++s.noRequestCallbacks;return;}
 ++s.requests;
 if(!ipc::Decode(s.request,s.requirements,s.tracking)||s.requirements.width>4096||s.requirements.height>4096||std::uint64_t(s.requirements.width)*s.requirements.height*40>256*1024*1024){Close("unsupported_request");return;}
 if(!s.device)s.device=device;
 if(std::memcmp(&s.requirements,&s.allocated,sizeof(s.allocated))){s.stage=Stage::Prepare;return;}
 if(Now()>=s.request.deadlineQpc){++s.expired;Skip();return;}
 const auto start=Now();const bool okay=render(scene,ri,s.requirements,s.tracking,s.eyes);const auto ms=Milliseconds(Now()-start);s.renderMs+=ms;s.renderMax=std::max(s.renderMax,ms);
 if(!okay){Close("native_render_or_restoration_failed");return;}
 ++s.rendered;s.stage=Stage::Rendered;
}catch(...){Close("level_exception");}}
void EyeBridgePresent(bool worldValid) noexcept {try{
 if(s.closed.load(std::memory_order_acquire))return;
 if(!s.thread)s.thread=GetCurrentThreadId();if(s.thread!=GetCurrentThreadId()){s.stop=true;return;}
 if(s.stop.load(std::memory_order_acquire)||!worldValid){Close(worldValid?"duration_finished":"world_changed");return;}
 xr::ControllerFrame controls{};std::uint64_t sequence=0;
 if(s.channel.ReadInput(controls,sequence)==ipc::ChannelResult::Ok&&sequence!=s.lastInput){s.lastInput=sequence;++s.inputs;}
 Reap();if(s.closed.load())return;
 if(s.stage==Stage::Prepare){if(!Prepare()){Close("transport_setup_failed");return;}Skip();return;}
 if(s.stage==Stage::Skip){Skip();return;}
 if(s.stage==Stage::Rendered){
  if(Now()>=s.request.deadlineQpc){for(auto& eye:s.eyes)eye.Reset();++s.expired;Skip();return;}
  const graphics::FrameMetadata metadata{++s.frame,s.tracking.spaceGeneration,s.tracking.generation,s.tracking.predictedNs};
  const auto result=s.transfer.Publish({s.eyes[0].Get(),s.eyes[1].Get()},metadata,s.ticket);
  for(auto& eye:s.eyes)eye.Reset();
  if(result!=graphics::TransferResult::Ok){Close("pixel_transfer_failed");return;}
  const auto timing=s.transfer.LastTiming();s.timing.resolveMs+=timing.resolveMs;s.timing.readbackMs+=timing.readbackMs;s.timing.convertMs+=timing.convertMs;s.timing.uploadMs+=timing.uploadMs;s.timing.publishMs+=timing.publishMs;
  const auto ms=s.transfer.LastTransferMs();s.transferMs+=ms;s.transferMax=std::max(s.transferMax,ms);s.stage=Stage::Publish;
 }
 if(s.stage==Stage::Publish){
  const auto result=s.channel.Publish(s.transfer.Descriptor(),s.ticket);
  if(result==ipc::ChannelResult::Busy)return;
  if(result==ipc::ChannelResult::Ok){++s.published;s.stage=Stage::Feedback;return;}
  s.transfer.PairConsumed(s.ticket,false);++s.discarded;
  if(result==ipc::ChannelResult::Closed){Close("host_closed");return;}Skip();
 }
}catch(...){Close("present_exception");}}
void WriteEyeBridgeReport(const std::filesystem::path& folder){
 std::ofstream out(folder/L"native-bridge.json");if(!EyeBridgeClosed()){out<<"{\"closed\":false,\"cleanup_pending_on_graphics_thread\":true}\n";return;}
 out<<std::boolalpha<<"{\"closed\":true,\"reason\":\""<<s.reason<<"\",\"reset_hook_restored\":"<<s.resetRestored<<",\"resources_released\":true,\"requests\":"<<s.requests<<",\"rendered_pairs\":"<<s.rendered<<",\"published_pairs\":"<<s.published<<",\"consumed_pairs\":"<<s.consumed<<",\"discarded_pairs\":"<<s.discarded<<",\"expired_before_transfer\":"<<s.expired<<",\"controller_samples\":"<<s.inputs<<",\"input_busy_reads\":"<<s.inputBusy<<",\"input_invalid_reads\":"<<s.inputInvalid<<",\"device_resets\":"<<s.resets<<",\"render_total_ms\":"<<s.renderMs<<",\"render_max_ms\":"<<s.renderMax<<",\"transfer_total_ms\":"<<s.transferMs<<",\"transfer_max_ms\":"<<s.transferMax<<",\"elapsed_ms\":"<<Milliseconds(s.finished-s.started)<<",\"level_callbacks\":"<<s.levelCallbacks<<",\"no_request_callbacks\":"<<s.noRequestCallbacks<<",\"pending_pair_callbacks\":"<<s.pendingPairCallbacks<<",\"resolve_total_ms\":"<<s.timing.resolveMs<<",\"readback_total_ms\":"<<s.timing.readbackMs<<",\"convert_total_ms\":"<<s.timing.convertMs<<",\"upload_total_ms\":"<<s.timing.uploadMs<<",\"publish_total_ms\":"<<s.timing.publishMs<<",\"world_scale_calibrated\":false}\n";
}
}
