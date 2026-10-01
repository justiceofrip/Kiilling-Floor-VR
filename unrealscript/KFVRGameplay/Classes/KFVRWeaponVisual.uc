// A world actor is required: the ordinary KF first-person overlay is not part of
// the independent stereo world views. Asset coordinates are measured at runtime.
class KFVRWeaponVisual extends Actor;
var Weapon SourceWeapon;
var KFVRMagazineGrip MagazineGrip;
var coords GripPose[32]; // Left palm + 15 joints, then right palm + 15 joints.
var bool bLeftGripPose,bRightGripPose;
var vector BladeAxis,ToolAxis;
var bool bToolAligned;
var vector ToolFrameX,ToolFrameY,ToolFrameZ,MeasuredToolAxis;
var bool bOffhandGun;
var name TipBone;
var vector PalmLocal, TipLocal, MagazineLocal, SlideLocal;
var float ModelUnitsPerMetre, BoneTranslationScale;
var vector MagazineX,MagazineY,MagazineZ,SupportLocal;
var bool bCalibrated,bTranslationCalibrated,bTranslationProbe,bPhysicalProfile;
var name MagazineBone,ActionBone;
var float ActionTravelMetres;
simulated function bool CaptureGripPose(bool Left)
{
    local int I,GripBase;
    local coords C,P;
    if (!Left) GripBase=16;
    P=GetBoneCoords(class'KFVRHandPoseMath'.static.SourceBone(Left,0));
    if (VSize(P.Origin)<1) return False;
    for (I=0;I<16;I++)
    {
        C=GetBoneCoords(class'KFVRHandPoseMath'.static.SourceBone(Left,I));
        if (I>0 && (VSize(C.Origin-P.Origin)<0.1 || VSize(C.Origin-P.Origin)>100)) return False;
        if (Abs(VSize(C.XAxis)-1)>0.01 || Abs(C.XAxis dot C.YAxis)>0.01) return False;
        GripPose[GripBase+I]=C;
    }
    return True;
}
simulated function coords GripBone(int Index) { return GripPose[Clamp(Index,0,31)]; }
simulated function bool Initialize(Weapon Source)
{
    local coords Palm,Tip,Part;
    local name PoseSequence;
    bHidden=True;
    if (Source==None || Source.Mesh==None) return false;
    SourceWeapon=Source; LinkMesh(Source.Mesh); Skins=Source.Skins;
    bUnlit=Source.bUnlit; AmbientGlow=Source.AmbientGlow;
    SetLocation(vect(0,0,0)); SetRotation(rot(0,0,0));
    SetDrawScale(1.0); SetDrawScale3D(vect(1,1,1));
    PoseSequence='Idle';
    if (Source.Class==class'Frag') PoseSequence=Frag(Source).TossAnim;
    if (!HasAnim(PoseSequence)) return false;
    PlayAnim(PoseSequence,0.0,0.0); SetAnimFrame(0.0); StopAnimating(); BoneRefresh();
    TipBone='tip';
    if (Source.Class==class'Frag') TipBone='Frag';
    if (KFVRTrackedKnife(Source)!=None) TipBone='knife';
    if (KFVRTrackedSyringe(Source)!=None) TipBone='Syringe';
    if (KFVRTrackedWelder(Source)!=None) TipBone='flame_tip';
    if (KFVRTrackedDualies(Source)!=None) { if (bOffhandGun) TipBone='Tip_Left'; else TipBone='Tip_Right'; }
    Palm=GetBoneCoords('Hands_RArmPalm'); Tip=GetBoneCoords(TipBone);
    // KF's toss animation begins with the grenade in the authored left palm.
    // The world proxy may attach that measured offset to either VR controller.
    if (bOffhandGun || Source.Class==class'Frag') Palm=GetBoneCoords('Hands_LArmPalm');
    PalmLocal=Palm.Origin; TipLocal=Tip.Origin;
    // Capture the installed idle grasp before hiding the stock arm hierarchy.
    bLeftGripPose=CaptureGripPose(True); bRightGripPose=CaptureGripPose(False);
    // Toss starts in the left hand facing back toward the player. Bring its
    // grenade centre forward into the right controller grasp (provisional 4 cm).
    if (Source.Class==class'Frag') PalmLocal=TipLocal-vect(12,0,0);
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
    if (KFVRTrackedSyringe(Source)!=None || KFVRTrackedWelder(Source)!=None)
    {
        // These tools were authored pointing up in the first-person idle pose.
        // Measure their working frame, then attach that frame to controller aim.
        ToolFrameX=Normal(ToolAxis);
        ToolFrameY=Normal(vect(0,1,0)-ToolFrameX*ToolFrameX.Y);
        if (VSize(ToolFrameX)<0.9 || VSize(ToolFrameY)<0.9) return False;
        ToolFrameZ=Normal(ToolFrameX cross ToolFrameY); bToolAligned=True;
    }
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
    if (Source.Class==class'Frag') bCalibrated=VSize(PalmLocal)>1 && VSize(TipLocal)>1 && VSize(TipLocal-PalmLocal)<150;
    Log("KFVR_WEAPON_CAL weapon=" $ Source.Class $ " palm=" $ PalmLocal $ " tip=" $ TipLocal $ " valid=" $ bCalibrated);
    if (!bCalibrated) return false;
    // Presentation scale is provisional: the FP mesh is roughly 300 asset units/m.
    // This is independent of the world's explicit metres-to-Unreal conversion.
    SetBoneScale(0,0.0,'Hands_LArmCollarbone');
    SetBoneScale(1,0.0,'Hands_RArmCollarbone');
    if (KFVRTrackedDualies(Source)!=None)
    { if (bOffhandGun) SetBoneScale(3,0.0,'9mm_Gun'); else SetBoneScale(3,0.0,'9mm_Gun01'); }
    if (bPhysicalProfile && KFVRWeaponRig(Owner)!=None && KFVRWeaponRig(Owner).bNativeHandTest
        && (KFVRTrackedSingle(Source)!=None || KFVRTrackedDeagle(Source)!=None))
    {
        MagazineGrip=Spawn(class'KFVRMagazineGrip',self);
        if (MagazineGrip!=None && !MagazineGrip.InitializeMagazine(self))
        { Log("KFVR_MAGAZINE_GRIP_ERROR weapon=" $ Source.Class); MagazineGrip.Destroy(); MagazineGrip=None; }
    }
    return true;
}
simulated function PoseAt(vector GripPosition, rotator GripRotation, float Units)
{
    local coords Part,Base;
    local float Measured;
    local vector X,Y,Z;
    local rotator ModelRotation;
    ModelRotation=GripRotation;
    if (bToolAligned)
    {
        GetAxes(GripRotation,X,Y,Z);
        // Inverse authored working frame followed by the current controller
        // frame. Rotate around the measured model grip, not the actor origin.
        ModelRotation=OrthoRotation(X*ToolFrameX.X+Y*ToolFrameY.X+Z*ToolFrameZ.X,
            X*ToolFrameX.Y+Y*ToolFrameY.Y+Z*ToolFrameZ.Y,
            X*ToolFrameX.Z+Y*ToolFrameY.Z+Z*ToolFrameZ.Z);
    }
    SetDrawScale(Units/ModelUnitsPerMetre);
    SetRotation(ModelRotation);
    SetLocation(GripPosition-((PalmLocal*DrawScale)>>ModelRotation));
    bHidden=False;
    // Physical reload queues magazine/action overrides next. Evaluating now
    // would cache last Tick's detached magazine through a sharp hand movement.
    // Keep the initial translation calibration on its established two-Tick path.
    if (bTranslationCalibrated && class'KFVRWeaponProfile'.static.Manual(KFWeapon(SourceWeapon))!=None) return;
    BoneRefresh();
    if (bToolAligned)
    {
        Part=GetBoneCoords(TipBone);
        if (KFVRTrackedWelder(SourceWeapon)!=None)
        { Base=GetBoneCoords('flame_base'); MeasuredToolAxis=Normal(Part.Origin-Base.Origin); }
        else MeasuredToolAxis=Normal(Part.XAxis);
    }
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
simulated function vector HeldMagazinePosition(vector HandPosition,rotator HandRotation)
{
    if (MagazineGrip!=None && MagazineGrip.bCalibrated)
        return HandPosition-((MagazineGrip.PalmLocal*DrawScale)>>HandRotation);
    return HandPosition;
}
simulated function Destroyed()
{
    if (MagazineGrip!=None) MagazineGrip.Destroy();
    Super.Destroyed();
}
simulated function PoseMagazine(bool Inserted, bool Held, vector HandPosition, float SlideTravel, optional rotator HandRotation)
{
    local vector Offset;
    local rotator Desired;
    SetBoneScale(2,float(Inserted || Held),MagazineBone);
    if (MagazineGrip!=None)
    {
        MagazineGrip.bHidden=!Held;
        if (Held) MagazineGrip.PoseAt(HandPosition,HandRotation,DrawScale*ModelUnitsPerMetre);
    }
    if (Held) HandPosition=HeldMagazinePosition(HandPosition,HandRotation);
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
