#include <Windows.h>
#include <MinHook.h>
#include <cstring>
#include <iostream>
#include <stdexcept>
struct Target {int count=0;__declspec(noinline) void Draw(void* viewport,int blit,unsigned char* hit,int* size){count+=blit;if(viewport==this&&hit&&size){*hit=7;*size=42;}}__declspec(noinline) void Tick(float dt){if(dt==.25f)count+=3;} __declspec(noinline) int Animate(float dt){if(dt==.5f)count+=4;return count;} __declspec(noinline) void Frame(void* actor,void* scene,void* vertices,int stride,int* result,unsigned long flags){if(actor==this&&scene==vertices&&stride==12&&flags==0x80000001)*result=count+9;} __declspec(noinline) void Event(void* function,void* params,void* result){if(function==this)*static_cast<int*>(result)=*static_cast<int*>(params)+7;}};
using Draw=void(__thiscall*)(void*,void*,int,unsigned char*,int*);using Tick=void(__thiscall*)(void*,float);
using Animate=int(__thiscall*)(void*,float);using Frame=void(__thiscall*)(void*,void*,void*,void*,int,int*,unsigned long);using Event=void(__thiscall*)(void*,void*,void*,void*);
Animate animate=nullptr;Frame frame=nullptr;Event event=nullptr;int animations=0,frames=0,events=0;
int __fastcall AnimateHook(void* self,void*,float dt){++animations;return animate(self,dt);}
void __fastcall FrameHook(void* self,void*,void* actor,void* scene,void* vertices,int stride,int* result,unsigned long flags){++frames;frame(self,actor,scene,vertices,stride,result,flags);}
void __fastcall EventHook(void* self,void*,void* function,void* params,void* result){++events;event(self,function,params,result);}
Draw draw=nullptr;Tick tick=nullptr;int draws=0,ticks=0;
void __fastcall DrawHook(void* self,void*,void* viewport,int blit,unsigned char* hit,int* size){++draws;draw(self,viewport,blit,hit,size);}
void __fastcall TickHook(void* self,void*,float dt){++ticks;tick(self,dt);}
using ResetCall=HRESULT(WINAPI*)(void*,unsigned*);
ResetCall resetOriginal=nullptr;unsigned resets=0;
__declspec(noinline) HRESULT WINAPI ResetTarget(void* self,unsigned* args){if(!self||!args)return E_POINTER;*args+=*static_cast<unsigned*>(self);return HRESULT(0x8876086c);}
HRESULT WINAPI ResetDetour(void* self,unsigned* args){++resets;return resetOriginal(self,args);}
template<class T>void* Address(T method){static_assert(sizeof(method)==sizeof(void*));void* p=nullptr;std::memcpy(&p,&method,sizeof(p));return p;}
#define CHECK(x) do{if(!(x))throw std::runtime_error(#x);}while(false)
int main(){try{CHECK(MH_Initialize()==MH_OK);auto d=Address(&Target::Draw),t=Address(&Target::Tick);CHECK(MH_CreateHook(d,reinterpret_cast<void*>(&DrawHook),reinterpret_cast<void**>(&draw))==MH_OK);CHECK(MH_CreateHook(t,reinterpret_cast<void*>(&TickHook),reinterpret_cast<void**>(&tick))==MH_OK);CHECK(MH_EnableHook(d)==MH_OK);CHECK(MH_EnableHook(t)==MH_OK);Target object;unsigned char hit=0;int size=0;auto volatile callDraw=reinterpret_cast<Draw>(d);auto volatile callTick=reinterpret_cast<Tick>(t);callDraw(&object,&object,5,&hit,&size);callTick(&object,.25f);CHECK(draws==1&&ticks==1&&object.count==8&&hit==7&&size==42);CHECK(MH_DisableHook(d)==MH_OK);CHECK(MH_DisableHook(t)==MH_OK);callDraw(&object,&object,2,&hit,&size);CHECK(draws==1&&object.count==10);auto a=Address(&Target::Animate),f=Address(&Target::Frame),e=Address(&Target::Event);
CHECK(MH_CreateHook(a,reinterpret_cast<void*>(&AnimateHook),reinterpret_cast<void**>(&animate))==MH_OK);CHECK(MH_CreateHook(f,reinterpret_cast<void*>(&FrameHook),reinterpret_cast<void**>(&frame))==MH_OK);CHECK(MH_CreateHook(e,reinterpret_cast<void*>(&EventHook),reinterpret_cast<void**>(&event))==MH_OK);
CHECK(MH_EnableHook(a)==MH_OK&&MH_EnableHook(f)==MH_OK&&MH_EnableHook(e)==MH_OK);
auto volatile callAnimate=reinterpret_cast<Animate>(a);auto volatile callFrame=reinterpret_cast<Frame>(f);auto volatile callEvent=reinterpret_cast<Event>(e);
CHECK(callAnimate(&object,.5f)==14);callFrame(&object,&object,&hit,&hit,12,&size,0x80000001);CHECK(size==23&&frames==1&&animations==1);int result=0;callEvent(&object,&object,&size,&result);CHECK(result==30&&events==1);
CHECK(MH_DisableHook(a)==MH_OK&&MH_DisableHook(f)==MH_OK&&MH_DisableHook(e)==MH_OK);callAnimate(&object,.5f);callFrame(&object,&object,&hit,&hit,12,&size,0x80000001);callEvent(&object,&object,&size,&result);CHECK(object.count==18&&size==27&&result==34&&animations==1&&frames==1&&events==1);
auto reset=reinterpret_cast<void*>(&ResetTarget);unsigned char beforeReset[16]{},afterReset[16]{};std::memcpy(beforeReset,reset,16);
CHECK(MH_CreateHook(reset,reinterpret_cast<void*>(&ResetDetour),reinterpret_cast<void**>(&resetOriginal))==MH_OK);CHECK(MH_EnableHook(reset)==MH_OK);
auto volatile callReset=reinterpret_cast<ResetCall>(reset);unsigned deviceValue=9,parameters=13;CHECK(callReset(&deviceValue,&parameters)==HRESULT(0x8876086c)&&parameters==22&&resets==1);
CHECK(MH_DisableHook(reset)==MH_OK);std::memcpy(afterReset,reset,16);CHECK(!std::memcmp(beforeReset,afterReset,16));CHECK(callReset(&deviceValue,&parameters)==HRESULT(0x8876086c)&&parameters==31&&resets==1);
CHECK(MH_Uninitialize()==MH_OK);std::cout<<"x86 thiscall/fastcall native Draw, Tick, animation, GetFrame and ProcessEvent trampoline ABI/restoration passed\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
