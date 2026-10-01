// Explicit private fixture. Ordinary Tick changes only transient level time;
// it never calls SetGameSpeed, saves configuration or starts stock ZED effects.
class KFVRReloadTimeTest extends Actor;
var KFVRWeaponRig Rig;
var PlayerController BoundController;
var Pawn BoundPawn;
var KFWeapon BoundGun;
var KFVRManualReload BoundManual;
var bool bInitialized,bApplied,bExpired;
var int TickCount,AppliedCount,RestoredCount;
var float SavedDilation,CurrentDilation,AppliedDilation;
var float GameTime,GameDelta,AppliedGameSeconds,MaxAppliedGameSeconds;
var float LastApplyGameTime,LastRestoreGameTime;

function bool Initialize(KFVRWeaponRig R)
{
    if (bInitialized || Level.NetMode!=NM_Standalone || R==None || R.bDeleteMe || R.Level!=Level) return False;
    if (R.PC==None || R.PC.bDeleteMe || R.PC.Player==None || R.Owner!=R.PC) return False;
    if (R.BoundPawn==None || R.BoundPawn.bDeleteMe || R.BoundPawn.Health<=0
        || R.PC.Pawn!=R.BoundPawn || R.BoundPawn.Controller!=R.PC) return False;
    if (!(Level.TimeDilation>0 && Level.TimeDilation<=10)) return False;
    Rig=R; BoundController=R.PC; BoundPawn=R.BoundPawn; SetOwner(R);
    SavedDilation=Level.TimeDilation; CurrentDilation=SavedDilation;
    GameTime=Level.TimeSeconds; bInitialized=True;
    return True;
}

function bool SameLivingOwner()
{
    return bInitialized && Level.NetMode==NM_Standalone && Rig!=None && !Rig.bDeleteMe
        && Owner==Rig && Rig.Level==Level && BoundController!=None && !BoundController.bDeleteMe
        && BoundController.Player!=None && Rig.Owner==BoundController && Rig.PC==BoundController
        && BoundPawn!=None && !BoundPawn.bDeleteMe && BoundPawn.Health>0
        && Rig.BoundPawn==BoundPawn && BoundController.Pawn==BoundPawn && BoundPawn.Controller==BoundController;
}

function bool HoldingReplacement()
{
    local KFVRManualReload M;
    if (!Rig.Active() || Rig.bHolstered || (Rig.Tracking.Flags & 3)!=3 || Level.Pauser!=None) return False;
    if (Rig.Gun==None || Rig.Gun.bDeleteMe || Rig.Gun.Owner!=BoundPawn) return False;
    M=Rig.ManualReload;
    return M!=None && !M.bDeleteMe && M.Owner==Rig && M.Rig==Rig && M.Gun==Rig.Gun
        && class'KFVRWeaponProfile'.static.Manual(Rig.Gun)==M && M.bValid
        && M.HeldMagazine==2 && M.SeatCount==0;
}

function RestoreDilation()
{
    if (!bApplied) return;
    Level.TimeDilation=SavedDilation;
    CurrentDilation=Level.TimeDilation; LastRestoreGameTime=Level.TimeSeconds;
    bApplied=False; RestoredCount++;
}

function Tick(float Delta)
{
    local bool Eligible;
    TickCount++; GameTime=Level.TimeSeconds; GameDelta=Delta; CurrentDilation=Level.TimeDilation;
    if (bApplied) AppliedGameSeconds+=FMax(0,Delta);
    if (!SameLivingOwner())
    {
        RestoreDilation();
        if (bInitialized) bExpired=True;
        return;
    }
    // The cumulative game-time ceiling bounds repeated tracking recoveries too.
    if (AppliedGameSeconds>=MaxAppliedGameSeconds) bExpired=True;
    if (bExpired) { RestoreDilation(); return; }
    Eligible=HoldingReplacement();
    if (bApplied)
    {
        if (!Eligible || Rig.Gun!=BoundGun || Rig.ManualReload!=BoundManual) RestoreDilation();
        return;
    }
    if (!Eligible) return;
    if (!(Level.TimeDilation>0 && Level.TimeDilation<=10)) { bExpired=True; return; }
    // Save immediately before each application so restoration is exact even if
    // the previous tracked interval ended at a different original game speed.
    SavedDilation=Level.TimeDilation; AppliedDilation=SavedDilation*0.2;
    BoundGun=Rig.Gun; BoundManual=Rig.ManualReload;
    Level.TimeDilation=AppliedDilation; CurrentDilation=Level.TimeDilation;
    LastApplyGameTime=Level.TimeSeconds; bApplied=True; AppliedCount++;
}

function Destroyed()
{
    RestoreDilation();
    Super.Destroyed();
}

defaultproperties
{
    bHidden=True
    RemoteRole=ROLE_None
    MaxAppliedGameSeconds=8.0
}
