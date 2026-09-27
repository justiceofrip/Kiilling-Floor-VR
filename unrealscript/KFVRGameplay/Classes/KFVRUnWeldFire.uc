class KFVRUnWeldFire extends UnWeldFire;
var bool bVRStroke;
function KFDoorMover GetDoor()
{
    local KFVRTrackedWelder W;
    W=KFVRTrackedWelder(Weapon);
    if (W!=None && W.VRRig!=None && W.VRRig.ActiveFor(W)) return W.GetContactDoor();
    return Super.GetDoor();
}
function bool AllowFire()
{
    local KFVRTrackedWelder W;
    W=KFVRTrackedWelder(Weapon);
    if (W!=None && W.VRRig!=None && W.VRRig.ActiveFor(W) && (!W.WantsWeld(ThisModeNum) || GetDoor()==None)) return False;
    return Super.AllowFire();
}
simulated event ModeDoFire()
{
    local KFVRTrackedWelder W;
    W=KFVRTrackedWelder(Weapon);
    bVRStroke=W!=None && W.VRRig!=None && W.VRRig.ActiveFor(W);
    Super.ModeDoFire();
}
simulated function Timer()
{
    if (bVRStroke) { if (KFVRTrackedWelder(Weapon)!=None) KFVRTrackedWelder(Weapon).ApplyContact(self); return; }
    Super.Timer();
}
simulated function ShakeView()
{
    if (!bVRStroke) Super.ShakeView();
}
