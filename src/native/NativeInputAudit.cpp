#include "NativeInputAudit.h"
#include <MinHook.h>
#include <atomic>
#include <cstring>
#include <fstream>
#include <cmath>
namespace kfvr::native {
namespace {
using ReadInput=void(__thiscall*)(void*,float);
ReadInput original=nullptr;void* target=nullptr;unsigned viewport=0,controller=0,tickName=0;std::array<unsigned,10> offsets{};std::array<unsigned char,16> bytes{};bool(*world)()=nullptr;
std::atomic<bool> active=false;std::atomic<unsigned> count=0;
struct Row {std::atomic<bool> ready=false;unsigned thread=0;float dt=0;std::array<float,10> before{},after{},tickBefore{},tickAfter{};bool finite=true,beforeTick=false,afterTick=false;};
std::array<Row,240> rows;thread_local Row* pending=nullptr;
bool Read(unsigned at,void* out,SIZE_T size){SIZE_T got=0;return at>=0x10000&&ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(at),out,size,&got)&&got==size;}
unsigned U32(unsigned at){unsigned v=0;Read(at,&v,4);return v;}
bool Axes(std::array<float,10>& values){bool okay=true;for(unsigned i=0;i<values.size();++i)okay&=Read(controller+offsets[i],&values[i],4)&&std::isfinite(values[i]);return okay;}
void __fastcall Hook(void* self,void*,float dt){
 Row* row=nullptr;
 if(active.load(std::memory_order_relaxed)&&reinterpret_cast<unsigned>(self)==viewport&&world()&&U32(viewport+0x34)==controller){
  if(pending){pending->ready.store(true,std::memory_order_release);pending=nullptr;}
  const auto index=count.fetch_add(1);if(index<rows.size()){row=&rows[index];row->thread=GetCurrentThreadId();row->dt=dt;row->finite=Axes(row->before);}
 }
 original(self,dt);
 if(row){row->finite&=Axes(row->after);pending=row;}
}
}
bool StartInputAudit(unsigned view,unsigned pc,const std::array<unsigned,10>& fields,unsigned name,const unsigned char* expected,bool(*validWorld)()){
 viewport=view;controller=pc;offsets=fields;tickName=name;world=validWorld;
 for(auto off:offsets)if(off<44||off>4092)return false;
 const auto engine=GetModuleHandleW(L"Engine.dll");target=reinterpret_cast<void*>(GetProcAddress(engine,"?ReadInput@UViewport@@UAEXM@Z"));MEMORY_BASIC_INFORMATION info{};
 if(!target||!VirtualQuery(target,&info,sizeof(info))||info.AllocationBase!=engine||!Read(reinterpret_cast<unsigned>(target),bytes.data(),bytes.size())||std::memcmp(expected,bytes.data(),16))return false;
 if(MH_CreateHook(target,reinterpret_cast<void*>(&Hook),reinterpret_cast<void**>(&original))!=MH_OK||MH_EnableHook(target)!=MH_OK)return false;active=true;return true;
}
void InputAuditEvent(void* self,void* function,bool after) noexcept{
 if(!active.load(std::memory_order_relaxed)||!pending||reinterpret_cast<unsigned>(self)!=controller||U32(reinterpret_cast<unsigned>(function)+0x24)!=tickName||!world())return;
 if(after){pending->finite&=Axes(pending->tickAfter);pending->afterTick=true;pending->ready.store(true,std::memory_order_release);pending=nullptr;}
 else {pending->finite&=Axes(pending->tickBefore);pending->beforeTick=true;}
}
bool StopInputAudit() noexcept{
 active=false;if(!target)return true;const auto result=MH_DisableHook(target);std::array<unsigned char,16> restored{};
 return (result==MH_OK||result==MH_ERROR_DISABLED)&&Read(reinterpret_cast<unsigned>(target),restored.data(),restored.size())&&restored==bytes;
}
void WriteInputAudit(const std::filesystem::path& folder){
 std::ofstream out(folder/L"native-input.json");out<<"{\"input_values_written\":false,\"hook_address\":"<<reinterpret_cast<unsigned>(target)<<",\"samples\":[";bool first=true;
 for(const auto& row:rows){if(!row.ready.load(std::memory_order_acquire))continue;if(!first)out<<',';first=false;
 out<<std::boolalpha<<"{\"thread\":"<<row.thread<<",\"dt\":"<<row.dt<<",\"finite\":"<<row.finite<<",\"before_tick_observed\":"<<row.beforeTick<<",\"after_tick_observed\":"<<row.afterTick;
 for(unsigned phase=0;phase<4;++phase){const char* labels[]={"before_read","after_read","before_tick","after_tick"};const auto& values=phase==0?row.before:phase==1?row.after:phase==2?row.tickBefore:row.tickAfter;out<<",\""<<labels[phase]<<"\":[";for(unsigned i=0;i<values.size();++i){if(i)out<<',';if(std::isfinite(values[i]))out<<values[i];else out<<"null";}out<<']';}out<<'}';
 }out<<"]}\n";
}
}
