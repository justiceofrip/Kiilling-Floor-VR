// Pure blade geometry shared by contact adapters. No actors, damage or timers.
class KFVRMeleeGeometry extends Object;
static function bool BoundedNumber(float Value,float Limit)
{
    return Value==Value && Value>= -Limit && Value<=Limit;
}
static function bool BoundedVector(vector Value,float Limit)
{
    return BoundedNumber(Value.X,Limit) && BoundedNumber(Value.Y,Limit) && BoundedNumber(Value.Z,Limit);
}
static function vector BladePoint(vector Base,vector Tip,int Index,int Intervals)
{
    return Base+(Tip-Base)*(float(Clamp(Index,0,Max(1,Intervals)))/float(Max(1,Intervals)));
}
// Adjacent trace boxes overlap along the blade. The knife's existing 20 cm
// span and 3 cm contact radius produce four intervals/five temporal traces.
static function int TraceIntervals(float LengthMetres,float RadiusMetres)
{
    if (!(LengthMetres>0 && LengthMetres<=1.5 && RadiusMetres>=0.005 && RadiusMetres<=0.10)) return 0;
    return Clamp(int(LengthMetres/(2*RadiusMetres))+1,1,160);
}
static function bool MovingContact(vector From,vector To,float Seconds,float Units,float MinimumSpeed)
{
    if (!BoundedVector(From,100000000) || !BoundedVector(To,100000000)
        || !(Seconds>0 && Seconds<=1) || !(Units>=1 && Units<=200)
        || !(MinimumSpeed>0 && MinimumSpeed<=15)) return False;
    return VSize(To-From)/(Seconds*Units)>=MinimumSpeed;
}
