class KFVRTrackedSyringe extends Syringe;
var KFVRWeaponRig VRRig;
var bool bVRControlled,bHealArmed,bHealPressed,bHealApproved,bHealFiring,bPendingHeal,bSelfContact;
var int SelectedHealMode,PendingHealMode,HealBegins,SelfHeals,OtherHeals,CancelledHeals,ChargeSpent,VRTotalAmmo;
var vector ChestPoint,NeedlePoint;
var float ContactDistance;
function bool ContactClear()
{
    local vector HitLocation,HitNormal;
    if (VRRig==None || !VRRig.ActiveFor(self) || VRRig.bHolstered || (VRRig.Tracking.Flags & 2)==0) return False;
    if (VSize(VRRig.MuzzleLocation-VRRig.Tracking.HeadPosition)>0.9*VRRig.Tracking.UnitsPerMetre) return False;
    return Instigator.Trace(HitLocation,HitNormal,VRRig.MuzzleLocation,VRRig.Tracking.HeadPosition,False)==None;
}
function bool TouchingSelf()
{
    local vector X,Y,Z,ToBody;
    if (!ContactClear()) return False;
    GetAxes(VRRig.Tracking.BodyRotation,X,Y,Z);
    ChestPoint=VRRig.Tracking.HeadPosition+(X*0.05-Z*0.32)*VRRig.Tracking.UnitsPerMetre;
    NeedlePoint=VRRig.MuzzleLocation; ToBody=ChestPoint-NeedlePoint;
    ContactDistance=VSize(ToBody)/VRRig.Tracking.UnitsPerMetre;
    return ContactDistance<0.12 && ContactDistance>0.005 && (Normal(ToBody) dot vector(VRRig.AimRotation))>0.6;
}
function KFHumanPawn ContactHealee()
{
    local vector HitLocation,HitNormal;
    local KFHumanPawn Target;
    if (!ContactClear()) return None;
    Target=KFHumanPawn(Instigator.Trace(HitLocation,HitNormal,
        VRRig.MuzzleLocation+vector(VRRig.AimRotation)*0.2*VRRig.Tracking.UnitsPerMetre,VRRig.MuzzleLocation,True));
    if (Target==None || Target==Instigator || Target.Health<=0 || !Target.bCanBeHealed || VSize(Target.Location-Instigator.Location)>70) return None;
    return Target;
}
function bool WantsHeal(int Mode)
{
    return VRRig!=None && VRRig.ActiveFor(self) && !VRRig.bHolstered && (VRRig.Tracking.Flags & 2)!=0
        && bHealApproved && SelectedHealMode==Mode && VRRig.Tracking.RightTrigger>0.75;
}
function BeginHeal(int Mode)
{
    bPendingHeal=True; PendingHealMode=Mode; HealBegins++;
}
function bool CanComplete(int Mode,optional KFHumanPawn Target)
{
    if (!bPendingHeal || PendingHealMode!=Mode || !WantsHeal(Mode) || AmmoAmount(Mode)<FireMode[Mode].AmmoPerFire) return False;
    if (Mode==1) return TouchingSelf() && Instigator.Health<Instigator.HealthMax;
    return Target!=None && ContactHealee()==Target && Target.Health<Target.HealthMax;
}
function CompleteHeal(int Mode)
{
    bPendingHeal=False;
    if (Mode==1) SelfHeals++; else OtherHeals++;
    VRRig.PulseRight(0.35,35);
}
function CancelHeal()
{
    if (bPendingHeal) CancelledHeals++;
    bPendingHeal=False;
    if (KFVRHealFire(FireMode[0])!=None) KFVRHealFire(FireMode[0]).CancelPending();
    if (FireMode[1]!=None) FireMode[1].SetTimer(0,False);
}
function ResetInput()
{
    if (!bVRControlled && !bPendingHeal) return;
    bVRControlled=False; CancelHeal(); bHealArmed=False; bHealPressed=False; bHealApproved=False; bHealFiring=False;
    StopFire(0); StopFire(1);
}
function UpdateControls()
{
    local int Mode;
    VRTotalAmmo=AmmoAmount(0); bSelfContact=TouchingSelf();
    if (VRRig==None || !VRRig.ActiveFor(self) || VRRig.bHolstered || (VRRig.Tracking.Flags & 2)==0) { ResetInput(); return; }
    bVRControlled=True;
    if (VRRig.Tracking.RightTrigger<0.15)
    { CancelHeal(); StopFire(0); StopFire(1); bHealArmed=True; bHealPressed=False; bHealApproved=False; bHealFiring=False; return; }
    if (VRRig.Tracking.RightTrigger>0.75 && bHealArmed && !bHealPressed)
    {
        bHealPressed=True; bHealArmed=False;
        if (bSelfContact) Mode=1;
        else if (ContactHealee()==None) return;
        SelectedHealMode=Mode; bHealApproved=True; bHealFiring=StartFire(Mode);
    }
    if (bPendingHeal && !CanComplete(PendingHealMode,KFVRHealFire(FireMode[0]).CachedHealee)) CancelHeal();
}
simulated function bool HackClientStartFire()
{
    if (VRRig!=None && VRRig.ActiveFor(self)) return False;
    return Super.HackClientStartFire();
}
simulated function bool StartFire(int Mode)
{
    if (VRRig!=None && VRRig.ActiveFor(self) && !WantsHeal(Mode)) return False;
    return Super.StartFire(Mode);
}
simulated event ClientStopFire(int Mode)
{
    if (WantsHeal(Mode)) return;
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
    FireModeClass(0)=class'KFVRHealFire'
    FireModeClass(1)=class'KFVRHealSelfFire'
}
