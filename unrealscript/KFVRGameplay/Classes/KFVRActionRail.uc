// Pure controller-relative rail geometry. Inputs are metres in the dominant
// controller aim frame, before weapon recoil or presentation offsets. The caller
// owns fresh input edges, tracking/reference validity, release policy and ammo.
class KFVRActionRail extends Object;
var bool bActive;
var vector StartPoint,PullAxis,SideAxis,UpAxis;
var float TravelMetres,StartTravel,Progress,AlongMetres,SideMetres,UpMetres;

function bool BoundedNumber(float Value,float Limit)
{
    // Ordered bounded comparisons reject infinities; self-comparison rejects
    // NaN without depending on a newer-engine IsFinite API.
    return Value==Value && Value>= -Limit && Value<=Limit;
}
function bool BoundedVector(vector Value,float Limit)
{
    return BoundedNumber(Value.X,Limit) && BoundedNumber(Value.Y,Limit)
        && BoundedNumber(Value.Z,Limit);
}
function Cancel()
{
    // Cancellation is not a return stroke. In particular, retain Progress so
    // even inspection cannot mistake tracking loss for a physical return.
    bActive=False;
}
function bool Begin(vector RelativeMetres,vector Axis,float InTravelMetres,optional float InitialTravel)
{
    local vector Pull,Side,Up;
    local float AxisLength;
    Cancel();
    if (!BoundedVector(RelativeMetres,4) || !BoundedVector(Axis,100)
        || !(InTravelMetres>=0.01 && InTravelMetres<=0.25)
        || !(InitialTravel>=0 && InitialTravel<=1)) return False;
    AxisLength=VSize(Axis);
    if (!(AxisLength>=0.0001 && AxisLength<=200)) return False;
    Pull=Axis/AxisLength;
    // Preserve the existing pistol's square Y/Z envelope exactly when its
    // pull axis is -X. Near a Y-parallel rail, use Z to avoid a degenerate basis.
    Side=vect(0,1,0)-Pull*Pull.Y;
    if (VSize(Side)<0.1) Side=vect(0,0,1)-Pull*Pull.Z;
    Side=Normal(Side); Up=Normal(Side cross Pull);
    if (!(VSize(Side)>0.99 && VSize(Up)>0.99)) return False;
    StartPoint=RelativeMetres; PullAxis=Pull; SideAxis=Side; UpAxis=Up;
    TravelMetres=InTravelMetres; StartTravel=InitialTravel; Progress=InitialTravel;
    AlongMetres=0; SideMetres=0; UpMetres=0; bActive=True;
    return True;
}
function bool Move(vector RelativeMetres)
{
    local vector Delta;
    if (!bActive) return False;
    if (!BoundedVector(RelativeMetres,4)) { Cancel(); return False; }
    Delta=RelativeMetres-StartPoint;
    AlongMetres=Delta dot PullAxis;
    SideMetres=Delta dot SideAxis; UpMetres=Delta dot UpAxis;
    if (!(Abs(SideMetres)<=0.12 && Abs(UpMetres)<=0.12))
    { Cancel(); return False; }
    Progress=FClamp(StartTravel+AlongMetres/TravelMetres,0,1);
    return True;
}
