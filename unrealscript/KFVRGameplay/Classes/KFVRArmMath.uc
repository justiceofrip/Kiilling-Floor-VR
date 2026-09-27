// Analytic two-link arm geometry in KF world coordinates; no gameplay writes.
class KFVRArmMath extends Object;
static function bool Solve(vector Anchor,vector Hand,vector Pole,float Upper,float Lower,out vector Shoulder,out vector Elbow)
{
    local vector Direction,Bend;
    local float Distance,Along,Height,Reach;
    if (Upper<=0 || Lower<=0) return False;
    Direction=Hand-Anchor; Distance=VSize(Direction);
    if (Distance<0.001) return False;
    Direction/=Distance; Shoulder=Anchor;
    Reach=Upper+Lower-0.001;
    // Preserve the controller wrist. A small clavicle shift handles extension.
    if (Distance>Reach) { Shoulder=Hand-Direction*Reach; Distance=Reach; }
    if (Distance<Abs(Upper-Lower)+0.001) { Distance=Abs(Upper-Lower)+0.001; Shoulder=Hand-Direction*Distance; }
    Bend=Pole-Direction*(Pole dot Direction);
    if (VSize(Bend)<0.001) Bend=vect(0,0,1)-Direction*Direction.Z;
    if (VSize(Bend)<0.001) Bend=vect(0,1,0)-Direction*Direction.Y;
    Bend=Normal(Bend);
    Along=(Upper*Upper-Lower*Lower+Distance*Distance)/(2*Distance);
    Height=Sqrt(FMax(0,Upper*Upper-Along*Along));
    Elbow=Shoulder+Direction*Along+Bend*Height;
    return VSize(Shoulder-Anchor)<0.3;
}

static function rotator PlaneFrame(vector Direction,vector Plane,float PlaneY,float PlaneZ)
{
    local vector X,Y,Z;
    X=Normal(Direction); Plane=Normal(Plane-X*(Plane dot X));
    Y=Normal(Plane*PlaneY+(Plane cross X)*PlaneZ); Z=Normal(X cross Y);
    return OrthoRotation(X,Y,Z);
}
