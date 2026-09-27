class KFVRHealFire extends SyringeFire;
var bool bVRHeal;
function KFHumanPawn GetHealee()
{
    local KFVRTrackedSyringe S;
    S=KFVRTrackedSyringe(Weapon);
    if (S!=None && S.VRRig!=None && S.VRRig.ActiveFor(S)) return S.ContactHealee();
    return Super.GetHealee();
}
function DoFireEffect()
{
    local KFVRTrackedSyringe S;
    S=KFVRTrackedSyringe(Weapon);
    bVRHeal=S!=None && S.VRRig!=None && S.VRRig.ActiveFor(S);
    if (bVRHeal) S.BeginHeal(0);
    Super.DoFireEffect();
}
function CancelPending() { SetTimer(0,False); CachedHealee=None; }
function Timer()
{
    local KFVRTrackedSyringe S;
    S=KFVRTrackedSyringe(Weapon);
    if (bVRHeal)
    {
        if (S==None) return;
        if (!S.CanComplete(0,CachedHealee)) { S.CancelHeal(); return; }
        Super.Timer(); S.CompleteHeal(0); return;
    }
    Super.Timer();
}
defaultproperties
{
    bWaitForRelease=True
}
