class KFVRMagazineTests extends Commandlet;
var int Checks,Failures;
function Check(bool B,string Message) { Checks++; if (!B) { Failures++; Log("KFVR_MAG_FAIL " $ Message); } }
// Every field other than supply Capacity and Revision must survive SetCapacity.
function string Contents(KFVRMagazineState A)
{
    local int I;
    local string S;
    S=A.MagazineCount $ ":" $ A.Inserted $ ":" $ A.Held $ ":" $ A.Reserve $ ":" $ A.Budget
        $ ":" $ A.Spent $ ":" $ A.Discarded $ ":" $ A.bChamber $ ":" $ A.bFeedArmed
        $ ":" $ A.bExtracted $ ":" $ A.bSlideLocked $ ":" $ A.Travel;
    for (I=0;I<64;I++) S=S $ ":" $ A.GetMagazineRounds(I+1) $ ":" $ A.GetMagazineCapacity(I+1);
    return S;
}
function CheckCapacity(KFVRMagazineState A,int NewCapacity,string Label)
{
    local int OldCapacity,OldRevision;
    local string Before;
    local bool Accepted;
    OldCapacity=A.Capacity; OldRevision=A.Revision; Before=Contents(A);
    Accepted=NewCapacity>=1 && NewCapacity<=100;
    Check(A.SetCapacity(NewCapacity)==Accepted,Label $ " accepted range");
    Check(Contents(A)==Before,Label $ " all contents and mechanics unchanged");
    if (Accepted)
    {
        Check(A.Capacity==NewCapacity,Label $ " supply capacity");
        Check(A.Revision==OldRevision+int(NewCapacity!=OldCapacity),Label $ " one revision only on change");
    }
    else Check(A.Capacity==OldCapacity && A.Revision==OldRevision,Label $ " rejected without mutation");
    Check(A.Valid(),Label $ " valid grandfathered identities");
}
function RejectInitialize(KFVRMagazineState A,int Cap,int Loaded,int Total)
{
    local string Before;
    Before=A.Capacity $ ":" $ A.Revision $ ":" $ Contents(A);
    Check(!A.Initialize(Cap,Loaded,Total),"invalid initialization rejected");
    Check(Before==(A.Capacity $ ":" $ A.Revision $ ":" $ Contents(A)),"invalid initialization leaves state untouched");
}
function CheckCapacities()
{
    local KFVRMagazineState A;
    local int I,Cap,Identity,ReserveBefore;
    A=new class'KFVRMagazineState'; Check(A.Initialize(40,40,240),"rifle supply seed");
    CheckCapacity(A,50,"upgrade inserted"); CheckCapacity(A,44,"downgrade inserted");
    Check(A.Loaded()==40 && A.GetMagazineRounds(1)==39 && A.GetMagazineCapacity(1)==40,"inserted magazine does not grow");
    Check(A.Eject(True),"catch rifle magazine"); CheckCapacity(A,48,"change while held");
    Check(A.Stow(),"store rifle magazine"); CheckCapacity(A,40,"change while stored");
    Check(A.GrabBelt(),"supply fresh magazine"); Identity=A.Held;
    Check(Identity==2 && A.GetMagazineRounds(Identity)==40 && A.GetMagazineCapacity(Identity)==40
        && A.GetMagazineRounds(1)==39 && A.Reserve==160,"fresh supply preserves stored partial");
    CheckCapacity(A,15,"downgrade held fresh magazine");
    Check(A.Seat() && A.Inserted==Identity && A.Loaded()==41,"grandfathered magazine seats unchanged");
    Check(A.ConsumeShot() && A.Loaded()==40 && A.Total()==239,"grandfathered magazine fires normally");
    A.MoveSlide(1); CheckCapacity(A,50,"change during pending chamber cycle");
    A.MoveSlide(0); Check(A.Valid(),"pending cycle still returns normally");
    CheckCapacity(A,50,"duplicate stock notification");
    CheckCapacity(A,0,"reject zero supply"); CheckCapacity(A,-1,"reject negative supply");
    CheckCapacity(A,101,"reject excessive supply");

    A=new class'KFVRMagazineState'; Check(A.Initialize(40,1,101),"empty retained shell seed");
    Check(A.Eject(True) && A.Stow(),"retain empty shell"); CheckCapacity(A,50,"upgrade empty stored shell");
    Check(A.GetMagazineCapacity(1)==40,"empty shell is not restamped by notification");
    Check(A.GrabBelt() && A.Held==1 && A.MagazineCount==1 && A.GetMagazineRounds(1)==50
        && A.GetMagazineCapacity(1)==50 && A.Reserve==50,"refill reuses identity and stamps new limit");
    CheckCapacity(A,15,"held full shell survives downgrade");
    Check(A.Seat() && A.Loaded()==51,"tactical old-capacity plus one");
    for (I=0;I<51;I++) Check(A.ConsumeShot(),"consume grandfathered supply to empty");
    Check(!A.CanFire() && A.Total()==50 && A.Spent==51,"no extra shot from capacity update");
    Check(A.Eject(True),"catch spent shell"); CheckCapacity(A,44,"change held empty shell");
    Check(A.GetMagazineCapacity(1)==50 && A.Stow() && A.GrabBelt() && A.Held==1
        && A.GetMagazineRounds(1)==44 && A.GetMagazineCapacity(1)==44 && A.Reserve==6,"only actual refill updates empty limit");

    A=new class'KFVRMagazineState'; Check(A.Initialize(2,2,1000),"bounded identity pool seed");
    Check(A.Eject(True) && A.Stow(),"retain first pool partial");
    for (I=1;I<64;I++)
        Check(A.GrabBelt() && A.Seat() && A.ConsumeShot() && A.Eject(True) && A.Stow(),"fill pool with distinct partials");
    Check(A.MagazineCount==64 && A.Valid(),"pool full without lost identities");
    CheckCapacity(A,50,"increase supply with full pool"); ReserveBefore=A.Reserve;
    Check(A.GrabBelt() && A.Held>0 && A.MagazineCount==64 && A.Reserve==ReserveBefore,
        "full pool falls back to usable retained magazine");
    for (I=0;I<64;I++) Check(A.GetMagazineRounds(I+1)==1 && A.GetMagazineCapacity(I+1)==2,"pool fallback cannot refill partials");

    A=new class'KFVRMagazineState'; Check(A.Initialize(50,51,61),"full tactical magazine seed");
    CheckCapacity(A,40,"downgrade overfull inserted magazine");
    Check(A.Eject(True) && A.Stow() && A.GrabBelt() && A.Held==1 && A.GetMagazineRounds(1)==50
        && A.GetMagazineCapacity(1)==50 && A.Reserve==10,"prefer retained larger magazine without clamping");
    A=new class'KFVRMagazineState'; Check(A.Initialize(40,0,7),"limited reserve seed");
    CheckCapacity(A,50,"upgrade limited reserve");
    Check(A.Eject(True) && A.Stow() && A.GrabBelt() && A.GetMagazineRounds(1)==7
        && A.GetMagazineCapacity(1)==50 && A.Reserve==0 && A.Total()==7,"partial fresh supply cannot invent rounds");

    for (I=0;I<4;I++)
    {
        Cap=40; if (I==1) Cap=44; else if (I==2) Cap=48; else if (I==3) Cap=50;
        A=new class'KFVRMagazineState';
        Check(A.Initialize(Cap,Cap+1,Cap+31) && A.Loaded()==Cap+1 && A.GetMagazineRounds(1)==Cap
            && A.GetMagazineCapacity(1)==Cap && A.Reserve==30 && A.Valid(),"initialize tactical plus one at each rifle perk capacity");
    }
    A=new class'KFVRMagazineState';
    Check(A.Initialize(40,51,80) && A.Loaded()==51 && A.GetMagazineCapacity(1)==50 && A.Reserve==29,
        "infer existing stock load after perk decrease");
    A=new class'KFVRMagazineState';
    Check(A.Initialize(1,101,101) && A.GetMagazineCapacity(1)==100 && A.Total()==101 && A.Valid(),"largest bounded imported magazine");
    RejectInitialize(A,0,1,1); RejectInitialize(A,101,1,1); RejectInitialize(A,40,-1,1);
    RejectInitialize(A,40,102,102); RejectInitialize(A,40,51,50); RejectInitialize(A,40,1,10001);
    Check(A.GetMagazineCapacity(0)==0 && A.GetMagazineCapacity(-1)==0
        && A.GetMagazineCapacity(A.MagazineCount+1)==0 && A.GetMagazineCapacity(65)==0,
        "capacity access rejects absent and invalid identities");
    Check(A.GetMagazineRounds(0)==0 && A.GetMagazineRounds(-1)==0
        && A.GetMagazineRounds(A.MagazineCount+1)==0 && A.GetMagazineRounds(65)==0,
        "round access rejects absent and invalid identities");
    Check(A.Valid(),"read-only invalid access leaves valid state");
}

