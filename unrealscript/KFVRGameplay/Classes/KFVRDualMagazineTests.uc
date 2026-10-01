class KFVRDualMagazineTests extends Commandlet;
var int Checks,Failures;
function Check(bool B,optional string Message) { Checks++; if (!B) { Failures++; Log("KFVR_DUAL_MAG_FAIL " $ Message); } }

function CheckSourceClock()
{
    local KFVRBeltInsertion G,Scaled;
    local int Trial,Step,Repeat;
    local float SourceNow,GameNow,Dilation,SavedProgress;
    local vector P;
    local bool Seated,ScaledSeated;
    G=new(None) class'KFVRBeltInsertion';
    Scaled=new(None) class'KFVRBeltInsertion';
    Check(!G.Update(vect(0.10,0,0),10,True),"source approach");
    Check(!G.Update(vect(0.04,0,0),10.03,True) && G.bApproached,"source partial push");
    SavedProgress=G.Progress;
    for (Repeat=0;Repeat<64;Repeat++)
    {
        Check(!G.Update(vect(0.04,0,0),10.03,True),"duplicate cannot seat");
        // Even a changed derived pose/alignment on the same source sample must
        // not shorten the next real motion interval or cancel an armed stroke.
        Check(!G.Update(vect(-0.02,0.20,0),10.03,False),"duplicate cannot alter pose");
        Check(G.Previous==vect(0.04,0,0) && G.LastTime==10.03
            && G.Progress==SavedProgress && G.bApproached,"duplicate keeps complete state");
    }
    Check(G.Update(vect(-0.02,0,0),10.06,True),"fresh motion uses full source interval");

    // The same 12 cm / 80 ms physical stroke must seat at normal game speed,
    // during ZED time and while dilation changes back. The comparison gate fed
    // scaled time reproduces the old failure, proving this is a meaningful path.
    for (Trial=0;Trial<3;Trial++)
    {
        G.Reset(); Scaled.Reset(); SourceNow=20; GameNow=20;
        Check(!G.Update(vect(0.10,0,0),SourceNow,True),"unscaled fast approach");
        Check(!Scaled.Update(vect(0.10,0,0),GameNow,True),"scaled comparison approach");
        for (Step=1;Step<=4;Step++)
        {
            Dilation=1.1;
            if (Trial==1 || (Trial==2 && Step<=2)) Dilation=0.22;
            SourceNow+=0.02; GameNow+=0.02*Dilation;
            P=vect(0,0,0); P.X=0.10-0.03*Step;
            Seated=G.Update(P,SourceNow,True);
            ScaledSeated=Scaled.Update(P,GameNow,True);
            Check(Seated==(Step==4),"source timing seats only at full stroke");
            for (Repeat=0;Repeat<8;Repeat++)
                Check(!G.Update(P,SourceNow,True),"duplicate source never creates another event");
        }
        Check(ScaledSeated==(Trial==0),"scaled clock reproduces slow-motion failure");
    }

    G.Reset();
    Check(!G.Update(vect(0.10,0,0),60,True),"pre-rollback approach");
    Check(!G.Update(vect(0.04,0,0),60.03,True),"pre-rollback partial");
    SavedProgress=G.Progress;
    Check(!G.Update(vect(-0.02,0,0),59.99,True),"older source time cannot seat");
    Check(G.LastTime==60.03 && G.Previous==vect(0.04,0,0)
        && G.Progress==SavedProgress && G.bApproached,"older source cannot mutate state");
    // Native source-clock rollback emits neutral; Suspend resets both gates.
    // A new lower clock epoch must still observe an actual outside approach.
    G.Reset();
    Check(!G.Update(vect(-0.02,0,0),0,True) && !G.bApproached,"reset seat acquisition rejected");
    Check(!G.Update(vect(-0.02,0,0),0.02,True),"reset dwell cannot seat");
    Check(!G.Update(vect(0.10,0,0),0.06,True) && G.bApproached,"new epoch fresh approach");
    Check(!G.Update(vect(0.04,0,0),0.09,True),"new epoch partial");
    Check(G.Update(vect(-0.02,0,0),0.12,True),"new epoch full push seats");
    Check(!G.Update(vect(-0.02,0,0),0.12,True),"new epoch duplicate cannot seat twice");
}

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
    CheckSourceClock();
    Log("KFVR_DUAL_MAG_TEST_RESULT checks=" $ Checks $ " failures=" $ Failures);
    if (Failures>0) return 1;
    return 0;
}
