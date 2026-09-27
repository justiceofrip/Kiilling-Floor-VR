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
};
var TrackingPacket Tracking;
var FeedbackPacket Feedback;
var PlayerController PC;
var KFWeapon Gun, HandSource;
var Pawn BoundPawn;
var KFWeapon InventoryWeapons[8];
var KFVRWeaponVisual InventoryVisuals[8];
var int InventorySlots[8];
var int InventoryCount, ActiveIndex, SwitchCount, ReloadCount;
var bool bReloadArmed, bReloadPrevious, bCycleArmed, bCyclePrevious;
var KFVRWeaponVisual Visual;
var KFVRHandVisual LeftHand, RightHand;
var KFVRManualReload ManualReload;
var KFVRManualReload InventoryManuals[8];
var bool bManualArmed,bManualPrevious,bUseArmed,bUsePrevious;
var int UseCount;
var bool bSupportHeld,bSupportArmed,bSupportPrevious;
var int SupportGrabs,SupportReleases,SupportFrames;
var vector SupportPosition;
var float SupportDirectionDot;
var vector MuzzleLocation, LastTraceStart, LastTraceDirection, HolsterLocation;
var rotator AimRotation;
var int LastSerial, AppliedFrames, TraceCount, HolsterCount, DrawCount;
var float LastInputTime, Recoil;
var bool bTracking, bTriggerArmed, bFiring, bHolstered, bGripPrevious, bGripArmed;

