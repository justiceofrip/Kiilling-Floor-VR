// One shared supply and identity pool for the two physical belt-fed pistols.
// Mode 0 is right; mode 1 is left, matching KF's existing fire-mode indexing.
class KFVRDualMagazineState extends Object;
var int Rounds[64],Inserted[2],Presented[2];
var byte Chamber[2],SlideLocked[2],BeltLatched[2];
var int Capacity,MagazineCount,Reserve,Budget,Spent,Revision,AssistCount;
function bool Initialize(int Cap,int RightLoaded,int LeftLoaded,int TotalRounds)
{
    if (Cap<1 || Cap>100 || RightLoaded<0 || LeftLoaded<0 || RightLoaded>Cap || LeftLoaded>Cap || TotalRounds<RightLoaded+LeftLoaded || TotalRounds>10000) return False;
    Capacity=Cap; MagazineCount=2; Inserted[0]=1; Inserted[1]=2;
    if (RightLoaded>0) Chamber[0]=1; else SlideLocked[0]=1;
    if (LeftLoaded>0) Chamber[1]=1; else SlideLocked[1]=1;
    Rounds[0]=Max(0,RightLoaded-1); Rounds[1]=Max(0,LeftLoaded-1);
    Reserve=TotalRounds-RightLoaded-LeftLoaded; Budget=TotalRounds;
    return Valid();
}
function int Loaded(int Hand)
{
    if (Hand<0 || Hand>1) return 0;
    if (Inserted[Hand]>0) return int(Chamber[Hand])+Rounds[Inserted[Hand]-1];
    return int(Chamber[Hand]);
}
function int Total()
{
    local int I,N;
    N=Reserve+int(Chamber[0])+int(Chamber[1]);
    for (I=0;I<MagazineCount;I++) N+=Rounds[I];
    return N;
}
function bool Stored(int Id)
{
    return Id>0 && Id<=MagazineCount && Id!=Inserted[0] && Id!=Inserted[1] && Id!=Presented[0] && Id!=Presented[1];
}
function bool Valid()
{
    local int I,J;
    if (Capacity<1 || MagazineCount<2 || MagazineCount>64 || Reserve<0 || Total()+Spent!=Budget) return False;
    for (I=0;I<MagazineCount;I++) if (Rounds[I]<0 || Rounds[I]>Capacity) return False;
    for (I=0;I<2;I++)
    {
        if (Inserted[I]<0 || Inserted[I]>MagazineCount || Presented[I]<0 || Presented[I]>MagazineCount || Chamber[I]>1 || SlideLocked[I]>1 || BeltLatched[I]>1) return False;
        if ((BeltLatched[I]!=0 && Inserted[I]==0) || (Chamber[I]!=0 && SlideLocked[I]!=0)) return False;
        for (J=0;J<2;J++) if (Inserted[I]>0 && Inserted[I]==Presented[J]) return False;
    }
    if ((Inserted[0]>0 && Inserted[0]==Inserted[1]) || (Presented[0]>0 && Presented[0]==Presented[1])) return False;
    return True;
}
function bool CanFire(int Hand)
{
    if (Hand<0 || Hand>1) return False;
    return Chamber[Hand]!=0 && BeltLatched[Hand]==0 && SlideLocked[Hand]==0 && Valid();
}
function bool Consume(int Hand)
{
    if (!CanFire(Hand)) return False;
    Chamber[Hand]=0; Spent++;
    if (Inserted[Hand]>0 && Rounds[Inserted[Hand]-1]>0) { Rounds[Inserted[Hand]-1]--; Chamber[Hand]=1; }
    if (Chamber[Hand]==0 && Inserted[Hand]>0) SlideLocked[Hand]=1;
    Revision++; return True;
}
function bool Eject(int Hand)
{
    if (Hand<0 || Hand>1 || Inserted[Hand]==0 || BeltLatched[Hand]!=0) return False;
    Inserted[Hand]=0; Revision++; return True;
}
function bool Present(int Hand)
{
    local int I,Best,Empty,Fresh;
    if (Hand<0 || Hand>1 || Inserted[Hand]>0 || Presented[Hand]>0) return False;
    Best=-1; Empty=-1; Fresh=Min(Capacity,Reserve);
    for (I=0;I<MagazineCount;I++) if (Stored(I+1))
    {
        if (Rounds[I]>0 && (Best<0 || Rounds[I]>Rounds[Best])) Best=I;
        if (Rounds[I]==0) Empty=I;
    }
    if (Best>=0 && (Rounds[Best]>=Fresh || (Empty<0 && MagazineCount==64))) Presented[Hand]=Best+1;
    else if (Fresh>0)
    {
        if (Empty<0) { if (MagazineCount>=64) return False; Empty=MagazineCount; MagazineCount++; }
        Rounds[Empty]=Fresh; Reserve-=Fresh; Presented[Hand]=Empty+1;
    }
    else return False;
    Revision++; return True;
}
function bool Seat(int Hand)
{
    if (Hand<0 || Hand>1 || Inserted[Hand]>0 || Presented[Hand]==0) return False;
    Inserted[Hand]=Presented[Hand]; Presented[Hand]=0; BeltLatched[Hand]=1;
    // The belt fixture supplies slide assistance with both hands occupied.
    // Tactical chambers remain intact; an empty chamber receives one actual round.
    if (Chamber[Hand]==0 && Rounds[Inserted[Hand]-1]>0)
    { Rounds[Inserted[Hand]-1]--; Chamber[Hand]=1; SlideLocked[Hand]=0; AssistCount++; }
    Revision++; return True;
}
function bool ClearBelt(int Hand)
{
    if (Hand<0 || Hand>1 || BeltLatched[Hand]==0) return False;
    BeltLatched[Hand]=0; Revision++; return True;
}
function StoreForSwitch()
{
    Presented[0]=0; Presented[1]=0; BeltLatched[0]=0; BeltLatched[1]=0; Revision++;
}
function ReconcileTotal(int Count)
{
    local int Delta,I,Pass,N;
    Count=Clamp(Count,0,10000); Delta=Count-Total();
    if (Delta==0) return;
    Budget+=Delta;
    if (Delta>0) Reserve+=Delta;
    else
    {
        Delta=-Delta; N=Min(Reserve,Delta); Reserve-=N; Delta-=N;
        for (Pass=0;Pass<3 && Delta>0;Pass++) for (I=0;I<MagazineCount && Delta>0;I++)
        {
            if ((Pass==0 && !Stored(I+1)) || (Pass==1 && I+1!=Presented[0] && I+1!=Presented[1]) || (Pass==2 && I+1!=Inserted[0] && I+1!=Inserted[1])) continue;
            N=Min(Rounds[I],Delta); Rounds[I]-=N; Delta-=N;
        }
        for (I=0;I<2 && Delta>0;I++) if (Chamber[I]!=0) { Chamber[I]=0; Delta--; }
    }
    Revision++;
}
