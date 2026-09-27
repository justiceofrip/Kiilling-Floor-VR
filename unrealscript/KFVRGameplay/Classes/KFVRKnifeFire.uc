// Physical contact replaces the desktop camera cone. Damage still enters KF's
// TakeDamage path with the stock knife damage type, so victim/perk rules execute.
class KFVRKnifeFire extends KnifeFire;
function bool Sweep(vector From,vector To,vector Grip)
{
    local KFVRTrackedKnife Knife;
    local KFVRWeaponRig R;
    local Actor Hit,Blocker;
    local vector Position,HitNormal,OcclusionPosition,OcclusionNormal,Direction;
    local int Damage;
    local bool Backstab;
    Knife=KFVRTrackedKnife(Weapon);
    if (Knife==None || Knife.VRRig==None || Level.NetMode!=NM_Standalone || Weapon.bDeleteMe) return False;
    R=Knife.VRRig;
    if (!R.ActiveFor(Weapon) || R.bHolstered || Knife.bNoHit) return False;
    // Prevent a controller pushed through a wall from starting a sweep beyond it.
    Blocker=Instigator.Trace(OcclusionPosition,OcclusionNormal,Grip,R.Tracking.HeadPosition,False);
    if (Blocker!=None) return False;
    Hit=Instigator.Trace(Position,HitNormal,To,From,True,vect(1.5,1.5,1.5));
    if (Hit==None) Hit=Instigator.Trace(Position,HitNormal,To,Grip,True,vect(1.5,1.5,1.5));
    if (Hit==None || Hit==Instigator || Hit==Weapon) return False;
    if (Hit.IsA('ExtendedZCollision') && KFMonster(Hit.Base)!=None) Hit=Hit.Base;
    Damage=MeleeDamage; Direction=Normal(To-From);
    Backstab=Pawn(Hit)!=None && Vehicle(Hit)==None && (Normal(Hit.Location-Instigator.Location) dot vector(Hit.Rotation))>0;
    if (Backstab) Damage*=2;
    if (KFMonster(Hit)!=None) KFMonster(Hit).bBackstabbed=Backstab;
    Hit.TakeDamage(Damage,Instigator,Position,Direction,hitDamageClass);
    Knife.LastHit=Hit; Knife.LastImpact=Position;
    if (Hit.bWorldGeometry) Knife.WorldHitCount++; else Knife.HitCount++;
    if (Pawn(Hit)!=None)
    {
        if (MeleeHitSounds.Length>0) Weapon.PlaySound(MeleeHitSounds[Rand(MeleeHitSounds.Length)],SLOT_None,MeleeHitVolume);
        if (Knife.BloodyMaterial!=None)
        { Weapon.Skins[Knife.BloodSkinSwitchArray]=Knife.BloodyMaterial; R.Visual.Skins=Weapon.Skins; }
    }
    else if (HitEffectClass!=None) Spawn(HitEffectClass,,,Position,rotator(HitNormal));
    R.PulseRight(0.65,40); return True;
}
simulated function Timer()
{
    if (KFVRTrackedKnife(Weapon)!=None && KFVRTrackedKnife(Weapon).bPhysicalOwned) return;
    Super.Timer();
}
simulated function ShakeView()
{
    if (KFVRTrackedKnife(Weapon)!=None && KFVRTrackedKnife(Weapon).bPhysicalOwned) return;
    Super.ShakeView();
}
