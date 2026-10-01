// Body-relative metres. Repeated render/tick samples must not manufacture velocity.
class KFVRMeleeState extends Object;
var vector Last,Origin,StrikeTip,Direction,Previous,SweepFrom;
var vector LastBase,OriginBase,PreviousBase,SweepBase;
// LastTime is controller time; NextStrike is game time and survives tracking resets.
var float LastTime,NextStrike,Speed,OriginTime,StepSeconds,SweepSeconds;
var bool bValid,bLocked,bMoved;
var int Strikes,RejectedJumps;
function ResetTracking() { bValid=False; bLocked=False; bMoved=False; Speed=0; StepSeconds=0; SweepSeconds=0; }
// Retraction or a stopped hand cannot use an earlier miss window to damage.
function bool ContactEligible()
{
    return bValid && bLocked && bMoved && Speed>=1.2 && ((Last-Previous) dot Direction)>0.0001;
}
function bool Sample(vector Point,float SourceNow,float GameNow,float Cooldown,optional vector Base)
{
    local float Delta,Distance,Step;
    local vector Stroke;
    bMoved=False;
    if (!class'KFVRMeleeGeometry'.static.BoundedVector(Point,4)
        || !class'KFVRMeleeGeometry'.static.BoundedVector(Base,4) || VSize(Point)>4
        || !(SourceNow>=0 && SourceNow<=1000000000) || !(GameNow>=0 && GameNow<=1000000000)
        || !(Cooldown>0 && Cooldown<=60)) { ResetTracking(); return False; }
    if (!bValid) { Last=Point; Origin=Point; LastBase=Base; OriginBase=Base; LastTime=SourceNow; OriginTime=SourceNow; bValid=True; return False; }
    Delta=SourceNow-LastTime;
    if (Delta<=0) return False;
    Step=VSize(Point-Last);
    if (Step<0.0001 && Delta<0.15) return False;
    Previous=Last; PreviousBase=LastBase; Last=Point; LastBase=Base; LastTime=SourceNow; StepSeconds=Delta;
    if (Delta>0.15) { Origin=Point; OriginBase=Base; OriginTime=SourceNow; Speed=0; return False; }
    Speed=Step/Delta;
    if (Speed>15) { RejectedJumps++; ResetTracking(); return False; }
    bMoved=Step>=0.0001;
    if (bLocked)
    {
        if (((StrikeTip-Point) dot Direction)>=0.16) { bLocked=False; Origin=Point; OriginBase=Base; OriginTime=SourceNow; }
        return False;
    }
    // Motion during the stock cooldown cannot accumulate a future stroke or
    // lengthen its contact interval. Preserve the deadline, but keep the tip,
    // base and source time at the same newest ineligible observation.
    if (Speed<1.2 || GameNow<NextStrike)
    { Origin=Point; OriginBase=Base; OriginTime=SourceNow; return False; }
    Stroke=Point-Origin; Distance=VSize(Stroke);
    if (Distance<0.20) return False;
    // A long series of fast short motions can age the origin even outside
    // cooldown. Admit the complete stroke with the same physical interval and
    // speed rule used by contact before consuming a swing or its cooldown.
    if (!class'KFVRMeleeGeometry'.static.MovingContact(Origin,Point,SourceNow-OriginTime,1,1.2))
    { Origin=Point; OriginBase=Base; OriginTime=SourceNow; return False; }
    SweepFrom=Origin; SweepBase=OriginBase; SweepSeconds=SourceNow-OriginTime; StrikeTip=Point; Direction=Stroke/Distance;
    bLocked=True; NextStrike=GameNow+FMax(0.2,Cooldown); Strikes++; return True;
}
