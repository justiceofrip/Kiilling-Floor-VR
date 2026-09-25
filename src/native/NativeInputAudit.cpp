#include "NativeInputAudit.h"
#include <MinHook.h>
#include <atomic>
#include <cstring>
#include <fstream>
#include <cmath>
namespace kfvr::native {
namespace {
using ReadInput=void(__thiscall*)(void*,float);
ReadInput original=nullptr;void* target=nullptr;unsigned viewport=0,controller=0,controllerIndex=0,tickName=0;std::array<unsigned,10> offsets{};std::array<unsigned char,16> bytes{};bool(*world)()=nullptr;
std::atomic<unsigned> tickUsers=0;thread_local bool tickOwned=false;
std::atomic<bool> active=false;std::atomic<unsigned> count=0;
struct Row {std::atomic<bool> ready=false;unsigned thread=0;float dt=0;std::array<float,10> before{},after{},tickBefore{},tickAfter{};bool finite=true,beforeTick=false,afterTick=false;};
std::array<Row,240> rows;thread_local Row* pending=nullptr;
MovementSource movement=nullptr;thread_local bool tickReady=false,applied=false;
thread_local std::array<float,2> saved{};std::atomic<bool> pendingRestore=false;
std::atomic<unsigned> writes=0,restores=0,restoreFailures=0;
struct MovementRow {std::atomic<bool> ready=false;std::uint64_t sequence=0,uptime=0;std::int64_t time=0;bool valid=false,written=false;float forward=0,strafe=0,consumedForward=0,consumedStrafe=0;bool restored=true,finite=true;};
std::array<MovementRow,1500> movementRows;unsigned movementCount=0;thread_local MovementRow* movementRow=nullptr;

bool Read(unsigned at,void* out,SIZE_T size){SIZE_T got=0;return at>=0x10000&&ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(at),out,size,&got)&&got==size;}
bool Write(unsigned at,float value){MEMORY_BASIC_INFORMATION info{};if(!std::isfinite(value)||!VirtualQuery(reinterpret_cast<void*>(at),&info,sizeof(info))||!(info.Protect&(PAGE_READWRITE|PAGE_WRITECOPY)))return false;SIZE_T wrote=0;return WriteProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(at),&value,4,&wrote)&&wrote==4;}
unsigned U32(unsigned at){unsigned v=0;Read(at,&v,4);return v;}
bool Restore(){
 if(U32(controller+4)!=controllerIndex)return false;
 const bool base=Write(controller+offsets[1],saved[0]);
 const bool strafe=Write(controller+offsets[7],saved[1]);return base&&strafe;
}
bool Axes(std::array<float,10>& values){bool okay=true;for(unsigned i=0;i<values.size();++i)okay&=Read(controller+offsets[i],&values[i],4)&&std::isfinite(values[i]);return okay;}
void __fastcall Hook(void* self,void*,float dt){
 Row* row=nullptr;const bool local=reinterpret_cast<unsigned>(self)==viewport;
 if(local)tickReady=false;
 if(active.load(std::memory_order_relaxed)&&reinterpret_cast<unsigned>(self)==viewport&&world()&&U32(viewport+0x34)==controller){
  if(pending){pending->ready.store(true,std::memory_order_release);pending=nullptr;}
  const auto index=count.fetch_add(1);if(index<rows.size()){row=&rows[index];row->thread=GetCurrentThreadId();row->dt=dt;row->finite=Axes(row->before);}
 }
 original(self,dt);
 if(row){row->finite&=Axes(row->after);pending=row;}
 if(local&&active.load()&&world())tickReady=std::isfinite(dt)&&dt>0&&dt<=.25f;
}
}
void SetMovementSource(MovementSource source){movement=source;}
bool StartInputAudit(unsigned view,unsigned pc,const std::array<unsigned,10>& fields,unsigned name,const unsigned char* expected,bool(*validWorld)()){
 viewport=view;controller=pc;controllerIndex=U32(pc+4);offsets=fields;tickName=name;world=validWorld;
 for(auto off:offsets)if(off<44||off>4092)return false;
 const auto engine=GetModuleHandleW(L"Engine.dll");target=reinterpret_cast<void*>(GetProcAddress(engine,"?ReadInput@UViewport@@UAEXM@Z"));MEMORY_BASIC_INFORMATION info{};
 if(!target||!VirtualQuery(target,&info,sizeof(info))||info.AllocationBase!=engine||!Read(reinterpret_cast<unsigned>(target),bytes.data(),bytes.size())||std::memcmp(expected,bytes.data(),16))return false;
 if(MH_CreateHook(target,reinterpret_cast<void*>(&Hook),reinterpret_cast<void**>(&original))!=MH_OK||MH_EnableHook(target)!=MH_OK)return false;active=true;return true;
}
void EndInputAuditEvent() noexcept {
 if(pending){pending->finite&=Axes(pending->tickAfter);pending->afterTick=true;pending->ready.store(true,std::memory_order_release);pending=nullptr;}
 if(movementRow){movementRow->finite=Read(controller+offsets[5],&movementRow->consumedForward,4)&&Read(controller+offsets[7],&movementRow->consumedStrafe,4)&&std::isfinite(movementRow->consumedForward)&&std::isfinite(movementRow->consumedStrafe);}
 if(applied){
  // Only restore the raw axes we own; retain normal gameplay/replication results.
  const bool okay=Restore();if(okay)++restores;else ++restoreFailures;
  if(movementRow)movementRow->restored=okay;
  applied=false;pendingRestore.store(false,std::memory_order_release);
 }
 if(movementRow){movementRow->ready.store(true,std::memory_order_release);movementRow=nullptr;}
 tickReady=false;tickOwned=false;tickUsers.fetch_sub(1,std::memory_order_release);
}
bool BeginInputAuditEvent(void* self,void* function) noexcept {
 if(reinterpret_cast<unsigned>(self)!=controller||U32(reinterpret_cast<unsigned>(function)+0x24)!=tickName||tickOwned)return false;
 tickUsers.fetch_add(1,std::memory_order_acq_rel);
 if(!active.load(std::memory_order_acquire)||!world()){tickUsers.fetch_sub(1,std::memory_order_release);return false;}
 tickOwned=true;

 if(pending){pending->finite&=Axes(pending->tickBefore);pending->beforeTick=true;}
 if(!movement||!tickReady||applied||restoreFailures.load())return true;
 float forward=0,strafe=0;std::uint64_t sequence=0;std::int64_t time=0;
 const bool valid=movement(forward,strafe,sequence,time)&&std::isfinite(forward)&&std::isfinite(strafe)&&std::abs(forward)<=1.001f&&std::abs(strafe)<=1.001f;
 if(movementCount<movementRows.size()){movementRow=&movementRows[movementCount++];movementRow->valid=valid;movementRow->uptime=GetTickCount64();movementRow->sequence=sequence;movementRow->time=time;movementRow->forward=valid?forward:0;movementRow->strafe=valid?strafe:0;}
 if(!valid)return true;
 // Exact reflected FloatProperty layout for this hash-gated Engine build.
 for(unsigned i=0;i<offsets.size();++i)if(offsets[i]!=1348+i*4)return true;
 if(U32(controller+4)!=controllerIndex||!Read(controller+offsets[1],&saved[0],4)||!Read(controller+offsets[7],&saved[1],4)||!std::isfinite(saved[0])||!std::isfinite(saved[1]))return true;
 pendingRestore=true;applied=true;
 const bool okay=Write(controller+offsets[1],6000.f*forward)&&Write(controller+offsets[7],6000.f*strafe);
 if(!okay){const bool restored=Restore();if(!restored)++restoreFailures;if(movementRow)movementRow->restored=restored;applied=false;pendingRestore=false;return true;}
 ++writes;
 if(movementRow)movementRow->written=true;
 return true;
}
bool StopInputAudit() noexcept{
 active=false;const auto deadline=GetTickCount64()+1000;while((tickUsers.load(std::memory_order_acquire)||pendingRestore.load(std::memory_order_acquire))&&GetTickCount64()<deadline)Sleep(1);if(tickUsers.load()||pendingRestore.load()||restoreFailures.load())return false;if(!target)return true;const auto result=MH_DisableHook(target);std::array<unsigned char,16> restored{};
 return (result==MH_OK||result==MH_ERROR_DISABLED)&&Read(reinterpret_cast<unsigned>(target),restored.data(),restored.size())&&restored==bytes;
}
void WriteInputAudit(const std::filesystem::path& folder){
 std::ofstream out(folder/L"native-input.json");out<<std::boolalpha<<"{\"input_values_written\":"<<(writes.load()>0)<<",\"movement_writes\":"<<writes.load()<<",\"movement_restores\":"<<restores.load()<<",\"restore_failures\":"<<restoreFailures.load()<<",\"hook_address\":"<<reinterpret_cast<unsigned>(target)<<",\"samples\":[";bool first=true;
 for(const auto& row:rows){if(!row.ready.load(std::memory_order_acquire))continue;if(!first)out<<',';first=false;
 out<<std::boolalpha<<"{\"thread\":"<<row.thread<<",\"dt\":"<<row.dt<<",\"finite\":"<<row.finite<<",\"before_tick_observed\":"<<row.beforeTick<<",\"after_tick_observed\":"<<row.afterTick;
 for(unsigned phase=0;phase<4;++phase){const char* labels[]={"before_read","after_read","before_tick","after_tick"};const auto& values=phase==0?row.before:phase==1?row.after:phase==2?row.tickBefore:row.tickAfter;out<<",\""<<labels[phase]<<"\":[";for(unsigned i=0;i<values.size();++i){if(i)out<<',';if(std::isfinite(values[i]))out<<values[i];else out<<"null";}out<<']';}out<<'}';
 }out<<"],\"movement\":[";first=true;
 for(const auto& row:movementRows){if(!row.ready.load(std::memory_order_acquire))continue;if(!first)out<<',';first=false;out<<"{\"uptime_ms\":"<<row.uptime<<",\"valid\":"<<row.valid<<",\"written\":"<<row.written<<",\"finite\":"<<row.finite<<",\"sequence\":"<<row.sequence<<",\"time_ns\":"<<row.time<<",\"forward\":"<<row.forward<<",\"strafe\":"<<row.strafe<<",\"consumed_forward\":"<<(row.finite?row.consumedForward:0)<<",\"consumed_strafe\":"<<(row.finite?row.consumedStrafe:0)<<",\"restored\":"<<row.restored<<'}';}out<<"]}\n";
}
}
