#include "kfvr/gameplay/MeleeGate.h"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace kfvr::gameplay;
#define CHECK(x) do{if(!(x))throw std::runtime_error(#x);}while(false)
int main(){try{
 MeleeGate gate;CHECK(!gate.Sample({0,0,0},{},0,true,true).eligible);
 CHECK(!gate.Sample({.1f,0,0},{},.04,true,true).eligible);
 auto hit=gate.Sample({.25f,0,0},{},.08,true,true);CHECK(hit.eligible&&hit.strength<=1&&hit.strength>0);CHECK(hit.from.x==.1f);
 for(int i=1;i<50;++i)CHECK(!gate.Sample({.25f+float(i%2)*.01f,0,0},{},.08+i*.02,true,true).eligible);
 CHECK(!gate.Sample({.05f,0,0},{},1.10,true,true).eligible);
 CHECK(!gate.Sample({.15f,0,0},{},1.14,true,true).eligible);
 CHECK(gate.Sample({.30f,0,0},{},1.18,true,true).eligible);
 // A dropout must not reset the native attack cooldown.
 CHECK(!gate.Sample({},{},1.19,false,true).eligible);
 CHECK(!gate.Sample({},{},1.20,true,true).eligible);
 CHECK(!gate.Sample({.25f,0,0},{},1.25,true,true).eligible);
 CHECK(!gate.Sample({100,0,0},{},2,true,true).eligible);
 CHECK(!gate.Sample({std::numeric_limits<float>::quiet_NaN(),0,0},{},2.1,true,true).eligible);
 MeleeGate waggle;unsigned strikes=0;for(unsigned i=0;i<10000;++i)strikes+=waggle.Sample({.02f*std::sin(float(i)),0,0},{},i*.01,true,true).eligible;CHECK(!strikes);
 MeleeGate teleport;teleport.Sample({},{},0,true,true);CHECK(!teleport.Sample({1,0,0},{},.001,true,true).eligible);
 MeleeGate timing;timing.Sample({},{},1,true,true);CHECK(!timing.Sample({.3f,0,0},{},.99,true,true).eligible);CHECK(!timing.Sample({.3f,0,0},{},1,true,true).eligible);
 std::cout<<"Melee swing, retraction, cooldown, reach, tracking loss, nonfinite input, teleport and 10000 waggle samples passed\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
