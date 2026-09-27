class KFVRDualFire extends DualiesFire;
function KFVRTrackedDualies Pair() { return KFVRTrackedDualies(Weapon); }
simulated function bool AllowFire()
{
    local KFVRTrackedDualies D;
    D=Pair(); if (D==None) return False;
    D.EnsureFeed();
    if (D.Belt!=None) { if (!D.Belt.Ammo.CanFire(ThisModeNum)) return False; }
    else if (D.Feed.Loaded(ThisModeNum)<1) return False;
    return Super.AllowFire();
}
event ModeDoFire()
{
    // Use KF's normal spread/perk/cadence/fire callback without the stock dual
    // animation-side alternation; the controller selecting this mode owns the ray.
    Super(KFFire).ModeDoFire();
}
function DoFireEffect()
{
    local KFVRTrackedDualies D;
    local KFVRWeaponRig R;
    local vector Position;
    local rotator Aim;
    D=Pair(); if (D==None) return;
    R=D.VRRig;
    if (R==None || !R.ActiveFor(Weapon)) { Super.DoFireEffect(); return; }
    if (ThisModeNum==1) { Position=D.LeftMuzzle; Aim=D.LeftAim; }
    else { Position=R.MuzzleLocation; Aim=R.AimRotation; }
    Aim=rotator(vector(Aim)+VRand()*FRand()*Spread);
    if (ThisModeNum==1)
    { D.LeftShots++; D.LeftTraceStart=Position; D.LeftTraceDirection=vector(Aim); R.PulseLeft(0.45,35); }
    else
    { D.RightShots++; D.RightTraceStart=Position; D.RightTraceDirection=vector(Aim); R.PulseRight(0.45,35); }
    R.TraceCount++; R.LastTraceStart=Position; R.LastTraceDirection=vector(Aim);
    DoTrace(Position,Aim);
}
function rotator AdjustAim(vector Start,float InAimError)
{
    if (Pair()!=None && Pair().VRRig!=None && Pair().VRRig.ActiveFor(Weapon))
    { if (ThisModeNum==1) return Pair().LeftAim; return Pair().VRRig.AimRotation; }
    return Super.AdjustAim(Start,InAimError);
}
function ShakeView()
{
    if (Pair()!=None && Pair().VRRig!=None && Pair().VRRig.ActiveFor(Weapon)) return;
    Super.ShakeView();
}
simulated function HandleRecoil(float Amount)
{
    local KFVRTrackedDualies D;
    D=Pair();
    if (D!=None && D.VRRig!=None && D.VRRig.ActiveFor(Weapon))
    {
        if (ThisModeNum==1) D.LeftRecoil=FMin(D.LeftRecoil+Amount*0.012,0.05);
        else D.RightRecoil=FMin(D.RightRecoil+Amount*0.012,0.05);
        return;
    }
    Super.HandleRecoil(Amount);
}
defaultproperties
{
    // Stock SingleFire cadence per hand; stock perk modifiers still apply.
    FireRate=0.175
    bModeExclusive=False
    bWaitForRelease=True
}
