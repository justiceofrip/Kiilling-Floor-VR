// Original sleeve-free gloves. Legacy arms remain an explicit developer experiment.
class KFVRHandVisual extends KFVRWeaponVisual;
#exec MESH MODELIMPORT MESH=KFVRGloveRight MODELFILE=Models\KFVRGloveRight.psk
#exec MESH ORIGIN MESH=KFVRGloveRight X=0 Y=0 Z=0 YAW=0 PITCH=0 ROLL=0
#exec MESH SCALE MESH=KFVRGloveRight X=1 Y=1 Z=1
#exec ANIM IMPORT ANIM=KFVRGloveRightAnim ANIMFILE=Models\KFVRGloveRight.psa IMPORTSEQS=1
#exec ANIM DIGEST ANIM=KFVRGloveRightAnim VERBOSE USERAWINFO
#exec MESH DEFAULTANIM MESH=KFVRGloveRight ANIM=KFVRGloveRightAnim
#exec MESH MODELIMPORT MESH=KFVRGloveLeft MODELFILE=Models\KFVRGloveLeft.psk
#exec MESH ORIGIN MESH=KFVRGloveLeft X=0 Y=0 Z=0 YAW=0 PITCH=0 ROLL=0
#exec MESH SCALE MESH=KFVRGloveLeft X=1 Y=1 Z=1
#exec ANIM IMPORT ANIM=KFVRGloveLeftAnim ANIMFILE=Models\KFVRGloveLeft.psa IMPORTSEQS=1
#exec ANIM DIGEST ANIM=KFVRGloveLeftAnim VERBOSE USERAWINFO
#exec MESH DEFAULTANIM MESH=KFVRGloveLeft ANIM=KFVRGloveLeftAnim
var bool bStandaloneGlove;
var ConstantColor GloveMaterial,PadMaterial;
var vector PalmForward,PalmRight,PalmUp,IndexDirection;
var float PalmAxisError,IndexAxisError;
var vector ExpectedIndexDirection;
var bool bLeftHand;
var name PalmBone;
var name FingerBones[15];
var vector PalmWorld, IndexTipWorld;
var float ThumbCurl, IndexCurl, GripCurl, PalmError;
var int PoseFrames,GripRole;
var float InputThumbCurl,InputIndexCurl,InputGripCurl;
var name UpperArmBone,ElbowBone,WristHelperBone,SleeveBone;
var vector WristFrameX,WristFrameY,WristFrameZ,SleeveFrameX,SleeveFrameY,SleeveFrameZ;
var vector UpperArmLocal,ElbowLocal,PalmAxisX,PalmAxisY,PalmAxisZ;
var vector UpperFrameX,UpperFrameY,UpperFrameZ,LowerFrameX,LowerFrameY,LowerFrameZ;
var vector ArmTranslationX,ArmTranslationY,ArmTranslationZ;
var vector ArmAnchor,ArmShoulder,ArmElbow,ArmWrist,MeasuredShoulder,MeasuredElbow;
var float UpperArmLength,LowerArmLength,ArmTranslationScale,ArmShoulderError,ArmElbowError,ArmWristError;
var float UpperAxisDot,LowerAxisDot,ArmAnchorShift,ArmBasisDeterminant;
var int ArmFallbacks;
var float UpperPlaneY,UpperPlaneZ,LowerPlaneY,LowerPlaneZ;
var int ArmCalibrationStage,ArmFrames;
var bool bArmValid,bArmSolved;
simulated function bool InitializeHand(Weapon Source, bool LeftHand)
{
    local coords C,U,E;
    local vector Plane;
    bLeftHand=LeftHand;
    if (KFVRWeaponRig(Owner)==None || !KFVRWeaponRig(Owner).bArmTest)
        return InitializeGlove(Source);
    if (!Initialize(Source)) return false;
    SetBoneScale(0,1.0,'Hands_LArmCollarbone');
    SetBoneScale(1,1.0,'Hands_RArmCollarbone');
    SetBoneScale(2,0.0,'9mm_Gun');
    if (KFVRTrackedDualies(Source)!=None) SetBoneScale(3,0.0,'9mm_Gun01');
    if (LeftHand) { PalmBone='Hands_LArmPalm'; SetBoneScale(1,0.0,'Hands_RArmCollarbone'); }
    else { PalmBone='Hands_RArmPalm'; SetBoneScale(0,0.0,'Hands_LArmCollarbone'); }
    // Reset placement before capturing the other palm's authored offset.
    SetLocation(vect(0,0,0)); SetRotation(rot(0,0,0)); SetDrawScale(1.0); BoneRefresh();
    C=GetBoneCoords(PalmBone); PalmLocal=C.Origin;
    if (LeftHand)
    {
        FingerBones[0]='Hands_LArmThumb1';
        FingerBones[1]='Hands_LArmThumb2';
        FingerBones[2]='Hands_LArmThumb3';
        FingerBones[3]='Hands_LArmFinger11';
        FingerBones[4]='Hands_LArmFinger12';
        FingerBones[5]='Hands_LArmFinger13';
        FingerBones[6]='Hands_LArmFinger21';
        FingerBones[7]='Hands_LArmFinger22';
        FingerBones[8]='Hands_LArmFinger23';
        FingerBones[9]='Hands_LArmFinger31';
        FingerBones[10]='Hands_LArmFinger32';
        FingerBones[11]='Hands_LArmFinger33';
        FingerBones[12]='Hands_LArmFinger41';
        FingerBones[13]='Hands_LArmFinger42';
        FingerBones[14]='Hands_LArmFinger43';
    }
    else
    {
        FingerBones[0]='Hands_RArmThumb1';
        FingerBones[1]='Hands_RArmThumb2';
        FingerBones[2]='Hands_RArmThumb3';
        FingerBones[3]='Hands_RArmFinger11';
        FingerBones[4]='Hands_RArmFinger12';
        FingerBones[5]='Hands_RArmFinger13';
        FingerBones[6]='Hands_RArmFinger21';
        FingerBones[7]='Hands_RArmFinger22';
        FingerBones[8]='Hands_RArmFinger23';
        FingerBones[9]='Hands_RArmFinger31';
        FingerBones[10]='Hands_RArmFinger32';
        FingerBones[11]='Hands_RArmFinger33';
        FingerBones[12]='Hands_RArmFinger41';
        FingerBones[13]='Hands_RArmFinger42';
        FingerBones[14]='Hands_RArmFinger43';
    }
    UpperArmBone='Hands_RArm1'; ElbowBone='Hands_RArm2';
    if (LeftHand) { UpperArmBone='Hands_LArm1'; ElbowBone='Hands_LArm2'; }
    U=GetBoneCoords(UpperArmBone); E=GetBoneCoords(ElbowBone); C=GetBoneCoords(PalmBone);
    UpperArmLocal=U.Origin; ElbowLocal=E.Origin;
    UpperFrameX=U.XAxis; UpperFrameY=U.YAxis; UpperFrameZ=U.ZAxis;
    LowerFrameX=E.XAxis; LowerFrameY=E.YAxis; LowerFrameZ=E.ZAxis;
    Plane=Normal((E.Origin-U.Origin) cross (C.Origin-E.Origin));
    UpperPlaneY=Plane dot U.YAxis; UpperPlaneZ=Plane dot U.ZAxis;
    LowerPlaneY=Plane dot E.YAxis; LowerPlaneZ=Plane dot E.ZAxis;
    PalmAxisX=C.XAxis; PalmAxisY=C.YAxis; PalmAxisZ=C.ZAxis;
    UpperArmLength=VSize(E.Origin-U.Origin); LowerArmLength=VSize(C.Origin-E.Origin);
    UpperAxisDot=Normal(E.Origin-U.Origin) dot U.XAxis; LowerAxisDot=Normal(C.Origin-E.Origin) dot E.XAxis;
    bArmValid=UpperArmLength>10 && UpperArmLength<200 && LowerArmLength>10 && LowerArmLength<200 && UpperAxisDot>0.99 && LowerAxisDot>0.99;
    WristHelperBone='Hands_R_wrist'; SleeveBone='Hands_R_Sleeve';
    if (LeftHand) { WristHelperBone='Hands_Lwrist'; SleeveBone='Hands_LSleeve'; }
    C=GetBoneCoords(WristHelperBone); WristFrameX=C.XAxis; WristFrameY=C.YAxis; WristFrameZ=C.ZAxis;
    C=GetBoneCoords(SleeveBone); SleeveFrameX=C.XAxis; SleeveFrameY=C.YAxis; SleeveFrameZ=C.ZAxis;
    C=GetBoneCoords(FingerBones[3]);
    Log("KFVR_HAND_CAL side=" $ LeftHand $ " palm=" $ PalmLocal $ " index=" $ C.Origin $ " X=" $ C.XAxis $ " Y=" $ C.YAxis $ " Z=" $ C.ZAxis);
    return VSize(PalmLocal)>1.0;
}
// Mesh origin and controller grip origin are identical; no borrowed FP offset.
simulated function bool InitializeGlove(Weapon Source)
{
    local coords C;
    SourceWeapon=Source; bStandaloneGlove=True; ModelUnitsPerMetre=100.0;
    if (bLeftHand) LinkMesh(SkeletalMesh'KFVRGloveLeft');
    else LinkMesh(SkeletalMesh'KFVRGloveRight');
    SetLocation(vect(0,0,0)); SetRotation(rot(0,0,0)); SetDrawScale(1.0);
    SetDrawScale3D(vect(1,1,1));
    if (!HasAnim('Idle')) { Log("KFVR_GLOVE_ERROR missing Idle"); return False; }
    PlayAnim('Idle',0.0,0.0); SetAnimFrame(0.0); StopAnimating(); BoneRefresh();
    PalmBone='Palm'; C=GetBoneCoords(PalmBone); PalmLocal=C.Origin;
    FingerBones[0]='Thumb1'; FingerBones[1]='Thumb2'; FingerBones[2]='Thumb3';
    FingerBones[3]='Index1'; FingerBones[4]='Index2'; FingerBones[5]='Index3';
    FingerBones[6]='Middle1'; FingerBones[7]='Middle2'; FingerBones[8]='Middle3';
    FingerBones[9]='Ring1'; FingerBones[10]='Ring2'; FingerBones[11]='Ring3';
    FingerBones[12]='Little1'; FingerBones[13]='Little2'; FingerBones[14]='Little3';
    GloveMaterial=new(None) class'ConstantColor';
    GloveMaterial.Color.R=115; GloveMaterial.Color.G=124; GloveMaterial.Color.B=118; GloveMaterial.Color.A=255;
    PadMaterial=new(None) class'ConstantColor';
    PadMaterial.Color.R=47; PadMaterial.Color.G=52; PadMaterial.Color.B=49; PadMaterial.Color.A=255;
    Skins[0]=GloveMaterial; Skins[1]=PadMaterial; bUnlit=False; AmbientGlow=48;
    bCalibrated=VSize(PalmLocal)<0.01;
    Log("KFVR_GLOVE_CAL left=" $ bLeftHand $ " palm=" $ PalmLocal $ " X=" $ C.XAxis $ " Y=" $ C.YAxis $ " Z=" $ C.ZAxis);
    return bCalibrated;
}
simulated function PoseGlove(vector Position,rotator HandRotation,float Units)
{
    local int Finger,Joint;
    local float Curl,Angle,Sign;
    local vector X,Y,Z,DesiredX,DesiredY,DesiredZ;
    local coords C;
    PoseAt(Position,HandRotation,Units);
    GetAxes(HandRotation,X,Y,Z); Sign=1; if (bLeftHand) Sign=-1;
    for (Finger=0;Finger<5;Finger++)
    {
        if (Finger==0) Curl=ThumbCurl;
        else if (Finger==1) Curl=IndexCurl;
        else Curl=GripCurl;
        for (Joint=0;Joint<3;Joint++)
        {
            if (Joint==0) Angle=1.3*Curl;
            else if (Joint==1) Angle=2.8*Curl;
            else Angle=3.7*Curl;
            DesiredX=X; DesiredY=Y*Cos(Angle)+Z*(Sign*Sin(Angle));
            DesiredZ=Z*Cos(Angle)-Y*(Sign*Sin(Angle));
            if (Finger==0)
            {
                // Oppose the thumb across the top of the grasp, progressively.
                DesiredZ=Normal(X*(0.85-0.95*Curl)+Z*(0.50-0.10*Curl)-Y*Sign*(0.15+0.65*Curl));
                DesiredY=Normal(Y-DesiredZ*(Y dot DesiredZ));
                DesiredX=Normal(DesiredY cross DesiredZ);
            }
            SetBoneDirection(FingerBones[Finger*3+Joint],OrthoRotation(DesiredX,DesiredY,DesiredZ),,1.0,1);
            if (Finger==1 && Joint==2) ExpectedIndexDirection=DesiredZ;
        }
    }
    BoneRefresh(); C=GetBoneCoords(PalmBone);
    PalmWorld=C.Origin; PalmError=VSize(PalmWorld-Position);
    PalmForward=C.XAxis; PalmRight=C.YAxis; PalmUp=C.ZAxis;
    PalmAxisError=FMax(VSize(C.XAxis-X),FMax(VSize(C.YAxis-Y),VSize(C.ZAxis-Z)));
    C=GetBoneCoords(FingerBones[5]); IndexTipWorld=C.Origin; IndexDirection=C.ZAxis;
    IndexAxisError=VSize(C.ZAxis-ExpectedIndexDirection); PoseFrames++;
}
simulated function ClearArmPose()
{
    SetBoneDirection(UpperArmBone,rot(0,0,0),,0.0,1);
    SetBoneLocation(UpperArmBone,vect(0,0,0),0.0);
    SetBoneDirection(ElbowBone,rot(0,0,0),,0.0,1);
    SetBoneDirection(PalmBone,rot(0,0,0),,0.0,1);
    SetBoneDirection(WristHelperBone,rot(0,0,0),,0.0,1);
    SetBoneDirection(SleeveBone,rot(0,0,0),,0.0,1);
}
simulated function PoseArm(vector Position,rotator HandRotation,float Units)
{
    local KFVRWeaponRig R;
    local coords C;
    local vector X,Y,Z,Pole,Upper,Lower,Offset,Probe,Plane;
    local rotator UpperTurn,LowerTurn;
    local float Sign;
    R=KFVRWeaponRig(Owner);
    if (R==None || !R.bArmTest || !bArmValid) return;
    if (ArmCalibrationStage==0)
    { SetBoneLocation(UpperArmBone,vect(1,0,0),1.0); ArmCalibrationStage=1; return; }
    if (ArmCalibrationStage<4)
    {
        C=GetBoneCoords(UpperArmBone); Probe=((C.Origin-Location)<<Rotation)/DrawScale-UpperArmLocal;
        ArmTranslationScale=VSize(Probe);
        if (ArmTranslationScale<0.01 || ArmTranslationScale>100) { bArmValid=False; return; }
        if (ArmCalibrationStage==1) { ArmTranslationX=Probe; SetBoneLocation(UpperArmBone,vect(0,1,0),1.0); }
        if (ArmCalibrationStage==2) { ArmTranslationY=Probe; SetBoneLocation(UpperArmBone,vect(0,0,1),1.0); }
        if (ArmCalibrationStage==3) { ArmTranslationZ=Probe; SetBoneLocation(UpperArmBone,vect(0,0,0),1.0); }
        ArmCalibrationStage++;
        if (ArmCalibrationStage==4)
        {
            ArmBasisDeterminant=ArmTranslationX dot (ArmTranslationY cross ArmTranslationZ);
            bArmValid=Abs(ArmBasisDeterminant)>0.01;
            if (!bArmValid) ClearArmPose();
        }
        return;
    }
    GetAxes(R.Tracking.BodyRotation,X,Y,Z); Sign=1; if (bLeftHand) Sign=-1;
    ArmAnchor=R.Tracking.HeadPosition+(Y*Sign*0.18-Z*0.22-X*0.03)*Units;
    // Keep the authored collar/sleeve root beside the body. Placing the whole
    // mesh at the controller stretches vertices weighted above the shoulder.
    SetRotation(R.Tracking.BodyRotation);
    SetLocation(ArmAnchor-((UpperArmLocal*DrawScale)>>Rotation));
    Pole=Y*Sign*0.55-Z*0.75-X*0.35; ArmWrist=Position;
    bArmSolved=class'KFVRArmMath'.static.Solve(ArmAnchor/Units,Position/Units,Pole,UpperArmLength/ModelUnitsPerMetre,LowerArmLength/ModelUnitsPerMetre,ArmShoulder,ArmElbow);
    ArmShoulder*=Units; ArmElbow*=Units;
    ArmAnchorShift=VSize(ArmShoulder-ArmAnchor)/Units;
    if (!bArmSolved)
    {
        // Extreme/unreachable tracking must never detach the interactive palm.
        ClearArmPose(); PoseAt(Position,HandRotation,Units); ArmFallbacks++; return;
    }
    Upper=Normal(ArmElbow-ArmShoulder); Lower=Normal(Position-ArmElbow);
    Plane=Normal(Upper cross Lower);
    UpperTurn=class'KFVRArmMath'.static.PlaneFrame(Upper,Plane,UpperPlaneY,UpperPlaneZ);
    LowerTurn=class'KFVRArmMath'.static.PlaneFrame(Lower,Plane,LowerPlaneY,LowerPlaneZ);
    SetBoneDirection(UpperArmBone,UpperTurn,,1.0,1);
    Offset=((ArmShoulder-Location)<<Rotation)/DrawScale-UpperArmLocal;
    // SetBoneLocation uses the parent bone frame. Its measured response basis
    // includes the mesh import scale; root-weapon translation axes do not apply.
    Probe.X=(Offset dot (ArmTranslationY cross ArmTranslationZ))/ArmBasisDeterminant;
    Probe.Y=(Offset dot (ArmTranslationZ cross ArmTranslationX))/ArmBasisDeterminant;
    Probe.Z=(Offset dot (ArmTranslationX cross ArmTranslationY))/ArmBasisDeterminant;
    SetBoneLocation(UpperArmBone,Probe,1.0);
    SetBoneDirection(ElbowBone,LowerTurn,,1.0,1);
    SetBoneDirection(PalmBone,OrthoRotation(PalmAxisX>>HandRotation,PalmAxisY>>HandRotation,PalmAxisZ>>HandRotation),,1.0,1);
    // Cuff and wrist helpers are siblings of the palm, not its children.
    // Give them the same authored controller twist to avoid a corkscrewed cuff.
    SetBoneDirection(WristHelperBone,OrthoRotation(WristFrameX>>HandRotation,WristFrameY>>HandRotation,WristFrameZ>>HandRotation),,1.0,1);
    SetBoneDirection(SleeveBone,OrthoRotation(SleeveFrameX>>HandRotation,SleeveFrameY>>HandRotation,SleeveFrameZ>>HandRotation),,1.0,1);
    C=GetBoneCoords(UpperArmBone); MeasuredShoulder=C.Origin; ArmShoulderError=VSize(C.Origin-ArmShoulder);
    C=GetBoneCoords(ElbowBone); MeasuredElbow=C.Origin; ArmElbowError=VSize(C.Origin-ArmElbow);
    C=GetBoneCoords(PalmBone); ArmWristError=VSize(C.Origin-Position); ArmFrames++;
}
simulated function PoseHand(vector Position, rotator HandRotation, float Units, float Thumb, float Index, float Grip, optional int GripKind)
{
    local rotator Bend;
    local int Finger,Joint;
    local float Curl,Angle,Spread;
    local vector Forward,Side,Up;
    local coords C,Palm;
    local KFVRWeaponRig R;
    ThumbCurl=FClamp(Thumb,0.0,1.0); IndexCurl=FClamp(Index,0.0,1.0); GripCurl=FClamp(Grip,0.0,1.0);
    InputThumbCurl=ThumbCurl; InputIndexCurl=IndexCurl; InputGripCurl=GripCurl;
    GripRole=Clamp(GripKind,0,3);
    // A latched weapon remains held when the physical grip button is released.
    // Thumb/index capacitance still layers onto its resting grasp.
    if (GripRole>0) { GripCurl=FMax(GripCurl,0.72); ThumbCurl=FMax(ThumbCurl,0.35); }
    if (GripRole==2) IndexCurl=FMax(IndexCurl,0.75);
    if (GripRole==3) IndexCurl=FMax(IndexCurl,0.45);
    if (bStandaloneGlove) { PoseGlove(Position,HandRotation,Units); return; }
    R=KFVRWeaponRig(Owner);
    if (R!=None && R.bArmTest && bArmValid && ArmCalibrationStage>=4)
    { SetDrawScale(Units/ModelUnitsPerMetre); bHidden=False; }
    else PoseAt(Position,HandRotation,Units);
    PoseArm(Position,HandRotation,Units);
    Palm=GetBoneCoords(PalmBone);
    for (Finger=0;Finger<5;Finger++)
    {
        if (Finger==0) Curl=ThumbCurl;
        else if (Finger==1) Curl=IndexCurl;
        else Curl=GripCurl;
        for (Joint=0;Joint<3;Joint++)
        {
            if (Joint==0) Angle=0.35*Curl;
            else if (Joint==1) Angle=1.2*Curl;
            else Angle=1.9*Curl;
            Spread=(2.0-Finger)*0.08;
            Forward=Normal(Palm.XAxis*Cos(Angle)+Palm.ZAxis*Sin(Angle)+Palm.YAxis*Spread);
            if (Finger==0) Forward=Normal(Palm.XAxis*(0.45+0.35*Curl)+Palm.YAxis*(0.8-0.4*Curl)+Palm.ZAxis*(0.2+0.35*Curl));
            Side=Normal(Palm.YAxis-Forward*(Palm.YAxis Dot Forward)); Up=Normal(Forward Cross Side);
            Bend=OrthoRotation(Forward,Side,Up);
            SetBoneDirection(FingerBones[Finger*3+Joint],Bend,,1.0,1);
        }
    }
    BoneRefresh();
    C=GetBoneCoords(PalmBone); PalmWorld=C.Origin; PalmError=VSize(PalmWorld-Position);
    C=GetBoneCoords(FingerBones[5]); IndexTipWorld=C.Origin; PoseFrames++;

}
