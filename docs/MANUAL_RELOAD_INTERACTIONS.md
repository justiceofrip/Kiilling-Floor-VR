# Physical reload pipeline

The **9mm now has live physical reloads** in the standalone development mutator: catch or discard its magazine, retain a partial magazine at the belt, take and insert a replacement, then physically cycle the slide when needed. It uses a game-thread script ledger and the installed model's magazine/slide bones. See [controls and coverage](TRACKED_WEAPONS.md). This has live native-game synthetic-controller evidence, separate from wearer acceptance.

The reusable C++ mechanics below also cover dual-gun belt reloads and stricter continuous insertion geometry. Those dual mechanics and the four-stage authored-animation adapter remain a separate tested foundation; they are not yet live dual-wield gameplay.

## Implemented behavior

| Stage | Physical input | Mechanical result |
| --- | --- | --- |
| Release | Explicit magazine release; dual belt mode also requires the barrel pointed down | Magazine leaves its gun; the chambered round survives |
| Hold / present | A retained hand magazine or a magazine presented at a belt dock | Magazine identity and remaining rounds are retained |
| Insert | Aligned approach from outside the magwell, then continuous travel to seating depth | The actual magazine moves into the selected gun once |
| Cycle | Grab the action at its current position, pull and return/release | Extraction and feeding occur at separate physical travel thresholds |

`PhysicalReload` owns one or two identical-caliber/capacity, closed-bolt self-loading guns and one shared reserve. Every magazine has an identity, round count and location: gun, hand, belt, storage or discarded. Tactical reloads retain the chamber. Partially loaded magazines retain their exact contents. Loose reserve creates replacement magazines without merging away partial ones. Storage is bounded, and empty retained shells can be reused. Available + spent + discarded/ejected rounds always equals the initial budget.

A partial action pull before extraction keeps the chambered round. Crossing extraction ejects it; reaching full travel arms feeding, and returning to battery feeds one round. An interrupted grab preserves completed mechanical state. Re-grabbing starts at the current action position. No animation timer completes these operations. The normalized thresholds in the generic profile are diagnostic defaults awaiting per-weapon calibration.

`InsertionGesture` checks socket alignment, approach direction, travel, speed, tracking, reference generation and sample order. `ActionStrokeGesture` maps hand travel onto the action axis. Both reject invalid/stale/replayed poses and require appropriate fresh physical input after interruption. The caller must supply focused, unexpired tracking; these are local integration primitives, not a network anti-cheat or server-pose validator.

## Dual-wield belt reload

`BeltReloadSession` connects geometry to the same ammunition ledger and command sequence for both pistols:

1. Deliberately point each gun down and press magazine release. Its old magazine goes to retained storage with its remaining rounds.
2. Two upward-facing replacement magazines present at the belt docks, subject to the shared ammunition supply.
3. Bring each empty magwell onto its magazine and push until seated. Each side works independently, including simultaneous seating.
4. Lift away from the dock before that gun can fire again.

The session emits separate ejection, presentation, seating and clearance events for visual/haptic adapters. Optional dual slide assist releases a locked-back slide after seating; it does not eject a live chambered round. The owner explicitly prefers this quick belt reload because both hands hold guns. One test with 20 spare rounds gives one dock 15 and the other 5, with no duplication. Partial magazines are available for later reuse.

`BeltMagazinePoses` follows an explicit body heading and a head-derived waist estimate. Pitching the head down does not tilt the belt. Actual waist tracking and physical placement calibration remain future work.

## Weapon and animation presentation

`SolveTrackedWeapon` attaches the weapon and its muzzle to a calibrated controller grip using the same body/reference transform as the eye camera. The current eye/head pose is not an input. Looking around therefore cannot drag a held gun or its proposed shot direction. The live rig now applies them to its native-world presentation and stock shot traces for Single, Deagle and Bullpup.

`KFVRReloadPoseProfile` provides four independently authored animation ranges. `KFVRReloadVisual` is a separate presentation actor: it links a weapon mesh and skins, samples the selected range with SetAnimFrame, and stops animation advancement. It is not a KFWeapon and contains no ammo-crediting callbacks. It is compiled but not spawned automatically. The profile tests use synthetic frame ranges; actual release/hold/insert/action markers have not been measured on the assets.

Read-only `tools/extract_reload_profiles.py` now obtains the six pistol/dual class defaults, actual mesh references, Reload animation names, fire classes, scales and muzzle-bone names. It follows MeshRef for assets loaded lazily and treats Unreal object names case-insensitively. Raw results stay in ignored reports/reload-assets-20260927.json. They are class defaults, not current perk-modified stats.

Dual rendering needs independently attached gun models. The stock combined dual-pistol mesh/animation is not an independent two-controller rig. Grip, muzzle, magwell, magazine-tip and action transforms, plus authored hand visibility/IK, still need calibration.

## Integration boundary

The older AmmoMachine and KFVRAmmoState remain the separately tested timed/tube-policy prototype. PhysicalReload is the new native detachable-magazine ledger; its dual magazine/object state has not been ported into the UnrealScript authority. The live 9mm selects KFVRMagazineState as its reload authority, blocks timed reload entry points, and mirrors each successful stock ConsumeAmmo once. Stock firing still owns cadence, damage and perk behavior. Wider pickup/trader scenarios and server replication remain unverified.

The live Single adapter is an early manual-reload implementation, with no wearer acceptance yet. Its current insertion uses proximity, alignment and a deliberate grip release; the portable continuous insertion-stroke checks are not yet connected. Next work includes dual independent presentation, other weapon mechanisms, server inventory and ergonomic refinement. Routine development continues without repeated wearer prompts.

The live standalone pistol adapter now supports both 9mm and Deagle through KFVRWeaponProfile. Independent inventory ledgers preserve each chamber and partial magazines across switches. Bullpup physical cycling is deferred because its installed skeleton has no separate action bone; it retains stock timed reload. See WORKLOG.md for consecutive same-process Deagle/9mm acceptance evidence.
