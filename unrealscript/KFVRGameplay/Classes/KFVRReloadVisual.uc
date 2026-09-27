// Optional presentation actor for physically scrubbed weapon poses. It is not
// a KFWeapon and has no fire/reload/ammo callbacks. It is not spawned or installed
// automatically. Calibrated asset profiles and native per-tick attachment follow.
class KFVRReloadVisual extends Actor;

var KFVRReloadPoseProfile Profile;
var name AppliedSequence;

simulated function bool Initialize(Weapon SourceWeapon, KFVRReloadPoseProfile NewProfile)
{
    local int I;
    Profile = None;
    AppliedSequence = '';
    bHidden = True;
    if (SourceWeapon == None || SourceWeapon.Mesh == None || NewProfile == None)
        return false;
    LinkMesh(SourceWeapon.Mesh);
    Skins = SourceWeapon.Skins;
    SetDrawScale(SourceWeapon.DrawScale);
    SetDrawScale3D(SourceWeapon.DrawScale3D);
    for (I=0; I<4; I++)
        if (NewProfile.Configured[I] == 0 || !HasAnim(NewProfile.Sequences[I]))
            return false;
    Profile = NewProfile;
    bHidden = False;
    return true;
}

simulated function bool ApplyPose(byte Stage, float PhysicalProgress)
{
    local name Sequence;
    local float Frame;
    if (Profile == None || !Profile.Resolve(Stage, PhysicalProgress, Sequence, Frame))
        return false;
    if (Sequence != AppliedSequence)
    {
        if (!PlayAnim(Sequence, 0.0, 0.0, 0))
            return false;
        EnableChannelNotify(0, 0);
        AppliedSequence = Sequence;
    }
    SetAnimFrame(Frame, 0, 0);
    StopAnimating();
    return true;
}

simulated function bool ApplyTrackedTransform(vector Position, rotator Orientation)
{
    if (!SetLocation(Position))
        return false;
    return SetRotation(Orientation);
}

defaultproperties
{
    DrawType=DT_Mesh
    Physics=PHYS_None
    RemoteRole=ROLE_None
    bCollideActors=False
    bCollideWorld=False
    bBlockActors=False
    bBlockPlayers=False
    bHidden=True
}
