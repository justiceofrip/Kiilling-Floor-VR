#include <Windows.h>
#include <MinHook.h>
#include <cstring>
#include <iostream>
#include <stdexcept>
struct Target {int count=0;__declspec(noinline) void Draw(void* viewport,int blit,unsigned char* hit,int* size){count+=blit;if(viewport==this&&hit&&size){*hit=7;*size=42;}}__declspec(noinline) void Tick(float dt){if(dt==.25f)count+=3;}};
using Draw=void(__thiscall*)(void*,void*,int,unsigned char*,int*);using Tick=void(__thiscall*)(void*,float);
Draw draw=nullptr;Tick tick=nullptr;int draws=0,ticks=0;
void __fastcall DrawHook(void* self,void*,void* viewport,int blit,unsigned char* hit,int* size){++draws;draw(self,viewport,blit,hit,size);}
void __fastcall TickHook(void* self,void*,float dt){++ticks;tick(self,dt);}
template<class T>void* Address(T method){static_assert(sizeof(method)==sizeof(void*));void* p=nullptr;std::memcpy(&p,&method,sizeof(p));return p;}
#define CHECK(x) do{if(!(x))throw std::runtime_error(#x);}while(false)
int main(){try{CHECK(MH_Initialize()==MH_OK);auto d=Address(&Target::Draw),t=Address(&Target::Tick);CHECK(MH_CreateHook(d,reinterpret_cast<void*>(&DrawHook),reinterpret_cast<void**>(&draw))==MH_OK);CHECK(MH_CreateHook(t,reinterpret_cast<void*>(&TickHook),reinterpret_cast<void**>(&tick))==MH_OK);CHECK(MH_EnableHook(d)==MH_OK);CHECK(MH_EnableHook(t)==MH_OK);Target object;unsigned char hit=0;int size=0;auto volatile callDraw=reinterpret_cast<Draw>(d);auto volatile callTick=reinterpret_cast<Tick>(t);callDraw(&object,&object,5,&hit,&size);callTick(&object,.25f);CHECK(draws==1&&ticks==1&&object.count==8&&hit==7&&size==42);CHECK(MH_DisableHook(d)==MH_OK);CHECK(MH_DisableHook(t)==MH_OK);callDraw(&object,&object,2,&hit,&size);CHECK(draws==1&&object.count==10);CHECK(MH_Uninitialize()==MH_OK);std::cout<<"x86 thiscall/fastcall native Draw and Tick trampoline ABI/restoration passed\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
