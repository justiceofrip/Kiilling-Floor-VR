class KFVRTrackedDualies extends Dualies;
var KFVRWeaponRig VRRig;
var KFVRDualFeedState Feed;
var KFVRWeaponVisual LeftVisual;
var bool bLeftArmed,bRightArmed,bLeftFiring,bRightFiring,bLeftVisualFailed;
var int LeftLoaded,RightLoaded,LeftShots,RightShots,VRTotalAmmo,ReloadCompletions;
var float LeftRecoil,RightRecoil;
var vector LeftMuzzle,LeftTraceStart,RightTraceStart,LeftTraceDirection,RightTraceDirection;
var rotator LeftAim;
function EnsureFeed()
{
    if (Feed==None) { Feed=new(None) class'KFVRDualFeedState'; Feed.Initialize(MagCapacity,MagAmmoRemaining); }
    LeftLoaded=Feed.LeftLoaded; RightLoaded=Feed.RightLoaded; VRTotalAmmo=AmmoAmount(0);
}
simulated function WeaponTick(float Delta)
{
    Super.WeaponTick(Delta); EnsureFeed(); Feed.Reconcile(MagAmmoRemaining);
    LeftLoaded=Feed.LeftLoaded; RightLoaded=Feed.RightLoaded; VRTotalAmmo=AmmoAmount(0);
}
function AddReloadedAmmo()
{
    Super.AddReloadedAmmo(); EnsureFeed(); Feed.Reconcile(MagAmmoRemaining);
    LeftLoaded=Feed.LeftLoaded; RightLoaded=Feed.RightLoaded; ReloadCompletions++;
}
simulated function bool ConsumeAmmo(int Mode,float Load,optional bool bAmountNeededIsMax)
{
    local bool Result;
    EnsureFeed();
    if (Load>0 && Feed.Loaded(Mode)<1) return False;
    // Both modes deliberately use the same normal KF ammunition object.
    Result=Super.ConsumeAmmo(Mode,Load,bAmountNeededIsMax);
    if (Result && Load>0) Feed.Consume(Mode);
    LeftLoaded=Feed.LeftLoaded; RightLoaded=Feed.RightLoaded;
    return Result;
}
function bool WantsFire(int Mode)
{
    if (VRRig==None || !VRRig.ActiveFor(self) || VRRig.bHolstered) return False;
    if (Mode==1) return bLeftArmed && (VRRig.Tracking.Flags & 1)!=0 && VRRig.Tracking.LeftTrigger>0.75;
    return bRightArmed && (VRRig.Tracking.Flags & 2)!=0 && VRRig.Tracking.RightTrigger>0.75;
}
simulated function bool StartFire(int Mode)
{
    if (VRRig!=None && VRRig.ActiveFor(self) && !WantsFire(Mode)) return False;
    return Super.StartFire(Mode);
}
simulated event ClientStopFire(int Mode)
{
    if (WantsFire(Mode)) return;
    Super.ClientStopFire(Mode);
}
function ResetInput(optional bool KeepVisual)
{
    bLeftArmed=False; bRightArmed=False; bLeftFiring=False; bRightFiring=False;
    StopFire(0); StopFire(1); LeftRecoil=0; RightRecoil=0;
    if (!KeepVisual && LeftVisual!=None) LeftVisual.bHidden=True;
}
function PoseLeft(bool Equipped,float Delta)
{
    local vector Position,X,Y,Z;
    local rotator R;
    if (VRRig==None) return;
    if (LeftVisual==None && !bLeftVisualFailed)
    {
        LeftVisual=Spawn(class'KFVRWeaponVisual',self);
        if (LeftVisual!=None)
        {
            LeftVisual.bOffhandGun=True;
            if (!LeftVisual.Initialize(self)) { LeftVisual.Destroy(); LeftVisual=None; bLeftVisualFailed=True; }
        }
    }
    if (LeftVisual==None) return;
    GetAxes(VRRig.Tracking.BodyRotation,X,Y,Z);
    if (Equipped && !VRRig.bHolstered)
    {
        R=VRRig.Tracking.LeftRotation; R.Pitch+=int(LeftRecoil*20000);
        Position=VRRig.Tracking.LeftPosition-vector(R)*LeftRecoil*VRRig.Tracking.UnitsPerMetre;
    }
    else
    { R=VRRig.Tracking.BodyRotation; R.Pitch=-16384; Position=VRRig.DockPosition(1)+X*0.2*VRRig.Tracking.UnitsPerMetre; }
    LeftVisual.PoseAt(Position,R,VRRig.Tracking.UnitsPerMetre);
    LeftVisual.bHidden=Equipped && (VRRig.Tracking.Flags & 1)==0;
    LeftMuzzle=LeftVisual.Location+((LeftVisual.TipLocal*LeftVisual.DrawScale)>>R); LeftAim=R;
    LeftRecoil=FMax(0,LeftRecoil-Delta*0.25); RightRecoil=FMax(0,RightRecoil-Delta*0.25);
}
function UpdateControls()
{
    if (VRRig==None || !VRRig.ActiveFor(self) || VRRig.bHolstered || LeftVisual==None) { ResetInput(VRRig!=None && VRRig.bTracking && VRRig.bHolstered); return; }
    EnsureFeed();
    if ((VRRig.Tracking.Flags & 1)==0) { bLeftArmed=False; StopFire(1); bLeftFiring=False; }
    else if (VRRig.Tracking.LeftTrigger<0.15) bLeftArmed=True;
    if ((VRRig.Tracking.Flags & 2)==0) { bRightArmed=False; StopFire(0); bRightFiring=False; }
    else if (VRRig.Tracking.RightTrigger<0.15) bRightArmed=True;
    if (WantsFire(0)) { if (!bRightFiring) bRightFiring=StartFire(0); }
    else if (bRightFiring) { StopFire(0); bRightFiring=False; }
    if (WantsFire(1)) { if (!bLeftFiring) bLeftFiring=StartFire(1); }
    else if (bLeftFiring) { StopFire(1); bLeftFiring=False; }
}
simulated event RenderOverlays(Canvas Canvas)
{
    if (VRRig==None || !VRRig.ActiveFor(self)) Super.RenderOverlays(Canvas);
}
simulated function Destroyed()
{
    ResetInput(); if (LeftVisual!=None) LeftVisual.Destroy(); Super.Destroyed();
}
defaultproperties
{
    FireModeClass(0)=class'KFVRDualFire'
    FireModeClass(1)=class'KFVRDualFire'
    bReduceMagAmmoOnSecondaryFire=True
}
