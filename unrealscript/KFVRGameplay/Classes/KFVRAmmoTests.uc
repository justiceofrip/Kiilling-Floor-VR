class KFVRAmmoTests extends Commandlet dependson(KFVRAmmoState);
var int Failures, Checks;
function Check(bool Condition, string Message) { Checks++; if (!Condition) { Failures++; Log("KFVR_TEST_FAIL:"@Message); } }
function KFVRAmmoState.AmmoProfile Profile() {
 local KFVRAmmoState.AmmoProfile P;
 P.Capacity=10; P.MaximumRounds=1000; P.RemoveMs=150; P.PrepareMs=150;
 P.InsertMs=250; P.ChargeMs=250; P.ShellMs=300; P.FireMs=200; return P;
}
function KFVRAmmoState NewState(KFVRAmmoState.AmmoProfile P, int Loaded, int Total) {
 local KFVRAmmoState A;
 A=new class'KFVRAmmoState'; Check(A.Initialize(P,Loaded,Total,1),"Initialize native total accounting"); return A;
}
function Perform(KFVRAmmoState A, KFVRAmmoState.EAction Action, int Sequence, int TimeMs) {
 Check(A.Request(Action,A.CurrentEpoch(),Sequence,TimeMs)==Accepted,"Begin timed action");
 A.Advance(TimeMs+600); Check(!A.Pending() && A.Valid(A.Snapshot()),"Complete conserved action");
}
function int Fold(int Hash, int Value) { return (Hash*257+Value)&65535; }
function RunParity(KFVRAmmoState.AmmoProfile P, int CaseNumber) {
 local KFVRAmmoState A;
 local KFVRAmmoState.AmmoSnapshot S;
 local KFVRAmmoState.EResult Result;
 local int Seed, Epoch, Hash, I;
 if(CaseNumber==0) A=NewState(P,10,100); else A=NewState(P,2,50);
 Seed=71+CaseNumber*31; Epoch=1;
 for(I=1;I<=10000;I++) {
  Seed=(Seed*25173+13849)&65535;
  if(I%97==0) { A.Suspend(); Epoch++; A.Activate(Epoch); }
  if(I%131==0) { Epoch++; A.Activate(Epoch); }
  Result=A.Request(EAction((Seed>>8)%8),Epoch,I,I*137); A.Advance(I*137+100); S=A.Snapshot();
  Hash=Fold(Hash,int(Result)); Hash=Fold(Hash,Epoch); Hash=Fold(Hash,int(A.Pending()));
  Hash=Fold(Hash,S.Magazine); Hash=Fold(Hash,S.Reserve); Hash=Fold(Hash,S.HandMagazine);
  Hash=Fold(Hash,int(S.bInserted)); Hash=Fold(Hash,int(S.bChamber)); Hash=Fold(Hash,int(S.bHandOccupied));
  Hash=Fold(Hash,S.Spent); Hash=Fold(Hash,S.Discarded); Hash=Fold(Hash,S.Budget); Hash=Fold(Hash,S.Revision);
  Check(A.Valid(S),"Cross-language action stream conserves ammo");
  if(I%1000==0) Log("KFVR_AMMO_PARITY case=" $ CaseNumber $ " step=" $ I $ " checksum=" $ Hash);
 }
}
event int Main(string Params) {
 local KFVRAmmoState A;
 local KFVRAmmoState.AmmoProfile P;
 local KFVRAmmoState.AmmoSnapshot S;
 local int I, N, Seed, Seq;
 P=Profile(); A=NewState(P,10,50);
 Check(A.KFTotal()==50 && A.KFLoaded()==10,"KF loaded rounds are already included in total");
 Perform(A,RemoveMagazine,1,1000); S=A.Snapshot(); Check(S.bChamber && S.HandMagazine==9 && !S.bInserted,"Magazine removal retains chamber");
 Perform(A,StowMagazine,2,2000); Perform(A,PrepareMagazine,3,3000); Perform(A,InsertMagazine,4,4000);
 S=A.Snapshot(); Check(S.Magazine==10 && S.bChamber && A.KFLoaded()==11 && A.KFTotal()==50,"Tactical reload has full magazine plus chamber");
 Perform(A,Charge,5,5000); S=A.Snapshot(); Check(S.Discarded==1 && S.Magazine==9 && S.bChamber && A.KFTotal()==49,"Racking a live round ejects it");
 Check(A.Request(Fire,1,6,6000)==Completed,"Fire chambered round");
 Check(A.Request(Fire,1,6,7000)==Stale,"Reject replayed shot");
 Check(A.Request(Fire,1,7,6100)==Ineligible,"Preserve fire cadence");
 A.Suspend(); Check(A.Request(Fire,1,8,6200)==Inactive,"Death/switch/drop suspends requests"); Check(A.Activate(2),"New ownership epoch");
 Check(A.Request(Fire,1,9,6200)==Stale,"Reject previous owner's queued action");
 Check(A.Request(Fire,2,1,6200)==Completed,"Resume after cadence deadline");
 Check(A.Request(Fire,2,2,6199)==BadClock,"Reject backwards server time");
 A=NewState(P,0,20); Perform(A,RemoveMagazine,1,1000); Perform(A,StowMagazine,2,2000); Perform(A,PrepareMagazine,3,3000); Perform(A,InsertMagazine,4,4000);
 Check(A.Request(Fire,1,5,5000)==Ineligible,"Empty reload requires charging"); Perform(A,Charge,6,6000); Check(A.Request(Fire,1,7,7000)==Completed,"Charge then fire");
 A=NewState(P,10,50); Check(A.Request(RemoveMagazine,1,1,1000)==Accepted,"Begin removal before ownership change"); Check(A.Activate(2),"Active ownership change accepted"); A.Advance(2000); S=A.Snapshot(); Check(S.bInserted && A.KFLoaded()==10 && !A.Pending(),"Ownership change cancels old unfinished gesture");
 S.Reserve++; Check(!A.Correct(S,2,2100),"Reject ammunition-creating correction"); S=A.Snapshot(); Check(A.Correct(S,2,2100),"Accept conserved trusted correction");
 P.Feed=FeedTube; P.bManualCycle=true; P.Capacity=6; A=NewState(P,1,8);
 Check(A.Request(InsertShell,1,1,1000)==Accepted,"Begin shell insertion"); Check(A.Request(Fire,1,2,1100)==Completed,"Chambered shot interrupts shell insertion"); A.Advance(2000); S=A.Snapshot(); Check(S.Magazine==0 && S.Reserve==7 && S.Spent==1,"Interrupted shell not magically credited");
 Perform(A,InsertShell,3,2100); Perform(A,Charge,4,3100); Check(A.Request(Fire,1,5,4100)==Completed,"Pump then fire"); Check(A.Request(Fire,1,6,4400)==Ineligible,"Manual action cannot auto chamber");
 P=Profile(); A=NewState(P,10,100); Seed=71;
 for(I=1;I<=10000;I++) { Seed=(Seed*25173+13849)&65535; N=(Seed>>8)%8; A.Request(EAction(N),1,I,I*137); A.Advance(I*137+100); Check(A.Valid(A.Snapshot()),"Randomized ammo budget conserved"); }
 A=NewState(P,10,20); Seq=2147483647; Check(A.Request(Fire,1,Seq,1000)==Completed,"Accept final 31-bit sequence"); Check(A.Request(Fire,1,0,1300)==Completed,"Accept wrapped 31-bit sequence"); Check(A.Request(Fire,1,Seq,1600)==Stale,"Reject stale sequence across wrap");
 P=Profile(); RunParity(P,0); P.Feed=FeedTube; P.bManualCycle=true; P.Capacity=6; RunParity(P,1);
 Log("KFVR_TEST_RESULT checks=" $ Checks $ " failures=" $ Failures);
 if(Failures>0) return 1;
 return 0;
}
defaultproperties {
 HelpCmd="KFVRAmmoTests"
 HelpOneLiner="Run KFVR ammunition conservation tests inside the actual KF scripting engine."
 LogToStdout=true
 IsClient=false
 IsServer=false
 IsEditor=false
 LazyLoad=true
 ShowErrorCount=true
 ShowBanner=false
}
