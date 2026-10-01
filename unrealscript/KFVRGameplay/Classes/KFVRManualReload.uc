// All gestures execute once on the ordinary rig Tick. Rendering only poses bones.
class KFVRManualReload extends Info;
var KFVRWeaponRig Rig;
var KFWeapon Gun;
var KFVRMagazineState Ammo;
var bool bLeftArmed,bLeftPrevious,bSlideHeld;
var vector MagWell,SlideLocation,BeltLocation,SlideGrabLocal,MagazineWorld,MagazineTarget;
var float MagazineError,SlideTravel,MagazineDirectionDot;
var int EjectCount,GrabCount,SeatCount,StowCount,DropCount,RackCount;
var int LoadedRounds,OwnedRounds,SpentRounds,DiscardedRounds,HeldMagazine,InsertedMagazine,Revision;
var bool bChambered,bValid;
function bool Initialize(KFVRWeaponRig R,KFWeapon W)
{
    if (W==None || class'KFVRWeaponProfile'.static.Manual(W)!=None || W.bIsReloading) return False;
    Rig=R; Gun=W; Ammo=new class'KFVRMagazineState';
    if (!Ammo.Initialize(W.MagCapacity,W.MagAmmoRemaining,W.AmmoAmount(0))) return False;
    class'KFVRWeaponProfile'.static.BindManual(Gun,self); Publish(); return True;
}
function Publish()
{
    if (Gun==None || Ammo==None) return;
    Gun.ClientForceAmmoUpdate(0,Ammo.Total()); Gun.MagAmmoRemaining=Ammo.Loaded();
    LoadedRounds=Ammo.Loaded(); OwnedRounds=Ammo.Total(); SpentRounds=Ammo.Spent;
    DiscardedRounds=Ammo.Discarded; HeldMagazine=Ammo.Held; InsertedMagazine=Ammo.Inserted;
    Revision=Ammo.Revision; bChambered=Ammo.bChamber; bValid=Ammo.Valid(); SlideTravel=Ammo.Travel;
}
function Reconcile()
{
    if (Gun==None || Ammo==None) return;
    Ammo.ReconcileTotal(Gun.AmmoAmount(0)); Publish();
}
function Suspend()
{
    bLeftArmed=False; bLeftPrevious=False;
    // Tracking loss cancels the grab without completing a feed transition.
    if (bSlideHeld)
    { Ammo.bFeedArmed=False; Ammo.bExtracted=False; Ammo.Travel=0; bSlideHeld=False; Publish(); }
}
function StoreForSwitch()
{
    Reconcile(); Suspend();
    if (Ammo.Held>0 && Ammo.Stow()) StowCount++;
    if (Rig.Visual!=None) Rig.Visual.PoseMagazine(Ammo.Inserted>0,False,vect(0,0,0),Ammo.Travel);
    Publish();
}
function bool CanDisable() { return Ammo!=None && Ammo.Held==0 && Ammo.Inserted>0 && !bSlideHeld && Ammo.Travel<=0.1; }
function EjectMagazine()
{
    local bool Catch;
    if (Gun!=Rig.Gun || Rig.bHolstered || bSlideHeld) return;
    Catch=(Rig.Tracking.Flags & 1)!=0 && Rig.Tracking.LeftGrip>0.55 && VSize(Rig.Visual.HeldMagazinePosition(Rig.Tracking.LeftPosition,Rig.Tracking.LeftRotation)-MagWell)<0.16*Rig.Tracking.UnitsPerMetre;
    if (Ammo.Eject(Catch))
    { EjectCount++; Rig.PulseRight(0.3,20); Publish(); }
}
function Update(float Delta)
{
    local vector X,Y,Z,Relative,Displacement;
    local bool Down,NearBelt,NearWell;
    local float Before;
    local coords Part;
    if (Rig.Gun!=Gun || !Rig.Active() || Rig.bHolstered || (Rig.Tracking.Flags & 3)!=3)
    { Suspend(); return; }
    Reconcile();
    GetAxes(Rig.Tracking.BodyRotation,X,Y,Z);
    BeltLocation=Rig.Tracking.HeadPosition+(-Y*0.2-Z*0.60+X*0.12)*Rig.Tracking.UnitsPerMetre;
    MagWell=Rig.Visual.MagazineRest(); SlideLocation=Rig.Visual.SlideRest();
    Down=Rig.Tracking.LeftGrip>0.7;
    NearBelt=VSize(Rig.Tracking.LeftPosition-BeltLocation)<0.18*Rig.Tracking.UnitsPerMetre;
    MagazineTarget=Rig.Visual.HeldMagazinePosition(Rig.Tracking.LeftPosition,Rig.Tracking.LeftRotation);
    NearWell=VSize(MagazineTarget-MagWell)<0.12*Rig.Tracking.UnitsPerMetre;
    Relative=(Rig.Tracking.LeftPosition-Rig.Tracking.RightPosition)<<Rig.Tracking.RightRotation;
    if (!Down) bLeftArmed=True;
    if (bLeftArmed && Down && !bLeftPrevious && Ammo.Held==0)
    {
        if (Rig.Tracking.LeftTrigger>0.5 && VSize(Rig.Tracking.LeftPosition-SlideLocation)<0.13*Rig.Tracking.UnitsPerMetre && Ammo.Inserted>0)
        { bSlideHeld=True; SlideGrabLocal=Relative; }
        else if (NearBelt && Ammo.GrabBelt()) { GrabCount++; Rig.PulseLeft(0.25,18); }
    }
    if (bSlideHeld)
    {
        Displacement=Relative-SlideGrabLocal;
        if (Abs(Displacement.Y)>0.12*Rig.Tracking.UnitsPerMetre || Abs(Displacement.Z)>0.12*Rig.Tracking.UnitsPerMetre) Suspend();
        else if (Down) Ammo.MoveSlide(-Displacement.X/(Rig.Visual.ActionTravelMetres*Rig.Tracking.UnitsPerMetre));
        else
        {
            Before=float(Ammo.bFeedArmed); Ammo.MoveSlide(0); bSlideHeld=False;
            if (Before>0) { RackCount++; Rig.PulseLeft(0.3,20); }
        }
    }
    else if (bLeftArmed && !Down && bLeftPrevious && Ammo.Held>0)
    {
        if (NearWell && (vector(Rig.Tracking.LeftRotation) dot vector(Rig.Tracking.RightRotation))>0.55 && Ammo.Seat())
        { SeatCount++; Rig.PulseLeft(0.4,25); }
        else if (NearBelt && Ammo.Stow()) { StowCount++; Rig.PulseLeft(0.2,15); }
        else if (Ammo.Drop()) DropCount++;
    }
    bLeftPrevious=Down;
    Rig.Visual.PoseMagazine(Ammo.Inserted>0,Ammo.Held>0,Rig.Tracking.LeftPosition,Ammo.Travel,Rig.Tracking.LeftRotation);
    Part=Rig.Visual.GetBoneCoords(Rig.Visual.MagazineBone); MagazineWorld=Part.Origin;
    MagazineError=0;
    MagazineDirectionDot=Normal(Part.XAxis) dot (Rig.Visual.MagazineX>>Rig.Tracking.LeftRotation);
    if (Ammo.Held>0) MagazineError=VSize(MagazineWorld-MagazineTarget);
    Publish();
}
function Destroyed()
{
    if (Gun!=None && class'KFVRWeaponProfile'.static.Manual(Gun)==self)
    {
        class'KFVRWeaponProfile'.static.BindManual(Gun,None);
        if (Ammo!=None) { Ammo.ReconcileTotal(Gun.AmmoAmount(0)); Gun.ClientForceAmmoUpdate(0,Ammo.Total()); Gun.MagAmmoRemaining=Min(Gun.MagCapacity,Ammo.Loaded()); }
    }
    Super.Destroyed();
}
defaultproperties
{
    RemoteRole=ROLE_None
}
