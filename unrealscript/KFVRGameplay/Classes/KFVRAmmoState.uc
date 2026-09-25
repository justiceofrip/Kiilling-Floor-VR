// KFVR authoritative ammunition policy. No client RPC accepts round counts.
// Caller owns this object on the server and supplies trusted monotonic time.
// KF TotalAmmo includes loaded rounds; Initialize takes that native total.
class KFVRAmmoState extends Object;

enum EFeed { FeedDetachable, FeedTube };
enum EAction { RemoveMagazine, StowMagazine, PrepareMagazine, InsertMagazine, Charge, InsertShell, Fire, DiscardMagazine };
enum EResult { Accepted, Completed, Stale, Inactive, Busy, Ineligible, BadClock };
struct AmmoProfile {
 var EFeed Feed;
 var bool bManualCycle;
 var int Capacity, MaximumRounds;
 var int RemoveMs, PrepareMs, InsertMs, ChargeMs, ShellMs, FireMs;
};
struct AmmoSnapshot {
 var int Magazine, Reserve, HandMagazine;
 var bool bInserted, bChamber, bHandOccupied;
 var int Spent, Discarded, Budget, Revision;
};
var private AmmoProfile Profile;
var private AmmoSnapshot Ledger;
var private int Epoch, LastSequence, ClockMs, NextFireMs, DueMs;
var private EAction PendingAction;
var private bool bInitialized, bActive, bHasSequence, bPending;

