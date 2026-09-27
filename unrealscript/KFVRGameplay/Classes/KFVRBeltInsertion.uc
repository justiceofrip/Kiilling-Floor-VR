// Position is the magazine well relative to the body-facing belt dock, in metres.
class KFVRBeltInsertion extends Object;
var vector Previous;
var float LastTime,Progress;
var bool bObserved,bApproached;
function Reset() { bObserved=False; bApproached=False; Progress=0; }
function bool Update(vector P,float Now,bool Aligned)
{
    local bool Continuous;
    local float Delta;
    if (bObserved && Now<=LastTime) return False;
    Delta=Now-LastTime;
    Continuous=bObserved && Delta>0 && Delta<=0.15 && VSize(P-Previous)<=4.0*Delta+0.005;
    if (!Continuous) bApproached=False;
    Previous=P; LastTime=Now; bObserved=True; Progress=0;
    if (!Aligned || Sqrt(P.Y*P.Y+P.Z*P.Z)>0.07 || P.X>0.25 || P.X< -0.04) { bApproached=False; return False; }
    if (P.X>=0.05) bApproached=True;
    if (!bApproached) return False;
    Progress=FClamp((0.08-P.X)/0.095,0,1);
    if (Continuous && P.X<= -0.015) { bApproached=False; return True; }
    return False;
}
