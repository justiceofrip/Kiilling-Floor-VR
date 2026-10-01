// Stock MK23 defaults, reload timing and perk paths are inherited.
class KFVRTrackedMK23 extends MK23Pistol;
var KFVRWeaponRig VRRig;
var KFVRManualReload Manual;
var int VRTotalAmmo;
var int VRTicks, VRStartAttempts, VRStartAccepted;
var int VRModeTicks, VRFireAttempts;
var bool VRAllowFire, VRReadyToFire;
simulated function WeaponTick(float Delta)
{
    VRTicks++; Super.WeaponTick(Delta);
    if (Manual!=None) Manual.Reconcile();
    VRTotalAmmo=AmmoAmount(0);
}
// Physical mode owns reload transfers; stock firing still owns cadence/damage.
exec function ReloadMeNow()
{
    if (Manual!=None) return;
    Super.ReloadMeNow();
}
simulated function bool AllowReload()
{
    if (Manual!=None) return False;
    return Super.AllowReload();
}
function AddReloadedAmmo()
{
    if (Manual!=None) return;
    Super.AddReloadedAmmo();
}
simulated function bool ConsumeAmmo(int Mode, float Load, optional bool bAmountNeededIsMax)
{
    local bool Result;
    if (Manual!=None && Mode==0 && Load>0 && !Manual.Ammo.CanFire()) return False;
    Result=Super.ConsumeAmmo(Mode,Load,bAmountNeededIsMax);
    if (Result && Manual!=None && Mode==0 && Load>0)
    { Manual.Ammo.ConsumeShot(); Manual.Publish(); }
    return Result;
}
simulated function bool StartFire(int Mode)
{
    local bool Accepted;
    if (VRRig!=None && VRRig.ActiveFor(self) && (Mode!=0 || !VRRig.WantsFireFor(self))) return false;
    VRStartAttempts++; VRAllowFire=FireMode[Mode].AllowFire(); VRReadyToFire=ReadyToFire(Mode);
    Accepted=Super.StartFire(Mode);
    if (Accepted) VRStartAccepted++;
    return Accepted;
}
// The native weapon tick otherwise releases the shot because desktop bFire is zero.
// Keep its regular ModeTick/ModeDoFire cadence; the rig owns only the trigger edge.
simulated event ClientStopFire(int Mode)
{
    if (Mode==0 && VRRig!=None && VRRig.WantsFireFor(self)) return;
    Super.ClientStopFire(Mode);
}
function SetTransferredAmmo(int Total, int Loaded)
{
    if (bNoAmmoInstances) AmmoCharge[0]=Total;
    else if (Ammo[0]!=None) Ammo[0].AmmoAmount=Total;
    MagAmmoRemaining=Loaded;
}
simulated event RenderOverlays(Canvas Canvas)
{
    if (VRRig == None || !VRRig.ActiveFor(self))
        Super.RenderOverlays(Canvas);
}
// Secondary fire remains the installed NoFire class.
defaultproperties
{
    FireModeClass(0)=class'KFVRMK23Fire'
}
