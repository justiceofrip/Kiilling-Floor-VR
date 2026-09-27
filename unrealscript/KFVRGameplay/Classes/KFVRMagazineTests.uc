class KFVRMagazineTests extends Commandlet;
var int Checks,Failures;
function Check(bool B,string Message) { Checks++; if (!B) { Failures++; Log("KFVR_MAG_FAIL " $ Message); } }
event int Main(string Params)
{
    local KFVRMagazineState A;
    local int I,Seed,Choice,Identity;
    A=new class'KFVRMagazineState'; Check(A.Initialize(15,15,100),"initialize stock total");
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
        Seed=(Seed*25173+13849)&65535; Choice=(Seed>>8)%10;
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
        }
        Check(A.Valid(),"random live ledger budget");
    }
    Log("KFVR_MAG_TEST_RESULT checks=" $ Checks $ " failures=" $ Failures);
    if (Failures>0) return 1;
    return 0;
}
