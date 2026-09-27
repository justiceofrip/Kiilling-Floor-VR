// A world actor is required: the ordinary KF first-person overlay is not part of
// the independent stereo world views. Asset coordinates are measured at runtime.
class KFVRWeaponVisual extends Actor;
var Weapon SourceWeapon;
var vector PalmLocal, TipLocal;
var float ModelUnitsPerMetre;
var bool bCalibrated;
simulated function bool Initialize(Weapon Source)
{
    local coords Palm,Tip;
    bHidden=True;
    if (Source==None || Source.Mesh==None) return false;
    SourceWeapon=Source; LinkMesh(Source.Mesh); Skins=Source.Skins;
    bUnlit=Source.bUnlit; AmbientGlow=Source.AmbientGlow;
    SetLocation(vect(0,0,0)); SetRotation(rot(0,0,0));
    SetDrawScale(1.0); SetDrawScale3D(vect(1,1,1));
    if (!HasAnim('Idle')) return false;
    PlayAnim('Idle',0.0,0.0); SetAnimFrame(0.0); StopAnimating(); BoneRefresh();
    Palm=GetBoneCoords('Hands_RArmPalm'); Tip=GetBoneCoords('tip');
    PalmLocal=Palm.Origin; TipLocal=Tip.Origin;
    bCalibrated=VSize(TipLocal-PalmLocal)>20.0 && VSize(TipLocal-PalmLocal)<150.0;
    if (!bCalibrated) return false;
    // Presentation scale is provisional: the FP mesh is roughly 300 asset units/m.
    // This is independent of the world's explicit metres-to-Unreal conversion.
    SetBoneScale(0,0.0,'Hands_LArmCollarbone');
    SetBoneScale(1,0.0,'Hands_RArmCollarbone');
    return true;
}
simulated function PoseAt(vector GripPosition, rotator GripRotation, float Units)
{
    SetDrawScale(Units/ModelUnitsPerMetre);
    SetRotation(GripRotation);
    SetLocation(GripPosition-((PalmLocal*DrawScale)>>GripRotation));
    bHidden=False; BoneRefresh();
}
defaultproperties
{
    ModelUnitsPerMetre=300.0
    DrawType=DT_Mesh
    Physics=PHYS_None
    RemoteRole=ROLE_None
    bCollideActors=False
    bCollideWorld=False
    bBlockActors=False
    bBlockPlayers=False
    bHidden=True
}
