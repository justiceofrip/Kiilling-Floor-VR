// Body-relative interaction location; presentation and gestures share one point.
class KFVRAmmoSupplyState extends Object;
static function vector ChestPosition(vector Head,rotator Body,float Units)
{
    local vector X,Y,Z;
    GetAxes(Body,X,Y,Z);
    return Head+(X*0.22-Y*0.10-Z*0.36)*Units;
}
