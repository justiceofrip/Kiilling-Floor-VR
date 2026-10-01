// All gestures execute once on the ordinary rig Tick. Rendering only poses bones.
class KFVRManualReload extends Info;
var KFVRWeaponRig Rig;
var KFWeapon Gun;
var KFVRMagazineState Ammo;
var KFVRMagazineInsertion Insertion;
var KFVRActionRail ActionRail;
var vector ActionPalmWorld,ActionPalmX,ActionPalmY,ActionPalmZ;
var rotator ActionPalmRotation;
var bool bEjectedVisible,bEjectedGrounded;
var vector EjectedPosition,EjectedVelocity;
var rotator EjectedRotation;
var float EjectedAge;
var int EjectedVisualCount;
var vector InsertionPosition;
var float InsertionProgress,InsertionLastDelta,InsertionLastDistance;
var int InsertionSpeedRejects,InsertionGapRejects;
var bool bInsertionApproached;
var bool bLeftArmed,bLeftPrevious,bSlideHeld,bRecoveryPending;
var int HeldInterruptions,RecoveryStowCount;
var vector MagWell,SlideLocation,BeltLocation,SupplyLocation,MagazineWorld,MagazineTarget;
var int ChestGrabCount,ChestStowCount;
var float MagazineError,SlideTravel,MagazineDirectionDot,MagazineAlignment;
var int EjectCount,GrabCount,SeatCount,StowCount,DropCount,RackCount;
var int LoadedRounds,OwnedRounds,SpentRounds,DiscardedRounds,HeldMagazine,InsertedMagazine,Revision;
var bool bChambered,bValid;
var int CurrentCapacity,InsertedCapacity,HeldCapacity,CapacityChanges;
function bool Initialize(KFVRWeaponRig R,KFWeapon W)
{
    if (W==None || class'KFVRWeaponProfile'.static.Manual(W)!=None || W.bIsReloading) return False;
    if (R==None || R.Visual==None || !R.Visual.ManualReloadReady()) return False;
    Rig=R; Gun=W; Ammo=new class'KFVRMagazineState';
    Insertion=new class'KFVRMagazineInsertion';
    if (R.Visual.ManualActionReady()) ActionRail=new class'KFVRActionRail';
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
    CurrentCapacity=Ammo.Capacity; InsertedCapacity=0; HeldCapacity=0;
    if (Ammo.Inserted>0) InsertedCapacity=Ammo.GetMagazineCapacity(Ammo.Inserted);
    if (Ammo.Held>0) HeldCapacity=Ammo.GetMagazineCapacity(Ammo.Held);
}
function Reconcile()
{
    local int PreviousCapacity;
    if (Gun==None || Ammo==None) return;
    // Stock WeaponTick updates perk capacity. Existing magazine identities keep
    // their rounds/limits; only subsequently supplied magazines use this value.
    PreviousCapacity=Ammo.Capacity;
    if (Ammo.SetCapacity(Gun.MagCapacity) && PreviousCapacity!=Ammo.Capacity) CapacityChanges++;
    Ammo.ReconcileTotal(Gun.AmmoAmount(0)); Publish();
}
function Suspend()
{
    local bool ResetPose;
    ResetPose=bEjectedVisible || bSlideHeld || (Ammo!=None && Ammo.Held>0);
    bLeftArmed=False; bLeftPrevious=False; bEjectedVisible=False;
    if (Insertion!=None) Insertion.Reset();
    if (ActionRail!=None) ActionRail.Cancel();
    InsertionProgress=0; bInsertionApproached=False;
    if (Ammo!=None && Ammo.Held>0)
    {
        if (!bRecoveryPending) HeldInterruptions++;
        bRecoveryPending=True;
    }
    // Tracking loss cancels the grab without completing a feed transition.
    if (bSlideHeld)
    { Ammo.bFeedArmed=False; Ammo.bExtracted=False; Ammo.Travel=0; bSlideHeld=False; Publish(); }
    // The gun stays visible through offhand-only loss. Clear the actual bone
    // overrides as well as state: no frozen dropped magazine or pulled slide.
    if (ResetPose && Ammo!=None && Rig!=None && Rig.Gun==Gun && Rig.Visual!=None)
        Rig.Visual.PoseMagazine(Ammo.Inserted>0,False,vect(0,0,0),Ammo.Travel);
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
    if (Gun!=Rig.Gun || Rig.bHolstered) return;
    // The release button must remain usable while the other hand is on the
    // action. Cancel that grasp without completing a feed, then evaluate catch.
    if (bSlideHeld) Suspend();
    MagWell=Rig.Visual.MagazineRest();
    Catch=(Rig.Tracking.Flags & 1)!=0 && Rig.Tracking.LeftGrip>0.55 && VSize(Rig.Visual.HeldMagazinePosition(Rig.Tracking.LeftPosition,Rig.Tracking.LeftRotation)-MagWell)<0.16*Rig.Tracking.UnitsPerMetre;
    if (Ammo.Eject(Catch))
    {
        EjectCount++; Insertion.Reset(); Rig.PulseRight(0.3,20);
        if (!Catch) BeginEjectedVisual(MagWell,Rig.Visual.Rotation);
        Publish();
    }
}
// The native magazine bone presents a discarded part briefly in world space.
// It carries no ammunition or pickup authority; a new held/seated part replaces
// this transient presentation. Floor contact prevents it falling through BSP.
function BeginEjectedVisual(vector Position,rotator Orientation)
{
    EjectedPosition=Position; EjectedRotation=Orientation;
    EjectedVelocity=(-(Rig.Visual.InsertionAxis>>Orientation)*0.6)*Rig.Tracking.UnitsPerMetre;
    EjectedAge=0; bEjectedVisible=True; bEjectedGrounded=False; EjectedVisualCount++;
}
function UpdateEjectedVisual(float Delta)
{
    local vector Next,HitLocation,HitNormal;
    local Actor Hit;
    if (!bEjectedVisible) return;
    EjectedAge+=Delta;
    if (Ammo.Held>0 || Ammo.Inserted>0 || EjectedAge>2.5) { bEjectedVisible=False; return; }
    if (bEjectedGrounded) return;
    Delta=FClamp(Delta,0,0.1);
    Next=EjectedPosition+EjectedVelocity*Delta;
    EjectedVelocity.Z-=9.81*Rig.Tracking.UnitsPerMetre*Delta;
    Hit=Trace(HitLocation,HitNormal,Next,EjectedPosition,False);
    if (Hit!=None) { Next=HitLocation+HitNormal*0.008*Rig.Tracking.UnitsPerMetre; bEjectedGrounded=True; }
    EjectedPosition=Next;
}
function bool ActionInReach()
{
    return Rig!=None && Ammo!=None && Ammo.Held==0
        && (Rig.Tracking.Flags & 3)==3 && (Rig.Tracking.Flags & 32)==0
        && Rig.Visual!=None && Rig.Visual.ManualActionReady()
        && VSize(Rig.Tracking.LeftPosition-Rig.Visual.ManualActionPoint(Ammo.Travel))<0.08*Rig.Tracking.UnitsPerMetre;
}
function CaptureActionPalm()
{
    local vector X,Y,Z;
    GetAxes(Rig.Tracking.LeftRotation,X,Y,Z);
    ActionPalmX=X<<Rig.Visual.Rotation; ActionPalmY=Y<<Rig.Visual.Rotation; ActionPalmZ=Z<<Rig.Visual.Rotation;
}
function UpdateActionPalm()
{
    if (!bSlideHeld) return;
    ActionPalmWorld=Rig.Visual.ManualActionPoint(Ammo.Travel);
    ActionPalmRotation=OrthoRotation(ActionPalmX>>Rig.Visual.Rotation,ActionPalmY>>Rig.Visual.Rotation,ActionPalmZ>>Rig.Visual.Rotation);
}
// Chest is the visible primary supply. Keep the old belt reach as a secondary
// stow/supply zone for existing users and isolated legacy diagnostics.
function vector CurrentSupplyLocation()
{
    return class'KFVRAmmoSupplyState'.static.ChestPosition(Rig.Tracking.HeadPosition,
        Rig.Tracking.BodyRotation,Rig.Tracking.UnitsPerMetre);
}
function bool NearSupply(vector Point)
{
    return VSize(Point-CurrentSupplyLocation())<0.16*Rig.Tracking.UnitsPerMetre;
}
// Current body-space belt position is shared by intent reservation and acquisition.
function vector CurrentBeltLocation()
{
    local vector X,Y,Z;
    GetAxes(Rig.Tracking.BodyRotation,X,Y,Z);
    return Rig.Tracking.HeadPosition+(-Y*0.2-Z*0.60+X*0.12)*Rig.Tracking.UnitsPerMetre;
}
// Read-only: support must yield before it can rotate the weapon or choose a hand
// grasp. Reserve a fresh belt attempt even if supply is empty; never mutate the
// magazine ledger speculatively while deciding which interaction owns the hand.
function bool ReservesOffhand()
{
    if (Rig==None || Ammo==None || Gun!=Rig.Gun) return False;
    if (Ammo.Held>0 || bSlideHeld || (Rig.Tracking.Flags & 32)!=0) return True;
    if (bLeftArmed && Rig.Tracking.LeftGrip>0.7 && !bLeftPrevious && ActionInReach()) return True;
    return bLeftArmed && Rig.Tracking.LeftGrip>0.7 && !bLeftPrevious && Ammo.Inserted==0
        && (NearSupply(Rig.Tracking.LeftPosition) || VSize(Rig.Tracking.LeftPosition-CurrentBeltLocation())<0.18*Rig.Tracking.UnitsPerMetre);
}
// Count a complete installed action return whether the player releases the knob
// or guides it forward while still gripping it. Cancellation never calls here.
function MoveAction(float Travel)
{
    local bool WasArmed;
    WasArmed=Ammo.bFeedArmed; Ammo.MoveSlide(Travel);
    if (WasArmed && !Ammo.bFeedArmed && Ammo.Travel<=0.1)
    { RackCount++; Rig.PulseLeft(0.3,20); }
}
function Update(float Delta)
{
    local vector Relative;
    local bool Down,NearBelt,AtSupply;
    local coords Part;
    if (Rig.Gun!=Gun || !Rig.Active() || Rig.bHolstered || (Rig.Tracking.Flags & 3)!=3)
    { Suspend(); return; }
    Reconcile();
    BeltLocation=CurrentBeltLocation(); SupplyLocation=CurrentSupplyLocation();
    MagWell=Rig.Visual.MagazineRest(); SlideLocation=vect(0,0,0);
    if (Rig.Visual.ManualActionReady()) SlideLocation=Rig.Visual.ManualActionPoint(Ammo.Travel);
    Down=Rig.Tracking.LeftGrip>0.7;
    NearBelt=VSize(Rig.Tracking.LeftPosition-BeltLocation)<0.18*Rig.Tracking.UnitsPerMetre;
    AtSupply=NearSupply(Rig.Tracking.LeftPosition);
    MagazineTarget=Rig.Visual.HeldMagazinePosition(Rig.Tracking.LeftPosition,Rig.Tracking.LeftRotation);
    MagazineAlignment=Rig.Visual.MagazineAlignmentCos(Rig.Tracking.LeftRotation);
    // Current supported firearm model axes coincide with controller aim.
    // Presentation recoil must never become physical action travel.
    Relative=(Rig.Tracking.LeftPosition-Rig.Tracking.RightPosition)<<Rig.Tracking.RightRotation;
    if (bRecoveryPending)
    {
        bRecoveryPending=False;
        // A release that happened while tracking was absent has no reliable
        // insertion/drop location. Preserve the exact magazine in storage;
        // never invent a seating or chambering gesture on reacquisition.
        if (!Down && Ammo.Held>0 && Ammo.Stow())
        { StowCount++; RecoveryStowCount++; Rig.PulseLeft(0.2,15); }
    }
    if (!Down) bLeftArmed=True;
    if (bLeftArmed && Down && !bLeftPrevious && Ammo.Held==0)
    {
        if (ActionInReach())
        {
            if (ActionRail!=None)
                bSlideHeld=ActionRail.Begin(Relative/Rig.Tracking.UnitsPerMetre,
                    Rig.Visual.ActionPullAxis,Rig.Visual.ManualActionTravel(),Ammo.Travel);
            if (bSlideHeld) CaptureActionPalm();
        }
        else if ((AtSupply || NearBelt) && Ammo.GrabBelt())
        { GrabCount++; if (AtSupply) ChestGrabCount++; Rig.PulseLeft(0.25,18); }
    }
    // Geometry supplies a one-shot event; the ledger alone transfers ownership.
    // A push can seat while grip stays held, without a release or animation timer.
    InsertionPosition=Rig.Visual.MagazineSocketPosition(MagazineTarget,Rig.Tracking.UnitsPerMetre);
    if (Insertion.Update(InsertionPosition,MagazineAlignment,Rig.Tracking.InputSerial,
        Rig.Tracking.InputSeconds,Ammo.Held,Down) && Ammo.Seat())
    {
        SeatCount++;
        // This installed model has no manual action. Finish its reload by
        // moving one existing magazine round into an empty chamber.
        if (Rig.Visual.bFeedOnSeat) Ammo.FeedSeatedMagazine();
        Rig.PulseLeft(0.4,25);
    }
    InsertionProgress=Insertion.Progress; bInsertionApproached=Insertion.bApproached;
    InsertionLastDelta=Insertion.LastDelta; InsertionLastDistance=Insertion.LastDistance;
    InsertionSpeedRejects=Insertion.SpeedRejects; InsertionGapRejects=Insertion.GapRejects;
    if (bSlideHeld && !Rig.Visual.ManualActionReady()) Suspend();
    if (bSlideHeld)
    {
        if (ActionRail==None || !ActionRail.Move(Relative/Rig.Tracking.UnitsPerMetre)) Suspend();
        else if (Down) MoveAction(ActionRail.Progress);
        else
        {
            // Spring return can feed only after a held full stroke. Invalid
            // or lost tracking cancels the rail without a feed transition.
            MoveAction(0); bSlideHeld=False; ActionRail.Cancel();
        }
    }
    else if (bLeftArmed && !Down && bLeftPrevious && Ammo.Held>0)
    {
        if ((AtSupply || NearBelt) && Ammo.Stow())
        { StowCount++; if (AtSupply) ChestStowCount++; Rig.PulseLeft(0.2,15); }
        else if (Ammo.Drop()) { DropCount++; BeginEjectedVisual(MagazineTarget,Rig.Tracking.LeftRotation); }
    }
    bLeftPrevious=Down;
    UpdateEjectedVisual(Delta); UpdateActionPalm();
    Rig.Visual.PoseMagazine(Ammo.Inserted>0,Ammo.Held>0,Rig.Tracking.LeftPosition,Ammo.Travel,Rig.Tracking.LeftRotation,
        bEjectedVisible,EjectedPosition,EjectedRotation);
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
