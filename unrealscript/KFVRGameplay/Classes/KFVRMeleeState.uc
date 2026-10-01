// Body-relative metres. Repeated render/tick samples must not manufacture velocity.
class KFVRMeleeState extends Object;
var vector Last,Origin,StrikeTip,Direction,Previous,SweepFrom;
// LastTime is controller time; NextStrike is game time and survives tracking resets.
var float LastTime,NextStrike,Speed;
var bool bValid,bLocked,bMoved;
var int Strikes,RejectedJumps;
function ResetTracking() { bValid=False; bLocked=False; bMoved=False; Speed=0; }
function bool Sample(vector Point,float SourceNow,float GameNow,float Cooldown)
{
    local float Delta,Distance,Step;
    local vector Stroke;
    bMoved=False;
    if (VSize(Point)>4 || SourceNow<0 || GameNow<0) { ResetTracking(); return False; }
    if (!bValid) { Last=Point; Origin=Point; LastTime=SourceNow; bValid=True; return False; }
    Delta=SourceNow-LastTime;
    if (Delta<=0) return False;
    Step=VSize(Point-Last);
    if (Step<0.0001 && Delta<0.15) return False;
    Previous=Last; Last=Point; LastTime=SourceNow;
    if (Delta>0.15) { Origin=Point; Speed=0; return False; }
    Speed=Step/Delta;
    if (Speed>15) { RejectedJumps++; ResetTracking(); return False; }
    bMoved=Step>=0.0001;
    if (bLocked)
    {
        if (((StrikeTip-Point) dot Direction)>=0.16) { bLocked=False; Origin=Point; }
        return False;
    }
    if (Speed<1.2) { Origin=Point; return False; }
    Stroke=Point-Origin; Distance=VSize(Stroke);
    if (Distance<0.20 || GameNow<NextStrike) return False;
    SweepFrom=Origin; StrikeTip=Point; Direction=Stroke/Distance;
    bLocked=True; NextStrike=GameNow+FMax(0.2,Cooldown); Strikes++; return True;
}
