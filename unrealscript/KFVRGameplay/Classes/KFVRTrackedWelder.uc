class KFVRTrackedWelder extends Welder;
var KFVRWeaponRig VRRig;
var bool bVRControlled,bWeldArmed,bWeldApproved,bWeldFiring;
var int SelectedWeldMode,WeldHits,UnweldHits,ChargeSpent,VRTotalAmmo,MaxCharge;
var KFDoorMover ContactDoor;
var vector ContactPoint;
var float DoorPercent;
function bool WantsWeld(int Mode)
{
    return VRRig!=None && VRRig.ActiveFor(self) && !VRRig.bHolstered && (VRRig.Tracking.Flags & 2)!=0
        && bWeldApproved && SelectedWeldMode==Mode && VRRig.Tracking.RightTrigger>0.75;
}
function KFDoorMover GetContactDoor()
{
    local vector HitLocation,HitNormal;
    local Actor Hit;
    local KFDoorMover Door;
    if (VRRig==None || !VRRig.ActiveFor(self) || VRRig.bHolstered || (VRRig.Tracking.Flags & 2)==0) return None;
    if (VSize(VRRig.MuzzleLocation-VRRig.Tracking.HeadPosition)>0.95*VRRig.Tracking.UnitsPerMetre) return None;
    // Include moving doors/actors in the clearance trace, not only BSP.
    Hit=Instigator.Trace(HitLocation,HitNormal,VRRig.MuzzleLocation,VRRig.Tracking.HeadPosition,True);
    if (Hit!=None) return None;
    Door=KFDoorMover(Instigator.Trace(HitLocation,HitNormal,VRRig.MuzzleLocation+vector(VRRig.AimRotation)*0.35*VRRig.Tracking.UnitsPerMetre,VRRig.MuzzleLocation,True));
    if (Door==None || Door.bHidden || Door.MyTrigger==None || !Door.bClosed) return None;
    if (VSize(HitLocation-(Instigator.Location+Instigator.EyePosition()))>WeldFire(FireMode[0]).weaponRange) return None;
    ContactPoint=HitLocation; return Door;
}
function ApplyContact(WeldFire Mode)
{
    local KFDoorMover Door;
    local int Amount;
    local float Before;
    if (!WantsWeld(Mode.ThisModeNum) || bNoHit) return;
    Door=GetContactDoor();
    Mode.LastHitActor=Door;
    if (Door==None || (Mode.ThisModeNum==0 && Door.bDisallowWeld) || (Mode.ThisModeNum==1 && Door.WeldStrength<=0)) return;
    Amount=Mode.MeleeDamage;
    if (Mode.maxAdditionalDamage>0) Amount+=Rand(Mode.maxAdditionalDamage);
    if (KFPlayerReplicationInfo(Instigator.PlayerReplicationInfo)!=None && KFPlayerReplicationInfo(Instigator.PlayerReplicationInfo).ClientVeteranSkill!=None)
        Amount=float(Amount)*KFPlayerReplicationInfo(Instigator.PlayerReplicationInfo).ClientVeteranSkill.static.GetWeldSpeedModifier(KFPlayerReplicationInfo(Instigator.PlayerReplicationInfo));
    Before=Door.WeldStrength;
    Door.TakeDamage(Amount,Instigator,ContactPoint,vector(VRRig.AimRotation),Mode.hitDamageClass);
    if (Door.WeldStrength==Before) return;
    if (Mode.ThisModeNum==0) WeldHits++; else UnweldHits++;
    Spawn(class'KFWelderHitEffect',,,ContactPoint,VRRig.AimRotation);
    VRRig.PulseRight(0.15,15);
}
function StopVRWork()
{
    StopFire(0); StopFire(1); FireMode[0].SetTimer(0,False); FireMode[1].SetTimer(0,False);
    bWeldApproved=False; bWeldFiring=False;
}
function ResetInput()
{
    if (!bVRControlled) return;
    StopVRWork(); bVRControlled=False; bWeldArmed=False; ContactDoor=None;
    WeldFire(FireMode[0]).LastHitActor=None; WeldFire(FireMode[1]).LastHitActor=None;
}
function UpdateControls()
{
    VRTotalAmmo=AmmoAmount(0); MaxCharge=FireMode[0].AmmoClass.Default.MaxAmmo;
    if (VRRig==None || !VRRig.ActiveFor(self) || VRRig.bHolstered || (VRRig.Tracking.Flags & 2)==0) { ResetInput(); return; }
    bVRControlled=True; ContactDoor=GetContactDoor(); DoorPercent=0;
    if (ContactDoor!=None) DoorPercent=100*ContactDoor.WeldStrength/FMax(1,ContactDoor.MaxWeld);
    if (VRRig.Tracking.RightTrigger<0.15) { StopVRWork(); bWeldArmed=True; return; }
    if (VRRig.Tracking.RightTrigger>0.75 && bWeldArmed)
    {
        bWeldArmed=False; SelectedWeldMode=0;
        if ((VRRig.Tracking.Flags & 1)!=0 && VRRig.Tracking.LeftTrigger>0.75) SelectedWeldMode=1;
        bWeldApproved=True; bWeldFiring=StartFire(SelectedWeldMode);
    }
    // Re-entering contact can resume a held, intentionally armed weld stroke.
    if (bWeldApproved && !bWeldFiring) bWeldFiring=StartFire(SelectedWeldMode);
}
simulated function bool StartFire(int Mode)
{
    if (VRRig!=None && VRRig.ActiveFor(self) && !WantsWeld(Mode)) return False;
    return Super.StartFire(Mode);
}
simulated event ClientStopFire(int Mode)
{
    if (WantsWeld(Mode)) return;
    Super.ClientStopFire(Mode);
}
simulated function bool ConsumeAmmo(int Mode,float Load,optional bool bAmountNeededIsMax)
{
    local bool Result;
    Result=Super.ConsumeAmmo(Mode,Load,bAmountNeededIsMax);
    if (Result && Load>0) ChargeSpent+=int(Load);
    VRTotalAmmo=AmmoAmount(0); return Result;
}
simulated event RenderOverlays(Canvas Canvas)
{
    if (VRRig==None || !VRRig.ActiveFor(self)) Super.RenderOverlays(Canvas);
}
simulated function Destroyed() { ResetInput(); Super.Destroyed(); }
defaultproperties
{
    FireModeClass(0)=class'KFVRWeldFire'
    FireModeClass(1)=class'KFVRUnWeldFire'
}
