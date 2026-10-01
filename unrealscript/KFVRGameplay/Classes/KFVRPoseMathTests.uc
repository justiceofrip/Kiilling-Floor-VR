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
    // Native quaternion blend must use the short arc across rotator wrap.
    Result=class'KFVRHandPoseMath'.static.BlendRotation(rot(0,32000,0),rot(0,-32000,0),0.5);
    GetAxes(Result,X,Y,Z); Check(X.X< -0.999 && Abs(X.Y)<0.005);
    Result=class'KFVRHandPoseMath'.static.BlendRotation(rot(0,0,0),rot(0,16384,0),0.5);
    GetAxes(Result,X,Y,Z); Check(Abs(X.X-0.707107)<0.001 && Abs(X.Y-0.707107)<0.001);
    Check(class'KFVRHandPoseMath'.static.SourceBone(True,10)=='Hands_LArmFinger32');
    Check(class'KFVRHandPoseMath'.static.SourceBone(False,15)=='Hands_RArmFinger44');
    Log("KFVR_SUPPORT_TEST_RESULT checks=" $ Checks $ " failures=" $ Failures);
    if (Failures>0) return 1;
    return 0;
}
