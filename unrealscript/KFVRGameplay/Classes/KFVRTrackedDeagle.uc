class KFVRTrackedDeagle extends Deagle;
var KFVRWeaponRig VRRig;
var int VRTotalAmmo;
var int VRTicks, VRStartAttempts, VRStartAccepted;
var int VRModeTicks, VRFireAttempts;
var bool VRAllowFire, VRReadyToFire;
simulated function WeaponTick(float Delta)
{
    VRTicks++; Super.WeaponTick(Delta); VRTotalAmmo=AmmoAmount(0);
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
defaultproperties
{
    FireModeClass(0)=class'KFVRDeagleFire'
}
