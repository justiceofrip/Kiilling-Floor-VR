class KFVRDualMagazineTests extends Commandlet;
var int Checks,Failures;
function Check(bool B) { Checks++; if (!B) Failures++; }
event int Main(string Params)
{
    local KFVRDualMagazineState S;
    local KFVRBeltInsertion G;
    local int I,H,Before,Choice;
    S=new(None) class'KFVRDualMagazineState'; Check(S.Initialize(15,5,6,41));
    Check(S.Eject(0) && S.Eject(1)); Check(S.Loaded(0)==1 && S.Loaded(1)==1);
    Check(S.Present(0) && S.Present(1)); Check(S.Reserve==0 && S.Presented[0]!=S.Presented[1]);
    Check(S.Seat(0) && S.Seat(1)); Check(S.Loaded(0)==16 && S.Loaded(1)==16);
    Check(!S.Consume(0) && !S.Consume(1)); Check(S.ClearBelt(0) && S.ClearBelt(1));
    Check(S.Consume(0) && S.Consume(1) && S.Total()==39 && S.Valid());
    S=new(None) class'KFVRDualMagazineState'; Check(S.Initialize(15,1,1,22));
    Check(S.Consume(0) && S.Consume(1)); Check(S.SlideLocked[0]==1 && S.SlideLocked[1]==1);
    Check(S.Eject(0) && S.Eject(1)); Check(S.Present(0) && S.Present(1));
    Check(S.Seat(0) && S.Seat(1)); Check(S.Loaded(0)==15 && S.Loaded(1)==5 && S.AssistCount==2 && S.Valid());
    Check(!S.Consume(0)); Check(S.ClearBelt(0) && S.Consume(0)); Check(!S.Consume(1));
    S.ReconcileTotal(4); Check(S.Valid() && S.Total()==4);
    S=new(None) class'KFVRDualMagazineState'; Check(S.Initialize(15,15,15,400));
    for (I=0;I<30000;I++)
    {
        H=Rand(2); Choice=Rand(7); Before=S.Total();
        if (Choice==0) { if (S.Consume(H)) Check(S.Total()==Before-1); else Check(S.Total()==Before); }
        else if (Choice==1) S.Eject(H);
        else if (Choice==2) S.Present(H);
        else if (Choice==3) S.Seat(H);
        else if (Choice==4) S.ClearBelt(H);
        else if (Choice==5) S.StoreForSwitch();
        else S.ReconcileTotal(Rand(501));
        Check(S.Valid());
    }
    G=new(None) class'KFVRBeltInsertion';
    Check(!G.Update(vect(0.10,0,0),1,True));
    Check(!G.Update(vect(0.03,0,0),1.04,True) && G.Progress>0);
    Check(G.Update(vect(-0.02,0,0),1.08,True));
    Check(!G.Update(vect(-0.02,0,0),1.08,True));
    Check(!G.Update(vect(-0.02,0,0),1.10,True));
    G.Reset(); Check(!G.Update(vect(-0.02,0,0),2,True));
    Check(!G.Update(vect(0.10,0,0),2.1,True));
    Check(!G.Update(vect(-0.02,0,0),2.101,True));
    G.Reset(); Check(!G.Update(vect(0.1,0,0),3,True));
    Check(!G.Update(vect(-0.02,0,0),3.4,True));
    Check(!G.Update(vect(0.1,0,0),3.45,True));
    Check(!G.Update(vect(-0.02,0,0),3.5,False));
    Check(!G.Update(vect(-0.02,0,0),3.55,True));
    G.Reset(); Check(!G.Update(vect(0.1,0.09,0),4,True));
    Check(!G.Update(vect(-0.02,0.09,0),4.05,True));
    Log("KFVR_DUAL_MAG_TEST_RESULT checks=" $ Checks $ " failures=" $ Failures);
    if (Failures>0) return 1;
    return 0;
}
