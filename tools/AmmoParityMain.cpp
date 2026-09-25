#include "kfvr/gameplay/AmmoMachine.h"
#include <iostream>
using namespace kfvr::gameplay;
unsigned Fold(unsigned hash,unsigned value){return (hash*257+value)&65535;}
int main(){for(unsigned test=0;test<2;++test){AmmoConfig profile;if(test){profile.feed=Feed::Tube;profile.cycle=Cycle::Manual;profile.capacity=6;}
 AmmoMachine machine(profile,test?2:10,test?48:90);unsigned seed=71+test*31,epoch=1,hash=0;
 for(unsigned i=1;i<=10000;++i){seed=(seed*25173+13849)&65535;if(i%97==0){machine.Suspend();machine.Activate(++epoch);}if(i%131==0)machine.Activate(++epoch);
  const auto result=machine.Request({epoch,i,AmmoAction((seed>>8)%8)},i*137);machine.Advance(i*137+100);const auto& s=machine.State();
  for(unsigned value:{unsigned(result),epoch,unsigned(machine.Pending()),s.magazine,s.reserve,s.handMagazine,unsigned(s.inserted),unsigned(s.chamber),unsigned(s.handOccupied),unsigned(s.spent),unsigned(s.discarded),unsigned(s.budget),unsigned(s.revision)})hash=Fold(hash,value);
  if(i%1000==0)std::cout<<"KFVR_AMMO_PARITY case="<<test<<" step="<<i<<" checksum="<<hash<<'\n';
 }
}return 0;}
