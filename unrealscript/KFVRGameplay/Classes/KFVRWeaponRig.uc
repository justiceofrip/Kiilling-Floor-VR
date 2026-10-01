// Explicit standalone development rig. Native input writes only Tracking on the
// existing game-thread Tick boundary; actors and weapon effects remain script-owned.
class KFVRWeaponRig extends Info;
struct TrackingPacket
{
    var vector LeftPosition;
    var rotator LeftRotation;
    var vector RightPosition;
    var rotator RightRotation;
    var float LeftTrigger, LeftGrip, RightTrigger, RightGrip;
    var float LeftThumb, LeftIndex, LeftCurl, RightThumb, RightIndex, RightCurl;
    var vector HeadPosition;
    var rotator BodyRotation;
    var float UnitsPerMetre;
    var int Flags, Serial;
};
struct FeedbackPacket
{
    var int Serial;
    var float LeftAmplitude, RightAmplitude;
    var int LeftMs, RightMs;
    var int LeftSerial, RightSerial;
};
var TrackingPacket Tracking;
var FeedbackPacket Feedback;
var PlayerController PC;
var KFWeapon Gun, HandSource;
var Pawn BoundPawn;
var KFWeapon InventoryWeapons[8];
var KFVRWeaponVisual InventoryVisuals[8];
var int InventorySlots[8];
var int DoorUseCount;
var byte InventoryVisualFailed[8];
var int InventoryCount, ActiveIndex, SwitchCount, ReloadCount;
var bool bReloadArmed, bReloadPrevious, bCycleArmed, bCyclePrevious;
var KFVRWeaponVisual Visual;
var KFVRHandVisual LeftHand, RightHand;
var KFVRStatusDisplay StatusDisplay;
var KFVRTraderPanel Trader;
var KFVRGrenadeControl GrenadeControl;
var bool bStatusFailed,bLeftHandFailed,bRightHandFailed;
var KFVRManualReload ManualReload;
var KFVRManualReload InventoryManuals[8];
var bool bManualArmed,bManualPrevious,bUseArmed,bUsePrevious;
var int UseCount;
var bool bMeleeTest,bArmTest,bNativeHandTest;
var KFVRMeleeTarget MeleeTarget;
var bool bSupportHeld,bSupportArmed,bSupportPrevious;
var int SupportGrabs,SupportReleases,SupportFrames;
var vector SupportPosition;
var float SupportDirectionDot;
var vector MuzzleLocation, LastTraceStart, LastTraceDirection, HolsterLocation;
var rotator AimRotation;
var int LastSerial, AppliedFrames, TraceCount, HolsterCount, DrawCount;
var float LastInputTime, Recoil;
var bool bTracking, bTriggerArmed, bFiring, bHolstered, bGripPrevious, bGripArmed;

