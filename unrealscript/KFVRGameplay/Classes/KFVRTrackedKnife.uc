class KFVRTrackedKnife extends Knife;
var KFVRWeaponRig VRRig;
var KFVRMeleeState Motion;
var int SwingCount,HitCount,WorldHitCount,RejectedJumps;
var float SwingSpeed,WindowUntil;
var vector BladeTip,LastSweepFrom,LastSweepTo,LastImpact;
var Actor LastHit;
var bool bPhysicalOwned,bMotionArmed;
var vector ArmPoint;
var float ArmTime;
function ResetMotion()
{
    if (Motion!=None) Motion.ResetTracking();
    WindowUntil=0; SwingSpeed=0; bPhysicalOwned=False; bMotionArmed=False; ArmTime=0;
}
function UpdatePhysical(vector Tip)
{
    local vector Point,Start;
    local bool Strike;
    local KFVRKnifeFire Mode;
    local float Now;
    if (VRRig==None || !VRRig.ActiveFor(self) || VRRig.bHolstered || (VRRig.Tracking.Flags & 2)==0)
    { ResetMotion(); return; }
    if (!bPhysicalOwned)
    { StopFire(0); StopFire(1); FireMode[0].SetTimer(0,False); FireMode[1].SetTimer(0,False); }
    bPhysicalOwned=True; BladeTip=Tip; Now=Level.TimeSeconds;
    if (VSize(Tip-VRRig.Tracking.HeadPosition)>1.5*VRRig.Tracking.UnitsPerMetre) { ResetMotion(); return; }
    if (VSize(VRRig.Tracking.RightPosition-VRRig.DockPosition(3))<0.22*VRRig.Tracking.UnitsPerMetre)
    { ResetMotion(); return; }
    if (Motion==None) Motion=new(None) class'KFVRMeleeState';
    Mode=KFVRKnifeFire(FireMode[0]);
    if (Mode==None) return;
    Point=((Tip-Instigator.Location)<<VRRig.Tracking.BodyRotation)/VRRig.Tracking.UnitsPerMetre;
    if (!bMotionArmed)
    {
        if (ArmTime==0 || VSize(Point-ArmPoint)>0.025) { ArmPoint=Point; ArmTime=Now; return; }
        if (Now-ArmTime<0.15) return;
        bMotionArmed=True; Motion.ResetTracking();
    }
    Strike=Motion.Sample(Point,Now,Mode.default.FireRate/FMax(0.1,Mode.GetFireSpeed()));
    SwingSpeed=Motion.Speed; RejectedJumps=Motion.RejectedJumps;
    if (Strike) { SwingCount++; WindowUntil=Now+0.15; Start=Motion.SweepFrom; }
    else Start=Motion.Previous;
    if ((Strike || Motion.bMoved) && Now<WindowUntil && Motion.Speed>=1.2)
    {
        LastSweepFrom=Instigator.Location+((Start*VRRig.Tracking.UnitsPerMetre)>>VRRig.Tracking.BodyRotation);
        LastSweepTo=Tip;
        if (Mode.Sweep(LastSweepFrom,Tip,VRRig.Tracking.RightPosition)) WindowUntil=0;
    }
}
simulated function bool StartFire(int Mode)
{
    if (bPhysicalOwned || (VRRig!=None && VRRig.bTracking)) return False;
    return Super.StartFire(Mode);
}
simulated event RenderOverlays(Canvas Canvas)
{
    if (VRRig==None || !VRRig.ActiveFor(self)) Super.RenderOverlays(Canvas);
}
defaultproperties
{
    FireModeClass(0)=class'KFVRKnifeFire'
}
