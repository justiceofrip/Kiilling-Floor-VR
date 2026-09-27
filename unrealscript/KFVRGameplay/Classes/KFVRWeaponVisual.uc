// A world actor is required: the ordinary KF first-person overlay is not part of
// the independent stereo world views. Asset coordinates are measured at runtime.
class KFVRWeaponVisual extends Actor;
var Weapon SourceWeapon;
var vector PalmLocal, TipLocal, MagazineLocal, SlideLocal;
var float ModelUnitsPerMetre, BoneTranslationScale;
var vector MagazineX,MagazineY,MagazineZ,SupportLocal;
var bool bCalibrated,bTranslationCalibrated,bTranslationProbe;
simulated function bool Initialize(Weapon Source)
{
    local coords Palm,Tip,Part;
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
    Part=GetBoneCoords('Hands_LArmPalm'); SupportLocal=Part.Origin;
    if (KFVRTrackedSingle(Source)!=None)
    {
        Part=GetBoneCoords('9mm_Mag'); MagazineLocal=Part.Origin;
        MagazineX=Normal(Part.XAxis); MagazineY=Normal(Part.YAxis); MagazineZ=Normal(Part.ZAxis);
        Part=GetBoneCoords('9mm_slide'); SlideLocal=Part.Origin;

    }
    bCalibrated=VSize(PalmLocal)>1.0 && VSize(TipLocal)>1.0 && VSize(TipLocal-PalmLocal)>20.0 && VSize(TipLocal-PalmLocal)<400.0;
    Log("KFVR_WEAPON_CAL weapon=" $ Source.Class $ " palm=" $ PalmLocal $ " tip=" $ TipLocal $ " valid=" $ bCalibrated);
    if (!bCalibrated) return false;
    // Presentation scale is provisional: the FP mesh is roughly 300 asset units/m.
    // This is independent of the world's explicit metres-to-Unreal conversion.
    SetBoneScale(0,0.0,'Hands_LArmCollarbone');
    SetBoneScale(1,0.0,'Hands_RArmCollarbone');
    return true;
}
simulated function PoseAt(vector GripPosition, rotator GripRotation, float Units)
{
    local coords Part;
    local float Measured;
    SetDrawScale(Units/ModelUnitsPerMetre);
    SetRotation(GripRotation);
    SetLocation(GripPosition-((PalmLocal*DrawScale)>>GripRotation));
    bHidden=False; BoneRefresh();
    // BoneRefresh does not invalidate the same-frame animation cache. Measure
    // a unit translation on the following ordinary Tick, after it has rendered.
    if (KFVRTrackedSingle(SourceWeapon)!=None && !bTranslationCalibrated)
    {
        if (bTranslationProbe)
        {
            Part=GetBoneCoords('9mm_Mag'); Measured=VSize(Part.Origin-MagazineRest())/DrawScale;
            if (Measured>0.01 && Measured<100)
            {
                BoneTranslationScale=Measured; bTranslationCalibrated=True;
                SetBoneLocation('9mm_Mag',vect(0,0,0),1.0);
            }
        }
        else
        { SetBoneLocation('9mm_Mag',vect(1,0,0),1.0); bTranslationProbe=True; }
    }
}
simulated function vector MagazineRest() { return Location+((MagazineLocal*DrawScale)>>Rotation); }
simulated function vector SlideRest() { return Location+((SlideLocal*DrawScale)>>Rotation); }
simulated function PoseMagazine(bool Inserted, bool Held, vector HandPosition, float SlideTravel, optional rotator HandRotation)
{
    local vector Offset;
    local rotator Desired;
    SetBoneScale(2,float(Inserted || Held),'9mm_Mag');
    if (Held) Offset=((HandPosition-Location)<<Rotation)/DrawScale-MagazineLocal;
    if (!Held) HandRotation=Rotation;
    Desired=OrthoRotation(MagazineX>>HandRotation,MagazineY>>HandRotation,MagazineZ>>HandRotation);
    SetBoneDirection('9mm_Mag',Desired,vect(0,0,0),1.0,1);
    SetBoneLocation('9mm_Mag',Offset/BoneTranslationScale,1.0);
    SetBoneLocation('9mm_slide',vect(-15,0,0)*SlideTravel/BoneTranslationScale,1.0);
    BoneRefresh();
}
defaultproperties
{
    ModelUnitsPerMetre=300.0
    BoneTranslationScale=1.0
    DrawType=DT_Mesh
    Physics=PHYS_None
    RemoteRole=ROLE_None
    bCollideActors=False
    bCollideWorld=False
    bBlockActors=False
    bBlockPlayers=False
    bHidden=True
}
