// Reuses the installed 9mm hand mesh; no proprietary asset is shipped in this mod.
class KFVRHandVisual extends KFVRWeaponVisual;
var bool bLeftHand;
var name PalmBone;
var name FingerBones[15];
var vector PalmWorld, IndexTipWorld;
var float ThumbCurl, IndexCurl, GripCurl, PalmError;
var int PoseFrames,GripRole;
var float InputThumbCurl,InputIndexCurl,InputGripCurl;
simulated function bool InitializeHand(Weapon Source, bool LeftHand)
{
    local coords C;
    bLeftHand=LeftHand;
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
    C=GetBoneCoords(FingerBones[3]);
    Log("KFVR_HAND_CAL side=" $ LeftHand $ " palm=" $ PalmLocal $ " index=" $ C.Origin $ " X=" $ C.XAxis $ " Y=" $ C.YAxis $ " Z=" $ C.ZAxis);
    return VSize(PalmLocal)>1.0;
}
simulated function PoseHand(vector Position, rotator HandRotation, float Units, float Thumb, float Index, float Grip, optional int GripKind)
{
    local rotator Bend;
    local int Finger,Joint;
    local float Curl,Angle,Spread;
    local vector Forward,Side,Up;
    local coords C,Palm;
    ThumbCurl=FClamp(Thumb,0.0,1.0); IndexCurl=FClamp(Index,0.0,1.0); GripCurl=FClamp(Grip,0.0,1.0);
    InputThumbCurl=ThumbCurl; InputIndexCurl=IndexCurl; InputGripCurl=GripCurl;
    GripRole=Clamp(GripKind,0,3);
    // A latched weapon remains held when the physical grip button is released.
    // Thumb/index capacitance still layers onto its resting grasp.
    if (GripRole>0) { GripCurl=FMax(GripCurl,0.72); ThumbCurl=FMax(ThumbCurl,0.35); }
    if (GripRole==2) IndexCurl=FMax(IndexCurl,0.75);
    if (GripRole==3) IndexCurl=FMax(IndexCurl,0.45);
    PoseAt(Position,HandRotation,Units);
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
