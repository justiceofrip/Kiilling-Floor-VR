# Installed weapon coverage

96 KFWeapon-derived classes, including abstract bases, skins, story tools and legacy variants. This is not a count of trader weapons.

Class names and inheritance come from the installed packages. Numerical defaults are read from loaded class-default buffers; they are not current perk-modified statistics. Candidate mechanical families need model/animation review before activation. This coverage ledger does not enable profiles. Standalone adapters enable 9mm, dual 9mm, Deagle, Bullpup, knife, syringe, welder and stock frag grenade interactions with inventory holsters. Single pistols support physical magazines/slides, dual 9mm supports physical belt reloads, the Bullpup has a support-hand grip, and the knife uses physical swings. Other families remain inactive.

| Class | Candidate family | Capacity | Reload seconds | Notes |
| --- | --- | ---: | ---: | --- |
| KFMod.KFWeapon | Unclassified | 0 | 0.0 | Abstract base |
| KFMod.KFMeleeGun | Melee | 0 | 0.0 | Pending interaction/asset validation |
| KFMod.BoomStick | BreakAction | 2 | 0.009999999776482582 | Pending interaction/asset validation |
| KFMod.Dualies | DualMagazineFed | 30 | 3.5 | Independent poses/fire, shared-ammo belt reloads, tactical 16+16; live synthetic checks passed |
| KFMod.Deagle | MagazineFed | 8 | 2.200000047683716 | Tracked fire, timed/physical reload, tactical 8+1, left hip slot |
| KFMod.SeekerSixRocketLauncher | MultiTubeLauncher | 6 | 3.130000114440918 | Pending interaction/asset validation |
| KFMod.FlareRevolver | Revolver | 6 | 3.200000047683716 | Pending interaction/asset validation |
| KFMod.MK23Pistol | MagazineFed | 12 | 2.5999999046325684 | Opt-in MK23Test fixture: tracked fire, continuous physical magazine/slide reload and tactical12+1 pass desktop tests; regular inventory/shop/dual support pending |
| KFMod.Syringe | Tool | 0 | 0.0 | Chest holster, tracked needle and stock self-healing pass live synthetic checks; teammate contact remains untested |
| KFMod.Magnum44Pistol | Revolver | 6 | 2.5250000953674316 | Pending interaction/asset validation |
| KFMod.M7A3MMedicGun | MagazineFed | 15 | 3.065999984741211 | Pending interaction/asset validation |
| KFMod.Single | MagazineFed | 15 | 2.0 | Live tracked aiming/fire/holster and physical reload adapter |
| KFMod.ZEDGun | EnergyWeapon | 100 | 3.5 | Pending interaction/asset validation |
| KFMod.Crossbow | SingleRoundBow | 1 | 0.009999999776482582 | Pending interaction/asset validation |
| KFMod.MP7MMedicGun | MagazineFed | 20 | 3.1659998893737793 | Pending interaction/asset validation |
| KFMod.GoldenDeagle | MagazineFed | 8 | 2.200000047683716 | Pending interaction/asset validation |
| KFMod.KFWeaponShotgun | Unclassified | 0 | 0.0 | Abstract base |
| KFMod.MP5MMedicGun | MagazineFed | 32 | 3.799999952316284 | Pending interaction/asset validation |
| KFMod.M14EBRBattleRifle | MagazineFed | 20 | 3.365999937057495 | Pending interaction/asset validation |
| KFMod.FlameThrower | FuelWeapon | 100 | 4.139999866485596 | Pending interaction/asset validation |
| KFMod.Frag | Throwable | 1 | 1.0 | Exact stock item; rear-waist draw, grip release throws, stock projectiles/ammo/damage pass live synthetic checks |
| KFMod.M79GrenadeLauncher | BreakActionLauncher | 1 | 0.009999999776482582 | Pending interaction/asset validation |
| KFMod.LAW | SingleRoundLauncher | 1 | 3.0 | Pending interaction/asset validation |
| KFMod.DualDeagle | DualMagazineFed | 16 | 3.5 | Pending interaction/asset validation |
| KFMod.M99SniperRifle | SingleRoundBoltAction | 1 | 4.375999927520752 | Pending interaction/asset validation |
| KFMod.DualFlareRevolver | DualRevolver | 12 | 4.849999904632568 | Pending interaction/asset validation |
| KFMod.KFMedicGun | MagazineFed | 0 | 0.0 | Pending interaction/asset validation |
| KFMod.KrissMMedicGun | MagazineFed | 25 | 3.3299999237060547 | Pending interaction/asset validation |
| KFMod.ThompsonSMG | MagazineFed | 30 | 3.5999999046325684 | Pending interaction/asset validation |
| KFMod.AK47AssaultRifle | MagazineFed | 30 | 3.0 | Pending interaction/asset validation |
| KFMod.M4AssaultRifle | MagazineFed | 30 | 3.6333000659942627 | Pending interaction/asset validation |
| KFMod.Crossbuzzsaw | SawLauncher | 1 | 0.009999999776482582 | Pending interaction/asset validation |
| KFMod.KSGShotgun | DualTubePumpShotgun | 12 | 3.1600000858306885 | Pending interaction/asset validation |
| KFMod.ZEDMKIIWeapon | EnergyWeapon | 30 | 1.899999976158142 | Pending interaction/asset validation |
| KFMod.AA12AutoShotgun | MagazineFed | 20 | 3.132999897003174 | Pending interaction/asset validation |
| KFMod.BenelliShotgun | TubeFedShotgun | 6 | 0.8999999761581421 | Pending interaction/asset validation |
| KFMod.SCARMK17AssaultRifle | MagazineFed | 20 | 2.9660000801086426 | Pending interaction/asset validation |
| KFMod.Huskgun | ChargeWeapon | 1 | 0.009999999776482582 | Pending interaction/asset validation |
| KFMod.SPSniperRifle | LeverAction | 10 | 2.865999937057495 | Pending interaction/asset validation |
| KFMod.Dual44Magnum | DualRevolver | 12 | 4.466700077056885 | Pending interaction/asset validation |
| KFMod.MKb42AssaultRifle | MagazineFed | 30 | 3.0 | Pending interaction/asset validation |
| KFMod.ThompsonDrumSMG | MagazineFed | 50 | 3.799999952316284 | Pending interaction/asset validation |
| KFMod.M32GrenadeLauncher | CylinderLauncher | 6 | 1.6339999437332153 | Pending interaction/asset validation |
| KFMod.SPThompsonSMG | MagazineFed | 40 | 3.799999952316284 | Pending interaction/asset validation |
| KFMod.Chainsaw | PoweredMelee | 0 | 0.0 | Pending interaction/asset validation |
| KFMod.Katana | Melee | 0 | 0.0 | Pending interaction/asset validation |
| KFMod.PipeBombExplosive | Deployable | 1 | 0.0 | Pending interaction/asset validation |
| KFMod.SPGrenadeLauncher | BreakActionLauncher | 1 | 0.009999999776482582 | Pending interaction/asset validation |
| KFMod.FNFAL_ACOG_AssaultRifle | MagazineFed | 20 | 3.5999999046325684 | Pending interaction/asset validation |
| KFMod.MAC10MP | MagazineFed | 30 | 3.0 | Pending interaction/asset validation |
| KFMod.Bullpup | MagazineFed | 40 | 1.9666670560836792 | Tracked automatic fire, timed reload, shoulder slot, support hand |
| KFMod.Welder | Tool | 0 | 0.0 | Tracked flame tip, stock charge/perks, real Offices door weld/unweld/Use verified |
| KFMod.Winchester | LeverAction | 10 | 0.6666666865348816 | Pending interaction/asset validation |
| KFMod.M4203AssaultRifle | MagazineFed+UnderbarrelLauncher | 30 | 3.6333000659942627 | Pending interaction/asset validation |
| KFMod.TrenchGun | PumpShotgun | 6 | 0.699999988079071 | Pending interaction/asset validation |
| KFMod.DwarfAxe | Melee | 0 | 0.0 | Pending interaction/asset validation |
| KFMod.DualMK23Pistol | DualMagazineFed | 24 | 4.466700077056885 | Pending interaction/asset validation |
| KFMod.NailGun | MagazineFedSpecial | 6 | 2.5999999046325684 | Pending interaction/asset validation |
| KFMod.SPAutoShotgun | MagazineFed | 10 | 3.299999952316284 | Pending interaction/asset validation |
| KFMod.SealSquealHarpoonBomber | HarpoonLauncher | 3 | 4.0 | Pending interaction/asset validation |
| KFMod.GoldenDualDeagle | DualMagazineFed | 16 | 3.5 | Pending interaction/asset validation |
| KFMod.ClaymoreSword | Melee | 0 | 0.0 | Pending interaction/asset validation |
| KFMod.BlowerThrower | SpecialWeapon | 50 | 4.139999866485596 | Pending interaction/asset validation |
| KFMod.Shotgun | PumpShotgun | 8 | 0.6666666865348816 | Pending interaction/asset validation |
| KFMod.Scythe | Melee | 0 | 0.0 | Pending interaction/asset validation |
| KFMod.GoldenAA12AutoShotgun | MagazineFed | 20 | 3.132999897003174 | Pending interaction/asset validation |
| KFMod.NeonSCARMK17AssaultRifle | MagazineFed | 20 | 2.9660000801086426 | Pending interaction/asset validation |
| KFMod.GoldenAK47AssaultRifle | MagazineFed | 30 | 3.0 | Pending interaction/asset validation |
| KFMod.GoldenBenelliShotgun | TubeFedShotgun | 6 | 0.8999999761581421 | Pending interaction/asset validation |
| KFMod.GoldenChainsaw | PoweredMelee | 0 | 0.0 | Pending interaction/asset validation |
| KFMod.NeonAK47AssaultRifle | MagazineFed | 30 | 3.0 | Pending interaction/asset validation |
| KFMod.NeonKSGShotgun | DualTubePumpShotgun | 12 | 3.1600000858306885 | Pending interaction/asset validation |
| KFMod.GoldenFlamethrower | FuelWeapon | 100 | 4.139999866485596 | Pending interaction/asset validation |
| KFMod.CamoShotgun | PumpShotgun | 8 | 0.6666666865348816 | Pending interaction/asset validation |
| KFMod.GoldenKatana | Melee | 0 | 0.0 | Pending interaction/asset validation |
| KFMod.CamoMP5MMedicGun | MagazineFed | 32 | 3.799999952316284 | Pending interaction/asset validation |
| KFMod.NeonKrissMMedicGun | MagazineFed | 25 | 3.3299999237060547 | Pending interaction/asset validation |
| KFMod.CamoM4AssaultRifle | MagazineFed | 30 | 3.6333000659942627 | Pending interaction/asset validation |
| KFMod.GoldenM79GrenadeLauncher | BreakActionLauncher | 1 | 0.009999999776482582 | Pending interaction/asset validation |
| KFMod.CamoM32GrenadeLauncher | CylinderLauncher | 6 | 1.6339999437332153 | Pending interaction/asset validation |
| KFMod.Axe | Melee | 0 | 0.0 | Pending interaction/asset validation |
| KFMod.knife | Melee | 0 | 0.0 | Live tracked blade/swing/contact fixture passed; wearer and zed/perk calibration pending |
| KFMod.Machete | Melee | 0 | 0.0 | Pending interaction/asset validation |
| KFMod.StunNade | Throwable | ? | ? | Pending interaction/asset validation |
| KFMod.Claws | Melee | ? | ? | Pending interaction/asset validation |
| KFMod.Bat | Melee | ? | ? | Pending interaction/asset validation |
| KFMod.WinchesterDM | LeverAction | ? | ? | Pending interaction/asset validation |
| KFMod.MachinePistol | MagazineFed | ? | ? | Pending interaction/asset validation |
| KFMod.LAWDM | SingleRoundLauncher | ? | ? | Pending interaction/asset validation |
| KFMod.FakeSyringe | Tool | ? | ? | Pending interaction/asset validation |
| KFMod.DualiesDM | DualMagazineFed | ? | ? | Pending interaction/asset validation |
| KFMod.CrossbowSP | SingleRoundBow | ? | ? | Pending interaction/asset validation |
| KFMod.BullpupSP | MagazineFed | ? | ? | Pending interaction/asset validation |
| KFMod.BullpupDM | MagazineFed | ? | ? | Pending interaction/asset validation |
| KFStoryGame.Dummy_JoggingWeapon | StoryTool | ? | ? | Pending interaction/asset validation |
| KFStoryGame.KF_PawnWelder | Tool | ? | ? | Pending interaction/asset validation |


