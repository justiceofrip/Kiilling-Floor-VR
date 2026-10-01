// Pure socket-space gesture gate; the caller retains all ammunition authority.
// P is the held magazine root in metres, relative to its installed root. Socket
// +Z points into the gun. The caller resets on tracking/reference/weapon changes.
class KFVRMagazineInsertion extends Object;
var vector Previous;
var float LastTime,Progress,Depth,Radius,LastDelta,LastDistance;
var float ApproachDistance,ApproachThreshold,RadialTolerance,SeatTolerance;
var float OvertravelTolerance,MaximumApproachDepth,MinimumAlignmentCos,MaximumSampleAge,MaximumSpeed,JumpTolerance;
var int LastSerial,MagazineId,SpeedRejects,GapRejects;
var bool bSampled,bObserved,bApproached,bSeated;

function Reset()
{
    Previous=vect(0,0,0); LastTime=0; LastSerial=0; MagazineId=0;
    // Lifetime rejection counters survive gesture/reference resets.
    Progress=0; Depth=0; Radius=0; LastDelta=0; LastDistance=0;
    bSampled=False; bObserved=False; bApproached=False; bSeated=False;
}

function bool Update(vector P,float AlignmentCos,int Serial,float Now,int Identity,bool Held)
{
    local float Delta,Distance;
    local bool Continuous,HadObservation;
    // Repeated game ticks must not turn one controller sample into travel, or
    // cancel a pending stroke. Replayed time/serials cannot mutate any state.
    if (Serial<0 || Now<0) return False;
    if (bSampled && (Serial<=LastSerial || Now<=LastTime)) return False;
    Delta=Now-LastTime; Distance=VSize(P-Previous);
    HadObservation=bObserved;
    Continuous=bObserved && Delta>0 && Delta<=MaximumSampleAge
        && Distance<=MaximumSpeed*Delta+JumpTolerance;
    Previous=P; LastSerial=Serial; LastTime=Now; bSampled=True;
    Depth=P.Z; Radius=Sqrt(P.X*P.X+P.Y*P.Y);
    if (Identity!=MagazineId)
    {
        MagazineId=Identity; bSeated=False; bApproached=False; Progress=0;
        HadObservation=False; Continuous=False;
    }
    LastDelta=0; LastDistance=0;
    if (!Held || Identity<=0)
    { bObserved=False; bApproached=False; Progress=0; return False; }
    if (HadObservation) { LastDelta=Delta; LastDistance=Distance; }
    bObserved=True;
    // A successful event stays consumed even if the same hand opens, leaves
    // the socket or returns. A changed identity or explicit Reset rearms it.
    if (bSeated) return False;
    // Diagnostics classify fresh held-pose discontinuities before geometry
    // rejection. They never alter the existing sample acceptance rules.
    if (HadObservation && !Continuous)
    {
        if (Delta>MaximumSampleAge) GapRejects++;
        if (Distance>MaximumSpeed*Delta+JumpTolerance) SpeedRejects++;
    }
    if (!(AlignmentCos>=MinimumAlignmentCos && AlignmentCos<=1.0001)
        || !(Radius<=RadialTolerance) || !(Depth<=OvertravelTolerance && Depth>= -MaximumApproachDepth))
    { bApproached=False; Progress=0; return False; }
    if (HadObservation && !Continuous)
    { bApproached=False; Progress=0; return False; }
    if (Depth<= -ApproachThreshold) bApproached=True;
    if (!bApproached) { Progress=0; return False; }
    Progress=FClamp((Depth+ApproachDistance)/(ApproachDistance-SeatTolerance),0,1);
    if (Continuous && Depth>= -SeatTolerance)
    { bSeated=True; Progress=1; return True; }
    return False;
}

defaultproperties
{
    ApproachDistance=0.06
    ApproachThreshold=0.05
    RadialTolerance=0.035
    SeatTolerance=0.01
    OvertravelTolerance=0.02
    MaximumApproachDepth=0.25
    MinimumAlignmentCos=0.8
    MaximumSampleAge=0.15
    MaximumSpeed=4.0
    JumpTolerance=0.005
}
