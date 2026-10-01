// Physical blade contact replaces the desktop camera cone. KFMonster.TakeDamage
// retains stock Berserker scaling, headshot/stun rules and damage-type handling.
class KFVRKnifeFire extends KnifeFire;
var Sound PhysicalSwingSound;
var string PhysicalSwingSoundRef;
var bool bPhysicalSoundResolved;
function bool PreparePhysicalSound()
{
    if (!bPhysicalSoundResolved)
    {
        bPhysicalSoundResolved=True;
        PhysicalSwingSound=Sound(DynamicLoadObject(PhysicalSwingSoundRef,class'Sound',True));
    }
    return PhysicalSwingSound!=None;
}
function BeginPhysicalSwing()
{
    local KFVRTrackedKnife K;
    K=KFVRTrackedKnife(Weapon);
    if (K==None || !K.bPhysicalOwned || !PreparePhysicalSound()) return;
    // The installed knife has no FireSound/FireSoundRef: flat swing audio uses
    // KFWeaponSoundNotify assets. Resolve one audited knife-bank sample for the
    // physical stroke without enabling a flat animation or changing its audio.
    Weapon.PlayOwnedSound(PhysicalSwingSound,SLOT_Interact,TransientSoundVolume,,TransientSoundRadius,,False);
    K.SwingSoundCount++;
}
function bool Sweep(vector From,vector To,vector Grip,vector FromBase,vector ToBase,float Seconds)
{
    local KFVRTrackedKnife Knife;
    local KFVRWeaponRig R;
    local Actor Hit,Blocker;
    local vector Position,HitNormal,OcclusionPosition,OcclusionNormal,Direction;
    local vector TraceFrom,TraceTo,Extent;
    local int Damage,PointIndex,Intervals;
    local float LengthMetres;
    local bool Backstab;
    Knife=KFVRTrackedKnife(Weapon);
    if (Knife==None || Knife.VRRig==None || Instigator==None || Level.NetMode!=NM_Standalone || Weapon.bDeleteMe) return False;
    R=Knife.VRRig;
    if (!R.ActiveFor(Weapon) || R.bHolstered || Knife.bNoHit || !Knife.bPhysicalOwned
        || Knife.SwingCount<=0 || Knife.LastHitSwing==Knife.SwingCount) return False;
    if (!(Seconds>0 && Seconds<=1) || !(R.Tracking.UnitsPerMetre>=1 && R.Tracking.UnitsPerMetre<=200)
        || !class'KFVRMeleeGeometry'.static.BoundedVector(Grip,100000000)
        || !class'KFVRMeleeGeometry'.static.BoundedVector(FromBase,100000000)
        || !class'KFVRMeleeGeometry'.static.BoundedVector(ToBase,100000000)) return False;
    // A displaced hand never authorizes a contact through intervening world geometry.
    Blocker=Instigator.Trace(OcclusionPosition,OcclusionNormal,Grip,R.Tracking.HeadPosition,False);
    if (Blocker!=None) { Knife.BlockedContacts++; return False; }
    LengthMetres=FMax(VSize(From-FromBase),VSize(To-ToBase))/R.Tracking.UnitsPerMetre;
    Intervals=class'KFVRMeleeGeometry'.static.TraceIntervals(LengthMetres,Knife.ContactRadiusMetres);
    if (Intervals<=0) return False;
    Extent=vect(1,1,1)*(Knife.ContactRadiusMetres*R.Tracking.UnitsPerMetre);
    // Trace temporal paths at overlapping points down the blade. A stationary
    // hilt/current blade line is not a damage ray. Tip-first preserves the
    // existing endpoint contact before considering contacts along the edge.
    for (PointIndex=Intervals;PointIndex>=0;PointIndex--)
    {
        TraceFrom=class'KFVRMeleeGeometry'.static.BladePoint(FromBase,From,PointIndex,Intervals);
        TraceTo=class'KFVRMeleeGeometry'.static.BladePoint(ToBase,To,PointIndex,Intervals);
        if (!class'KFVRMeleeGeometry'.static.MovingContact(TraceFrom,TraceTo,Seconds,R.Tracking.UnitsPerMetre,1.2)) continue;
        Knife.ContactTraceCount++;
        Hit=Instigator.Trace(Position,HitNormal,TraceTo,TraceFrom,True,Extent);
        if (Hit==None || Hit==Instigator || Hit==Weapon) continue;
        // A tip can protrude through a thin wall with the grip still outside.
        // Resolve visibility to the actual contact before any actor takes damage.
        Blocker=Instigator.Trace(OcclusionPosition,OcclusionNormal,Position,Grip,False);
        if (Blocker!=None && Blocker!=Hit)
        {
            Knife.BlockedContacts++;
            Hit=Blocker; Position=OcclusionPosition; HitNormal=OcclusionNormal;
        }
        Direction=Normal(TraceTo-TraceFrom);
        break;
    }
    if (Hit==None || Hit==Instigator || Hit==Weapon || Hit.bDeleteMe) return False;
    if (Hit.IsA('ExtendedZCollision') && KFMonster(Hit.Base)!=None) Hit=Hit.Base;
    Damage=MeleeDamage;
    Backstab=Pawn(Hit)!=None && Vehicle(Hit)==None && (Normal(Hit.Location-Instigator.Location) dot vector(Hit.Rotation))>0;
    if (Backstab) Damage*=2;
    if (KFMonster(Hit)!=None) KFMonster(Hit).bBackstabbed=Backstab;
    // Commit the contact token before invoking victim code. Lingering contact
    // and repeated dispatch cannot damage twice during the same accepted swing.
    Knife.LastHitSwing=Knife.SwingCount;
    Hit.TakeDamage(Damage,Instigator,Position,Direction,hitDamageClass);
    Knife.LastDamage=Damage; Knife.bLastBackstab=Backstab;
    Knife.LastHit=Hit; Knife.LastImpact=Position;
    if (Hit.bWorldGeometry) Knife.WorldHitCount++; else Knife.HitCount++;
    if (Pawn(Hit)!=None)
    {
        if (MeleeHitSounds.Length>0) Weapon.PlaySound(MeleeHitSounds[Rand(MeleeHitSounds.Length)],SLOT_None,MeleeHitVolume);
        if ((KFMonster(Hit)!=None || KFHumanPawn(Hit)!=None) && Knife.BloodyMaterial!=None)
        { Weapon.Skins[Knife.BloodSkinSwitchArray]=Knife.BloodyMaterial; if (R.Visual!=None) R.Visual.Skins=Weapon.Skins; }
        // Preserve the installed melee running-hit knockdown condition; damage
        // and Berserker modifiers have already passed through the stock victim.
        if (KFMonster(Hit)!=None && VSize(Instigator.Velocity)>300 && Hit.Mass<=Instigator.Mass)
            KFMonster(Hit).FlipOver();
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

defaultproperties
{
    // Existing sound imported by installed KF_Weapons_Trip; selected
    // physical feedback profile, not a claim to reproduce its notify variants.
    PhysicalSwingSoundRef="KF_KnifeSnd.KnifeFireBase.Knife_Fire1"
}
