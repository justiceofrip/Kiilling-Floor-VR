// Installed-model profiles for the currently supported closed-bolt weapons.
class KFVRWeaponProfile extends Object;
static function name MagazineBone(KFWeapon W)
{
    if (KFVRTrackedSingle(W)!=None) return '9mm_Mag';
    if (KFVRTrackedDeagle(W)!=None || KFVRTrackedBullpup(W)!=None) return 'Magazine';
    return '';
}
static function name ActionBone(KFWeapon W)
{
    if (KFVRTrackedSingle(W)!=None) return '9mm_slide';
    if (KFVRTrackedDeagle(W)!=None) return 'Slide';
    // This installed Bullpup skeleton has no independent action/bolt bone.
    // Keep its physical action profile disabled until a presentation is authored.
    return '';
}
static function float ActionTravel(KFWeapon W)
{
    if (KFVRTrackedBullpup(W)!=None) return 0.07;
    return 0.05;
}
static function KFVRManualReload Manual(KFWeapon W)
{
    if (KFVRTrackedSingle(W)!=None) return KFVRTrackedSingle(W).Manual;
    if (KFVRTrackedDeagle(W)!=None) return KFVRTrackedDeagle(W).Manual;
    if (KFVRTrackedBullpup(W)!=None) return KFVRTrackedBullpup(W).Manual;
    return None;
}
static function BindManual(KFWeapon W,KFVRManualReload M)
{
    if (KFVRTrackedSingle(W)!=None) KFVRTrackedSingle(W).Manual=M;
    if (KFVRTrackedDeagle(W)!=None) KFVRTrackedDeagle(W).Manual=M;
    if (KFVRTrackedBullpup(W)!=None) KFVRTrackedBullpup(W).Manual=M;
}
