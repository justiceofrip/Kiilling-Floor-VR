class KFVRArmMathTests extends Commandlet;
var int Checks,Failures;
function Check(bool B) { Checks++; if (!B) Failures++; }
event int Main(string Params)
{
    local vector A,H,P,S,E,X,Y,Z,OriginalX,OriginalY,OriginalZ;
    local rotator R,Aligned;
    local int I;
    local bool Solved;
    A=vect(0.03,0.18,-0.22); P=vect(-0.35,0.55,-0.75);
    for (I=0;I<720;I++)
    {
        H.X=0.35+0.2*Cos(I*0.013); H.Y=0.18+0.3*Sin(I*0.021); H.Z=-0.22+0.4*Sin(I*0.017);
        Solved=class'KFVRArmMath'.static.Solve(A,H,P,0.28,0.25,S,E);
        R.Yaw=I*91; R.Pitch=I*37; R.Roll=I*53;
        GetAxes(R,OriginalX,OriginalY,OriginalZ);
        Aligned=class'KFVRArmMath'.static.PlaneFrame(OriginalX,OriginalY*0.6+OriginalZ*0.8,0.6,0.8); GetAxes(Aligned,X,Y,Z);
        Check((X dot OriginalX)>0.9999); Check((Y dot OriginalY)>0.9999 && (Z dot OriginalZ)>0.9999);
        Check(Solved); Check(Abs(VSize(E-S)-0.28)<0.0001); Check(Abs(VSize(H-E)-0.25)<0.0001);
        Check(((E-S) cross (H-E)) dot ((H-S) cross P)<0.0001);
    }
    Check(!class'KFVRArmMath'.static.Solve(A,A,P,0.28,0.25,S,E));
    Check(!class'KFVRArmMath'.static.Solve(A,vect(5,0,0),P,0.28,0.25,S,E));
    Check(!class'KFVRArmMath'.static.Solve(A,vect(0.4,0,0),P,0,0.25,S,E));
    Check(class'KFVRArmMath'.static.Solve(vect(0,0,0),vect(0,0,0.4),vect(0,0,1),0.28,0.25,S,E));
    Check(Abs(VSize(E)-0.28)<0.0001 && Abs(VSize(E-vect(0,0,0.4))-0.25)<0.0001);
    Aligned=class'KFVRArmMath'.static.PlaneFrame(vect(-1,0,0),vect(0,1,0),1,0); Check(vector(Aligned) dot vect(-1,0,0)>0.9999);
    Log("KFVR_ARM_TEST_RESULT checks=" $ Checks $ " failures=" $ Failures);
    if (Failures>0) return 1;
    return 0;
}
