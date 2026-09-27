// Standalone-only development entry point. No player config or original package edits.
class KFVRDevMutator extends Mutator;
var KFVRWeaponRig Rig;
var bool bUnattended;
function PostBeginPlay()
{
    Super.PostBeginPlay();
    if (Level.NetMode!=NM_Standalone) { Destroy(); return; }
    bUnattended=InStr(Caps(Level.GetLocalURL()),"KFVRUNATTENDED=1")>=0;
    SetTimer(0.5,True);
}
function Timer()
{
    local PlayerController PC;
    local Inventory I;
    local Single Old;
    local Weapon Previous;
    local KFVRTrackedSingle NewGun;
    local int Total,Loaded;
    local KFMonster Enemy;
    if (bUnattended)
    {
        if (KFGameType(Level.Game)!=None) KFGameType(Level.Game).NextMonsterTime=Level.TimeSeconds+30.0;
        foreach DynamicActors(class'KFMonster',Enemy) Enemy.Destroy();
    }
    foreach DynamicActors(class'PlayerController',PC)
    {
        if (PC.Player==None) continue;
        if (bUnattended)
        {
            PC.bGodMode=True;
            if (KFPlayerController(PC)!=None && KFPlayerController(PC).SteamStatsAndAchievements!=None)
                KFPlayerController(PC).SteamStatsAndAchievements.bUsedCheats=True;
            if (PC.Pawn==None && Level.Game.bWaitingToStartMatch && PC.PlayerReplicationInfo!=None)
                PC.ServerRestartPlayer();
        }
        if (PC.Pawn==None || PC.Pawn.Health<=0) continue;
        if (Rig==None || Rig.Owner!=PC)
        {
            if (Rig!=None) Rig.Destroy();
            Rig=Spawn(class'KFVRWeaponRig',PC);
        }
        if (Rig==None) return;
        for (I=PC.Pawn.Inventory; I!=None; I=I.Inventory)
            if (KFVRTrackedSingle(I)!=None) { Rig.Gun=KFVRTrackedSingle(I); Rig.Gun.VRRig=Rig; return; }
        for (I=PC.Pawn.Inventory; I!=None; I=I.Inventory)
            if (I.Class==class'Single') { Old=Single(I); break; }
        if (Old==None) return;
        Total=Old.AmmoAmount(0); Loaded=Old.MagAmmoRemaining; Previous=PC.Pawn.Weapon;
        NewGun=Spawn(class'KFVRTrackedSingle',PC.Pawn);
        if (NewGun==None) return;
        Old.StopFire(0); Old.StopFire(1);
        PC.Pawn.DeleteInventory(Old); Old.Destroy();
        NewGun.GiveTo(PC.Pawn);
        NewGun.SetTransferredAmmo(Total,Loaded);
        Rig.Gun=NewGun; NewGun.VRRig=Rig;
        if (Previous!=None) { Previous.StopFire(0); Previous.PutDown(); }
        PC.Pawn.PendingWeapon=None; PC.Pawn.Weapon=NewGun; NewGun.BringUp();
        Log("KFVR_RIG_READY owner=" $ PC $ " gun=" $ NewGun $ " ammo=" $ Total $ " loaded=" $ Loaded);
        return;
    }
}
function Mutate(string Text, PlayerController Sender)
{
    if (Level.NetMode==NM_Standalone && Caps(Text)=="KFVRTEST" && Sender!=None)
    { Sender.bGodMode=True; Sender.ClientMessage("KFVR local development: god mode on"); }
    Super.Mutate(Text,Sender);
}
defaultproperties
{
    FriendlyName="KFVR standalone 6DoF development"
    Description="Local tracked weapon and hand integration."
    GroupName="KFVR"
    bAddToServerPackages=True
}
