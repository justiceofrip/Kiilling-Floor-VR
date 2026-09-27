// Explicit unattended-only collision fixture; never installed into normal maps.
class KFVRMeleeTarget extends Actor;
var int HitCount,DamageReceived;
var Pawn LastInstigator;
var class<DamageType> LastDamageType;
var vector LastImpact;
function TakeDamage(int Damage,Pawn InstigatedBy,vector HitLocation,vector Momentum,class<DamageType> DamageType,optional int HitIndex)
{
    HitCount++; DamageReceived+=Damage; LastInstigator=InstigatedBy;
    LastDamageType=DamageType; LastImpact=HitLocation;
}
defaultproperties
{
    RemoteRole=ROLE_None
    bHidden=True
    bCollideActors=True
    bBlockActors=False
    bBlockPlayers=False
    bProjTarget=True
    CollisionRadius=4.0
    CollisionHeight=6.0
}
