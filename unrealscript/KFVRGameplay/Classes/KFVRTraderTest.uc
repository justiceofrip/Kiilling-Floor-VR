// Explicit private fixture: actual map shop, stock pawn and real transaction rules.
class KFVRTraderTest extends Actor;
var PlayerController PC;
var ShopVolume TestShop;
var bool bReady,bPreviousCommand,bShopOpen;
var int TestStage;
var float OriginalMaxWeight;
var float Cash,CarryWeight;
var int WeaponCount,Deagles,Bullpups,Duals,PistolAmmo,GrenadeAmmo;
function bool Initialize(PlayerController Player)
{
    local ShopVolume S,T;
    local vector Candidate;
    local int I;
    local Weapon Grenades;
    PC=Player;
    if (Level.NetMode!=NM_Standalone || PC==None || PC.Pawn==None) return False;
    KFGameType(Level.Game).bWaveInProgress=False; KFGameType(Level.Game).WaveCountDown=300;
    KFGameReplicationInfo(Level.Game.GameReplicationInfo).bWaveInProgress=False;
    foreach AllActors(class'ShopVolume',S)
    {
        if (S.bAlwaysClosed || S.MyTrader==None) continue;
        S.OpenShop();
        for (I=0;I<3;I++)
        {
            Candidate=S.Location+vect(0,0,1)*(I*40);
            if (!PC.Pawn.SetLocation(Candidate)) continue;
            foreach PC.Pawn.TouchingActors(class'ShopVolume',T)
                if (T==S)
                {
                    TestShop=S; bReady=True; OriginalMaxWeight=KFHumanPawn(PC.Pawn).MaxCarryWeight; PC.SetRotation(rot(0,0,0)); PC.Pawn.SetRotation(rot(0,0,0));
                    PC.PlayerReplicationInfo.Score=2000; PC.Pawn.Health=40; PC.Pawn.ShieldStrength=0;
                    Grenades=Weapon(PC.Pawn.FindInventoryType(class'Frag')); if (Grenades!=None) Grenades.ClientForceAmmoUpdate(0,2);
                    Log("KFVR_TRADER_TEST_READY shop=" $ S $ " pawn=" $ PC.Pawn.Location); return True;
                }
        }
    }
    Log("KFVR_TRADER_TEST_FAILED"); return False;
}
function Tick(float Delta)
{
    local Inventory I;
    local KFVRWeaponRig Rig;
    local bool Command;
    if (!bReady || PC==None || PC.Pawn==None) return;
    KFGameType(Level.Game).WaveCountDown=300;
    Rig=KFVRDevMutator(Owner).Rig;
    Command=Rig!=None && Rig.bTracking && (Rig.Tracking.Flags & 16)!=0;
    if (Command && !bPreviousCommand)
    {
        TestStage++;
        if (TestStage==1) PC.PlayerReplicationInfo.Score=0;
        if (TestStage==2) { PC.PlayerReplicationInfo.Score=2000; KFHumanPawn(PC.Pawn).MaxCarryWeight=KFHumanPawn(PC.Pawn).CurrentWeight; }
        if (TestStage==3) { KFHumanPawn(PC.Pawn).MaxCarryWeight=OriginalMaxWeight; TestShop.CloseShop(); }
        if (TestStage==4) { TestShop.OpenShop(); PC.PlayerReplicationInfo.Score=2000; }
    }
    bPreviousCommand=Command; bShopOpen=TestShop.bCurrentlyOpen;
    Cash=PC.PlayerReplicationInfo.Score; CarryWeight=KFHumanPawn(PC.Pawn).CurrentWeight;
    WeaponCount=0; Deagles=0; Bullpups=0; Duals=0;
    for (I=PC.Pawn.Inventory;I!=None;I=I.Inventory)
    {
        if (KFWeapon(I)!=None) WeaponCount++;
        if (Deagle(I)!=None) Deagles++;
        if (Bullpup(I)!=None) Bullpups++;
        if (Dualies(I)!=None) Duals++;
        if (I.Class==class'Frag') GrenadeAmmo=Weapon(I).AmmoAmount(0);
        if (Single(I)!=None || Dualies(I)!=None) PistolAmmo=Weapon(I).AmmoAmount(0);
    }
}
defaultproperties
{
    bHidden=True
    RemoteRole=ROLE_None
}
