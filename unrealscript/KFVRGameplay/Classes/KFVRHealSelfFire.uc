class KFVRHealSelfFire extends SyringeAltFire;
var bool bVRHeal;
function DoFireEffect()
{
    local KFVRTrackedSyringe S;
    S=KFVRTrackedSyringe(Weapon);
    bVRHeal=S!=None && S.VRRig!=None && S.VRRig.ActiveFor(S);
    if (bVRHeal) S.BeginHeal(1);
    Super.DoFireEffect();
}
function Timer()
{
    local KFVRTrackedSyringe S;
    S=KFVRTrackedSyringe(Weapon);
    if (bVRHeal)
    {
        if (S==None) return;
        if (!S.CanComplete(1)) { S.CancelHeal(); return; }
        Super.Timer(); S.CompleteHeal(1); return;
    }
    Super.Timer();
}
defaultproperties
{
    bWaitForRelease=True
}
