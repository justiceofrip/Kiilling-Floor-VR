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

// Snapshot excludes only the two fields a capacity notification may change.
function string Contents(KFVRDualMagazineState A)
{
    local int I;
    local string S;
    S=A.MagazineCount $ ":" $ A.Reserve $ ":" $ A.Budget $ ":" $ A.Spent $ ":" $ A.AssistCount;
    for (I=0;I<2;I++) S=S $ ":" $ A.Inserted[I] $ ":" $ A.Presented[I]
        $ ":" $ A.Chamber[I] $ ":" $ A.SlideLocked[I] $ ":" $ A.BeltLatched[I];
    for (I=0;I<64;I++) S=S $ ":" $ A.GetMagazineRounds(I+1) $ ":" $ A.GetMagazineCapacity(I+1);
    return S;
}
function CheckCapacity(KFVRDualMagazineState A,int NewCapacity,string Label)
{
    local int OldCapacity,OldRevision;
    local string Before;
    local bool Accepted;
    OldCapacity=A.Capacity; OldRevision=A.Revision; Before=Contents(A);
    Accepted=NewCapacity>=1 && NewCapacity<=100;
    Check(A.SetCapacity(NewCapacity)==Accepted,Label $ " accepted range");
    Check(Contents(A)==Before,Label $ " ownership contents and mechanics unchanged");
    if (Accepted)
    {
        Check(A.Capacity==NewCapacity,Label $ " current supply");
        Check(A.Revision==OldRevision+int(NewCapacity!=OldCapacity),Label $ " revision only on change");
    }
    else Check(A.Capacity==OldCapacity && A.Revision==OldRevision,Label $ " rejection has no mutation");
    Check(A.Valid(),Label $ " independent old limits remain valid");
}
function RejectInitialize(KFVRDualMagazineState A,int Cap,int RightLoaded,int LeftLoaded,int Total)
{
    local string Before;
    Before=A.Capacity $ ":" $ A.Revision $ ":" $ Contents(A);
    Check(!A.Initialize(Cap,RightLoaded,LeftLoaded,Total),"invalid dual initialization rejected");
    Check(Before==(A.Capacity $ ":" $ A.Revision $ ":" $ Contents(A)),"invalid initialization leaves all fields untouched");
}
function CheckCapacities()
{
    local KFVRDualMagazineState A;
    local int I,RightId,LeftId,ReserveBefore;
    A=new(None) class'KFVRDualMagazineState'; Check(A.Initialize(40,40,41,300),"dual capacity seed");
    CheckCapacity(A,50,"upgrade inserted pair");
    Check(A.Loaded(0)==40 && A.Loaded(1)==41 && A.GetMagazineCapacity(1)==40 && A.GetMagazineCapacity(2)==40,
        "upgrade does not fill inserted magazines");
    Check(A.Eject(0) && A.Present(0),"present newly supplied right magazine"); RightId=A.Presented[0];
    Check(RightId==3 && A.GetMagazineRounds(RightId)==50 && A.GetMagazineCapacity(RightId)==50,"new right supply capacity");
    CheckCapacity(A,15,"downgrade presented right and inserted left");
    Check(A.Eject(1),"store old left magazine"); CheckCapacity(A,44,"change stored and presented pair");
    Check(A.Present(1),"present replacement left magazine"); LeftId=A.Presented[1];
    Check(LeftId==4 && A.GetMagazineRounds(LeftId)==44 && A.GetMagazineCapacity(LeftId)==44
        && A.GetMagazineRounds(1)==39 && A.GetMagazineRounds(2)==40,"fresh left supply retains earlier identities");
    CheckCapacity(A,40,"both presented magazines retain earlier limits");
    Check(A.Seat(0) && A.Seat(1) && A.Loaded(0)==51 && A.Loaded(1)==45,"both old supplies seat with retained chambers");
    CheckCapacity(A,50,"capacity change while belt latched");
    Check(!A.Consume(0) && !A.Consume(1),"capacity change never clears belt latch");
    Check(A.ClearBelt(0) && A.ClearBelt(1) && A.Consume(0) && A.Consume(1) && A.Total()==298,
        "grandfathered magazines fire with normal ownership rules");
    CheckCapacity(A,50,"duplicate supply notification");
    CheckCapacity(A,0,"zero supply rejected"); CheckCapacity(A,-1,"negative supply rejected");
    CheckCapacity(A,101,"excessive supply rejected");

    A=new(None) class'KFVRDualMagazineState'; Check(A.Initialize(40,1,5,76),"empty shell with independent loaded left");
    Check(A.Eject(0),"store empty right shell"); CheckCapacity(A,50,"upgrade empty stored shell");
    Check(A.GetMagazineCapacity(1)==40 && A.Present(0) && A.Presented[0]==1 && A.MagazineCount==2
        && A.GetMagazineRounds(1)==50 && A.GetMagazineCapacity(1)==50 && A.Reserve==20,"refill stamps reused identity only on supply");
    CheckCapacity(A,15,"downgrade presented full shell");
    Check(A.Seat(0) && A.ClearBelt(0),"seat full grandfathered shell");
    for (I=0;I<51;I++) Check(A.Consume(0),"consume full grandfathered right supply");
    Check(!A.CanFire(0) && A.Loaded(1)==5 && A.GetMagazineRounds(2)==4 && A.GetMagazineCapacity(2)==40 && A.Total()==25,
        "right depletion preserves independent left contents");
    Check(A.Eject(0) && A.Present(0) && A.Presented[0]==1 && A.GetMagazineRounds(1)==15
        && A.GetMagazineCapacity(1)==15 && A.Reserve==5,"empty refill adopts smaller supply limit");

    A=new(None) class'KFVRDualMagazineState'; Check(A.Initialize(50,51,1,62),"larger retained dual magazine seed");
    CheckCapacity(A,40,"downgrade retained dual supply");
    Check(A.Eject(0) && A.Present(0) && A.Presented[0]==1 && A.GetMagazineRounds(1)==50 && A.Reserve==10,
        "prefer full retained magazine to smaller fresh supply");
    A=new(None) class'KFVRDualMagazineState'; Check(A.Initialize(40,0,0,7),"limited dual reserve seed");
    CheckCapacity(A,50,"upgrade limited dual reserve");
    Check(A.Eject(0) && A.Present(0),"supply partial replacement"); RightId=A.Presented[0];
    Check(A.GetMagazineRounds(RightId)==7 && A.GetMagazineCapacity(RightId)==50 && A.Total()==7 && A.Reserve==0,
        "partial dual supply never invents rounds");

    A=new(None) class'KFVRDualMagazineState'; Check(A.Initialize(2,2,2,1000),"full pool dual seed");
    Check(A.Eject(0) && A.Eject(1),"retain initial dual partials");
    for (I=2;I<64;I++)
        Check(A.Present(0) && A.Seat(0) && A.ClearBelt(0) && A.Consume(0) && A.Eject(0),"fill dual pool with retained partials");
    Check(A.MagazineCount==64 && A.Valid(),"all dual pool identities retained");
    CheckCapacity(A,50,"increase supply at dual pool limit"); ReserveBefore=A.Reserve;
    Check(A.Present(0) && A.Presented[0]>0 && A.MagazineCount==64 && A.Reserve==ReserveBefore,
        "full dual pool falls back to retained partial");
    for (I=0;I<64;I++) Check(A.GetMagazineRounds(I+1)==1 && A.GetMagazineCapacity(I+1)==2,"dual fallback never refills partials");

    A=new(None) class'KFVRDualMagazineState';
    Check(A.Initialize(15,16,16,62) && A.Loaded(0)==16 && A.Loaded(1)==16 && A.Reserve==30,
        "initialize dual tactical plus one");
    A=new(None) class'KFVRDualMagazineState';
    Check(A.Initialize(40,51,45,126) && A.GetMagazineCapacity(1)==50 && A.GetMagazineCapacity(2)==44
        && A.Loaded(0)==51 && A.Loaded(1)==45 && A.Reserve==30 && A.Valid(),"infer each old stock magazine limit independently");
    A=new(None) class'KFVRDualMagazineState';
    Check(A.Initialize(1,101,101,202) && A.GetMagazineCapacity(1)==100 && A.GetMagazineCapacity(2)==100 && A.Valid(),
        "bounded maximum initial dual loads");
    RejectInitialize(A,0,1,1,2); RejectInitialize(A,101,1,1,2);
    RejectInitialize(A,40,-1,1,2); RejectInitialize(A,40,1,-1,2);
    RejectInitialize(A,40,102,1,103); RejectInitialize(A,40,1,102,103);
    RejectInitialize(A,40,51,51,101); RejectInitialize(A,40,1,1,10001);
    Check(A.GetMagazineCapacity(0)==0 && A.GetMagazineCapacity(-1)==0
        && A.GetMagazineCapacity(A.MagazineCount+1)==0 && A.GetMagazineCapacity(65)==0,
        "capacity access rejects absent and invalid identities");
    Check(A.GetMagazineRounds(0)==0 && A.GetMagazineRounds(-1)==0
        && A.GetMagazineRounds(A.MagazineCount+1)==0 && A.GetMagazineRounds(65)==0,
        "round access rejects absent and invalid identities");
    Check(A.Valid(),"read-only invalid access leaves valid state");
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
        H=Rand(2); Choice=Rand(8); Before=S.Total();
        if (Choice==0) { if (S.Consume(H)) Check(S.Total()==Before-1); else Check(S.Total()==Before); }
        else if (Choice==1) S.Eject(H);
        else if (Choice==2) S.Present(H);
        else if (Choice==3) S.Seat(H);
        else if (Choice==4) S.ClearBelt(H);
        else if (Choice==5) S.StoreForSwitch();
        else if (Choice==6) S.ReconcileTotal(Rand(501));
        else CheckCapacity(S,1+Rand(100),"random dual capacity change");
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
    CheckCapacities();
    Log("KFVR_DUAL_MAG_TEST_RESULT checks=" $ Checks $ " failures=" $ Failures);
    if (Failures>0) return 1;
    return 0;
}
