// Standalone-only development entry point. No player config or original package edits.
class KFVRDevMutator extends Mutator;
var KFVRWeaponRig Rig;
var bool bUnattended,bDualTest;
var Pawn TestPawn;
function PostBeginPlay()
{
    Super.PostBeginPlay();
    if (Level.NetMode!=NM_Standalone) { Destroy(); return; }
    bUnattended=InStr(Caps(Level.GetLocalURL()),"KFVRUNATTENDED=1")>=0;
    bDualTest=bUnattended && InStr(Caps(Level.GetLocalURL()),"KFVRDUALTEST=1")>=0;
    SetTimer(0.5,True);
}
function class<KFWeapon> Replacement(KFWeapon W)
{
    if (W.Class==class'Dualies') return class'KFVRTrackedDualies';
    if (W.Class==class'Knife') return class'KFVRTrackedKnife';
    if (W.Class==class'Single') return class'KFVRTrackedSingle';
    if (W.Class==class'Deagle') return class'KFVRTrackedDeagle';
    if (W.Class==class'Bullpup') return class'KFVRTrackedBullpup';
    return None;
}
function bool AttachRig(KFWeapon W)
{
    if (KFVRTrackedDualies(W)!=None)
    { KFVRTrackedDualies(W).VRRig=Rig; KFVRTrackedDualies(W).EnsureFeed(); Rig.RegisterWeapon(W,0); return True; }
    if (KFVRTrackedKnife(W)!=None)
    { KFVRTrackedKnife(W).VRRig=Rig; Rig.RegisterWeapon(W,3); return True; }
    if (KFVRTrackedSingle(W)!=None)
    { KFVRTrackedSingle(W).VRRig=Rig; Rig.RegisterWeapon(W,0); return True; }
    if (KFVRTrackedDeagle(W)!=None)
    { KFVRTrackedDeagle(W).VRRig=Rig; Rig.RegisterWeapon(W,1); return True; }
    if (KFVRTrackedBullpup(W)!=None)
    { KFVRTrackedBullpup(W).VRRig=Rig; Rig.RegisterWeapon(W,2); return True; }
    return False;
}
function Timer()
{
    local PlayerController PC;
    local Inventory I;
    local KFWeapon Old,NewGun;
    local Weapon Previous;
    local class<KFWeapon> NewType;
    local int Total,Loaded;
    local bool WasSelected;
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
            if (PC.Pawn==None && Level.Game.bWaitingToStartMatch && PC.PlayerReplicationInfo!=None) PC.ServerRestartPlayer();
        }
        if (PC.Pawn==None || PC.Pawn.Health<=0) continue;
        if (Rig==None || Rig.Owner!=PC)
        {
            if (Rig!=None) Rig.Destroy();
            Rig=Spawn(class'KFVRWeaponRig',PC);
        }
        if (Rig==None) return;
        Rig.PC=PC; Rig.bMeleeTest=bUnattended && InStr(Caps(Level.GetLocalURL()),"KFVRMELEETEST=1")>=0;
        for (I=PC.Pawn.Inventory; I!=None; I=I.Inventory)
        {
            Old=KFWeapon(I);
            if (Old==None) continue;
            if (AttachRig(Old)) continue;
            NewType=Replacement(Old);
            if (NewType==None) continue;
            Total=Old.AmmoAmount(0); Loaded=Old.MagAmmoRemaining;
            Previous=PC.Pawn.Weapon; WasSelected=Previous==Old;
            NewGun=Spawn(NewType,PC.Pawn);
            if (NewGun==None) return;
            Old.StopFire(0); Old.StopFire(1); PC.Pawn.DeleteInventory(Old); Old.Destroy();
            NewGun.GiveTo(PC.Pawn); NewGun.ClientForceAmmoUpdate(0,Total); NewGun.MagAmmoRemaining=Loaded;
            AttachRig(NewGun);
            if (WasSelected || PC.Pawn.Weapon==None || (Rig.InventoryCount==1 && KFVRTrackedSingle(NewGun)!=None))
            { PC.Pawn.PendingWeapon=NewGun; PC.Pawn.ChangedWeapon(); }
            else if (Previous!=None) { PC.Pawn.PendingWeapon=Previous; PC.Pawn.ChangedWeapon(); }
            Log("KFVR_RIG_READY owner=" $ PC $ " gun=" $ NewGun $ " ammo=" $ Total $ " loaded=" $ Loaded);
            return;
        }
        if (bUnattended && TestPawn!=PC.Pawn)
        {
            TestPawn=PC.Pawn; Previous=PC.Pawn.Weapon;
            if (bDualTest) { NewGun=Spawn(class'Dualies',PC.Pawn); if (NewGun!=None) NewGun.GiveTo(PC.Pawn); }
            NewGun=Spawn(class'Deagle',PC.Pawn); if (NewGun!=None) NewGun.GiveTo(PC.Pawn);
            NewGun=Spawn(class'Bullpup',PC.Pawn); if (NewGun!=None) NewGun.GiveTo(PC.Pawn);
            if (Previous!=None) { PC.Pawn.PendingWeapon=Previous; PC.Pawn.ChangedWeapon(); }
        }
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