var int PresentationSerial;
function PulseRight(float Strength, int DurationMs)
{
    Feedback.RightAmplitude=FClamp(Strength,0.0,1.0); Feedback.RightMs=Clamp(DurationMs,0,100);
    Feedback.RightSerial++; Feedback.Serial++;
}
function PulseLeft(float Strength, int DurationMs)
{
    Feedback.LeftAmplitude=FClamp(Strength,0.0,1.0); Feedback.LeftMs=Clamp(DurationMs,0,100);
    Feedback.LeftSerial++; Feedback.Serial++;
}
function ToggleManualReload()
{
    if (Gun==None || Visual==None || !Visual.bPhysicalProfile || !Visual.bTranslationCalibrated || Gun.bIsReloading || bHolstered) return;
    if (KFVRTrackedDualies(Gun)!=None) { KFVRTrackedDualies(Gun).ToggleBelt(); return; }
    if (ManualReload!=None)
    {
        if (!ManualReload.CanDisable()) return;
        if (Visual!=None) Visual.ClearMechanicalPose();
        ManualReload.Destroy(); ManualReload=None; InventoryManuals[ActiveIndex]=None;
    }
    else
    {
        ManualReload=Spawn(class'KFVRManualReload',self);
        if (ManualReload!=None && !ManualReload.Initialize(self,Gun))
        { ManualReload.Destroy(); ManualReload=None; return; }
    }
    InventoryManuals[ActiveIndex]=ManualReload;
    bTriggerArmed=False; Gun.StopFire(0); bFiring=False; PulseLeft(0.5,40);
}
function PruneInventory()
{
    local int I,J;
    for (I=InventoryCount-1;I>=0;I--)
    {
        if (InventoryWeapons[I]!=None && InventoryWeapons[I].Owner==BoundPawn) continue;
        if (InventoryVisuals[I]!=None) InventoryVisuals[I].Destroy();
        if (InventoryManuals[I]!=None) InventoryManuals[I].Destroy();
        for (J=I;J<InventoryCount-1;J++)
        {
            InventoryWeapons[J]=InventoryWeapons[J+1]; InventoryVisuals[J]=InventoryVisuals[J+1];
            InventorySlots[J]=InventorySlots[J+1]; InventoryVisualFailed[J]=InventoryVisualFailed[J+1]; InventoryManuals[J]=InventoryManuals[J+1];
        }
        InventoryCount--; InventoryWeapons[InventoryCount]=None; InventoryVisuals[InventoryCount]=None; InventoryManuals[InventoryCount]=None;
    }
    if (ManualReload!=None && (ManualReload.Gun==None || ManualReload.Gun.Owner!=BoundPawn))
    { ManualReload.Destroy(); ManualReload=None; }
    if (Gun==None || Gun.Owner!=BoundPawn) { if (Gun!=None) Gun.StopFire(0); Gun=None; Visual=None; bFiring=False; bTriggerArmed=False; }
    for (I=0;I<InventoryCount;I++) if (InventoryWeapons[I]==Gun) ActiveIndex=I;
    if (Gun==None && InventoryCount>0) { Gun=InventoryWeapons[0]; ActiveIndex=0; }
    ManualReload=InventoryManuals[ActiveIndex];
}
function RegisterWeapon(KFWeapon W, int Slot)
{
    local int I;
    if (W==None) return;
    if (BoundPawn!=Pawn(W.Owner))
    {
        ReleaseInput();
        for (I=0;I<InventoryCount;I++)
        {
            if (InventoryVisuals[I]!=None) InventoryVisuals[I].Destroy();
            if (InventoryManuals[I]!=None) InventoryManuals[I].Destroy();
            InventoryManuals[I]=None; InventoryVisuals[I]=None; InventoryWeapons[I]=None; InventoryVisualFailed[I]=0;
        }
        if (ManualReload!=None) { ManualReload.Destroy(); ManualReload=None; }
        if (LeftHand!=None) { LeftHand.Destroy(); LeftHand=None; }
        if (RightHand!=None) { RightHand.Destroy(); RightHand=None; }
        bLeftHandFailed=False; bRightHandFailed=False;
        InventoryCount=0; Gun=None; Visual=None; HandSource=None;
        BoundPawn=Pawn(W.Owner);
    }
    PruneInventory();
    if (KFVRTrackedSingle(W)!=None || KFVRTrackedDualies(W)!=None) HandSource=W;
    for (I=0;I<InventoryCount;I++) if (InventoryWeapons[I]==W) return;
    if (InventoryCount>=8) return;
    InventoryWeapons[InventoryCount]=W; InventorySlots[InventoryCount]=Slot; InventoryVisualFailed[InventoryCount]=0;
    InventoryCount++;
    if (Gun==None) { Gun=W; ActiveIndex=InventoryCount-1; }
}
function bool ActiveFor(Weapon W) { return Active() && Gun==W; }
function bool WantsFireFor(Weapon W) { return Gun==W && WantsFire(); }
function UseTracked()
{
    local vector HitLocation,HitNormal,End;
    local KFDoorMover Door;
    local Actor T;
    local ShopVolume S;
    if (!Active() || PC.Pawn==None || !PC.Pawn.bCanUse || (Tracking.Flags & 2)==0) return;
    if (PC.Pawn.Trace(HitLocation,HitNormal,Tracking.RightPosition,Tracking.HeadPosition,True)!=None) return;
    End=Tracking.RightPosition+vector(Tracking.RightRotation)*1.4*Tracking.UnitsPerMetre;
    Door=KFDoorMover(PC.Pawn.Trace(HitLocation,HitNormal,End,Tracking.RightPosition,True));
    if (Door!=None && Door.MyTrigger!=None)
    {
        // Retain stock proximity eligibility and the trigger's lock/refire rules.
        foreach PC.Pawn.TouchingActors(class'Actor',T)
            if (T==Door.MyTrigger) { Door.MyTrigger.UsedBy(PC.Pawn); DoorUseCount++; return; }
        return;
    }
    foreach PC.Pawn.TouchingActors(class'ShopVolume',S)
        if (S.bCurrentlyOpen && !S.bAlwaysClosed)
        {
            if (Trader==None)
            { Trader=Spawn(class'KFVRTraderPanel',self); if (Trader!=None && !Trader.Initialize(self)) { Trader.Destroy(); Trader=None; } }
            if (Trader!=None && Trader.OpenShop(S)) return;
        }
    PC.Use();
}
function vector DockPosition(int Slot)
{
    local vector X,Y,Z;
    GetAxes(Tracking.BodyRotation,X,Y,Z);
    if (Slot==6) return Tracking.HeadPosition+(Y*0.22-Z*0.48-X*0.28)*Tracking.UnitsPerMetre;
    if (Slot==5) return Tracking.HeadPosition+(Y*0.18-Z*0.28+X*0.08)*Tracking.UnitsPerMetre;
    if (Slot==4) return Tracking.HeadPosition+(-Y*0.18-Z*0.28+X*0.08)*Tracking.UnitsPerMetre;
    if (Slot==1) return Tracking.HeadPosition+(-Y*0.22-Z*0.62-X*0.05)*Tracking.UnitsPerMetre;
    if (Slot==3) return Tracking.HeadPosition+(-Y*0.22-Z*0.15-X*0.18)*Tracking.UnitsPerMetre;
    if (Slot==2) return Tracking.HeadPosition+(Y*0.22-Z*0.15-X*0.18)*Tracking.UnitsPerMetre;
    return Tracking.HeadPosition+(Y*0.22-Z*0.62-X*0.05)*Tracking.UnitsPerMetre;
}
function SelectInventoryWeapon(int Index)
{
    local KFWeapon Previous;
    if (Index<0 || Index>=InventoryCount || InventoryVisualFailed[Index]!=0 || InventoryWeapons[Index]==None || PC.Pawn==None || InventoryWeapons[Index].Owner!=PC.Pawn) return;
    Previous=Gun;
    if (GrenadeControl!=None) GrenadeControl.ResetInput();
    if (KFVRTrackedSyringe(Previous)!=None) KFVRTrackedSyringe(Previous).ResetInput();
    if (KFVRTrackedWelder(Previous)!=None) KFVRTrackedWelder(Previous).ResetInput();
    if (KFVRTrackedDualies(Previous)!=None) KFVRTrackedDualies(Previous).StoreForSwitch();
    if (KFVRTrackedKnife(Previous)!=None) KFVRTrackedKnife(Previous).ResetMotion();
    if (ManualReload!=None && Previous==ManualReload.Gun) ManualReload.StoreForSwitch();
    if (Previous!=None) { Previous.StopFire(0); Previous.StopFire(1); }
    bFiring=False; bTriggerArmed=False; Recoil=0.0; bSupportHeld=False; bSupportArmed=False; bSupportPrevious=False;
    Gun=InventoryWeapons[Index]; ActiveIndex=Index; Visual=InventoryVisuals[Index]; ManualReload=InventoryManuals[Index];
    PC.Pawn.PendingWeapon=Gun; PC.Pawn.ChangedWeapon();
    bHolstered=False; SwitchCount++; DrawCount++; PulseRight(0.25,18);
}
function UpdateInventoryVisuals()
{
    local int I;
    local rotator DockRotation;
    for (I=0;I<InventoryCount;I++)
    {
        if (InventoryWeapons[I]==None || InventoryWeapons[I].Owner!=PC.Pawn)
        {
            if (InventoryVisuals[I]!=None) InventoryVisuals[I].bHidden=True;
            continue;
        }
        if (InventoryVisuals[I]==None && InventoryVisualFailed[I]==0)
        {
            InventoryVisuals[I]=Spawn(class'KFVRWeaponVisual',self);
            if (InventoryVisuals[I]!=None && !InventoryVisuals[I].Initialize(InventoryWeapons[I]))
            { InventoryVisuals[I].Destroy(); InventoryVisuals[I]=None; InventoryVisualFailed[I]=1; }
        }
        if (InventoryVisuals[I]==None) continue;
        if (I!=ActiveIndex || bHolstered)
        {
            DockRotation=Tracking.BodyRotation; DockRotation.Pitch=-16384;
            InventoryVisuals[I].PoseAt(DockPosition(InventorySlots[I]),DockRotation,Tracking.UnitsPerMetre);
        }
    }
    Visual=InventoryVisuals[ActiveIndex];
}
function rotator SupportedRotation()
{
    local bool Down,ValidSupport;
    local vector Hands,LocalSupport;
    local rotator Result;
    Result=Tracking.RightRotation;
    Down=Tracking.LeftGrip>0.7;
    ValidSupport=KFVRTrackedBullpup(Gun)!=None && !bHolstered && (Tracking.Flags & 3)==3;
    if (ManualReload!=None && (ManualReload.Ammo.Held>0 || ManualReload.bSlideHeld || Tracking.LeftTrigger>0.5)) ValidSupport=False;
    if (!ValidSupport)
    { if (bSupportHeld) SupportReleases++; bSupportHeld=False; bSupportArmed=False; bSupportPrevious=False; return Result; }
    LocalSupport=Visual.SupportLocal-Visual.PalmLocal;
    SupportPosition=Tracking.RightPosition+((LocalSupport*(Tracking.UnitsPerMetre/Visual.ModelUnitsPerMetre))>>Result);
    Hands=Tracking.LeftPosition-Tracking.RightPosition;
    if (!Down) bSupportArmed=True;
    if (bSupportHeld && (!Down || VSize(Hands)>0.85*Tracking.UnitsPerMetre || VSize(Hands)<0.1*Tracking.UnitsPerMetre || Abs(VSize(Hands)-VSize(LocalSupport)*(Tracking.UnitsPerMetre/Visual.ModelUnitsPerMetre))>0.18*Tracking.UnitsPerMetre))
    { bSupportHeld=False; SupportReleases++; }
    if (bSupportArmed && Down && !bSupportPrevious && VSize(Tracking.LeftPosition-SupportPosition)<0.18*Tracking.UnitsPerMetre)
    { bSupportHeld=True; SupportGrabs++; PulseLeft(0.2,15); }
    if (bSupportHeld)
    {
        Result=class'KFVRPoseMath'.static.AlignSupport(Result,LocalSupport,Hands);
        SupportDirectionDot=Normal(LocalSupport>>Result) dot Normal(Hands); SupportFrames++;
    }
    bSupportPrevious=Down; return Result;
}
function bool Active()
{
    return bTracking && PC!=None && PC.Pawn!=None && PC.Pawn.Health>0
        && PC.Pawn.Weapon==Gun && Level.TimeSeconds-LastInputTime<=0.1;
}
function bool WantsFire()
{
    return Active() && (Tracking.Flags & 2)!=0 && bTriggerArmed && !bHolstered && Tracking.RightTrigger>0.75;
}
function RestoreTraderWeapon(optional KFWeapon Preferred)
{
    local int I;
    if (PC==None || PC.Pawn==None || PC.Pawn.Health<=0) return;
    PruneInventory();
    for (I=0;I<InventoryCount;I++)
        if (Preferred!=None && InventoryWeapons[I]==Preferred && InventoryVisualFailed[I]==0) { SelectInventoryWeapon(I); return; }
    for (I=0;I<InventoryCount;I++) if (PC.Pawn.Weapon==InventoryWeapons[I] && InventoryWeapons[I].Class!=class'Frag') return;
    // Stock selling may select its hidden quick-grenade item. Prefer a firearm after shopping.
    for (I=0;I<InventoryCount;I++)
        if (InventoryWeapons[I]==Gun && InventoryWeapons[I].Class!=class'Frag' && InventoryVisualFailed[I]==0) { SelectInventoryWeapon(I); return; }
    for (I=0;I<InventoryCount;I++)
        if (InventoryWeapons[I].Class!=class'Frag' && InventoryVisualFailed[I]==0) { SelectInventoryWeapon(I); return; }
}
function ParkForTrader()
{
    local int I;
    if (GrenadeControl!=None) GrenadeControl.ResetInput();
    if (Gun!=None) { Gun.StopFire(0); Gun.StopFire(1); }
    bFiring=False; bTriggerArmed=False; bHolstered=True;
    bSupportHeld=False; bSupportArmed=False; bGripArmed=False; bReloadArmed=False; bCycleArmed=False; bManualArmed=False;
    for (I=0;I<InventoryCount;I++)
    {
        if (InventoryManuals[I]!=None) InventoryManuals[I].StoreForSwitch();
        if (InventoryVisuals[I]!=None) InventoryVisuals[I].bHidden=True;
        if (KFVRTrackedDualies(InventoryWeapons[I])!=None) KFVRTrackedDualies(InventoryWeapons[I]).StoreForSwitch();
        if (KFVRTrackedWelder(InventoryWeapons[I])!=None) KFVRTrackedWelder(InventoryWeapons[I]).ResetInput();
        if (KFVRTrackedSyringe(InventoryWeapons[I])!=None) KFVRTrackedSyringe(InventoryWeapons[I]).ResetInput();
        if (KFVRTrackedKnife(InventoryWeapons[I])!=None) KFVRTrackedKnife(InventoryWeapons[I]).ResetMotion();
    }
    if (StatusDisplay!=None) StatusDisplay.HideDisplay();
}
function ReleaseInput()
{
    local int I;
    if (Trader!=None) Trader.CloseShop();
    if (GrenadeControl!=None) GrenadeControl.ResetInput();
    if (bSupportHeld) SupportReleases++;
    bSupportHeld=False; bSupportArmed=False; bSupportPrevious=False;
    bManualArmed=False; bManualPrevious=False; bUseArmed=False; bUsePrevious=False;
    if (ManualReload!=None) ManualReload.Suspend();
    for (I=0;I<InventoryCount;I++) if (KFVRTrackedWelder(InventoryWeapons[I])!=None) KFVRTrackedWelder(InventoryWeapons[I]).ResetInput();
    for (I=0;I<InventoryCount;I++) if (KFVRTrackedSyringe(InventoryWeapons[I])!=None) KFVRTrackedSyringe(InventoryWeapons[I]).ResetInput();
    for (I=0;I<InventoryCount;I++) if (KFVRTrackedDualies(InventoryWeapons[I])!=None) KFVRTrackedDualies(InventoryWeapons[I]).ResetInput();
    if (KFVRTrackedKnife(Gun)!=None) KFVRTrackedKnife(Gun).ResetMotion();
    bReloadArmed=False; bReloadPrevious=False; bCycleArmed=False; bCyclePrevious=False;
    for (I=0;I<InventoryCount;I++) if (InventoryVisuals[I]!=None)
    {
        InventoryVisuals[I].bHidden=True;
        if (InventoryVisuals[I].MagazineGrip!=None) InventoryVisuals[I].MagazineGrip.bHidden=True;
    }
    bTracking=False; bTriggerArmed=False; bGripArmed=False; bGripPrevious=False;
    if (bFiring && Gun!=None) Gun.StopFire(0);
    bFiring=False;
    if (Visual!=None) Visual.bHidden=True;
    if (LeftHand!=None) LeftHand.bHidden=True;
    if (RightHand!=None) RightHand.bHidden=True;
    if (StatusDisplay!=None) StatusDisplay.HideDisplay();
}
function int HandGripRole(bool Left)
{
    if (bHolstered || Gun==None) return 0;
    if (!Left) { if (KFVRTrackedKnife(Gun)!=None || Gun.Class==class'Frag') return 2; return 1; }
    if (KFVRTrackedDualies(Gun)!=None) return 1;
    if (bSupportHeld) return 3;
    if (ManualReload!=None && (ManualReload.HeldMagazine>0 || ManualReload.bSlideHeld)) return 3;
    return 0;
}
// Load the presentation while the private match starts, before XR requests its
// first pair. The actors stay hidden until fresh controller input is delivered.
function EnsureHandVisuals()
{
    if (HandSource==None) return;
    if (LeftHand==None && !bLeftHandFailed)
    {
        LeftHand=Spawn(class'KFVRHandVisual',self);
        if (LeftHand!=None && !LeftHand.InitializeHand(HandSource,True)) { LeftHand.Destroy(); LeftHand=None; bLeftHandFailed=True; }
    }
    if (RightHand==None && !bRightHandFailed)
    {
        RightHand=Spawn(class'KFVRHandVisual',self);
        if (RightHand!=None && !RightHand.InitializeHand(HandSource,False)) { RightHand.Destroy(); RightHand=None; bRightHandFailed=True; }
    }
    if (!bTracking)
    {
        if (LeftHand!=None) LeftHand.bHidden=True;
        if (RightHand!=None) RightHand.bHidden=True;
    }
}
// Pose hands after weapon support/recoil, switching and mechanical controls.
// This avoids displaying the previous weapon frame or a stale grasp role.
function Tick(float Delta)
{
    UpdateRig(Delta);
    if (bTracking) UpdateHands();
    PresentationSerial=Tracking.Serial;
}
function UpdateHands()
{
    local KFVRWeaponVisual LeftGrasp,RightGrasp;
    local int LeftRole,RightRole;
    if (Gun!=None && !bHolstered && (Trader==None || !Trader.bOpen))
    {
        RightGrasp=Visual;
        // The grenade toss is authored in the opposite hand; retain its
        // provisional right-hand grasp until a matching profile is available.
        if (Gun.Class==class'Frag') RightGrasp=None;
        if (KFVRTrackedDualies(Gun)!=None) LeftGrasp=KFVRTrackedDualies(Gun).LeftVisual;
        else if (bSupportHeld) LeftGrasp=Visual;
        else if (ManualReload!=None && ManualReload.HeldMagazine>0 && Visual!=None && !Visual.bHidden && (Tracking.Flags & 3)==3) LeftGrasp=Visual.MagazineGrip;
        LeftRole=HandGripRole(True); RightRole=HandGripRole(False);
    }
    if (LeftHand!=None)
    {
        LeftHand.bHidden=(Tracking.Flags & 1)==0;
        if (!LeftHand.bHidden) LeftHand.PoseHand(Tracking.LeftPosition,Tracking.LeftRotation,Tracking.UnitsPerMetre,Tracking.LeftThumb,Tracking.LeftIndex,Tracking.LeftCurl,LeftRole,LeftGrasp,True);
    }
    if (RightHand!=None)
    {
        RightHand.bHidden=(Tracking.Flags & 2)==0;
        if (!RightHand.bHidden) RightHand.PoseHand(Tracking.RightPosition,Tracking.RightRotation,Tracking.UnitsPerMetre,Tracking.RightThumb,Tracking.RightIndex,Tracking.RightCurl,RightRole,RightGrasp,False);
    }
}
function UpdateRig(float Delta)
{
    local vector X,Y,Z,Position;
    local rotator GunRotation;
    local bool GripDown,ReloadDown,CycleDown,ManualDown,UseDown;
    local int I;
    local coords Muzzle;
    PC=PlayerController(Owner);
    if (Level.NetMode!=NM_Standalone || PC==None || PC.Pawn==None || PC.Pawn.Health<=0 || Level.Pauser!=None)
    { ReleaseInput(); return; }
    EnsureHandVisuals();
    if (Tracking.Serial!=LastSerial)
    {
        LastSerial=Tracking.Serial; LastInputTime=Level.TimeSeconds;
        bTracking=(Tracking.Flags & 3)!=0;
    }
    if (!bTracking || Level.TimeSeconds-LastInputTime>0.1 || Tracking.UnitsPerMetre<1 || Tracking.UnitsPerMetre>200)
    { ReleaseInput(); return; }
    if (Trader!=None && Trader.bOpen)
    {
        Trader.UpdateDisplay();
        return;
    }
    if (Gun==None) { ReleaseInput(); return; }
    for (I=0;I<InventoryCount;I++)
        if (PC.Pawn.Weapon==InventoryWeapons[I] && Gun!=InventoryWeapons[I])
        {
            if (KFVRTrackedSyringe(Gun)!=None) KFVRTrackedSyringe(Gun).ResetInput();
            if (KFVRTrackedWelder(Gun)!=None) KFVRTrackedWelder(Gun).ResetInput();
            if (KFVRTrackedKnife(Gun)!=None) KFVRTrackedKnife(Gun).ResetMotion();
            if (KFVRTrackedDualies(Gun)!=None) KFVRTrackedDualies(Gun).StoreForSwitch();
            if (ManualReload!=None) ManualReload.StoreForSwitch();
            if (GrenadeControl!=None) GrenadeControl.ResetInput();
            Gun=InventoryWeapons[I]; ActiveIndex=I; ManualReload=InventoryManuals[I]; bFiring=False; bTriggerArmed=False; bHolstered=False;
        }
    if (PC.Pawn.Weapon!=Gun) bHolstered=True;
    UpdateInventoryVisuals();
    if (Visual==None)
    {
        for (I=0;I<InventoryCount;I++) if (InventoryVisuals[I]!=None) { SelectInventoryWeapon(I); break; }
        if (Visual==None) { ReleaseInput(); return; }
    }
    if (StatusDisplay==None && !bStatusFailed)
    {
        StatusDisplay=Spawn(class'KFVRStatusDisplay',self);
        if (StatusDisplay==None) bStatusFailed=True;
        else if (!StatusDisplay.Initialize(self)) { StatusDisplay.Destroy(); StatusDisplay=None; bStatusFailed=True; }
    }
    if (StatusDisplay!=None) StatusDisplay.UpdateDisplay();
    if ((Tracking.Flags & 2)==0)
    {
        Visual.bHidden=True;
        if (Visual.MagazineGrip!=None) Visual.MagazineGrip.bHidden=True;
        bTriggerArmed=False; bGripArmed=False; bGripPrevious=False;
        if (bFiring) Gun.StopFire(0); bFiring=False;
        if (KFVRTrackedSyringe(Gun)!=None) KFVRTrackedSyringe(Gun).ResetInput();
        if (KFVRTrackedWelder(Gun)!=None) KFVRTrackedWelder(Gun).ResetInput();
        if (KFVRTrackedKnife(Gun)!=None) KFVRTrackedKnife(Gun).ResetMotion();
        if (GrenadeControl!=None) GrenadeControl.ResetInput();
        if (KFVRTrackedDualies(Gun)!=None) KFVRTrackedDualies(Gun).ResetInput();
        return;
    }
    GetAxes(Tracking.BodyRotation,X,Y,Z);
    HolsterLocation=DockPosition(InventorySlots[ActiveIndex]);
    GripDown=Tracking.RightGrip>0.7;
    if (!GripDown) bGripArmed=True;
    if (bGripArmed && GripDown && !bGripPrevious)
    {
        for (I=0;I<InventoryCount;I++)
            if ((I!=ActiveIndex || bHolstered) && VSize(Tracking.RightPosition-DockPosition(InventorySlots[I]))<0.18*Tracking.UnitsPerMetre)
            { SelectInventoryWeapon(I); break; }
    }
    else if (bGripArmed && !bHolstered && !GripDown && bGripPrevious && VSize(Tracking.RightPosition-HolsterLocation)<0.18*Tracking.UnitsPerMetre)
    { if (ManualReload!=None && ManualReload.Gun==Gun) ManualReload.StoreForSwitch(); if (KFVRTrackedDualies(Gun)!=None) KFVRTrackedDualies(Gun).StoreForSwitch(); bHolstered=True; HolsterCount++; PulseRight(0.2,15); }
    bGripPrevious=GripDown;
    UseDown=(Tracking.Flags & 8)!=0;
    if (!UseDown) bUseArmed=True;
    if (bUseArmed && UseDown && !bUsePrevious) { UseTracked(); UseCount++; }
    bUsePrevious=UseDown;
    if (Trader!=None && Trader.bOpen) return;
    ReloadDown=(Tracking.Flags & 32)!=0; CycleDown=(Tracking.Flags & 4)!=0;
    ManualDown=(Tracking.Flags & 16)!=0;
    if (!ManualDown) bManualArmed=True;
    if (bManualArmed && ManualDown && !bManualPrevious)
    {
        if (bMeleeTest && KFVRTrackedKnife(Gun)!=None && !bHolstered)
        {
            if (MeleeTarget!=None) MeleeTarget.Destroy();
            MeleeTarget=Spawn(class'KFVRMeleeTarget',self,,MuzzleLocation+X*0.3*Tracking.UnitsPerMetre);
        }
        else ToggleManualReload();
    }
    bManualPrevious=ManualDown;
    if (!ReloadDown) bReloadArmed=True;
    if (!CycleDown) bCycleArmed=True;
    if (bReloadArmed && ReloadDown && !bReloadPrevious && !bHolstered)
    {
        Gun.StopFire(0); bFiring=False; bTriggerArmed=False;
        if (KFVRTrackedDualies(Gun)!=None) KFVRTrackedDualies(Gun).ResetInput(True);
        if (KFVRTrackedDualies(Gun)!=None && KFVRTrackedDualies(Gun).Belt!=None) KFVRTrackedDualies(Gun).Belt.EjectDownward();
        else if (ManualReload!=None && ManualReload.Gun==Gun) ManualReload.EjectMagazine();
        else if (Gun.AllowReload()) { Gun.ReloadMeNow(); ReloadCount++; }
    }
    if (bCycleArmed && CycleDown && !bCyclePrevious && InventoryCount>1)
        SelectInventoryWeapon((ActiveIndex+1)%InventoryCount);
    bReloadPrevious=ReloadDown; bCyclePrevious=CycleDown;
    if (bHolstered)
    {
        Position=HolsterLocation; GunRotation=Tracking.BodyRotation; GunRotation.Pitch=-16384;
        bTriggerArmed=False;
    }
    else
    {
        GunRotation=SupportedRotation();
        if (KFVRTrackedDualies(Gun)!=None) Recoil=KFVRTrackedDualies(Gun).RightRecoil;
        GunRotation.Pitch+=int(Recoil*20000.0);
        Position=Tracking.RightPosition-vector(GunRotation)*Recoil*Tracking.UnitsPerMetre;
    }
    Visual.PoseAt(Position,GunRotation,Tracking.UnitsPerMetre);
    if (ManualReload!=None) ManualReload.Update(Delta);
    Visual.BoneRefresh(); Visual.SampleMagazinePose(); Muzzle=Visual.GetBoneCoords(Visual.TipBone);
    MuzzleLocation=Muzzle.Origin; AimRotation=GunRotation;
    if (KFVRTrackedKnife(Gun)!=None || KFVRTrackedSyringe(Gun)!=None || KFVRTrackedWelder(Gun)!=None) MuzzleLocation=Visual.Location+((Visual.TipLocal*Visual.DrawScale)>>Visual.Rotation);
    if (KFVRTrackedSyringe(Gun)!=None || KFVRTrackedWelder(Gun)!=None) AimRotation=rotator(Visual.ToolAxis>>Visual.Rotation);
    // A missing tip must not silently turn an invalid origin into a shot.
    if (VSize(MuzzleLocation-Position)>1.4*Tracking.UnitsPerMetre) { ReleaseInput(); return; }
    AppliedFrames++; Recoil=FMax(0.0,Recoil-Delta*0.25);
    for (I=0;I<InventoryCount;I++)
        if (KFVRTrackedDualies(InventoryWeapons[I])!=None) KFVRTrackedDualies(InventoryWeapons[I]).PoseLeft(InventoryWeapons[I]==Gun,Delta);
    if (KFVRTrackedDualies(Gun)!=None)
    {
        if (KFVRTrackedDualies(Gun).Belt!=None) KFVRTrackedDualies(Gun).Belt.Update(Delta);
        KFVRTrackedDualies(Gun).UpdateControls();
        bFiring=KFVRTrackedDualies(Gun).bLeftFiring || KFVRTrackedDualies(Gun).bRightFiring;
        return;
    }
    if (Gun.Class==class'Frag')
    {
        if (GrenadeControl==None) { GrenadeControl=Spawn(class'KFVRGrenadeControl',self); if (GrenadeControl!=None) GrenadeControl.Initialize(self); }
        if (GrenadeControl!=None) GrenadeControl.UpdateControls();
        Gun.StopFire(0); Gun.StopFire(1); bFiring=False; return;
    }
    if (KFVRTrackedWelder(Gun)!=None)
    { KFVRTrackedWelder(Gun).UpdateControls(); bFiring=KFVRTrackedWelder(Gun).bWeldFiring; return; }
    if (KFVRTrackedSyringe(Gun)!=None)
    { KFVRTrackedSyringe(Gun).UpdateControls(); bFiring=KFVRTrackedSyringe(Gun).bHealFiring; return; }
    if (KFVRTrackedKnife(Gun)!=None)
    {
        MuzzleLocation=Visual.Location+((Visual.TipLocal*Visual.DrawScale)>>GunRotation);
        KFVRTrackedKnife(Gun).UpdatePhysical(MuzzleLocation);
        if (bFiring) Gun.StopFire(0); bFiring=False; return;
    }
    if (Tracking.RightTrigger<0.15) bTriggerArmed=True;
    if (bTriggerArmed && !bHolstered && Tracking.RightTrigger>0.75)
    {
        if (!bFiring) { bFiring=Gun.StartFire(0); }
    }
    else if (bFiring) { Gun.StopFire(0); bFiring=False; }
}
function Destroyed()
{
    local int I;
    ReleaseInput();
    if (MeleeTarget!=None) MeleeTarget.Destroy();
    if (StatusDisplay!=None) StatusDisplay.Destroy();
    if (Trader!=None) Trader.Destroy();
    if (GrenadeControl!=None) GrenadeControl.Destroy();
    for (I=0;I<InventoryCount;I++)
    { if (InventoryVisuals[I]!=None) InventoryVisuals[I].Destroy(); if (InventoryManuals[I]!=None) InventoryManuals[I].Destroy(); }
    if (LeftHand!=None) LeftHand.Destroy();
    if (RightHand!=None) RightHand.Destroy();
    Super.Destroyed();
}
defaultproperties
{
    RemoteRole=ROLE_None
    bAlwaysTick=True
}
