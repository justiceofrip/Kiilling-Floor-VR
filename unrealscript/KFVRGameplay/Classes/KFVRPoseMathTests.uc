class KFVRPoseMathTests extends Commandlet;
var int Checks,Failures;
function Check(bool B) { Checks++; if (!B) Failures++; }
event int Main(string Params)
{
    local vector Source,Target,X,Y,Z;
    local rotator R,Result;
    local int I;
    Source=vect(0.31,-0.06,0.04);
    for (I=0;I<360;I++)
    {
        R.Yaw=I*182; R.Pitch=(I%70)*100-3500; R.Roll=(I%100)*160-8000;
        Target=Normal(vect(0.9,0.3,0.2)>>R);
        Result=class'KFVRPoseMath'.static.AlignSupport(R,Source,Target);
        Check((Normal(Source>>Result) dot Target)>0.999);
        GetAxes(Result,X,Y,Z); Check(Abs(X dot Y)<0.0001 && Abs(Y dot Z)<0.0001);
    }
    Log("KFVR_SUPPORT_TEST_RESULT checks=" $ Checks $ " failures=" $ Failures);
    if (Failures>0) return 1;
    return 0;
}
