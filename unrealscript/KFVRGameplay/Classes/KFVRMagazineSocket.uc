// Hidden, short-lived measurement of final authored magazine travel.
// Each pose is read on a later ordinary Tick: UE2 caches same-frame GetFrame.
class KFVRMagazineSocket extends Actor;
var KFVRWeaponVisual Target;
var KFWeapon Gun;
var name ChassisBone,TranslationBone;
var vector Markers,StartLocal,EndLocal,TranslationRest;
var bool bTranslationQueued;
var int Stage;
var float QueuedAt;
simulated function vector InFrame(vector P,coords C)
{
    local vector D,V;
    D=P-C.Origin; V.X=D dot C.XAxis; V.Y=D dot C.YAxis; V.Z=D dot C.ZAxis;
    return V;
}
simulated function vector FromFrame(vector V,coords C)
{
    return C.XAxis*V.X+C.YAxis*V.Y+C.ZAxis*V.Z;
}
simulated function bool Initialize(KFVRWeaponVisual V)
{
    if (V==None || V.bDeleteMe || V.MagazineBone=='') return False;
    Target=V; Gun=KFWeapon(V.SourceWeapon);
    ChassisBone=class'KFVRWeaponProfile'.static.InsertionChassis(Gun);
    Markers=class'KFVRWeaponProfile'.static.InsertionFrames(Gun);
    if (Gun==None || Gun.bDeleteMe || ChassisBone=='' || Markers.Y<=Markers.X) return False;
    LinkMesh(Gun.Mesh); SetDrawScale(1); SetDrawScale3D(vect(1,1,1));
    SetLocation(vect(0,0,0)); SetRotation(rot(0,0,0));
    if (!HasAnim(Gun.ReloadAnim) || !PlayAnim(Gun.ReloadAnim,0,0,0)) return False;
    EnableChannelNotify(0,0); SetAnimFrame(Markers.X,0,0); StopAnimating(); QueuedAt=Level.TimeSeconds;
    return True;
}
// A queued translation belongs only to this hidden clone, never the live mesh.
simulated function RestoreTranslation()
{
    if (bTranslationQueued && TranslationBone!='')
        SetBoneLocation(TranslationBone,vect(0,0,0),1.0);
    bTranslationQueued=False;
}
simulated function Destroyed()
{
    RestoreTranslation();
    Super.Destroyed();
}
simulated function Fail()
{
    RestoreTranslation();
    if (Target!=None && !Target.bDeleteMe)
    {
        Target.bInsertionFailed=True; Target.bInsertionCalibrated=False;
        if (Gun!=None) Log("KFVR_INSERTION_PROFILE_ERROR weapon=" $ Gun.Class $ " stage=" $ Stage);
        else Log("KFVR_INSERTION_PROFILE_ERROR missing weapon stage=" $ Stage);
    }
    Destroy();
}
simulated function Tick(float Delta)
{
    local coords Chassis,Magazine;
    local vector Direction,RestLocal,EndOffset;
    local float Travel,RestError,StartDistance,ForwardTravel,TranslationScale;
    if (Target==None || Target.bDeleteMe || Gun==None || Gun.bDeleteMe
        || Target.SourceWeapon!=Gun) { Destroy(); return; }
    if (Stage==3 && (!Target.bAuthoredActionProfile || Target.MagazineBone!=TranslationBone))
    { Fail(); return; }
    if (Level.TimeSeconds<=QueuedAt) return;
    BoneRefresh(); Chassis=GetBoneCoords(ChassisBone); Magazine=GetBoneCoords(Target.MagazineBone);
    if (VSize(Chassis.Origin)<1 || VSize(Magazine.Origin)<1) { Fail(); return; }
    if (Stage==3)
    {
        // DrawScale is one and this actor stays at the origin. This measures
        // the same unit-offset magnitude as the native visible calibration,
        // on the next ordinary Tick so its skeletal cache includes the probe.
        TranslationScale=VSize(Magazine.Origin-TranslationRest);
        if (!(TranslationScale>0.01 && TranslationScale<100)) { Fail(); return; }
        RestoreTranslation();
        Target.BoneTranslationScale=TranslationScale;
        Target.bTranslationCalibrated=True; Target.bHiddenTranslationCalibrated=True;
        Target.bInsertionCalibrated=True;
        Log("KFVR_HIDDEN_TRANSLATION_PROFILE weapon=" $ Gun.Class $ " scale=" $ TranslationScale);
        Log("KFVR_INSERTION_PROFILE weapon=" $ Gun.Class $ " axis=" $ Target.InsertionAxis $ " travel=" $ Target.InsertionSampleTravel $ " end=" $ Target.InsertionEndDistance);
        Destroy(); return;
    }
    if (Stage==0)
    {
        StartLocal=InFrame(Magazine.Origin,Chassis);
        SetAnimFrame(Markers.Y,0,0); StopAnimating(); Stage=1; QueuedAt=Level.TimeSeconds; return;
    }
    if (Stage==1)
    {
        EndLocal=InFrame(Magazine.Origin,Chassis);
        PlayAnim('Idle',0,0,0); EnableChannelNotify(0,0); SetAnimFrame(0,0,0); StopAnimating();
        Stage=2; QueuedAt=Level.TimeSeconds; return;
    }
    RestLocal=InFrame(Magazine.Origin,Chassis);
    Direction=FromFrame(RestLocal-StartLocal,Chassis);
    Travel=VSize(EndLocal-StartLocal)/Target.ModelUnitsPerMetre;
    EndOffset=FromFrame(EndLocal-RestLocal,Chassis)/Target.ModelUnitsPerMetre;
    RestError=VSize(Magazine.Origin-Target.MagazineLocal)/Target.ModelUnitsPerMetre;
    if (Travel<0.025 || Travel>0.20 || VSize(EndOffset)>0.04 || RestError>0.001) { Fail(); return; }
    StartDistance=VSize(RestLocal-StartLocal)/Target.ModelUnitsPerMetre;
    ForwardTravel=((EndLocal-StartLocal) dot Normal(RestLocal-StartLocal))/Target.ModelUnitsPerMetre;
    // Markers must describe insertion toward the installed root, not merely
    // motion near it. Require 5 mm of approach and a broad 60-degree cone.
    if (!(StartDistance-VSize(EndOffset)>=0.005 && ForwardTravel>=0.5*Travel)) { Fail(); return; }
    Target.InsertionAxis=Normal(Direction);
    Target.InsertionY=Normal(Target.MagazineY-Target.InsertionAxis*(Target.MagazineY dot Target.InsertionAxis));
    Target.InsertionX=Normal(Target.InsertionY cross Target.InsertionAxis);
    if (VSize(Target.InsertionX)<0.99 || VSize(Target.InsertionY)<0.99) { Fail(); return; }
    Target.InsertionSampleTravel=Travel; Target.InsertionEndDistance=VSize(EndOffset);
    Target.InsertionRestError=RestError;
    if (Target.bAuthoredActionProfile)
    {
        // Authored actions need magazine motion without probing the visible
        // rifle. Keep readiness closed until this hidden clone measures it.
        Target.bInsertionCalibrated=False;
        Target.bTranslationCalibrated=False; Target.bHiddenTranslationCalibrated=False;
        TranslationRest=Magazine.Origin; TranslationBone=Target.MagazineBone;
        SetBoneLocation(TranslationBone,vect(1,0,0),1.0); bTranslationQueued=True;
        Stage=3; QueuedAt=Level.TimeSeconds; return;
    }
    Target.bInsertionCalibrated=True;
    Log("KFVR_INSERTION_PROFILE weapon=" $ Gun.Class $ " axis=" $ Target.InsertionAxis $ " travel=" $ Travel $ " end=" $ Target.InsertionEndDistance);
    Destroy();
}
defaultproperties
{
    DrawType=DT_None
    bHidden=True
    RemoteRole=ROLE_None
    bCollideActors=False
    bCollideWorld=False
    bBlockActors=False
    bBlockPlayers=False
}
