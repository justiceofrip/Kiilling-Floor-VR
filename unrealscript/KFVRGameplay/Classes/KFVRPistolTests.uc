// Installed-class compatibility checks for the tracked pistol adapters.
// Keep stock balance values in KFMod. Object-only checks below do not claim
// to exercise live authority, perk callbacks, traces or inactive-world fallback.
class KFVRPistolTests extends Commandlet;
var int Checks,Failures;
function Check(bool B,string Message)
{
    Checks++;
    if (!B) { Failures++; Log("KFVR_PISTOL_FAIL " $ Message); }
}
function CheckFamily(string Label,class<KFWeapon> Stock,class<KFWeapon> Tracked,
    class<KFFire> StockFire,class<KFFire> TrackedFire,bool bDual)
{
    Check(ClassIsChildOf(Tracked,Stock),Label $ ": installed weapon ancestry");
    Check(ClassIsChildOf(TrackedFire,StockFire),Label $ ": installed trace and firing ancestry");
    // These are the relevant stock Sharpshooter weapon-family casts. In
    // particular MK23 must not accidentally acquire the Deagle/Single ancestry.
    Check(ClassIsChildOf(Tracked,class'Single')==ClassIsChildOf(Stock,class'Single'),Label $ ": Single perk cast eligibility");
    Check(ClassIsChildOf(Tracked,class'Deagle')==ClassIsChildOf(Stock,class'Deagle'),Label $ ": Deagle perk cast eligibility");
    Check(ClassIsChildOf(Tracked,class'Dualies')==ClassIsChildOf(Stock,class'Dualies'),Label $ ": Dualies perk cast eligibility");
    Check(ClassIsChildOf(Tracked,class'MK23Pistol')==ClassIsChildOf(Stock,class'MK23Pistol'),Label $ ": MK23 perk cast eligibility");
    Check(Tracked.default.FireModeClass[0]==TrackedFire,Label $ ": primary controller adapter");
    Check(Tracked.default.MeshRef==Stock.default.MeshRef,Label $ ": installed first-person mesh reference");
    // Installed pistols use either an eagerly loaded Mesh or a lazy MeshRef.
    Check(Tracked.default.Mesh==Stock.default.Mesh
        && (Stock.default.Mesh!=None || Stock.default.MeshRef!=""),Label $ ": unchanged valid native mesh source");
    Check(Tracked.default.PickupClass==Stock.default.PickupClass,Label $ ": pickup identity for perks and pricing");
    Check(Tracked.default.AttachmentClass==Stock.default.AttachmentClass,Label $ ": installed third-person attachment");
    Check(Tracked.default.MagCapacity==Stock.default.MagCapacity,Label $ ": native magazine capacity");
    Check(Tracked.default.ReloadRate==Stock.default.ReloadRate,Label $ ": inactive physical-mode reload timing");
    Check(Tracked.default.ReloadAnimRate==Stock.default.ReloadAnimRate,Label $ ": reload animation speed");
    Check(Tracked.default.ReloadAnim==Stock.default.ReloadAnim,Label $ ": installed reload sequence");
    Check(Tracked.default.Weight==Stock.default.Weight,Label $ ": carry weight");
    Check(Tracked.default.InventoryGroup==Stock.default.InventoryGroup,Label $ ": inventory group");
    Check(Tracked.default.bNoAmmoInstances==Stock.default.bNoAmmoInstances,Label $ ": native ammunition storage policy");
    Check(Tracked.default.bConsumesPhysicalAmmo==Stock.default.bConsumesPhysicalAmmo,Label $ ": native ammunition consumption policy");
    Check(TrackedFire.default.AmmoClass==StockFire.default.AmmoClass,Label $ ": ammunition identity for perks and pickups");
    Check(TrackedFire.default.AmmoPerFire==StockFire.default.AmmoPerFire,Label $ ": rounds consumed per shot");
    Check(TrackedFire.default.DamageType==StockFire.default.DamageType,Label $ ": damage identity for perks");
    Check(TrackedFire.default.DamageMin==StockFire.default.DamageMin,Label $ ": minimum damage");
    Check(TrackedFire.default.DamageMax==StockFire.default.DamageMax,Label $ ": maximum damage");
    Check(TrackedFire.default.TraceRange==StockFire.default.TraceRange,Label $ ": hitscan range");
    Check(TrackedFire.default.Momentum==StockFire.default.Momentum,Label $ ": hit momentum");
    Check(TrackedFire.default.bInstantHit==StockFire.default.bInstantHit,Label $ ": hitscan firing policy");
    // The tracked ray policy must not erase stock burst/perk state or change
    // the defaults consulted by KFFire.GetSpread and KFFire.ModeDoFire.
    Check(TrackedFire.default.Spread==StockFire.default.Spread,Label $ ": initial spread state");
    Check(TrackedFire.default.MaxSpread==StockFire.default.MaxSpread,Label $ ": sustained burst spread limit");
    Check(TrackedFire.default.SpreadStyle==StockFire.default.SpreadStyle,Label $ ": stock spread distribution default");
    Check(TrackedFire.default.bAccuracyBonusForSemiAuto==StockFire.default.bAccuracyBonusForSemiAuto,Label $ ": semi-auto accuracy policy");
    Check(TrackedFire.default.FireAnimRate==StockFire.default.FireAnimRate,Label $ ": base firing animation rate used by perks");
    Check(TrackedFire.default.ReloadAnimRate==StockFire.default.ReloadAnimRate,Label $ ": base reload animation rate used by perks");
    Check(TrackedFire.default.RecoilRate==StockFire.default.RecoilRate,Label $ ": fallback recoil rate");
    Check(TrackedFire.default.maxVerticalRecoilAngle==StockFire.default.maxVerticalRecoilAngle,Label $ ": fallback vertical recoil");
    Check(TrackedFire.default.maxHorizontalRecoilAngle==StockFire.default.maxHorizontalRecoilAngle,Label $ ": fallback horizontal recoil");
    Check(TrackedFire.default.RecoilVelocityScale==StockFire.default.RecoilVelocityScale,Label $ ": fallback movement recoil scaling");
    Check(TrackedFire.default.FireAnim==StockFire.default.FireAnim,Label $ ": firing animation");
    Check(TrackedFire.default.FireAimedAnim==StockFire.default.FireAimedAnim,Label $ ": aimed firing animation");
    Check(TrackedFire.default.FireSoundRef==StockFire.default.FireSoundRef,Label $ ": fire sound source");
    Check(TrackedFire.default.StereoFireSoundRef==StockFire.default.StereoFireSoundRef,Label $ ": stereo fire sound source");
    Check(TrackedFire.default.ShellEjectClass==StockFire.default.ShellEjectClass,Label $ ": shell effects");
    Check(TrackedFire.default.ShellEjectBoneName==StockFire.default.ShellEjectBoneName,Label $ ": shell origin");
    if (bDual)
    {
        // Independent controller modes already intentionally differ from the
        // stock alternating dual animation. Each hand has SingleFire cadence.
        Check(Tracked.default.FireModeClass[1]==TrackedFire,Label $ ": secondary independent controller adapter");
        Check(Tracked.default.bReduceMagAmmoOnSecondaryFire,Label $ ": secondary consumes native magazine ammo");
        Check(TrackedFire.default.FireRate==class'SingleFire'.default.FireRate,Label $ ": installed single-pistol cadence per hand");
        Check(!TrackedFire.default.bModeExclusive,Label $ ": independent simultaneous hand modes");
        Check(TrackedFire.default.bWaitForRelease==class'SingleFire'.default.bWaitForRelease,Label $ ": installed single-pistol trigger release policy");
    }
    else
    {
        Check(Tracked.default.FireModeClass[1]==Stock.default.FireModeClass[1],Label $ ": unchanged secondary fire mode");
        Check(Tracked.default.bReduceMagAmmoOnSecondaryFire==Stock.default.bReduceMagAmmoOnSecondaryFire,Label $ ": secondary ammunition policy");
        Check(TrackedFire.default.FireRate==StockFire.default.FireRate,Label $ ": stock cadence before perk modifier");
        Check(TrackedFire.default.bModeExclusive==StockFire.default.bModeExclusive,Label $ ": stock fire-mode exclusivity");
        Check(TrackedFire.default.bWaitForRelease==StockFire.default.bWaitForRelease,Label $ ": stock trigger release policy");
    }
}
event int Main(string Params)
{
    local KFVRSingleFire Nine;
    local KFVRDeagleFire Deagle;
    local KFVRMK23Fire MK23;
    local KFVRDualFire Dual;
    CheckFamily("9mm",class'Single',class'KFVRTrackedSingle',class'SingleFire',class'KFVRSingleFire',False);
    CheckFamily("Deagle",class'Deagle',class'KFVRTrackedDeagle',class'DeagleFire',class'KFVRDeagleFire',False);
    CheckFamily("MK23",class'MK23Pistol',class'KFVRTrackedMK23',class'MK23Fire',class'KFVRMK23Fire',False);
    CheckFamily("dual 9mm",class'Dualies',class'KFVRTrackedDualies',class'DualiesFire',class'KFVRDualFire',True);
    Nine=new class'KFVRSingleFire'; Deagle=new class'KFVRDeagleFire';
    MK23=new class'KFVRMK23Fire'; Dual=new class'KFVRDualFire';
    Check(Nine.Rig()==None,"unbound 9mm adapter has no tracked authority");
    Check(Deagle.Rig()==None,"unbound Deagle adapter has no tracked authority");
    Check(MK23.Rig()==None,"unbound MK23 adapter has no tracked authority");
    Check(Dual.Pair()==None,"unbound dual adapter has no feed owner");
    Check(!Dual.AllowFire(),"unbound dual adapter cannot consume or fire a round");
    Log("KFVR_PISTOL_TEST_RESULT checks=" $ Checks $ " failures=" $ Failures);
    if (Failures>0) return 1;
    return 0;
}
