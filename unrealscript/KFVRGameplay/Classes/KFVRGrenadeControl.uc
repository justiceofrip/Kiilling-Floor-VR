// Physical throws retain the exact stock Frag inventory item and projectile factory.
class KFVRGrenadeControl extends Info;
var KFVRWeaponRig Rig;
var Frag GrenadeWeapon;
var KFVRThrowMotion Motion;
var Projectile LastProjectile;
var bool bPinRemoved,bTriggerArmed,bGripHeld;
var int LastSerial,ArmedCount,ThrowCount,CancelCount,BlockedCount,SpawnFailures,AmmoCount,AmmoBefore,AmmoAfter,MotionSamples;
var float NextThrowTime,ReleaseSpeed;
var vector ReleaseOrigin,ReleaseVelocity,MeasuredVelocity;
function Initialize(KFVRWeaponRig R)
{
    Rig=R; Motion=new class'KFVRThrowMotion';
}
function ResetInput()
{
    if (bPinRemoved) CancelCount++;
    bPinRemoved=False; bTriggerArmed=False; bGripHeld=False; LastSerial=0;
    if (Motion!=None) Motion.Reset();
}
function bool Throw()
{
    local vector Hit,NormalHit,WantedVelocity;
    local FragFire F;
    local Projectile P;
    if (GrenadeWeapon==None || GrenadeWeapon.Class!=class'Frag' || GrenadeWeapon.Owner!=Rig.PC.Pawn
        || !Rig.ActiveFor(GrenadeWeapon) || Rig.bHolstered || GrenadeWeapon.AmmoAmount(0)<1 || Level.TimeSeconds<NextThrowTime
        || !Motion.Ready(Level.TimeSeconds) || GrenadeWeapon.bTossActive || KFPawn(Rig.PC.Pawn).bThrowingNade) return False;
    ReleaseOrigin=Rig.MuzzleLocation;
    if (VSize(ReleaseOrigin-Rig.Tracking.HeadPosition)>1.3*Rig.Tracking.UnitsPerMetre
        || Rig.PC.Pawn.Trace(Hit,NormalHit,ReleaseOrigin,Rig.Tracking.HeadPosition,True,vect(1,1,1))!=None)
    { BlockedCount++; return False; }
    F=FragFire(GrenadeWeapon.GetFireMode(0));
    if (F==None || F.GetDesiredProjectileClass()==None) return False;
    WantedVelocity=(Motion.Velocity>>Rig.Tracking.BodyRotation)*Rig.Tracking.UnitsPerMetre+Rig.PC.Pawn.Velocity;
    AmmoBefore=GrenadeWeapon.AmmoAmount(0);
    if (!GrenadeWeapon.ConsumeAmmo(0,1)) return False;
    P=F.SpawnProjectile(ReleaseOrigin,Rig.Tracking.RightRotation);
    if (P==None)
    {
        // Stock spawn failure has created no live grenade; return the one debit.
        GrenadeWeapon.AddAmmo(1,0); SpawnFailures++; AmmoAfter=GrenadeWeapon.AmmoAmount(0); return False;
    }
    // The stock factory selected the perk grenade and applied its damage setup.
    // Replace only the launch motion, before its first ordinary physics update.
    if (P.MaxSpeed>0 && VSize(WantedVelocity)>P.MaxSpeed) WantedVelocity=Normal(WantedVelocity)*P.MaxSpeed;
    P.Velocity=WantedVelocity; P.Speed=VSize(WantedVelocity);
    if (Nade(P)!=None) { Nade(P).RotMag=vect(0,0,0); Nade(P).OffsetMag=vect(0,0,0); }
    LastProjectile=P; ReleaseOrigin=P.Location; ReleaseVelocity=P.Velocity; ReleaseSpeed=P.Speed;
    AmmoAfter=GrenadeWeapon.AmmoAmount(0); AmmoCount=AmmoAfter;
    ThrowCount++; NextThrowTime=Level.TimeSeconds+F.FireRate;
    Rig.PC.Pawn.MakeNoise(1.0); GrenadeWeapon.PlaySound(GrenadeWeapon.ThrowSound,SLOT_Interact,2.0);
    Rig.PC.Pawn.DeactivateSpawnProtection(); Rig.PulseRight(0.65,35);
    bPinRemoved=False; bTriggerArmed=False; bGripHeld=False; Motion.Reset();
    Rig.bHolstered=True; Rig.bTriggerArmed=False;
    if (Rig.Visual!=None) Rig.Visual.bHidden=True;
    return True;
}
function UpdateControls()
{
    local vector Relative;
    if (Rig==None || Rig.PC==None || Rig.PC.Pawn==None || Motion==None) { ResetInput(); return; }
    GrenadeWeapon=Frag(Rig.Gun);
    if (GrenadeWeapon==None || GrenadeWeapon.Class!=class'Frag' || !Rig.ActiveFor(GrenadeWeapon)
        || Rig.bHolstered || (Rig.Tracking.Flags & 2)==0)
    { ResetInput(); return; }
    AmmoCount=GrenadeWeapon.AmmoAmount(0);
    if (Rig.Tracking.Serial!=LastSerial)
    {
        LastSerial=Rig.Tracking.Serial;
        Relative=((Rig.Tracking.RightPosition-Rig.PC.Pawn.Location)<<Rig.Tracking.BodyRotation)/Rig.Tracking.UnitsPerMetre;
        Motion.Add(Relative,Level.TimeSeconds); MotionSamples=Motion.Samples; MeasuredVelocity=Motion.Velocity;
        if (Motion.bDiscontinuity) { ResetInput(); return; }
    }
    if (Rig.Tracking.RightGrip>0.7) bGripHeld=True;
    if (Rig.Tracking.RightTrigger<0.15) bTriggerArmed=True;
    // Trigger arms the held grenade. Fuse starts on release through the stock spawn.
    if (bTriggerArmed && !bPinRemoved && bGripHeld && Rig.Tracking.RightGrip>0.7 && Rig.Tracking.RightTrigger>0.75
        && AmmoCount>0 && Level.TimeSeconds>=NextThrowTime)
    { bPinRemoved=True; bTriggerArmed=False; ArmedCount++; Rig.PulseRight(0.25,18); }
    if (bGripHeld && Rig.Tracking.RightGrip<0.2)
    {
        if (bPinRemoved && !Throw()) ResetInput();
        bGripHeld=False;
    }
}
defaultproperties
{
    RemoteRole=ROLE_None
}
