class KFVRDualBeltReload extends Info;
var KFVRTrackedDualies Gun;
var KFVRWeaponRig Rig;
var KFVRDualMagazineState Ammo;
var KFVRBeltInsertion Gates[2];
var vector RightDock,LeftDock,RightMagazineWorld,LeftMagazineWorld;
var float RightMagazineError,LeftMagazineError,RightProgress,LeftProgress;
var int RightInserted,LeftInserted,RightPresented,LeftPresented,RightLoaded,LeftLoaded;
var int OwnedRounds,SpentRounds,ReserveRounds,Revision,AssistCount;
var int CurrentCapacity,RightInsertedCapacity,LeftInsertedCapacity,RightPresentedCapacity,LeftPresentedCapacity,CapacityChanges;
var int EjectCount,PresentCount,SeatCount,ClearCount,StowCount;
var bool bValid,bRightChamber,bLeftChamber,bRightLatched,bLeftLatched;
function bool Initialize(KFVRTrackedDualies W)
{
    if (W==None || W.Belt!=None || W.bIsReloading || (W.MagCapacity%2)!=0) return False;
    Gun=W; Rig=W.VRRig; Ammo=new(None) class'KFVRDualMagazineState';
    if (!Ammo.Initialize(W.MagCapacity/2,W.RightLoaded,W.LeftLoaded,W.AmmoAmount(0))) return False;
    Gates[0]=new(None) class'KFVRBeltInsertion'; Gates[1]=new(None) class'KFVRBeltInsertion';
    Gun.Belt=self; Publish(); return True;
}
function Publish()
{
    if (Gun==None || Ammo==None) return;
    RightLoaded=Ammo.Loaded(0); LeftLoaded=Ammo.Loaded(1);
    OwnedRounds=Ammo.Total(); SpentRounds=Ammo.Spent; ReserveRounds=Ammo.Reserve;
    Gun.ClientForceAmmoUpdate(0,OwnedRounds); Gun.MagAmmoRemaining=RightLoaded+LeftLoaded;
    Gun.RightLoaded=RightLoaded; Gun.LeftLoaded=LeftLoaded; Gun.VRTotalAmmo=OwnedRounds;
    RightInserted=Ammo.Inserted[0]; LeftInserted=Ammo.Inserted[1];
    RightPresented=Ammo.Presented[0]; LeftPresented=Ammo.Presented[1];
    bRightChamber=Ammo.Chamber[0]!=0; bLeftChamber=Ammo.Chamber[1]!=0;
    bRightLatched=Ammo.BeltLatched[0]!=0; bLeftLatched=Ammo.BeltLatched[1]!=0;
    Revision=Ammo.Revision; AssistCount=Ammo.AssistCount; bValid=Ammo.Valid();
    CurrentCapacity=Ammo.Capacity;
    RightInsertedCapacity=0; LeftInsertedCapacity=0; RightPresentedCapacity=0; LeftPresentedCapacity=0;
    if (Ammo.Inserted[0]>0) RightInsertedCapacity=Ammo.GetMagazineCapacity(Ammo.Inserted[0]);
    if (Ammo.Inserted[1]>0) LeftInsertedCapacity=Ammo.GetMagazineCapacity(Ammo.Inserted[1]);
    if (Ammo.Presented[0]>0) RightPresentedCapacity=Ammo.GetMagazineCapacity(Ammo.Presented[0]);
    if (Ammo.Presented[1]>0) LeftPresentedCapacity=Ammo.GetMagazineCapacity(Ammo.Presented[1]);
}
function Reconcile()
{
    local int PreviousCapacity;
    if (Gun==None || Ammo==None) return;
    PreviousCapacity=Ammo.Capacity;
    if ((Gun.MagCapacity%2)==0 && Ammo.SetCapacity(Gun.MagCapacity/2)
        && PreviousCapacity!=Ammo.Capacity) CapacityChanges++;
    Ammo.ReconcileTotal(Gun.AmmoAmount(0)); Publish();
}
function Suspend()
{
    if (Gates[0]!=None) Gates[0].Reset();
    if (Gates[1]!=None) Gates[1].Reset();
}
function StoreForSwitch()
{
    Reconcile();
    if (Ammo.Presented[0]>0 || Ammo.Presented[1]>0) StowCount++;
    Ammo.StoreForSwitch(); Suspend(); Publish();
    if (Rig.Visual!=None && Rig.Gun==Gun) Rig.Visual.PoseMagazine(Ammo.Inserted[0]>0,False,vect(0,0,0),float(Ammo.SlideLocked[0]));
    if (Gun.LeftVisual!=None) Gun.LeftVisual.PoseMagazine(Ammo.Inserted[1]>0,False,vect(0,0,0),float(Ammo.SlideLocked[1]));
}
function bool CanDisable()
{
    return Ammo!=None && Ammo.Inserted[0]>0 && Ammo.Inserted[1]>0 && Ammo.Presented[0]==0 && Ammo.Presented[1]==0 && Ammo.BeltLatched[0]==0 && Ammo.BeltLatched[1]==0;
}
function Pulse(int Hand,float Strength,int Ms)
{
    if (Hand==0) Rig.PulseRight(Strength,Ms); else Rig.PulseLeft(Strength,Ms);
}
function EjectDownward()
{
    local int I,Mask;
    local vector Direction;
    local bool Changed;
    if (Rig.Gun!=Gun || !Rig.Active() || Rig.bHolstered) return;
    Reconcile();
    for (I=0;I<2;I++)
    {
        if (I==0) { Mask=2; Direction=vector(Rig.Tracking.RightRotation); }
        else { Mask=1; Direction=vector(Rig.Tracking.LeftRotation); }
        if ((Rig.Tracking.Flags & Mask)==0 || Direction.Z> -0.65) continue;
        Changed=False;
        if (Ammo.Eject(I)) { EjectCount++; Changed=True; }
        if (Ammo.Present(I)) { PresentCount++; Changed=True; }
        if (Changed) { Gates[I].Reset(); Pulse(I,0.3,20); }
    }
    Publish();
}
function Update(float Delta)
{
    local int I,Mask;
    local vector X,Y,Z,GX,GY,GZ,Dock,Well,Relative;
    local KFVRWeaponVisual V;
    local rotator DockRotation;
    local coords Part;
    if (Rig.Gun!=Gun || !Rig.Active() || Rig.bHolstered) { Suspend(); return; }
    Reconcile(); GetAxes(Rig.Tracking.BodyRotation,X,Y,Z);
    RightDock=Rig.Tracking.HeadPosition+(Y*0.22-Z*0.62+X*0.18)*Rig.Tracking.UnitsPerMetre;
    LeftDock=Rig.Tracking.HeadPosition+(-Y*0.22-Z*0.62+X*0.18)*Rig.Tracking.UnitsPerMetre;
    DockRotation=Rig.Tracking.BodyRotation; DockRotation.Pitch=-16384;
    for (I=0;I<2;I++)
    {
        if (I==0) { V=Rig.Visual; Dock=RightDock; Mask=2; }
        else { V=Gun.LeftVisual; Dock=LeftDock; Mask=1; }
        if (V==None || !V.bTranslationCalibrated) { Gates[I].Reset(); continue; }
        Well=V.MagazineRest(); Relative=((Well-Dock)<<Rig.Tracking.BodyRotation)/Rig.Tracking.UnitsPerMetre;
        if ((Rig.Tracking.Flags & Mask)==0) Gates[I].Reset();
        else if (Ammo.BeltLatched[I]!=0)
        {
            if (VSize(Relative)>0.16 && Ammo.ClearBelt(I)) { ClearCount++; Gates[I].Reset(); Pulse(I,0.2,15); }
        }
        else if (Ammo.Presented[I]>0)
        {
            GetAxes(V.Rotation,GX,GY,GZ);
            // Cached source time ignores duplicate Ticks and stays physical in ZED time.
            if (Gates[I].Update(Relative,Rig.Tracking.InputSeconds,GX.Z< -0.85 && (GY dot Y)>0.85) && Ammo.Seat(I))
            { SeatCount++; Pulse(I,0.45,30); }
        }
        else Gates[I].Reset();
        V.PoseMagazine(Ammo.Inserted[I]>0,Ammo.Presented[I]>0,Dock,float(Ammo.SlideLocked[I]),DockRotation);
        Part=V.GetBoneCoords(V.MagazineBone);
        if (I==0)
        { RightMagazineWorld=Part.Origin; RightMagazineError=0; if (Ammo.Presented[I]>0) RightMagazineError=VSize(Part.Origin-Dock); RightProgress=Gates[I].Progress; }
        else
        { LeftMagazineWorld=Part.Origin; LeftMagazineError=0; if (Ammo.Presented[I]>0) LeftMagazineError=VSize(Part.Origin-Dock); LeftProgress=Gates[I].Progress; }
    }
    Publish();
}
function Destroyed()
{
    if (Gun!=None && Gun.Belt==self)
    {
        if (Ammo!=None) Reconcile(); Gun.Belt=None;
        Gun.Feed.Initialize(Gun.MagCapacity,0);
        Gun.Feed.RightLoaded=Min(Gun.Feed.RightCapacity,RightLoaded);
        Gun.Feed.LeftLoaded=Min(Gun.Feed.LeftCapacity,LeftLoaded);
        Gun.MagAmmoRemaining=Gun.Feed.Total(); Gun.EnsureFeed();
    }
    Super.Destroyed();
}
defaultproperties
{
    RemoteRole=ROLE_None
}