## Deferred VR surface inspection

October 1 wearer feedback: the assault rifle's rear surface is missing when viewed from angles unavailable in flat play. The pistol and other inspected items did not show the same obvious issue to the wearer; coverage across the catalog is unknown. Audit each model from all sides and distinguish absent geometry, backface culling, material flags and missing textures before choosing a repair. Keep repairs separate from the accepted rifle grip/aim transform. No blanket two-sided-material change or mesh repair has been made.


## Bullpup reload asset audit

The installed `Bullpup_Trip` / `BullPup_anim` reload has a usable magazine sequence: proposed normalized insertion markers .62/.76 move the magazine root from 55.469 mm to 5.229 mm from its idle position, along a 50.240 mm segment. Replacement grasp .62 passes the existing palm/finger geometry guards; removal grasp .40 is a possible separate future role. Magazine and translation parent are `Magazine` and `Bullpup`. The root is near the top of the roughly 174 mm magazine, so interaction distances refer to that root rather than its center.

No independent charging-handle or bolt bone exists among the 55 bones or seven animation sequences. `Mag Release` only presses about 4 mm laterally; it is not a charging handle. The existing .07 action-travel value is an unsupported placeholder. Physical rifle reload remains disabled until an explicit usable action presentation/policy exists. The accepted idle palm, support and muzzle transforms are preserved; offline predictions match existing engine measurements within 0.0000171 model UU.

Local provenance and reproducible measurements are in ignored `reports/bullpup-reload-asset-audit.json` and `reports/bullpup_reload_asset_audit.py`. Runtime animation sampling and wearer acceptance remain separate. Rifle integration also needs explicit offhand ownership between support, magazine and action interactions, and must retain stock automatic-fire/perk cadence. Commando changes the stock 40-round capacity to 44/48/50; retained magazines require per-identity limits when that capacity changes.