// Adapter-selected seating policy must preserve the ledger on every rejection.
function RejectSeatedFeed(KFVRMagazineState A,string Label)
{
    local string Before;
    Before=A.Capacity $ ":" $ A.Revision $ ":" $ Contents(A);
    Check(!A.FeedSeatedMagazine(),Label $ " rejects feed");
    Check(Before==(A.Capacity $ ":" $ A.Revision $ ":" $ Contents(A)),Label $ " leaves every field unchanged");
    Check(A.Valid(),Label $ " preserves valid conserved state");
}
function CheckSeatedFeed()
{
    local KFVRMagazineState A;
    local int Identity,BeforeRevision,BeforeReserve,BeforeTotal;
    A=new class'KFVRMagazineState';
    Check(A.Initialize(40,1,120),"seated feed empty chamber seed");
    Check(A.Eject(True) && A.ConsumeShot() && A.Stow(),"spend retained chamber and preserve empty shell");
    Check(A.GrabBelt() && A.Seat() && !A.bChamber && A.bSlideLocked,"seat replacement without implicit feed");
    Identity=A.Inserted; BeforeRevision=A.Revision; BeforeReserve=A.Reserve; BeforeTotal=A.Total();
    Check(A.GetMagazineRounds(Identity)==40 && A.Loaded()==40,"replacement begins full with empty chamber");
    Check(A.FeedSeatedMagazine(),"explicit seated policy feeds empty chamber");
    Check(A.bChamber && !A.bSlideLocked && A.CanFire(),"one round enters chamber and clears empty lock");
    Check(A.GetMagazineRounds(Identity)==39 && A.Loaded()==40,"feed moves one round without increasing loaded total");
    Check(A.Total()==BeforeTotal && A.Reserve==BeforeReserve && A.Budget==120
        && A.Spent==1 && A.Discarded==0 && A.Valid(),"feed preserves reserve and total ammunition budget");
    Check(A.Inserted==Identity && A.Held==0 && A.MagazineCount==1
        && A.GetMagazineCapacity(Identity)==40 && A.Capacity==40,"feed preserves magazine identity and capacity");
    Check(A.Revision==BeforeRevision+1 && A.Travel==0 && !A.bFeedArmed && !A.bExtracted,
        "feed changes revision once without creating a mechanical stroke");
    RejectSeatedFeed(A,"duplicate seated feed");
    Check(A.ConsumeShot() && A.bChamber && A.GetMagazineRounds(Identity)==38
        && A.Loaded()==39 && A.Spent==2 && A.Discarded==0 && A.Valid(),"fed round uses ordinary self-loading shot path");

    A=new class'KFVRMagazineState';
    Check(A.Initialize(40,1,100) && A.Eject(True) && A.Stow() && A.GrabBelt() && A.Seat(),
        "tactical seating preserves existing chamber seed");
    Check(A.bChamber && A.Loaded()==41 && A.GetMagazineRounds(A.Inserted)==40,"tactical chamber plus full magazine");
    RejectSeatedFeed(A,"already chambered tactical reload");
    Check(A.bChamber && A.Loaded()==41 && A.Spent==0 && A.Discarded==0,"tactical feed never extracts the live chamber");

    A=new class'KFVRMagazineState';
    Check(A.Initialize(40,0,10),"empty inserted magazine with reserve seed");
    RejectSeatedFeed(A,"empty inserted magazine cannot draw from reserve");
    Check(A.Eject(True),"remove empty magazine");
    RejectSeatedFeed(A,"absent inserted magazine with empty held shell");
    A=new class'KFVRMagazineState';
    Check(A.Initialize(40,40,80) && A.Eject(True) && A.ConsumeShot(),"full held magazine with empty chamber seed");
    RejectSeatedFeed(A,"absent magazine cannot feed held rounds");
    Check(A.Stow(),"retain full held magazine in storage");
    RejectSeatedFeed(A,"absent magazine cannot feed stored rounds");

    A=new class'KFVRMagazineState';
    Check(A.Initialize(40,0,80) && A.Eject(True) && A.Stow() && A.GrabBelt() && A.Seat(),
        "open action with full inserted magazine seed");
    A.MoveSlide(0.5);
    RejectSeatedFeed(A,"partial open action");
    A.MoveSlide(1.0);
    Check(A.bFeedArmed && !A.bChamber,"full pull arms ordinary feed but keeps chamber empty");
    RejectSeatedFeed(A,"fully open armed action");
    A.MoveSlide(0.1001);
    RejectSeatedFeed(A,"action just above closed threshold");

    A=new class'KFVRMagazineState';
    Check(A.Initialize(40,0,1) && A.Eject(True) && A.Stow() && A.GrabBelt() && A.Seat(),
        "single remaining round seed");
    A.MoveSlide(0.1); BeforeRevision=A.Revision;
    Check(!A.bChamber && A.GetMagazineRounds(A.Inserted)==1,"partial return alone does not invent feed");
    Check(A.FeedSeatedMagazine() && A.bChamber && A.GetMagazineRounds(A.Inserted)==0,
        "closed threshold transfers the final magazine round");
    Check(A.Revision==BeforeRevision+1 && A.Travel==0.1 && A.Loaded()==1 && A.Total()==1
        && A.Spent==0 && A.Discarded==0 && A.Valid(),"threshold feed preserves mechanics and one-round budget");
    RejectSeatedFeed(A,"repeated final-round feed");
    Check(A.ConsumeShot() && !A.bChamber && A.Total()==0 && A.Spent==1 && A.Valid(),"last supplied round fires exactly once");
    RejectSeatedFeed(A,"spent magazine cannot create another chamber round");
}

