class KFVRPoseMath extends Object;
static function vector TurnVector(vector V,vector Axis,float C,float S)
{ return V*C+(Axis cross V)*S+Axis*(Axis dot V)*(1-C); }
// Shortest rotation aligns a measured model grip vector with the two hands,
// retaining the dominant controller's roll. It does not use the headset axes.
static function rotator AlignSupport(rotator Dominant,vector LocalSupport,vector Hands)
{
    local vector From,Target,Axis,X,Y,Z;
    local float C,S;
    From=Normal(LocalSupport>>Dominant); Target=Normal(Hands);
    C=FClamp(From dot Target,-1,1); Axis=From cross Target; S=VSize(Axis);
    if (S<0.0001 || C< -0.95) return Dominant;
    Axis/=S; GetAxes(Dominant,X,Y,Z);
    return OrthoRotation(TurnVector(X,Axis,C,S),TurnVector(Y,Axis,C,S),TurnVector(Z,Axis,C,S));
}
