// Route the existing MK23 trace from the tracked muzzle; inherited DoTrace
// retains its installed penetration, damage and perk behavior.
class KFVRMK23Fire extends MK23Fire;
event ModeTick(float Delta)
{
    KFVRTrackedMK23(Weapon).VRModeTicks++;
    Super.ModeTick(Delta);
}
event ModeDoFire()
{
    KFVRTrackedMK23(Weapon).VRFireAttempts++;
    Super.ModeDoFire();
}
function KFVRWeaponRig Rig()
{
    if (KFVRTrackedMK23(Weapon) != None)
        return KFVRTrackedMK23(Weapon).VRRig;
    return None;
}
simulated function bool AllowFire()
{
    if (KFVRTrackedMK23(Weapon)!=None && KFVRTrackedMK23(Weapon).Manual!=None && !KFVRTrackedMK23(Weapon).Manual.Ammo.CanFire()) return False;
    return Super.AllowFire();
}
function DoFireEffect()
{
    local KFVRWeaponRig R;
    local rotator Aim;
    R=Rig();
    if (R == None || !R.ActiveFor(Weapon)) { Super.DoFireEffect(); return; }
    // Physical pistol sights own the tracked ray. Keep the inherited fire
    // cadence, perk/damage handling and bloom state for stock fallback, but
    // do not add desktop hip-fire dispersion to the controller's aim.
    Aim=R.AimRotation;
    R.TraceCount++; R.PulseRight(0.45,35);
    R.LastTraceStart=R.MuzzleLocation;
    R.LastTraceDirection=vector(Aim);
    R.RecordShot(Weapon,ThisModeNum,R.MuzzleLocation,R.AimRotation,Aim,Spread,0.0);
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
