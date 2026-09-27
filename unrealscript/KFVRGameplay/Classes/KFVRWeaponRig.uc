// Explicit standalone development rig. Native input writes only Tracking on the
// existing game-thread Tick boundary; actors and weapon effects remain script-owned.
class KFVRWeaponRig extends Info;
struct TrackingPacket
{
    var vector LeftPosition;
    var rotator LeftRotation;
    var vector RightPosition;
    var rotator RightRotation;
    var float LeftTrigger, LeftGrip, RightTrigger, RightGrip;
    var float LeftThumb, LeftIndex, LeftCurl, RightThumb, RightIndex, RightCurl;
    var vector HeadPosition;
    var rotator BodyRotation;
    var float UnitsPerMetre;
    var int Flags, Serial;
};
struct FeedbackPacket
{
    var int Serial;
    var float LeftAmplitude, RightAmplitude;
    var int LeftMs, RightMs;
};
var TrackingPacket Tracking;
var FeedbackPacket Feedback;
var PlayerController PC;
var KFVRTrackedSingle Gun;
var KFVRWeaponVisual Visual;
var KFVRHandVisual LeftHand, RightHand;
var vector MuzzleLocation, LastTraceStart, LastTraceDirection, HolsterLocation;
var rotator AimRotation;
var int LastSerial, AppliedFrames, TraceCount, HolsterCount, DrawCount;
var float LastInputTime, Recoil;
var bool bTracking, bTriggerArmed, bFiring, bHolstered, bGripPrevious, bGripArmed;

function PulseRight(float Strength, int DurationMs)
{
    Feedback.LeftAmplitude=0.0; Feedback.LeftMs=0;
    Feedback.RightAmplitude=FClamp(Strength,0.0,1.0); Feedback.RightMs=Clamp(DurationMs,0,100);
    Feedback.Serial++;
}
function bool Active()
{
    return bTracking && PC!=None && PC.Pawn!=None && PC.Pawn.Health>0
        && PC.Pawn.Weapon==Gun && Level.TimeSeconds-LastInputTime<=0.1;
}
function bool WantsFire()
{
    return Active() && (Tracking.Flags & 2)!=0 && bTriggerArmed && !bHolstered && Tracking.RightTrigger>0.75;
}
function ReleaseInput()
{
    bTracking=False; bTriggerArmed=False; bGripArmed=False; bGripPrevious=False;
    if (bFiring && Gun!=None) Gun.StopFire(0);
    bFiring=False;
    if (Visual!=None) Visual.bHidden=True;
    if (LeftHand!=None) LeftHand.bHidden=True;
    if (RightHand!=None) RightHand.bHidden=True;
}
function Tick(float Delta)
{
    local vector X,Y,Z,Position;
    local rotator GunRotation;
    local bool GripDown;
    local coords Muzzle;
    PC=PlayerController(Owner);
    if (Level.NetMode!=NM_Standalone || PC==None || PC.Pawn==None || PC.Pawn.Health<=0 || Level.Pauser!=None)
    { ReleaseInput(); return; }
    if (Tracking.Serial!=LastSerial)
    {
        LastSerial=Tracking.Serial; LastInputTime=Level.TimeSeconds;
        bTracking=(Tracking.Flags & 3)!=0;
    }
    if (!bTracking || Level.TimeSeconds-LastInputTime>0.1 || Tracking.UnitsPerMetre<1 || Tracking.UnitsPerMetre>200)
    { ReleaseInput(); return; }
    if (Gun==None || PC.Pawn.Weapon!=Gun) { ReleaseInput(); return; }
    if (Visual==None)
    {
        Visual=Spawn(class'KFVRWeaponVisual',self);
        if (Visual==None || !Visual.Initialize(Gun)) { ReleaseInput(); return; }
    }
    if (LeftHand==None)
    {
        LeftHand=Spawn(class'KFVRHandVisual',self);
        if (LeftHand!=None && !LeftHand.InitializeHand(Gun,True)) { LeftHand.Destroy(); LeftHand=None; }
    }
    if (RightHand==None)
    {
        RightHand=Spawn(class'KFVRHandVisual',self);
        if (RightHand!=None && !RightHand.InitializeHand(Gun,False)) { RightHand.Destroy(); RightHand=None; }
    }
    if (LeftHand!=None)
    {
        LeftHand.bHidden=(Tracking.Flags & 1)==0;
        if (!LeftHand.bHidden) LeftHand.PoseHand(Tracking.LeftPosition,Tracking.LeftRotation,Tracking.UnitsPerMetre,Tracking.LeftThumb,Tracking.LeftIndex,Tracking.LeftCurl);
    }
    if (RightHand!=None)
    {
        RightHand.bHidden=(Tracking.Flags & 2)==0;
        if (!RightHand.bHidden) RightHand.PoseHand(Tracking.RightPosition,Tracking.RightRotation,Tracking.UnitsPerMetre,Tracking.RightThumb,Tracking.RightIndex,Tracking.RightCurl);
    }
    if ((Tracking.Flags & 2)==0)
    {
        Visual.bHidden=True; bTriggerArmed=False; bGripArmed=False; bGripPrevious=False;
        if (bFiring) Gun.StopFire(0); bFiring=False; return;
    }
    GetAxes(Tracking.BodyRotation,X,Y,Z);
    HolsterLocation=Tracking.HeadPosition + (Y*0.22-Z*0.62-X*0.05)*Tracking.UnitsPerMetre;
    GripDown=Tracking.RightGrip>0.7;
    if (!GripDown) bGripArmed=True;
    if (bGripArmed && VSize(Tracking.RightPosition-HolsterLocation)<0.22*Tracking.UnitsPerMetre)
    {
        if (bHolstered && GripDown && !bGripPrevious) { bHolstered=False; DrawCount++; PulseRight(0.25,18); }
        else if (!bHolstered && !GripDown && bGripPrevious) { bHolstered=True; HolsterCount++; PulseRight(0.2,15); }
    }
    bGripPrevious=GripDown;
    if (bHolstered)
    {
        Position=HolsterLocation; GunRotation=Tracking.BodyRotation; GunRotation.Pitch=-16384;
        bTriggerArmed=False;
    }
    else
    {
        GunRotation=Tracking.RightRotation;
        GunRotation.Pitch+=int(Recoil*20000.0);
        Position=Tracking.RightPosition-vector(GunRotation)*Recoil*Tracking.UnitsPerMetre;
    }
    Visual.PoseAt(Position,GunRotation,Tracking.UnitsPerMetre);
    Visual.BoneRefresh(); Muzzle=Visual.GetBoneCoords('tip');
    MuzzleLocation=Muzzle.Origin; AimRotation=GunRotation;
    // A missing tip must not silently turn an invalid origin into a shot.
    if (VSize(MuzzleLocation-Position)>0.6*Tracking.UnitsPerMetre) { ReleaseInput(); return; }
    AppliedFrames++; Recoil=FMax(0.0,Recoil-Delta*0.25);
    if (Tracking.RightTrigger<0.15) bTriggerArmed=True;
    if (bTriggerArmed && !bHolstered && Tracking.RightTrigger>0.75)
    {
        if (!bFiring) { bFiring=Gun.StartFire(0); }
    }
    else if (bFiring) { Gun.StopFire(0); bFiring=False; }
}
function Destroyed()
{
    ReleaseInput();
    if (Visual!=None) Visual.Destroy();
    if (LeftHand!=None) LeftHand.Destroy();
    if (RightHand!=None) RightHand.Destroy();
    Super.Destroyed();
}
defaultproperties
{
    RemoteRole=ROLE_None
    bAlwaysTick=True
}
