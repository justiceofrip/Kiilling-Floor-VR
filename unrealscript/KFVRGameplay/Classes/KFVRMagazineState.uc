// Game-thread closed-bolt ledger. Slots are identities; partial magazines are
// retained separately. No animation notify or visual actor can credit ammunition.
class KFVRMagazineState extends Object;
var int Rounds[64],MagazineCapacity[64];
// Capacity is the current supply limit; existing identities keep their own limit.
var int MagazineCount, Inserted, Held, Capacity, Reserve, Budget, Spent, Discarded, Revision;
var bool bChamber, bFeedArmed, bExtracted, bSlideLocked;
var float Travel;
function bool Initialize(int InCapacity, int Loaded, int Total)
{
    if (InCapacity<1 || InCapacity>100 || Loaded<0 || Loaded>101 || Total<Loaded || Total>10000) return False;
    Capacity=InCapacity; Reserve=Total-Loaded; Budget=Total;
    Inserted=1; MagazineCount=1; Rounds[0]=Max(0,Loaded-1); bChamber=Loaded>0;
    // Stock can retain an older, larger load after a perk decrease.
    MagazineCapacity[0]=Max(InCapacity,Rounds[0]);
    return Valid();
}
// A trusted stock capacity change affects only future supply/refills.
// Invalid values and repeated notifications leave every field untouched.
function bool SetCapacity(int NewCapacity)
{
    if (NewCapacity<1 || NewCapacity>100) return False;
    if (Capacity==NewCapacity) return True;
    Capacity=NewCapacity; Revision++; return True;
}
// UE2 cannot encode external context access to a 256-byte int[64] field.
// Expose bounded, one-based identities without exposing that bytecode limit.
function int GetMagazineCapacity(int Id)
{
    if (Id<1 || Id>MagazineCount || Id>64) return 0;
    return MagazineCapacity[Id-1];
}
function int GetMagazineRounds(int Id)
{
    if (Id<1 || Id>MagazineCount || Id>64) return 0;
    return Rounds[Id-1];
}
function int Loaded() { return int(bChamber)+(int(Inserted>0)*Rounds[Max(0,Inserted-1)]); }
function int Total()
{
    local int I,N;
    N=Reserve+int(bChamber);
    for (I=0;I<MagazineCount;I++) N+=Rounds[I];
    return N;
}
function bool Valid()
{
    local int I;
    if (Capacity<1 || Capacity>100 || MagazineCount<1 || MagazineCount>64 || Inserted<0 || Inserted>MagazineCount || Held<0 || Held>MagazineCount || (Inserted>0 && Inserted==Held) || Reserve<0 || Travel<0 || Travel>1) return False;
    for (I=0;I<MagazineCount;I++) if (MagazineCapacity[I]<1 || MagazineCapacity[I]>100 || Rounds[I]<0 || Rounds[I]>MagazineCapacity[I]) return False;
    return Total()+Spent+Discarded==Budget;
}
function bool CanFire() { return bChamber && Travel<=0.1 && Valid(); }
function bool ConsumeShot()
{
    if (!CanFire()) return False;
    bChamber=False; Spent++;
    if (Inserted>0 && Rounds[Inserted-1]>0) { Rounds[Inserted-1]--; bChamber=True; }
    bSlideLocked=!bChamber; Revision++; return True;
}
function bool Eject(bool Catch)
{
    if (Inserted==0 || (Catch && Held>0)) return False;
    if (Catch) Held=Inserted;
    else { Discarded+=Rounds[Inserted-1]; Rounds[Inserted-1]=0; }
    Inserted=0; Revision++; return True;
}
function bool Stow()
{
    if (Held==0) return False;
    Held=0; Revision++; return True;
}
function bool Drop()
{
    if (Held==0) return False;
    Discarded+=Rounds[Held-1]; Rounds[Held-1]=0; Held=0; Revision++; return True;
}
function bool GrabBelt()
{
    local int I,Best,Empty,Fresh;
    if (Held>0 || Inserted>0) return False;
    Best=-1; Empty=-1;
    for (I=0;I<MagazineCount;I++)
    {
        if (I+1==Inserted) continue;
        if (Best<0 || Rounds[I]>Rounds[Best]) Best=I;
        if (Rounds[I]==0) Empty=I;
    }
    Fresh=Min(Capacity,Reserve);
    if (Best>=0 && Rounds[Best]>0 && (Rounds[Best]>=Fresh || (Empty<0 && MagazineCount==64))) Held=Best+1;
    else if (Fresh>0)
    {
        if (Empty<0) { if (MagazineCount>=64) return False; Empty=MagazineCount; MagazineCount++; }
        MagazineCapacity[Empty]=Capacity; Rounds[Empty]=Fresh; Reserve-=Fresh; Held=Empty+1;
    }
    else return False;
    Revision++; return True;
}
function bool Seat()
{
    if (Inserted>0 || Held==0) return False;
    Inserted=Held; Held=0; Revision++; return True;
}
function bool MoveSlide(float Amount)
{
    Amount=FClamp(Amount,0,1);
    if (Amount==Travel) return False;
    Travel=Amount;
    if (Travel>=0.65 && !bExtracted)
    { if (bChamber) { bChamber=False; Discarded++; } bExtracted=True; }
    if (Travel>=0.9) { bFeedArmed=True; bSlideLocked=False; }
    if (Travel<=0.1)
    {
        if (bFeedArmed && !bChamber && Inserted>0 && Rounds[Inserted-1]>0)
        { Rounds[Inserted-1]--; bChamber=True; }
        bFeedArmed=False; bExtracted=False;
    }
    Revision++; return True;
}
// A pickup/trader change adjusts the owned budget, never a chamber or reload stage.
// Unexpected reductions remove loose/stored rounds first and cannot create ammo.
function ReconcileTotal(int NewTotal)
{
    local int Difference,I,Take;
    NewTotal=Clamp(NewTotal,0,10000); Difference=NewTotal-Total();
    if (Difference==0) return;
    Budget+=Difference;
    if (Difference>0) Reserve+=Difference;
    else
    {
        Difference=-Difference; Take=Min(Reserve,Difference); Reserve-=Take; Difference-=Take;
        for (I=0;I<MagazineCount && Difference>0;I++)
        {
            if (I+1==Inserted || I+1==Held) continue;
            Take=Min(Rounds[I],Difference); Rounds[I]-=Take; Difference-=Take;
        }
        if (Held>0) { Take=Min(Rounds[Held-1],Difference); Rounds[Held-1]-=Take; Difference-=Take; }
        if (Inserted>0) { Take=Min(Rounds[Inserted-1],Difference); Rounds[Inserted-1]-=Take; Difference-=Take; }
        if (Difference>0) bChamber=False;
    }
    Revision++;
}