function EmptyWellActionAndSupply()
{
    local KFVRMagazineState A;
    local int Revision;
    local string Before;
    A=new class'KFVRMagazineState';
    Check(A.Initialize(15,15,60),"empty well action seed");
    Before=Contents(A); Revision=A.Revision;
    Check(A.SupplyRounds()==15 && Contents(A)==Before && A.Revision==Revision,"supply preview is read-only");
    Check(A.Eject(False) && A.Inserted==0 && A.Held==0 && A.bChamber,"optional uncaught ejection retains chamber");
    Check(A.MoveSlide(1) && !A.bChamber && A.Discarded==15,"empty-well pull extracts one chambered round");
    Check(A.MoveSlide(0) && !A.bChamber && !A.bFeedArmed && A.Loaded()==0,"empty-well return cannot feed");
    Check(!A.ConsumeShot(),"empty-well dry fire consumes nothing");
    Before=Contents(A);
    Check(A.MoveSlide(1) && A.MoveSlide(0) && A.Valid() && A.Discarded==15 && A.Total()==45,"repeated empty rack cannot manufacture rounds");
    Check(A.GrabBelt() && A.Held>0 && A.GetMagazineRounds(A.Held)==15,"supply works without catching previous magazine");
    Check(A.Seat() && !A.bChamber,"seating pistol still awaits slide");
    Check(A.MoveSlide(1) && A.MoveSlide(0) && A.bChamber && A.Loaded()==15,"full rack feeds supplied magazine");
    Check(A.ConsumeShot() && A.Total()+A.Spent+A.Discarded==60,"new magazine firing conserves original budget");
}
event int Main(string Params)
{
    local KFVRMagazineState A;
    local int I,Seed,Choice,Identity;
    A=new class'KFVRMagazineState'; Check(A.Initialize(15,15,100),"initialize stock total");
    EmptyWellActionAndSupply();
    Check(A.Eject(True) && A.Loaded()==1 && A.Held==1 && A.Total()==100,"catch retains chamber");
    Check(A.ConsumeShot() && !A.CanFire() && A.Total()==99,"one chamber shot with no magazine");
    Check(A.Stow() && A.GrabBelt() && A.Held!=1,"preserve partial identity, choose fresh");
    Identity=A.Held; Check(A.Seat() && !A.CanFire(),"insertion alone cannot chamber");
    A.MoveSlide(0.5); A.MoveSlide(0); Check(!A.CanFire(),"short pull cannot chamber");
    A.MoveSlide(1); A.MoveSlide(0); Check(A.CanFire() && A.Loaded()==15,"full pull and return feeds");
    Check(A.Inserted==Identity && A.Total()==99 && A.Valid(),"mag identity and budget preserved");
    A.MoveSlide(1); A.MoveSlide(0); Check(A.Discarded==1 && A.Loaded()==14 && A.Valid(),"live round extracted once");
    Check(A.Eject(True) && A.Stow() && A.GrabBelt() && A.Seat() && A.Loaded()==16,"tactical plus one");
    A.ReconcileTotal(A.Total()+10); Check(A.Valid() && A.Loaded()==16,"pickup only adds reserve");
    A.ReconcileTotal(2); Check(A.Valid() && A.Total()==2,"external reduction conserved");
    Seed=41;
    for (I=0;I<20000;I++)
    {
        Seed=(Seed*25173+13849)&65535; Choice=(Seed>>8)%11;
        switch (Choice)
        {
        case 0: A.Eject(True); break;
        case 1: A.Eject(False); break;
        case 2: A.GrabBelt(); break;
        case 3: A.Stow(); break;
        case 4: A.Drop(); break;
        case 5: A.Seat(); break;
        case 6: A.MoveSlide((Seed%11)/10.0); break;
        case 7: A.ConsumeShot(); break;
        case 8: A.ReconcileTotal(Min(1000,A.Total()+3)); break;
        case 9: A.ReconcileTotal(Max(0,A.Total()-2)); break;
        case 10: CheckCapacity(A,1+(Seed%100),"random capacity update"); break;
        }
        Check(A.Valid(),"random live ledger budget");
    }
    CheckCapacities();
    CheckSeatedFeed();
    Log("KFVR_MAG_TEST_RESULT checks=" $ Checks $ " failures=" $ Failures);
    if (Failures>0) return 1;
    return 0;
}
