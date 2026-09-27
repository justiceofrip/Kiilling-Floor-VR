class KFVRBullpupFire extends BullpupFire;
event ModeTick(float Delta)
{
    KFVRTrackedBullpup(Weapon).VRModeTicks++;
    Super.ModeTick(Delta);
}
event ModeDoFire()
{
    KFVRTrackedBullpup(Weapon).VRFireAttempts++;
    Super.ModeDoFire();
}
function KFVRWeaponRig Rig()
{
    if (KFVRTrackedBullpup(Weapon) != None)
        return KFVRTrackedBullpup(Weapon).VRRig;
    return None;
}
simulated function bool AllowFire()
{
    if (KFVRTrackedBullpup(Weapon)!=None && KFVRTrackedBullpup(Weapon).Manual!=None && !KFVRTrackedBullpup(Weapon).Manual.Ammo.CanFire()) return False;
    return Super.AllowFire();
}
function DoFireEffect()
{
    local KFVRWeaponRig R;
    local rotator Aim;
    R=Rig();
    if (R == None || !R.ActiveFor(Weapon)) { Super.DoFireEffect(); return; }
    Aim=rotator(vector(R.AimRotation) + VRand()*FRand()*Spread);
    R.TraceCount++; R.PulseRight(0.45,35);
    R.LastTraceStart=R.MuzzleLocation;
    R.LastTraceDirection=vector(Aim);
    DoTrace(R.MuzzleLocation, Aim);
}
function rotator AdjustAim(vector Start, float InAimError)
{
    local KFVRWeaponRig R;
    R=Rig();
    if (R != None && R.ActiveFor(Weapon)) return R.AimRotation;
    return Super.AdjustAim(Start, InAimError);
}
simulated function vector GetFireStart(vector X, vector Y, vector Z)
{
    local KFVRWeaponRig R;
    R=Rig();
    if (R != None && R.ActiveFor(Weapon)) return R.MuzzleLocation;
    return Super.GetFireStart(X,Y,Z);
}
function ShakeView()
{
    if (Rig()!=None && Rig().ActiveFor(Weapon)) return;
    Super.ShakeView();
}
simulated function HandleRecoil(float Amount)
{
    local KFVRWeaponRig R;
    R=Rig();
    if (R != None && R.ActiveFor(Weapon)) { R.Recoil=FMin(R.Recoil+Amount*0.012,0.05); return; }
    Super.HandleRecoil(Amount);
}
