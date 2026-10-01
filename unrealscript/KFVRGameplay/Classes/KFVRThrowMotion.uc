// Controller motion in body-relative metres. The caller adds pawn velocity once.
class KFVRThrowMotion extends Object;
var vector Previous,Velocity;
var float LastTime;
var int Samples,Rejected;
var bool bDiscontinuity;
function Reset()
{
    Samples=0; Velocity=vect(0,0,0); bDiscontinuity=False;
}
// True means a fresh source sample was consumed, not that throw history is ready.
// The caller owns the serial watermark so lifecycle resets can start a new host.
function bool AddSource(vector Position,float SourceSeconds,int Serial,out int LastSerial)
{
    if (Serial<=0 || Serial<=LastSerial) return False;
    LastSerial=Serial;
    Add(Position,SourceSeconds);
    return True;
}
function bool Add(vector Position,float Now)
{
    local float Delta,Speed;
    local vector Instant;
    bDiscontinuity=False;
    if (Samples==0) { Previous=Position; LastTime=Now; Samples=1; Velocity=vect(0,0,0); return False; }
    Delta=Now-LastTime;
    if (Delta<0.001 || Delta>0.1)
    { Reset(); Rejected++; bDiscontinuity=True; return False; }
    Instant=(Position-Previous)/Delta; Speed=VSize(Instant);
    if (Speed>12)
    { Reset(); Rejected++; bDiscontinuity=True; return False; }
    if (Samples==1) Velocity=Instant;
    else Velocity+= (Instant-Velocity)*FClamp(Delta/0.065,0,1);
    Previous=Position; LastTime=Now; Samples++; return Samples>=3;
}
function bool Ready(float Now)
{
    return Samples>=3 && Now>=LastTime && Now-LastTime<=0.1 && VSize(Velocity)<=12.001;
}
