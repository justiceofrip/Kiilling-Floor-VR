// Installed-model profiles for the currently supported closed-bolt weapons.
class KFVRWeaponProfile extends Object;
static function name MagazineBone(KFWeapon W)
{
    if (KFVRTrackedSingle(W)!=None || KFVRTrackedDualies(W)!=None) return '9mm_Mag';
    if (KFVRTrackedDeagle(W)!=None || KFVRTrackedMK23(W)!=None || KFVRTrackedBullpup(W)!=None || KFVRTrackedSCAR(W)!=None) return 'Magazine';
    return '';
}
static function name ActionBone(KFWeapon W)
{
    if (KFVRTrackedSingle(W)!=None || KFVRTrackedDualies(W)!=None) return '9mm_slide';
    if (KFVRTrackedDeagle(W)!=None || KFVRTrackedMK23(W)!=None) return 'Slide';
    // No separate action exists in the installed Bullpup reload animation.
    return '';
}
// Explicit policy for a magazine-only reload. Unknown weapons do not inherit it.
static function bool FeedOnSeat(KFWeapon W) { return KFVRTrackedBullpup(W)!=None || KFVRTrackedSCAR(W)!=None; }
static function vector ActionAxis(KFWeapon W) { return vect(-1,0,0); }
// Authored normalized markers, sampled from the owner's installed Reload.
// Geometry/axes are measured at runtime, not copied out of the game assets.
static function name InsertionChassis(KFWeapon W)
{
    if (KFVRTrackedSingle(W)!=None) return '9mm_Gun';
    if (KFVRTrackedDeagle(W)!=None) return 'Gun';
    if (KFVRTrackedMK23(W)!=None) return 'MK23';
    if (KFVRTrackedBullpup(W)!=None) return 'Bullpup';
    if (KFVRTrackedSCAR(W)!=None) return 'Scar';
    return '';
}
// These audited magazine/slide parts are direct children of their chassis.
// Keep separate parent selectors so future assets need not share a hierarchy.
// Other mechanisms retain their existing translation path until audited.
static function name MagazineTranslationParent(KFWeapon W)
{
    return InsertionChassis(W);
}
static function name ActionTranslationParent(KFWeapon W)
{
    if (ActionBone(W)=='') return '';
    return InsertionChassis(W);
}
static function vector InsertionFrames(KFWeapon W)
{
    if (KFVRTrackedSingle(W)!=None) return vect(0.55,0.60,0);
    if (KFVRTrackedDeagle(W)!=None) return vect(0.62,0.66,0);
    if (KFVRTrackedMK23(W)!=None) return vect(0.61,0.63,0);
    if (KFVRTrackedBullpup(W)!=None) return vect(0.62,0.76,0);
    if (KFVRTrackedSCAR(W)!=None) return vect(0.63,0.66,0);
    return vect(0,0,0);
}
// A negative marker means no measured native magazine grasp is available.
static function float MagazineGraspFrame(KFWeapon W)
{
    if (KFVRTrackedSingle(W)!=None || KFVRTrackedDeagle(W)!=None) return 0.4;
    if (KFVRTrackedMK23(W)!=None) return 0.51;
    if (KFVRTrackedBullpup(W)!=None) return 0.62;
    if (KFVRTrackedSCAR(W)!=None) return 0.58;
    return -1;
}
// SCAR's long magazine has a distant origin; native finger contact is audited.
// Keep the stricter default for every other model. Bounds are metres.
static function float MagazineGraspLimit(KFWeapon W)
{
    if (KFVRTrackedSCAR(W)!=None) return 0.19;
    return 0.15;
}
static function bool SupportsOffhand(KFWeapon W)
{
    return KFVRTrackedBullpup(W)!=None || KFVRTrackedSCAR(W)!=None;
}
// Provisional overhand palm contact, in metres relative to the slide origin.
// This is authored hand placement, not a claimed stock chambering animation.
static function vector ActionPalmOffset(KFWeapon W)
{
    if (ActionBone(W)!='') return vect(-0.02,-0.035,0.025);
    return vect(0,0,0);
}
static function float ActionTravel(KFWeapon W)
{
    if (ActionBone(W)=='') return 0;
    if (KFVRTrackedMK23(W)!=None) return 0.046;
    return 0.05;
}
static function KFVRManualReload Manual(KFWeapon W)
{
    if (KFVRTrackedSingle(W)!=None) return KFVRTrackedSingle(W).Manual;
    if (KFVRTrackedDeagle(W)!=None) return KFVRTrackedDeagle(W).Manual;
    if (KFVRTrackedMK23(W)!=None) return KFVRTrackedMK23(W).Manual;
    if (KFVRTrackedBullpup(W)!=None) return KFVRTrackedBullpup(W).Manual;
    if (KFVRTrackedSCAR(W)!=None) return KFVRTrackedSCAR(W).Manual;
    return None;
}
static function BindManual(KFWeapon W,KFVRManualReload M)
{
    if (KFVRTrackedSingle(W)!=None) KFVRTrackedSingle(W).Manual=M;
    if (KFVRTrackedDeagle(W)!=None) KFVRTrackedDeagle(W).Manual=M;
    if (KFVRTrackedMK23(W)!=None) KFVRTrackedMK23(W).Manual=M;
    if (KFVRTrackedBullpup(W)!=None) KFVRTrackedBullpup(W).Manual=M;
    if (KFVRTrackedSCAR(W)!=None) KFVRTrackedSCAR(W).Manual=M;
}
