// Pure magnetic magazine rail. Caller owns socket basis/orientation, tracking
// validity, native presentation and ammunition. Inputs are raw controller-palm
// and FREELY held magazine-root positions in socket metres; +Z enters the gun.
// Never feed the snapped visual palm back into this gesture. Reset on socket,
// weapon, tracking or reference changes. Provisional tolerances need wearer QA.
class KFVRMagazineGuide extends Object;
var vector PreviousPalm,PreviousRoot,CapturePalm;
var float LastTime,LastDelta,LastDistance;
var float CaptureRootDepth,CompletionDepth,RawRailDepth,RailDepth,Progress;
var float InwardTravel,LateralDistance,PeakDepth;
var float CaptureRadius,MinimumCaptureDepth,MaximumCaptureDepth,MinimumAlignmentCos;
var float MinimumPush,SeatTolerance,LateralBreakDistance,RetreatDistance;
var float MaximumSampleAge,MaximumSpeed,JumpTolerance;
var int LastSerial,MagazineId,GuidedSamples,InwardSamples;
var int CaptureCount,CancelCount,SeatCount,CaptureRejects;
var int SpeedRejects,GapRejects,ClockRejects,InvalidRejects;
var bool bSampled,bObserved,bCaptured,bSeated;

function bool BoundedNumber(float Value,float Limit)
{
    return Value==Value && Value>= -Limit && Value<=Limit;
}
function bool BoundedVector(vector Value,float Limit)
{
    return BoundedNumber(Value.X,Limit) && BoundedNumber(Value.Y,Limit)
        && BoundedNumber(Value.Z,Limit);
}
function CancelCapture()
{
    if (bCaptured) CancelCount++;
    bCaptured=False; Progress=0; GuidedSamples=0; InwardSamples=0;
}
function Reset()
{
    CancelCapture();
    PreviousPalm=vect(0,0,0); PreviousRoot=vect(0,0,0); CapturePalm=vect(0,0,0);
    LastTime=0; LastDelta=0; LastDistance=0; LastSerial=0; MagazineId=0;
    CaptureRootDepth=0; CompletionDepth=0; RawRailDepth=0; RailDepth=0;
    InwardTravel=0; LateralDistance=0; PeakDepth=0;
    bSampled=False; bObserved=False; bSeated=False;
    // Capture/cancel/seat/rejection counters are lifetime diagnostics.
}
function Observe(vector Palm,vector FreeRoot,int Serial,float Now)
{
    PreviousPalm=Palm; PreviousRoot=FreeRoot; LastSerial=Serial; LastTime=Now; bSampled=True;
}
function bool Update(vector Palm,vector FreeRoot,float AlignmentCos,int Serial,float Now,int Identity,bool Held)
{
    local float Delta,Distance,PreviousDepth;
    local vector Side;
    local bool HadObservation;
    // Loss of ownership must cancel even when no newer controller packet exists.
    // Retain the consumed event for the same magazine across release/regrip.
    if (!Held || Identity<=0)
    { CancelCapture(); bObserved=False; bSampled=False; return False; }
    if (!BoundedVector(Palm,4) || !BoundedVector(FreeRoot,4)
        || !BoundedNumber(AlignmentCos,1.0001) || !(Now>=0 && Now<=3600) || Serial<=0)
    { InvalidRejects++; CancelCapture(); bObserved=False; bSampled=False; return False; }
    if (Identity!=MagazineId)
    {
        CancelCapture(); MagazineId=Identity; bSeated=False;
        bSampled=False; bObserved=False; Progress=0;
    }
    if (bSampled)
    {
        // Ordinary duplicate game Ticks preserve the pending stroke unchanged.
        if (Serial==LastSerial && Now==LastTime) return False;
        if (Serial<=LastSerial || Now<=LastTime)
        {
            ClockRejects++; CancelCapture(); Observe(Palm,FreeRoot,Serial,Now);
            bObserved=False; return False;
        }
    }
    HadObservation=bObserved;
    Delta=Now-LastTime; Distance=VSize(Palm-PreviousPalm);
    // Before capture both freely moving points must be continuous. After
    // capture wrist rotation may move FreeRoot, but cannot drive rail travel.
    if (!bCaptured) Distance=FMax(Distance,VSize(FreeRoot-PreviousRoot));
    LastDelta=0; LastDistance=0;
    if (HadObservation) { LastDelta=Delta; LastDistance=Distance; }
    Observe(Palm,FreeRoot,Serial,Now); bObserved=True;
    if (bSeated) return False;
    if (HadObservation && (!(Delta>0 && Delta<=MaximumSampleAge)
        || Distance>MaximumSpeed*Delta+JumpTolerance))
    {
        if (Delta>MaximumSampleAge) GapRejects++;
        if (Distance>MaximumSpeed*Delta+JumpTolerance) SpeedRejects++;
        CancelCapture(); return False;
    }
    if (!bCaptured)
    {
        if (!(FreeRoot.Z<= -MinimumCaptureDepth && FreeRoot.Z>= -MaximumCaptureDepth)
            || !(Sqrt(FreeRoot.X*FreeRoot.X+FreeRoot.Y*FreeRoot.Y)<=CaptureRadius)
            || !(AlignmentCos>=MinimumAlignmentCos))
        { CaptureRejects++; return False; }
        CapturePalm=Palm; CaptureRootDepth=FreeRoot.Z;
        CompletionDepth=FMax(-SeatTolerance,CaptureRootDepth+MinimumPush);
        RawRailDepth=CaptureRootDepth; RailDepth=RawRailDepth; PeakDepth=RawRailDepth;
        InwardTravel=0; LateralDistance=0; Progress=0; GuidedSamples=0; InwardSamples=0;
        bCaptured=True; CaptureCount++;
        return False; // Acquisition, even an interior pose, never seats.
    }
    PreviousDepth=RawRailDepth;
    // Subtract the raw palm samples first: identical source positions must
    // produce exactly zero travel, without adding/subtracting the root offset.
    InwardTravel=Palm.Z-CapturePalm.Z;
    RawRailDepth=CaptureRootDepth+InwardTravel;
    RailDepth=FMin(0,RawRailDepth); // End stop; never draw through the gun.
    Side=Palm-CapturePalm; Side.Z=0; LateralDistance=VSize(Side);
    if (LateralDistance>LateralBreakDistance || PeakDepth-RawRailDepth>RetreatDistance)
    { CancelCapture(); return False; }
    PeakDepth=FMax(PeakDepth,RawRailDepth);
    GuidedSamples++;
    if (RawRailDepth-PreviousDepth>0.0005) InwardSamples++;
    Progress=FClamp(InwardTravel/(CompletionDepth-CaptureRootDepth),0,1);
    // A quick valid push may confirm on a subsequent stable fresh observation.
    // At least one actual inward translation and a complete minimum stroke are
    // still necessary; a changed wrist/root/alignment never supplies distance.
    if (GuidedSamples>=2 && InwardSamples>0 && InwardTravel>=MinimumPush
        && RawRailDepth>=CompletionDepth)
    {
        bCaptured=False; bSeated=True; RailDepth=0; Progress=1; SeatCount++;
        return True;
    }
    return False;
}

defaultproperties
{
    CaptureRadius=0.08
    MinimumCaptureDepth=0.035
    MaximumCaptureDepth=0.18
    MinimumAlignmentCos=0.5
    MinimumPush=0.03
    SeatTolerance=0.005
    LateralBreakDistance=0.12
    RetreatDistance=0.04
    MaximumSampleAge=0.15
    MaximumSpeed=4.0
    JumpTolerance=0.005
}
