// Hidden, short-lived measurement of final authored magazine travel.
// Each pose is read on a later ordinary Tick: UE2 caches same-frame GetFrame.
class KFVRMagazineSocket extends Actor;
var KFVRWeaponVisual Target;
var KFWeapon Gun;
var name ChassisBone;
var vector Markers,StartLocal,EndLocal;
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
    Target=V; Gun=KFWeapon(V.SourceWeapon);
    ChassisBone=class'KFVRWeaponProfile'.static.InsertionChassis(Gun);
    Markers=class'KFVRWeaponProfile'.static.InsertionFrames(Gun);
    if (Gun==None || ChassisBone=='' || Markers.Y<=Markers.X) return False;
    LinkMesh(Gun.Mesh); SetDrawScale(1); SetDrawScale3D(vect(1,1,1));
    SetLocation(vect(0,0,0)); SetRotation(rot(0,0,0));
    if (!HasAnim(Gun.ReloadAnim) || !PlayAnim(Gun.ReloadAnim,0,0,0)) return False;
    EnableChannelNotify(0,0); SetAnimFrame(Markers.X,0,0); StopAnimating(); QueuedAt=Level.TimeSeconds;
    return True;
}
simulated function Fail()
{
    if (Target!=None) { Target.bInsertionFailed=True; Log("KFVR_INSERTION_PROFILE_ERROR weapon=" $ Gun.Class); }
    Destroy();
}
simulated function Tick(float Delta)
{
    local coords Chassis,Magazine;
    local vector Direction,RestLocal,EndOffset;
    local float Travel,RestError,StartDistance,ForwardTravel;
    if (Target==None || Target.bDeleteMe || Gun==None) { Destroy(); return; }
    if (Level.TimeSeconds<=QueuedAt) return;
    BoneRefresh(); Chassis=GetBoneCoords(ChassisBone); Magazine=GetBoneCoords(Target.MagazineBone);
    if (VSize(Chassis.Origin)<1 || VSize(Magazine.Origin)<1) { Fail(); return; }
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
    Target.InsertionRestError=RestError; Target.bInsertionCalibrated=True;
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
