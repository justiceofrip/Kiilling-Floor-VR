// Native KF magazine-cluster supply marker. The rig owns availability/placement;
// this actor never grants ammo, handles input, or runs a gameplay Tick.
class KFVRAmmoSupplyVisual extends Actor;
var string NativeAssetPath;
var vector LocalCenter,SupplyCenter,MeasuredCenter,ExpectedLocation;
var rotator LocalDisplayRotation,ExpectedRotation;
var float LocalRadius,DisplayDiameterMetres,DisplayRadiusMetres,PositionError;
var int PoseCalls;
var bool bInitialized;

simulated function bool Bounded(float Value,float Limit)
{
    return Value==Value && Value>= -Limit && Value<=Limit;
}
simulated function HideDisplay()
{
    bHidden=True;
}
simulated function bool Initialize(optional float DiameterMetres)
{
    local StaticMesh NativeMesh;
    local plane Bounds;
    bHidden=True; bInitialized=False;
    NativeAssetPath=class'KFVRAmmoSupplyProfile'.static.NativeMeshPath();
    if (DiameterMetres==0) DiameterMetres=class'KFVRAmmoSupplyProfile'.static.DiameterMetres();
    if (!(DiameterMetres>=0.08 && DiameterMetres<=0.32)) return False;
    // Only this audited mesh/material dependency is loaded; no replacement art,
    // runtime package scan, pickup subclass or broad fallback is involved.
    NativeMesh=StaticMesh(DynamicLoadObject(NativeAssetPath,class'StaticMesh',True));
    if (NativeMesh==None)
    { Log("KFVR_AMMO_SUPPLY_ERROR missing native asset=" $ NativeAssetPath); return False; }
    SetStaticMesh(NativeMesh);
    SetLocation(vect(0,0,0)); SetRotation(rot(0,0,0));
    SetDrawScale(1); SetDrawScale3D(vect(1,1,1));
    // At the identity transform this engine query supplies the installed
    // model's centre/radius. Extracted coordinates are not compiled into KFVR.
    Bounds=GetRenderBoundingSphere();
    LocalCenter.X=Bounds.X; LocalCenter.Y=Bounds.Y; LocalCenter.Z=Bounds.Z;
    LocalRadius=Bounds.W;
    if (!Bounded(LocalCenter.X,10000) || !Bounded(LocalCenter.Y,10000)
        || !Bounded(LocalCenter.Z,10000) || !(LocalRadius>0.01 && LocalRadius<10000))
    { Log("KFVR_AMMO_SUPPLY_ERROR invalid native bounds=" $ NativeAssetPath); return False; }
    DisplayDiameterMetres=DiameterMetres; DisplayRadiusMetres=DiameterMetres*0.5;
    LocalDisplayRotation=class'KFVRAmmoSupplyProfile'.static.LocalRotation();
    bInitialized=True;
    Log("KFVR_AMMO_SUPPLY_CAL asset=" $ NativeAssetPath $ " center=" $ LocalCenter $ " radius=" $ LocalRadius);
    return True;
}
// Centre is the exact interaction anchor. BodyRotation is the body frame; the
// configurable local tilt keeps this originally flat pickup readable from above.
simulated function PoseAt(vector Center,rotator BodyRotation,float Units)
{
    local vector X,Y,Z;
    local plane Bounds;
    local float Scale;
    if (!bInitialized || StaticMesh==None || !(Units>=1 && Units<=200)
        || !(DisplayDiameterMetres>=0.08 && DisplayDiameterMetres<=0.32)
        || !Bounded(Center.X,1000000) || !Bounded(Center.Y,1000000) || !Bounded(Center.Z,1000000))
    { HideDisplay(); return; }
    DisplayRadiusMetres=DisplayDiameterMetres*0.5;
    Scale=Units*DisplayRadiusMetres/LocalRadius;
    GetAxes(LocalDisplayRotation,X,Y,Z);
    ExpectedRotation=OrthoRotation(X>>BodyRotation,Y>>BodyRotation,Z>>BodyRotation);
    SupplyCenter=Center;
    ExpectedLocation=Center-((LocalCenter*Scale)>>ExpectedRotation);
    SetDrawScale(Scale); SetRotation(ExpectedRotation); SetLocation(ExpectedLocation);
    Bounds=GetRenderBoundingSphere();
    MeasuredCenter.X=Bounds.X; MeasuredCenter.Y=Bounds.Y; MeasuredCenter.Z=Bounds.Z;
    PositionError=VSize(MeasuredCenter-SupplyCenter); PoseCalls++; bHidden=False;
}
simulated function Destroyed()
{
    bHidden=True; bInitialized=False;
    SetStaticMesh(None);
    Super.Destroyed();
}
defaultproperties
{
    DrawType=DT_StaticMesh
    Physics=PHYS_None
    RemoteRole=ROLE_None
    bHidden=True
    bUnlit=False
    bStaticLighting=False
    AmbientGlow=24
    bCollideActors=False
    bCollideWorld=False
    bBlockActors=False
    bBlockPlayers=False
    bUseCollisionStaticMesh=False
}
