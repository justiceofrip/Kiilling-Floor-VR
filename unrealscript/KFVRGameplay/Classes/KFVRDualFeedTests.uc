class KFVRDualFeedTests extends Commandlet;
var int Checks,Failures;
function Check(bool B) { Checks++; if (!B) Failures++; }
event int Main(string Params)
{
    local KFVRDualFeedState S;
    local int I,Before,Mode;
    S=new(None) class'KFVRDualFeedState'; S.Initialize(30,30);
    for (I=0;I<15;I++) Check(S.Consume(1));
    Check(!S.Consume(1) && S.RightLoaded==15 && S.Total()==15);
    Check(S.Consume(0) && S.Total()==14);
    S.Reconcile(30); Check(S.LeftLoaded==15 && S.RightLoaded==15);
    for (I=0;I<20000;I++)
    {
        Mode=I%2; Before=S.Total();
        if (I%7==0) S.Reconcile(I%31);
        else if (S.Consume(Mode)) Check(S.Total()==Before-1);
        else Check(S.Loaded(Mode)==0 && S.Total()==Before);
        Check(S.Valid());
    }
    Log("KFVR_DUAL_TEST_RESULT checks=" $ Checks $ " failures=" $ Failures);
    if (Failures>0) return 1;
    return 0;
}
