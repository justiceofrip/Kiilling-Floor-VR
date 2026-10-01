// Compatibility contract for the SCAR adapter. Compare the installed defaults;
// do not copy proprietary balance numbers into this package.
class KFVRSCARTests extends Commandlet;
var int Checks,Failures;
function Check(bool B,string Message)
{
    Checks++;
    if (!B) { Failures++; Log("KFVR_SCAR_FAIL " $ Message); }
}
event int Main(string Params)
{
    local class<KFWeapon> Stock,Tracked;
    local class<KFFire> StockFire,TrackedFire;
    local KFVRSCARFire F;
    Stock=class'SCARMK17AssaultRifle'; Tracked=class'KFVRTrackedSCAR';
    StockFire=class'SCARMK17Fire'; TrackedFire=class'KFVRSCARFire';
    Check(ClassIsChildOf(Tracked,Stock),"stock weapon ancestry used by Commando casts");
    Check(ClassIsChildOf(TrackedFire,StockFire),"stock SCAR trace and firing ancestry");
    Check(Tracked.default.FireModeClass[0]==TrackedFire,"primary uses controller adapter");
    Check(Tracked.default.FireModeClass[1]==Stock.default.FireModeClass[1],"unchanged secondary mode");
    Check(Tracked.default.MeshRef==Stock.default.MeshRef && Stock.default.MeshRef!="","installed first-person mesh reference");
    Check(Tracked.default.Mesh==Stock.default.Mesh,"unchanged lazy mesh state");
    Check(Tracked.default.PickupClass==Stock.default.PickupClass,"stock pickup identity for perk pricing");
    Check(Tracked.default.AttachmentClass==Stock.default.AttachmentClass,"stock third-person attachment");
    Check(Tracked.default.MagCapacity==Stock.default.MagCapacity,"native magazine capacity");
    Check(Tracked.default.ReloadRate==Stock.default.ReloadRate,"native reload timing outside physical mode");
    Check(Tracked.default.ReloadAnimRate==Stock.default.ReloadAnimRate,"native reload animation speed");
    Check(Tracked.default.ReloadAnim==Stock.default.ReloadAnim,"installed reload sequence");
    Check(Tracked.default.Weight==Stock.default.Weight,"native carry weight");
    Check(Tracked.default.InventoryGroup==Stock.default.InventoryGroup,"native inventory group");
    Check(Tracked.default.bNoAmmoInstances==Stock.default.bNoAmmoInstances,"native ammunition storage policy");
    Check(Tracked.default.bConsumesPhysicalAmmo==Stock.default.bConsumesPhysicalAmmo,"native ammunition consumption policy");
    Check(TrackedFire.default.AmmoClass==StockFire.default.AmmoClass,"stock ammunition identity for perks and pickups");
    Check(TrackedFire.default.DamageType==StockFire.default.DamageType,"stock damage identity for Commando damage");
    Check(TrackedFire.default.DamageMin==StockFire.default.DamageMin,"stock minimum damage");
    Check(TrackedFire.default.DamageMax==StockFire.default.DamageMax,"stock maximum damage");
    Check(TrackedFire.default.TraceRange==StockFire.default.TraceRange,"stock hitscan range");
    Check(TrackedFire.default.Momentum==StockFire.default.Momentum,"stock hit momentum");
    Check(TrackedFire.default.FireRate==StockFire.default.FireRate,"stock automatic cadence");
    Check(TrackedFire.default.AmmoPerFire==StockFire.default.AmmoPerFire,"stock rounds consumed per shot");
    Check(TrackedFire.default.bWaitForRelease==StockFire.default.bWaitForRelease,"stock automatic or semi-auto default");
    Check(TrackedFire.default.bInstantHit==StockFire.default.bInstantHit,"stock hitscan firing policy");
    Check(TrackedFire.default.Spread==StockFire.default.Spread,"stock initial spread");
    Check(TrackedFire.default.MaxSpread==StockFire.default.MaxSpread,"stock sustained burst spread");
    Check(TrackedFire.default.bAccuracyBonusForSemiAuto==StockFire.default.bAccuracyBonusForSemiAuto,"stock semi-auto accuracy policy");
    Check(TrackedFire.default.FireAnim==StockFire.default.FireAnim,"stock firing animation");
    Check(TrackedFire.default.FireAimedAnim==StockFire.default.FireAimedAnim,"stock aimed firing animation");
    Check(TrackedFire.default.FireSoundRef==StockFire.default.FireSoundRef,"stock fire sound source");
    Check(TrackedFire.default.StereoFireSoundRef==StockFire.default.StereoFireSoundRef,"stock stereo fire sound source");
    Check(TrackedFire.default.ShellEjectClass==StockFire.default.ShellEjectClass,"stock shell effects");
    Check(TrackedFire.default.ShellEjectBoneName==StockFire.default.ShellEjectBoneName,"stock shell origin");
    F=new class'KFVRSCARFire';
    Check(F.Rig()==None,"unbound fire adapter has no VR authority");
    Log("KFVR_SCAR_TEST_RESULT checks=" $ Checks $ " failures=" $ Failures);
    if (Failures>0) return 1;
    return 0;
}
