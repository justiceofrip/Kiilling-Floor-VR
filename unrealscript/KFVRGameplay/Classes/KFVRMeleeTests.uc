class KFVRMeleeTests extends Commandlet;
var int Checks,Failures;
function Check(bool B) { Checks++; if (!B) Failures++; }
event int Main(string Params)
{
    local KFVRMeleeState S;
    local int I;
    local vector P;
    S=new(None) class'KFVRMeleeState';
    Check(!S.Sample(vect(0,0,0),0,0.6));
    Check(!S.Sample(vect(0.1,0,0),0.04,0.6));
    // Duplicate game ticks must not reset a physically continuing stroke.
    Check(!S.Sample(vect(0.1,0,0),0.05,0.6));
    Check(S.Sample(vect(0.25,0,0),0.08,0.6));
    for (I=1;I<=60;I++) Check(!S.Sample(vect(0.25,0,0),0.08+I*0.02,0.6));
    Check(!S.Sample(vect(0.05,0,0),1.32,0.6));
    Check(!S.Sample(vect(0.15,0,0),1.36,0.6));
    Check(S.Sample(vect(0.30,0,0),1.40,0.6));
    S.ResetTracking(); Check(!S.Sample(vect(0,0,0),1.41,0.6));
    Check(!S.Sample(vect(0.3,0,0),1.47,0.6));
    S=new(None) class'KFVRMeleeState';
    for (I=0;I<10000;I++) { P.X=0.02*Sin(I); Check(!S.Sample(P,I*0.01,0.6)); }
    Check(S.Strikes==0);
    S.ResetTracking(); Check(!S.Sample(vect(0,0,0),101,0.6));
    Check(!S.Sample(vect(1,0,0),101.001,0.6)); Check(S.RejectedJumps>0);
    Check(!S.Sample(vect(1,0,0),102,0.6));
    Check(!S.Sample(vect(1.3,0,0),101,0.6));
    Log("KFVR_MELEE_TEST_RESULT checks=" $ Checks $ " failures=" $ Failures);
    if (Failures>0) return 1;
    return 0;
}
