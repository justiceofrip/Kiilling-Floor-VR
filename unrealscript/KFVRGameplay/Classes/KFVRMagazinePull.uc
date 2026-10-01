// Raw palm motion in magazine socket metres, +Z inward. The caller owns the
// fresh grip edge, contact geometry and the single ledger transfer on completion.
class KFVRMagazinePull extends Object;
var vector StartPalm,Previous;
var float LastTime,Progress,Distance,Travel,MaximumAge,MaximumSpeed,JumpTolerance;
var int LastSerial,MagazineId,FreshMoves,CompleteCount,CancelCount;
var bool bActive,bComplete;
function bool Bounded(vector P)
{
    return P.X==P.X && P.Y==P.Y && P.Z==P.Z
        && Abs(P.X)<=4 && Abs(P.Y)<=4 && Abs(P.Z)<=4;
}
function Cancel()
{
    if (bActive) CancelCount++;
    bActive=False; bComplete=False;
}
function bool Begin(vector Palm,int Serial,float Now,int Identity)
{
    Cancel(); Progress=0; Distance=0; FreshMoves=0;
    if (!Bounded(Palm) || Serial<0 || !(Now>=0 && Now<10000000) || Identity<=0) return False;
    StartPalm=Palm; Previous=Palm; LastSerial=Serial; LastTime=Now;
    MagazineId=Identity; bActive=True; return True;
}
function bool Update(vector Palm,int Serial,float Now,int Identity,bool Held)
{
    local vector Stroke;
    local float Dt;
    if (!bActive) return False;
    if (!Held || Identity!=MagazineId || !Bounded(Palm) || Serial<LastSerial || !(Now>=LastTime && Now<10000000))
    { Cancel(); return False; }
    if (Serial==LastSerial || Now==LastTime) return False;
    Dt=Now-LastTime;
    if (Dt>MaximumAge || VSize(Palm-Previous)>MaximumSpeed*Dt+JumpTolerance)
    { Cancel(); return False; }
    Previous=Palm; LastSerial=Serial; LastTime=Now; FreshMoves++;
    Stroke=Palm-StartPalm;
    if (Sqrt(Stroke.X*Stroke.X+Stroke.Y*Stroke.Y)>0.12 || Stroke.Z>0.04)
    { Cancel(); return False; }
    Distance=FMax(0,-Stroke.Z); Progress=FClamp(Distance/Travel,0,1);
    if (Progress>=1 && FreshMoves>=2)
    { bActive=False; bComplete=True; CompleteCount++; return True; }
    return False;
}
defaultproperties
{
    Travel=0.065
    MaximumAge=0.15
    MaximumSpeed=4.0
    JumpTolerance=0.005
}