function PulseRight(float Strength, int DurationMs)
{
    Feedback.LeftAmplitude=0.0; Feedback.LeftMs=0;
    Feedback.RightAmplitude=FClamp(Strength,0.0,1.0); Feedback.RightMs=Clamp(DurationMs,0,100);
    Feedback.Serial++;
}
function PulseLeft(float Strength, int DurationMs)
{
    Feedback.RightAmplitude=0.0; Feedback.RightMs=0;
    Feedback.LeftAmplitude=FClamp(Strength,0.0,1.0); Feedback.LeftMs=Clamp(DurationMs,0,100);
    Feedback.Serial++;
}
function ToggleManualReload()
{
    if (Gun==None || Visual==None || !Visual.bPhysicalProfile || !Visual.bTranslationCalibrated || Gun.bIsReloading || bHolstered) return;
    if (ManualReload!=None)
    {
        if (!ManualReload.CanDisable()) return;
        if (Visual!=None) Visual.PoseMagazine(True,False,vect(0,0,0),0);
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
            InventorySlots[J]=InventorySlots[J+1]; InventoryManuals[J]=InventoryManuals[J+1];
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
            InventoryManuals[I]=None; InventoryVisuals[I]=None; InventoryWeapons[I]=None;
        }
        if (ManualReload!=None) { ManualReload.Destroy(); ManualReload=None; }
        if (LeftHand!=None) { LeftHand.Destroy(); LeftHand=None; }
        if (RightHand!=None) { RightHand.Destroy(); RightHand=None; }
        InventoryCount=0; Gun=None; Visual=None; HandSource=None;
        BoundPawn=Pawn(W.Owner);
    }
    PruneInventory();
    if (KFVRTrackedSingle(W)!=None) HandSource=W;
    for (I=0;I<InventoryCount;I++) if (InventoryWeapons[I]==W) return;
    if (InventoryCount>=8) return;
    InventoryWeapons[InventoryCount]=W; InventorySlots[InventoryCount]=Slot;
    InventoryCount++;
    if (Gun==None) { Gun=W; ActiveIndex=InventoryCount-1; }
}
function bool ActiveFor(Weapon W) { return Active() && Gun==W; }
function bool WantsFireFor(Weapon W) { return Gun==W && WantsFire(); }
function vector DockPosition(int Slot)
{
    local vector X,Y,Z;
    GetAxes(Tracking.BodyRotation,X,Y,Z);
    if (Slot==1) return Tracking.HeadPosition+(-Y*0.22-Z*0.62-X*0.05)*Tracking.UnitsPerMetre;
    if (Slot==2) return Tracking.HeadPosition+(Y*0.22-Z*0.15-X*0.18)*Tracking.UnitsPerMetre;
    return Tracking.HeadPosition+(Y*0.22-Z*0.62-X*0.05)*Tracking.UnitsPerMetre;
}
function SelectInventoryWeapon(int Index)
{
    local KFWeapon Previous;
    if (Index<0 || Index>=InventoryCount || InventoryWeapons[Index]==None || PC.Pawn==None || InventoryWeapons[Index].Owner!=PC.Pawn) return;
    Previous=Gun;
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
        if (InventoryVisuals[I]==None)
        {
            InventoryVisuals[I]=Spawn(class'KFVRWeaponVisual',self);
            if (InventoryVisuals[I]!=None && !InventoryVisuals[I].Initialize(InventoryWeapons[I]))
            { InventoryVisuals[I].Destroy(); InventoryVisuals[I]=None; }
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
    if (bSupportHeld && (!Down || VSize(Hands)>0.85*Tracking.UnitsPerMetre || VSize(Hands)<0.1*Tracking.UnitsPerMetre))
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
function ReleaseInput()
{
    local int I;
    if (bSupportHeld) SupportReleases++;
    bSupportHeld=False; bSupportArmed=False; bSupportPrevious=False;
    bManualArmed=False; bManualPrevious=False; bUseArmed=False; bUsePrevious=False;
    if (ManualReload!=None) ManualReload.Suspend();
    bReloadArmed=False; bReloadPrevious=False; bCycleArmed=False; bCyclePrevious=False;
    for (I=0;I<InventoryCount;I++) if (InventoryVisuals[I]!=None) InventoryVisuals[I].bHidden=True;
    bTracking=False; bTriggerArmed=False; bGripArmed=False; bGripPrevious=False;
    if (bFiring && Gun!=None) Gun.StopFire(0);
    bFiring=False;
    if (Visual!=None) Visual.bHidden=True;
    if (LeftHand!=None) LeftHand.bHidden=True;
    if (RightHand!=None) RightHand.bHidden=True;
}
function Tick(float Delta)
{
    local vector X,Y,Z,Position;
    local rotator GunRotation;
    local bool GripDown,ReloadDown,CycleDown,ManualDown,UseDown;
    local int I;
    local coords Muzzle;
    PC=PlayerController(Owner);
    if (Level.NetMode!=NM_Standalone || PC==None || PC.Pawn==None || PC.Pawn.Health<=0 || Level.Pauser!=None)
    { ReleaseInput(); return; }
    if (Tracking.Serial!=LastSerial)
    {
        LastSerial=Tracking.Serial; LastInputTime=Level.TimeSeconds;
        bTracking=(Tracking.Flags & 3)!=0;
    }
    if (!bTracking || Level.TimeSeconds-LastInputTime>0.1 || Tracking.UnitsPerMetre<1 || Tracking.UnitsPerMetre>200)
    { ReleaseInput(); return; }
    if (Gun==None) { ReleaseInput(); return; }
    for (I=0;I<InventoryCount;I++)
        if (PC.Pawn.Weapon==InventoryWeapons[I] && Gun!=InventoryWeapons[I])
        {
            if (ManualReload!=None) ManualReload.StoreForSwitch();
            Gun=InventoryWeapons[I]; ActiveIndex=I; ManualReload=InventoryManuals[I]; bFiring=False; bTriggerArmed=False; bHolstered=False;
        }
    if (PC.Pawn.Weapon!=Gun) bHolstered=True;
    UpdateInventoryVisuals();
    if (Visual==None) { ReleaseInput(); return; }
    if (LeftHand==None)
    {
        LeftHand=Spawn(class'KFVRHandVisual',self);
        if (LeftHand!=None && !LeftHand.InitializeHand(HandSource,True)) { LeftHand.Destroy(); LeftHand=None; }
    }
    if (RightHand==None)
    {
        RightHand=Spawn(class'KFVRHandVisual',self);
        if (RightHand!=None && !RightHand.InitializeHand(HandSource,False)) { RightHand.Destroy(); RightHand=None; }
    }
    if (LeftHand!=None)
    {
        LeftHand.bHidden=(Tracking.Flags & 1)==0;
        if (!LeftHand.bHidden) LeftHand.PoseHand(Tracking.LeftPosition,Tracking.LeftRotation,Tracking.UnitsPerMetre,Tracking.LeftThumb,Tracking.LeftIndex,Tracking.LeftCurl);
    }
    if (RightHand!=None)
    {
        RightHand.bHidden=(Tracking.Flags & 2)==0;
        if (!RightHand.bHidden) RightHand.PoseHand(Tracking.RightPosition,Tracking.RightRotation,Tracking.UnitsPerMetre,Tracking.RightThumb,Tracking.RightIndex,Tracking.RightCurl);
    }
    if ((Tracking.Flags & 2)==0)
    {
        Visual.bHidden=True; bTriggerArmed=False; bGripArmed=False; bGripPrevious=False;
        if (bFiring) Gun.StopFire(0); bFiring=False; return;
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
    { if (ManualReload!=None && ManualReload.Gun==Gun) ManualReload.StoreForSwitch(); bHolstered=True; HolsterCount++; PulseRight(0.2,15); }
    bGripPrevious=GripDown;
    UseDown=(Tracking.Flags & 8)!=0;
    if (!UseDown) bUseArmed=True;
    if (bUseArmed && UseDown && !bUsePrevious) { PC.Use(); UseCount++; }
    bUsePrevious=UseDown;
    ReloadDown=(Tracking.Flags & 32)!=0; CycleDown=(Tracking.Flags & 4)!=0;
    ManualDown=(Tracking.Flags & 16)!=0;
    if (!ManualDown) bManualArmed=True;
    if (bManualArmed && ManualDown && !bManualPrevious) ToggleManualReload();
    bManualPrevious=ManualDown;
    if (!ReloadDown) bReloadArmed=True;
    if (!CycleDown) bCycleArmed=True;
    if (bReloadArmed && ReloadDown && !bReloadPrevious && !bHolstered)
    {
        Gun.StopFire(0); bFiring=False; bTriggerArmed=False;
        if (ManualReload!=None && ManualReload.Gun==Gun) ManualReload.EjectMagazine();
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
        GunRotation.Pitch+=int(Recoil*20000.0);
        Position=Tracking.RightPosition-vector(GunRotation)*Recoil*Tracking.UnitsPerMetre;
    }
    Visual.PoseAt(Position,GunRotation,Tracking.UnitsPerMetre);
    if (ManualReload!=None) ManualReload.Update(Delta);
    Visual.BoneRefresh(); Muzzle=Visual.GetBoneCoords('tip');
    MuzzleLocation=Muzzle.Origin; AimRotation=GunRotation;
    // A missing tip must not silently turn an invalid origin into a shot.
    if (VSize(MuzzleLocation-Position)>1.4*Tracking.UnitsPerMetre) { ReleaseInput(); return; }
    AppliedFrames++; Recoil=FMax(0.0,Recoil-Delta*0.25);
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
