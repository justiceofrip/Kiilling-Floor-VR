// Hidden runtime measurement of the installed reload grasp; no gameplay callbacks.
// Only the normalized sampling time is authored here, not extracted pose data.
class KFVRMagazineGrip extends KFVRWeaponVisual;
var name SampleSequence;
var float SampleFrame,SampleRate,SourcePalmDistance;
var coords ReloadMagazine;
simulated function bool InitializeMagazine(KFVRWeaponVisual Source)
{
    local coords C,P,Rest;
    local int I;
    local KFWeapon W;
    if (Source==None || Source.SourceWeapon==None || Source.MagazineBone=='') return False;
    W=KFWeapon(Source.SourceWeapon);
    if (W==None || (KFVRTrackedSingle(W)==None && KFVRTrackedDeagle(W)==None)) return False;
    SourceWeapon=W; LinkMesh(W.Mesh);
    SetLocation(vect(0,0,0)); SetRotation(rot(0,0,0));
    SetDrawScale(1); SetDrawScale3D(vect(1,1,1));
    // Both installed reloads hold the replacement magazine steadily here.
    if (!HasAnim(W.ReloadAnim) || !PlayAnim(W.ReloadAnim,0,0,0)) return False;
    EnableChannelNotify(0,0); SetAnimFrame(0.4,0,0); StopAnimating();
    GetAnimParams(0,SampleSequence,SampleFrame,SampleRate);
    BoneRefresh(); ReloadMagazine=GetBoneCoords(Source.MagazineBone);
    P=GetBoneCoords(class'KFVRHandPoseMath'.static.SourceBone(True,0));
    SourcePalmDistance=VSize(P.Origin-ReloadMagazine.Origin);
    if (SampleSequence!=W.ReloadAnim || Abs(SampleFrame-0.4)>0.01 || SourcePalmDistance<5 || SourcePalmDistance>45) return False;
    Rest.XAxis=Source.MagazineX; Rest.YAxis=Source.MagazineY; Rest.ZAxis=Source.MagazineZ;
    for (I=0;I<16;I++)
    {
        C=GetBoneCoords(class'KFVRHandPoseMath'.static.SourceBone(True,I));
        if (I>0 && (VSize(C.Origin-P.Origin)<0.1 || VSize(C.Origin-P.Origin)>100)) return False;
        if (Abs(VSize(C.XAxis)-1)>0.01 || Abs(C.XAxis dot C.YAxis)>0.01) return False;
        C.Origin=class'KFVRHandPoseMath'.static.ChangeBasis(C.Origin-ReloadMagazine.Origin,ReloadMagazine,Rest);
        C.XAxis=class'KFVRHandPoseMath'.static.ChangeBasis(C.XAxis,ReloadMagazine,Rest);
        C.YAxis=class'KFVRHandPoseMath'.static.ChangeBasis(C.YAxis,ReloadMagazine,Rest);
        C.ZAxis=class'KFVRHandPoseMath'.static.ChangeBasis(C.ZAxis,ReloadMagazine,Rest);
        GripPose[I]=C;
    }
    C=GripBone(0); PalmLocal=C.Origin;
    bLeftGripPose=True; bCalibrated=True;
    Log("KFVR_MAGAZINE_GRIP weapon=" $ W.Class $ " sequence=" $ SampleSequence $ " frame=" $ SampleFrame $ " palm=" $ PalmLocal);
    return True;
}
simulated function PoseAt(vector GripPosition,rotator GripRotation,float Units)
{
    SetDrawScale(Units/ModelUnitsPerMetre); SetRotation(GripRotation);
    SetLocation(GripPosition-((PalmLocal*DrawScale)>>GripRotation));
    bHidden=False;
}
defaultproperties
{
    DrawType=DT_None
    bHidden=True
}
