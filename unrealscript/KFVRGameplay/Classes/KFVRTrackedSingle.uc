class KFVRTrackedSingle extends Single;
var KFVRWeaponRig VRRig;
var int VRTicks, VRStartAttempts, VRStartAccepted;
var int VRModeTicks, VRFireAttempts;
var bool VRAllowFire, VRReadyToFire;
simulated function WeaponTick(float Delta)
{
    VRTicks++; Super.WeaponTick(Delta);
}
simulated function bool StartFire(int Mode)
{
    local bool Accepted;
    if (VRRig!=None && VRRig.Active() && (Mode!=0 || !VRRig.WantsFire())) return false;
    VRStartAttempts++; VRAllowFire=FireMode[Mode].AllowFire(); VRReadyToFire=ReadyToFire(Mode);
    Accepted=Super.StartFire(Mode);
    if (Accepted) VRStartAccepted++;
    return Accepted;
}
// The native weapon tick otherwise releases the shot because desktop bFire is zero.
// Keep its regular ModeTick/ModeDoFire cadence; the rig owns only the trigger edge.
simulated event ClientStopFire(int Mode)
{
    if (Mode==0 && VRRig!=None && VRRig.WantsFire()) return;
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
    if (VRRig == None || !VRRig.Active())
        Super.RenderOverlays(Canvas);
}
defaultproperties
{
    FireModeClass(0)=class'KFVRSingleFire'
}
