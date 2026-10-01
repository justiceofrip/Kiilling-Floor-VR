class KFVRTrackedKnife extends Knife;
var KFVRWeaponRig VRRig;
var KFVRMeleeState Motion;
var int SwingCount,HitCount,WorldHitCount,RejectedJumps;
var int LastHitSwing,SwingSoundCount,ContactTraceCount,BlockedContacts,CancelledWindows;
var int LastDamage,StockMeleeDamage;
var bool bLastBackstab,bSwingSoundReady;
var float StockFireSpeed,StockFireInterval;
var float BladeLengthMetres,ContactRadiusMetres;
var float SwingSpeed,WindowUntil;
var vector BladeTip,BladeBase,LastSweepFrom,LastSweepTo,LastSweepBaseFrom,LastSweepBaseTo,LastImpact;
var Actor LastHit;
var bool bPhysicalOwned,bMotionArmed;
var vector ArmPoint;
var float ArmTime,LastMotionTime;
var int LastMotionSerial;
var bool bMotionSampled,bArmSettling;
function ResetMotion()
{
    if (Motion!=None) Motion.ResetTracking();
    WindowUntil=0; SwingSpeed=0; bPhysicalOwned=False; bMotionArmed=False; ArmTime=0;
    bMotionSampled=False; bArmSettling=False; LastMotionSerial=0; LastMotionTime=0;
}
function UpdatePhysical(vector Tip)
{
    local vector Point,Start,BasePoint,BaseStart;
    local bool Strike;
    local KFVRKnifeFire Mode;
    local float Now,SweepSeconds;
    if (VRRig==None || Instigator==None || !VRRig.ActiveFor(self) || VRRig.bHolstered || (VRRig.Tracking.Flags & 2)==0)
    { ResetMotion(); return; }
    if (VRRig.Visual==None || !VRRig.Visual.bCalibrated
        || !class'KFVRMeleeGeometry'.static.BoundedVector(Tip,100000000)
        || !(BladeLengthMetres>0 && BladeLengthMetres<=1.5)) { ResetMotion(); return; }
    // Rendered endpoint diagnostics stay current on duplicate presentation
    // Ticks; only the later source-admitted branch can advance motion/damage.
    BladeTip=Tip;
    BladeBase=Tip-(VRRig.Visual.BladeAxis>>VRRig.Visual.Rotation)*(BladeLengthMetres*VRRig.Tracking.UnitsPerMetre);
    Now=VRRig.Tracking.InputSeconds;
    if (VRRig.Tracking.InputSerial<=0 || !(Now>=0 && Now<=1000000000)) { ResetMotion(); return; }
    // A repeated presentation Tick must not reuse bMoved or sweep the old
    // strike again. Native source-clock faults first emit neutral/reset input.
    if (bMotionSampled && (VRRig.Tracking.InputSerial<=LastMotionSerial || Now<=LastMotionTime)) return;
    bMotionSampled=True; LastMotionSerial=VRRig.Tracking.InputSerial; LastMotionTime=Now;
    if (!bPhysicalOwned)
    { StopFire(0); StopFire(1); FireMode[0].SetTimer(0,False); FireMode[1].SetTimer(0,False); }
    bPhysicalOwned=True;
    if (VSize(Tip-VRRig.Tracking.HeadPosition)>1.5*VRRig.Tracking.UnitsPerMetre) { ResetMotion(); return; }
    if (VSize(VRRig.Tracking.RightPosition-VRRig.DockPosition(3))<0.22*VRRig.Tracking.UnitsPerMetre)
    { ResetMotion(); return; }
    if (Motion==None) Motion=new(None) class'KFVRMeleeState';
    Mode=KFVRKnifeFire(FireMode[0]);
    if (Mode==None) return;
    Point=((Tip-Instigator.Location)<<VRRig.Tracking.BodyRotation)/VRRig.Tracking.UnitsPerMetre;
    BasePoint=((BladeBase-Instigator.Location)<<VRRig.Tracking.BodyRotation)/VRRig.Tracking.UnitsPerMetre;
    if (!bMotionArmed)
    {
        if (!bArmSettling || VSize(Point-ArmPoint)>0.025)
        { bArmSettling=True; ArmPoint=Point; ArmTime=Now; return; }
        if (Now-ArmTime<0.15) return;
        bMotionArmed=True; Motion.ResetTracking();
    }
    // Motion/settling/window use physical time; the stock firing cooldown
    // remains in game time, including ZED-time and source-epoch resets.
    StockMeleeDamage=Mode.MeleeDamage; StockFireSpeed=Mode.GetFireSpeed(); bSwingSoundReady=Mode.PreparePhysicalSound();
    StockFireInterval=Mode.default.FireRate/FMax(0.1,StockFireSpeed);
    Strike=Motion.Sample(Point,Now,Level.TimeSeconds,StockFireInterval,BasePoint);
    SwingSpeed=Motion.Speed; RejectedJumps=Motion.RejectedJumps;
    if (Strike)
    {
        SwingCount++; WindowUntil=Now+0.15;
        Start=Motion.SweepFrom; BaseStart=Motion.SweepBase; SweepSeconds=Motion.SweepSeconds;
        Mode.BeginPhysicalSwing();
    }
    else
    {
        Start=Motion.Previous; BaseStart=Motion.PreviousBase; SweepSeconds=Motion.StepSeconds;
        if (Now<WindowUntil && !Motion.ContactEligible()) { WindowUntil=0; CancelledWindows++; }
    }
    if ((Strike || Motion.ContactEligible()) && Now<WindowUntil)
    {
        LastSweepFrom=Instigator.Location+((Start*VRRig.Tracking.UnitsPerMetre)>>VRRig.Tracking.BodyRotation);
        LastSweepTo=Tip;
        LastSweepBaseFrom=Instigator.Location+((BaseStart*VRRig.Tracking.UnitsPerMetre)>>VRRig.Tracking.BodyRotation);
        LastSweepBaseTo=BladeBase;
        if (Mode.Sweep(LastSweepFrom,Tip,VRRig.Tracking.RightPosition,LastSweepBaseFrom,BladeBase,SweepSeconds)) WindowUntil=0;
    }
}
simulated function bool StartFire(int Mode)
{
    if (bPhysicalOwned || (VRRig!=None && VRRig.bTracking)) return False;
    return Super.StartFire(Mode);
}
simulated event RenderOverlays(Canvas Canvas)
{
    if (VRRig==None || Instigator==None || !VRRig.ActiveFor(self)) Super.RenderOverlays(Canvas);
}
defaultproperties
{
    FireModeClass(0)=class'KFVRKnifeFire'
    // Matches the current visual's authored 20 cm endpoint and former 1.5 UU
    // trace extent at the nominal 50 UU/m scale. Wearer calibration is pending.
    BladeLengthMetres=0.20
    ContactRadiusMetres=0.03
}