function bool Initialize(AmmoProfile P, int Loaded, int TotalAmmo, int NewEpoch) {
 if (bInitialized || (P.Feed!=FeedDetachable && P.Feed!=FeedTube) || NewEpoch<=0 || P.Capacity<=0 || P.Capacity>1000 || P.MaximumRounds<0 || P.MaximumRounds>1000000 ||
     Loaded<0 || Loaded>P.Capacity || TotalAmmo<Loaded || TotalAmmo>P.MaximumRounds ||
     P.FireMs<=0 || P.FireMs>60000 || P.RemoveMs<0 || P.RemoveMs>60000 || P.PrepareMs<0 || P.PrepareMs>60000 ||
     P.InsertMs<0 || P.InsertMs>60000 || P.ChargeMs<0 || P.ChargeMs>60000 || P.ShellMs<0 || P.ShellMs>60000)
  return false;
 Profile=P; Epoch=NewEpoch; Ledger.bInserted=true; Ledger.bChamber=Loaded>0;
 Ledger.Magazine=Max(0,Loaded-1); Ledger.Reserve=TotalAmmo-Loaded; Ledger.Budget=TotalAmmo;
 bInitialized=true; bActive=true; return true;
}
function AmmoSnapshot Snapshot() { return Ledger; }
function int CurrentEpoch() { return Epoch; }
function bool Pending() { return bPending; }
function int KFLoaded() { return Ledger.Magazine+int(Ledger.bChamber); }
function int KFTotal() { return Ledger.Magazine+Ledger.Reserve+Ledger.HandMagazine+int(Ledger.bChamber); }
function bool Valid(AmmoSnapshot S) {
 if (S.Magazine<0 || S.Magazine>Profile.Capacity || S.HandMagazine<0 || S.HandMagazine>Profile.Capacity ||
     S.Reserve<0 || S.Reserve>Profile.MaximumRounds || (!S.bInserted && S.Magazine!=0) ||
     (!S.bHandOccupied && S.HandMagazine!=0) || (Profile.Feed==FeedTube && (!S.bInserted || S.bHandOccupied)) ||
     S.Budget<0 || S.Budget>Profile.MaximumRounds || S.Spent<0 || S.Spent>S.Budget || S.Discarded<0 || S.Discarded>S.Budget || S.Revision<0)
  return false;
 return S.Magazine+S.Reserve+S.HandMagazine+int(S.bChamber)+S.Spent+S.Discarded==S.Budget;
}
function bool Eligible(EAction A) {
 switch (A) {
  case RemoveMagazine: return Profile.Feed==FeedDetachable && Ledger.bInserted && !Ledger.bHandOccupied;
  case StowMagazine: case DiscardMagazine: return Ledger.bHandOccupied;
  case PrepareMagazine: return Profile.Feed==FeedDetachable && !Ledger.bHandOccupied && Ledger.Reserve>0;
  case InsertMagazine: return Profile.Feed==FeedDetachable && !Ledger.bInserted && Ledger.bHandOccupied;
  case Charge: return Ledger.bChamber || (Ledger.bInserted && Ledger.Magazine>0);
  case InsertShell: return Profile.Feed==FeedTube && Ledger.Magazine<Profile.Capacity && Ledger.Reserve>0;
  case Fire: return Ledger.bChamber;
 }
 return false;
}
function int Duration(EAction A) {
 switch (A) {
  case RemoveMagazine: return Profile.RemoveMs;
  case StowMagazine: case DiscardMagazine: case PrepareMagazine: return Profile.PrepareMs;
  case InsertMagazine: return Profile.InsertMs;
  case Charge: return Profile.ChargeMs;
  case InsertShell: return Profile.ShellMs;
 }
 return 0;
}
private function Commit(EAction A) {
 switch (A) {
  case RemoveMagazine:
   Ledger.HandMagazine=Ledger.Magazine; Ledger.bHandOccupied=true; Ledger.Magazine=0; Ledger.bInserted=false; break;
  case StowMagazine:
   Ledger.Reserve+=Ledger.HandMagazine; Ledger.HandMagazine=0; Ledger.bHandOccupied=false; break;
  case DiscardMagazine:
   Ledger.Discarded+=Ledger.HandMagazine; Ledger.HandMagazine=0; Ledger.bHandOccupied=false; break;
  case PrepareMagazine:
   Ledger.HandMagazine=Min(Ledger.Reserve,Profile.Capacity); Ledger.Reserve-=Ledger.HandMagazine; Ledger.bHandOccupied=true; break;
  case InsertMagazine:
   Ledger.Magazine=Ledger.HandMagazine; Ledger.HandMagazine=0; Ledger.bHandOccupied=false; Ledger.bInserted=true; break;
  case Charge:
   Ledger.Discarded+=int(Ledger.bChamber); Ledger.bChamber=false;
   if (Ledger.bInserted && Ledger.Magazine>0) { Ledger.Magazine--; Ledger.bChamber=true; } break;
  case InsertShell: Ledger.Reserve--; Ledger.Magazine++; break;
  case Fire:
   Ledger.Spent++; Ledger.bChamber=false;
   if (!Profile.bManualCycle && Ledger.bInserted && Ledger.Magazine>0) { Ledger.Magazine--; Ledger.bChamber=true; } break;
 }
 Ledger.Revision++;
}
function Advance(int NowMs) {
 local EAction A;
 if (NowMs<ClockMs) return;
 ClockMs=NowMs;
 if (bPending && NowMs>=DueMs) { A=PendingAction; bPending=false; if (bActive && Eligible(A)) Commit(A); }
}
function EResult Request(EAction A, int RequestEpoch, int Sequence, int NowMs) {
 local int Delta;
 if (NowMs<ClockMs || NowMs>2147423647) return BadClock;
 if (!bInitialized || !bActive) return Inactive;
 // UnrealScript has signed 32-bit integers: this RPC sequence domain is 31 bits.
 Delta=(Sequence-LastSequence) & 2147483647;
 if (RequestEpoch!=Epoch || Sequence<0 || (bHasSequence && (Delta==0 || Delta>=1073741824))) return Stale;
 LastSequence=Sequence; bHasSequence=true; Advance(NowMs);
 if (A==Fire) {
  if (!Eligible(A) || NowMs<NextFireMs) return Ineligible;
  if (bPending && PendingAction==Charge) return Busy;
  bPending=false; Commit(A); NextFireMs=NowMs+Profile.FireMs; return Completed;
 }
 if (bPending) return Busy;
 if (!Eligible(A)) return Ineligible;
 bPending=true; PendingAction=A; DueMs=NowMs+Duration(A); Advance(NowMs);
 if (bPending) return Accepted;
 return Completed;
}
function Suspend() { bActive=false; bPending=false; }
function bool Activate(int NewEpoch) {
 if (!bInitialized || NewEpoch<=0 || NewEpoch==Epoch) return false;
 Epoch=NewEpoch; bHasSequence=false; bPending=false; bActive=true; return true;
}
// Trusted server snapshot only. Never expose this as a client-to-server RPC.
function bool Correct(AmmoSnapshot S, int SnapshotEpoch, int NowMs) {
 if (!bInitialized || SnapshotEpoch!=Epoch || NowMs<ClockMs || S.Revision<Ledger.Revision || S.Budget!=Ledger.Budget || !Valid(S)) return false;
 Ledger=S; bPending=false; ClockMs=NowMs; return true;
}
