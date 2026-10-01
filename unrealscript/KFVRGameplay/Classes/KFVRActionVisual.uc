// Original prototype handle; presentation only, never an ammunition authority.
// Geometry is authored at 300 model units/metre, with its base at the origin.
class KFVRActionVisual extends Actor;
#exec STATICMESH IMPORT NAME=KFVRChargingHandle FILE=Models\KFVRChargingHandle.lwo
var KFVRWeaponVisual SourceVisual;
var ConstantColor HandleColor;
var FinalBlend HandleMaterial;
var vector AnchorLocal,PullAxisLocal,ExpectedPosition;
var rotator ExpectedRotation;
var float TravelMetres,AppliedTravel,PositionError;
var int PoseCalls;
var bool bInitialized;

simulated function bool Initialize(KFVRWeaponVisual Source,vector InAnchorLocal,vector InPullAxisLocal,float InTravelMetres)
{
    local color C;
    local float AxisLength;
    bHidden=True; bInitialized=False;
    AxisLength=VSize(InPullAxisLocal);
    if (Source==None || Source.bDeleteMe || !(VSize(InAnchorLocal)<10000)
        || !(AxisLength>0.0001 && AxisLength<10000)
        || !(InTravelMetres>0 && InTravelMetres<=0.30)) return False;
    SourceVisual=Source; AnchorLocal=InAnchorLocal; PullAxisLocal=Normal(InPullAxisLocal);
    TravelMetres=InTravelMetres; AppliedTravel=0;
    HandleColor=new(None) class'ConstantColor';
    C.R=46; C.G=51; C.B=56; C.A=255; HandleColor.Color=C;
    HandleMaterial=new(None) class'FinalBlend'; HandleMaterial.Material=HandleColor;
    HandleMaterial.FrameBufferBlending=FB_Overwrite;
    HandleMaterial.ZTest=True; HandleMaterial.ZWrite=True;
    HandleMaterial.AlphaTest=False; HandleMaterial.TwoSided=False;
    Skins[0]=HandleMaterial; SetDrawScale3D(vect(1,1,1));
    bInitialized=True; return True;
}

simulated function Hide()
{
    bHidden=True;
}

simulated function PoseAction(float Travel)
{
    local vector LocalPosition;
    if (!bInitialized || SourceVisual==None || SourceVisual.bDeleteMe || SourceVisual.bHidden)
    { Hide(); return; }
    AppliedTravel=FClamp(Travel,0,1);
    LocalPosition=AnchorLocal+PullAxisLocal*(TravelMetres*SourceVisual.ModelUnitsPerMetre*AppliedTravel);
    ExpectedPosition=SourceVisual.Location+((LocalPosition*SourceVisual.DrawScale)>>SourceVisual.Rotation);
    ExpectedRotation=SourceVisual.Rotation;
    SetDrawScale(SourceVisual.DrawScale*SourceVisual.ModelUnitsPerMetre/300.0); SetRotation(ExpectedRotation); SetLocation(ExpectedPosition);
    PositionError=VSize(Location-ExpectedPosition); PoseCalls++; bHidden=False;
}

simulated function Destroyed()
{
    Skins[0]=None;
    if (HandleMaterial!=None) HandleMaterial.Material=None;
    HandleMaterial=None; HandleColor=None; SourceVisual=None;
    Super.Destroyed();
}

defaultproperties
{
    DrawType=DT_StaticMesh
    StaticMesh=StaticMesh'KFVRChargingHandle'
    Style=STY_Normal
    Physics=PHYS_None
    RemoteRole=ROLE_None
    bHidden=True
    bUnlit=False
    AmbientGlow=24
    bStaticLighting=False
    bCollideActors=False
    bCollideWorld=False
    bBlockActors=False
    bBlockPlayers=False
}
