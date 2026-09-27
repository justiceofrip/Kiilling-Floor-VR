// A world actor is required: the ordinary KF first-person overlay is not part of
// the independent stereo world views. Asset coordinates are measured at runtime.
class KFVRWeaponVisual extends Actor;
var Weapon SourceWeapon;
var vector BladeAxis,ToolAxis;
var bool bOffhandGun;
var name TipBone;
var vector PalmLocal, TipLocal, MagazineLocal, SlideLocal;
var float ModelUnitsPerMetre, BoneTranslationScale;
var vector MagazineX,MagazineY,MagazineZ,SupportLocal;
var bool bCalibrated,bTranslationCalibrated,bTranslationProbe,bPhysicalProfile;
var name MagazineBone,ActionBone;
var float ActionTravelMetres;
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
    TipBone='tip';
    if (KFVRTrackedKnife(Source)!=None) TipBone='knife';
    if (KFVRTrackedSyringe(Source)!=None) TipBone='Syringe';
    if (KFVRTrackedWelder(Source)!=None) TipBone='flame_tip';
    if (KFVRTrackedDualies(Source)!=None) { if (bOffhandGun) TipBone='Tip_Left'; else TipBone='Tip_Right'; }
    Palm=GetBoneCoords('Hands_RArmPalm'); Tip=GetBoneCoords(TipBone);
    if (bOffhandGun) Palm=GetBoneCoords('Hands_LArmPalm');
    PalmLocal=Palm.Origin; TipLocal=Tip.Origin;
    if (KFVRTrackedKnife(Source)!=None)
    {
        BladeAxis=Normal(Tip.ZAxis);
        // Authored provisional 20 cm blade contact endpoint from the real knife bone.
        // The shipped knife has no independent tip bone; visual/wearer tuning remains.
        TipLocal+=BladeAxis*60.0;
        Log("KFVR_KNIFE_CAL bone=" $ Tip.Origin $ " x=" $ Tip.XAxis $ " y=" $ Tip.YAxis $ " z=" $ Tip.ZAxis);
    }
    if (KFVRTrackedSyringe(Source)!=None)
    {
        // The installed mesh has no needle-tip bone. This authored endpoint is
        // provisional until measured against the rendered needle and wearer grip.
        ToolAxis=Normal(Tip.XAxis); TipLocal+=ToolAxis*45.0;
        SetBoneScale(3,0.0,'Syringe Capsule');
    }
    if (KFVRTrackedWelder(Source)!=None)
    { Part=GetBoneCoords('flame_base'); ToolAxis=Normal(Tip.Origin-Part.Origin); }
    Part=GetBoneCoords('Hands_LArmPalm'); SupportLocal=Part.Origin;
    MagazineBone=class'KFVRWeaponProfile'.static.MagazineBone(KFWeapon(Source));
    ActionBone=class'KFVRWeaponProfile'.static.ActionBone(KFWeapon(Source));
    ActionTravelMetres=class'KFVRWeaponProfile'.static.ActionTravel(KFWeapon(Source));
    if (KFVRTrackedDualies(Source)!=None && bOffhandGun) { MagazineBone='9mm_Mag01'; ActionBone='9mm_slide01'; }
    if (MagazineBone!='' && ActionBone!='')
    {
        Part=GetBoneCoords(MagazineBone); MagazineLocal=Part.Origin;
        MagazineX=Normal(Part.XAxis); MagazineY=Normal(Part.YAxis); MagazineZ=Normal(Part.ZAxis);
        Part=GetBoneCoords(ActionBone); SlideLocal=Part.Origin;
        bPhysicalProfile=VSize(MagazineLocal)>1 && VSize(SlideLocal)>1 && VSize(MagazineLocal-PalmLocal)<300 && VSize(SlideLocal-PalmLocal)<300;
        Log("KFVR_MECHANICAL_CAL weapon=" $ Source.Class $ " magazine=" $ MagazineBone $ ":" $ MagazineLocal $ " action=" $ ActionBone $ ":" $ SlideLocal $ " valid=" $ bPhysicalProfile);

    }
    bCalibrated=VSize(PalmLocal)>1.0 && VSize(TipLocal)>1.0 && VSize(TipLocal-PalmLocal)>20.0 && VSize(TipLocal-PalmLocal)<400.0;
    Log("KFVR_WEAPON_CAL weapon=" $ Source.Class $ " palm=" $ PalmLocal $ " tip=" $ TipLocal $ " valid=" $ bCalibrated);
    if (!bCalibrated) return false;
    // Presentation scale is provisional: the FP mesh is roughly 300 asset units/m.
    // This is independent of the world's explicit metres-to-Unreal conversion.
    SetBoneScale(0,0.0,'Hands_LArmCollarbone');
    SetBoneScale(1,0.0,'Hands_RArmCollarbone');
    if (KFVRTrackedDualies(Source)!=None)
    { if (bOffhandGun) SetBoneScale(3,0.0,'9mm_Gun'); else SetBoneScale(3,0.0,'9mm_Gun01'); }
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
    if (bPhysicalProfile && KFVRHandVisual(self)==None && !bTranslationCalibrated)
    {
        if (bTranslationProbe)
        {
            Part=GetBoneCoords(MagazineBone); Measured=VSize(Part.Origin-MagazineRest())/DrawScale;
            if (Measured>0.01 && Measured<100)
            {
                BoneTranslationScale=Measured; bTranslationCalibrated=True;
                SetBoneLocation(MagazineBone,vect(0,0,0),1.0);
            }
        }
        else
        { SetBoneLocation(MagazineBone,vect(1,0,0),1.0); bTranslationProbe=True; }
    }
}
simulated function vector MagazineRest() { return Location+((MagazineLocal*DrawScale)>>Rotation); }
simulated function vector SlideRest() { return Location+((SlideLocal*DrawScale)>>Rotation); }
simulated function PoseMagazine(bool Inserted, bool Held, vector HandPosition, float SlideTravel, optional rotator HandRotation)
{
    local vector Offset;
    local rotator Desired;
    SetBoneScale(2,float(Inserted || Held),MagazineBone);
    if (Held) Offset=((HandPosition-Location)<<Rotation)/DrawScale-MagazineLocal;
    if (!Held) HandRotation=Rotation;
    Desired=OrthoRotation(MagazineX>>HandRotation,MagazineY>>HandRotation,MagazineZ>>HandRotation);
    SetBoneDirection(MagazineBone,Desired,vect(0,0,0),1.0,1);
    SetBoneLocation(MagazineBone,Offset/BoneTranslationScale,1.0);
    SetBoneLocation(ActionBone,vect(-1,0,0)*ModelUnitsPerMetre*ActionTravelMetres*SlideTravel/BoneTranslationScale,1.0);
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
