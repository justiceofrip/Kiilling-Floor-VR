# Physical reload pipeline

The **9mm and Deagle now have live physical reloads** in the standalone development mutator: catch or discard its magazine, retain a partial magazine at the chest or belt, take a fresh chest magazine and insert it, then physically cycle the slide when needed. It uses a game-thread script ledger and the installed model's magazine/slide bones. See [controls and coverage](TRACKED_WEAPONS.md). This has live native-game synthetic-controller evidence, separate from wearer acceptance.

The reusable C++ mechanics below also cover dual-gun belt reloads and stricter continuous insertion geometry. The live dual-9mm adapter now provides the same shared-supply belt interaction through a game-thread script ledger; the four-stage authored-animation adapter remains a separate foundation.

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

Dual rendering uses two actors linked to the installed combined mesh, each hiding the opposite gun branch. Both palms, muzzle bones, magazine/slide bones and bone-translation scales have been measured in the live game. Wearer ergonomics and full arm IK remain unverified.

## Integration boundary

The older AmmoMachine and KFVRAmmoState remain the separately tested timed/tube-policy prototype. PhysicalReload is the new native detachable-magazine ledger; the live KFVRDualMagazineState supplies corresponding magazine identity and shared-reserve accounting in standalone UnrealScript. The live 9mm selects KFVRMagazineState as its reload authority, blocks timed reload entry points, and mirrors each successful stock ConsumeAmmo once. Stock firing still owns cadence, damage and perk behavior. Wider pickup/trader scenarios and server replication remain unverified.

The live single-pistol adapter now seats a magazine through a continuous push while the offhand grip remains held. KFVRMagazineInsertion supplies a one-shot gesture event to KFVRMagazineState; only that ledger moves ammunition. Appearing at the final position, an incomplete push, an over-speed jump, wrong orientation, or a return inside the socket after tracking/recenter interruption cannot seat. Updated wearer acceptance is still pending. Other weapon mechanisms, server inventory and ergonomic refinement remain separate work.

The live standalone pistol adapter now supports both 9mm and Deagle through KFVRWeaponProfile. Independent inventory ledgers preserve each chamber and partial magazines across switches. Bullpup has an explicit magazine-only physical profile: seating completes its reload without an invented action. Timed reload remains available outside physical mode. See WORKLOG.md for consecutive same-process Deagle/9mm acceptance evidence.

The live dual belt gate requires a fresh aligned approach and continuous movement through seating depth; a hand appearing inside the dock after focus/tracking loss cannot seat. The chosen convenience rule assists any empty chamber with one actual magazine round, including a chamber fired after its magazine was removed. A latched pistol cannot fire until withdrawn. Ejection retains partial magazines, and holstering stores presented magazines without adding ammunition. Disabling physical mode requires both magazines seated and both guns clear; it clamps any plus-one loaded count to the stock capacity while preserving the total owned rounds.


Native-hand presentation now has a measured detached-magazine grasp for 9mm and Deagle. It samples the installed reload pose at runtime and anchors its palm at the controller; catch/insertion proximity follows the resulting magazine root. This is independent of the optional four-stage whole-weapon animation scrubber above. The active gun remains in its tracked idle presentation, and physical state continues to come from the live script ledger. The magazine-specific pose has no wearer acceptance yet.


October 1 headset retest rejected magazine clipping/attachment; it did not establish a successful physical catch. Repeated mode toggles exposed a shared presentation bug: an inserted magazine retained an absolute world rotation after physical mode was disabled. The corrected visual path clears that override for seated magazines and clears all mechanical overrides on mode exit. This applies through the common weapon visual rather than per-pistol offset patches. Updated wearer acceptance remains pending.


Tracking interruption while holding a single-pistol magazine now has an explicit recovery rule. Returning with the grip held resumes the same magazine; returning with the hand open stores it with its remaining rounds. Reappearing beside the magwell cannot itself insert or chamber a round. This shared rule applies to native and fallback hand presentation.

The live insertion gate measures complete magazine orientation against the installed socket frame. Forward aim alone was insufficient: a magazine rolled 180 degrees could previously seat. Sideways/upside-down strokes now fail the shared alignment gate; releasing an unseated magazine retains the existing belt-store/drop behavior. The current geometric tolerances still need ergonomic assessment. The socket reference is the magazine bone root, not a measured feed tip or mesh collision surface.


## Shared continuous insertion calibration

`KFVRMagazineSocket` privately samples two authored reload frames and Idle on separate ordinary game ticks. It measures magazine travel relative to the gun chassis, derives the insertion axis toward the installed root, and verifies that the sampled movement approaches that root. Per-weapon profiles provide bone names and animation markers; extracted asset coordinates remain local. Hidden owned weapon visuals preload this measurement before the first tracked reload. Existing palm/grip transforms are unchanged.

The current single-pistol defaults arm at least 5 cm outside the seated root, finish within 1 cm, permit 3.5 cm radial error and require full-frame cosine at least 0.8. Travel above 4 m/s plus 5 mm observation allowance or a sample gap above 150 ms cancels the pending stroke. These are provisional interaction bounds, not collision detection. Magazine roots lie within the magazine body, so the gesture measures the final portion of insertion rather than first contact between the feed tip and well.

The 132-byte tracking packet separates game/presentation `Serial`, fresh-controller `InputSerial`, and unscaled pose timestamp `InputSeconds`. Both single-pistol and belt insertion use controller time, so duplicate game Ticks and ZED-time dilation cannot shorten physical travel intervals. Pose/focus/expiry loss and source-clock faults reach the normal rig Tick and suspend the interaction; a reference-generation change delivers a neutral Tick even when new eye/controller frames arrive together. Delivery uses request ABI15, with reflected package/layout verification before native writes. The source clock subtracts an epoch before float conversion and rejects rollback, mutated/replayed timestamps and large jumps; no input expiry was extended.

For an installed manual-action profile, release the offhand grip after seating before a fresh slide grab. Seating alone does not chamber a round; a retained chamber remains retained, and an empty chamber still needs the existing full slide stroke. No timed reload or animation completion credits ammunition.


## Extending the pistol mechanism

The MK23 development fixture reuses the same magazine ledger, insertion gate, socket sampler and hand-pose path. Its profile adds the installed Magazine/Slide/MK23 bones, reload insertion markers .61/.63, a stable magazine grasp at .51 and a provisional 46 mm mechanical slide stroke measured from the full reload. Its stock-derived weapon/fire adapters retain the installed fire trace implementation and add the existing tracked-muzzle and successful-consumption hooks. No new ammunition algorithm or magazine position offset is needed.

Launch with `Start-KFVRDev.ps1 -Unattended -NativeHands -MK23Test -VRProfile`. This isolated seven-weapon fixture substitutes MK23 for Deagle at the left hip; the normal loadout and conversion list remain unchanged. It does not implement general sidearm-slot selection, MK23 shop/dual conversion, multiplayer or achievement credit for physical reloads. Use `--mk23-reload` with the normal, insertion, interruption or alignment receiver variant. Mixing it with `--deagle-reload` is rejected before startup.


Mechanical translations now project model-space offsets into the measured installed parent frame before applying the engine's measured translation scale. Magazine rotation continues to follow the hand independently of that rest parent frame. Separate magazine/action parent selectors avoid assuming identical hierarchies on future profiles. The legacy dual-belt path remains unchanged until separately audited.


## Magazine capacity changes

Each physical magazine records its own capacity. The weapon's current perk-adjusted capacity controls newly supplied magazines and refilled empty stored shells; a capacity notification does not resize an inserted, held, presented, stored or discarded magazine, change its rounds, or complete a mechanical action. Repeated notifications are no-ops. Invalid capacities leave the ledger untouched.

Binding an existing stock weapon preserves all observed loaded rounds, including a chambered plus-one or a larger load retained after a perk decrease. The initial identity limit is the larger of current supply capacity and observed magazine rounds, within the ledger's existing hard bounds. Later supply uses the current stock capacity. Ordinary mode exit keeps its existing policy: loaded rounds are clamped to the current timed-feed capacity while the total remains conserved as loaded plus reserve.

Live single/dual adapters reconcile capacity after stock weapon updates and publish current and per-slot limits for read-only inspection. Script magazine limits remain 1â€“100 rounds (up to 101 loaded including chamber); the separate portable prototype retains its 1â€“1000 bound. The portable identity ledger and engine-run single/dual ledgers have separate regression suites; the existing AmmoMachine parity stream is a different model and is not evidence of identity-ledger parity.


## Support and reload ownership

A manual magazine/action grasp reserves the offhand before two-handed weapon support chooses a pose. A fresh chest/belt supply-grab attempt gets the same priority, using the current body frame. Seating while the squeeze remains held cannot acquire support: release and squeeze again first. Tracking loss, switching and holstering also require a fresh release/press before support returns. Physical reload still updates after the weapon's current pose, so magazine and action gestures use that completed weapon pose.

Magazine capability alone does not imply an action exists. The installed Bullpup has a magazine and no separate reload action. Its explicit magazine-only policy finishes an empty reload on a successful seat, moving one existing round into the chamber. A retained chamber remains untouched. Unknown weapons receive neither an invented part nor an automatic feed policy.

## Current controls

Use `Start-KFVRDev.ps1 -Unattended -NativeHands -VRProfile -PhysicalReloads` for the current private test. Supported non-dual guns enter physical mode automatically; Y is not needed. Without that option, the legacy Y toggle remains available. Dual pistols retain their separate Y belt-mode toggle.

- Right B releases the magazine. Catching is optional: left grip near the chest supply takes a replacement after ejection. Hold left grip near the gun's measured magazine grasp during ejection only when you want to retain the old magazine.
- Grip the chest supply with the left hand, carry the magazine to the magwell and push it along the measured insertion path. Release a held partial magazine at the chest to retain it; the old belt zone remains available as a secondary supply/stow location.
- For a pistol with an empty chamber, release left grip once after seating, squeeze it near the slide, pull fully and release or guide it forward. Neither left trigger nor right grip is required. The visible native hand attaches to the slide during this grasp.
- Bullpup and the current SCAR adapter finish at magazine insertion. SCAR has a modeled charging handle, stationary during Reload; its side-slap/bolt-release interaction is expressly deferred.

The uncaught magazine presentation is temporary and cannot be picked back up. It disappears when replaced by a held/seated magazine, when tracking/switch cancels it, or after 2.5 seconds. This adds visible release feedback, not dropped-item persistence. Exact palm contact and appearance still need wearer assessment.

## Audited expansion map

An October 1 read-only audit examined the installed mesh weights, animation tracks and SDK. The supplied video could not be fetched. Repeatable local evidence is in `reports/reload_family_asset_audit.py` and `reports/reload-family-asset-audit.json`; proprietary exports stay ignored under `local/reload-family-audit`.

| Weapon | Existing animation behavior | Adapter work still required |
|---|---|---|
| Bullpup | Magazine replacement, no reload action | Current explicit seat-completion policy |
| AK47 | Native bolt after magazine replacement | Magazine grasp/root calibration, then installed rail |
| SCAR | Modeled handle stays still; wearer reference shows a side slap | Current magazine-only adapter; future bolt-release/side-slap interaction deferred |
| M4 | Two magazine meshes plus handle, latch and bolt | Replacement-magazine handoff and coupled action parts |
| AA12 | Handle operates before magazine replacement | Action-before-magazine sequence |
| Pump shotgun | Individual shells, pump and charging handle move together | Tube ledger and coupled pump action |
| Benelli | Shell loading, rotating tray and terminal bolt operation | Shell ledger and bolt interaction |
| KSG | Modified stock loading piece plus pump | Audit its actual loading geometry; do not substitute real-world assumptions |
| BoomStick | Hinge, latch and shell tracks in Fire_Both/Fire_Last, no Reload clip | Two chambers and hinged breech; block stock timed refill |

Except for the Bullpup and SCAR adapters described below, this is an implementation map, not enabled weapon coverage. AK47 still needs its grasp/root calibration; SCAR now has the measured per-profile bound below. M4 switches between two magazine meshes. Simply supplying bone names would produce incorrect interactions. Current 9mm/Deagle 50 mm interaction strokes and action-palm contacts remain provisional; measured reload slide motion is about 38.66/32.60 mm, versus MK23's measured 45.86 mm. All metre values assume the current 300 model-units/metre presentation scale.


## Chest supply and empty-magwell operation

`KFVRAmmoSupplyState` places the shared supply 22 cm forward, 10 cm left and 36 cm below the head in the body frame, with a 16 cm interaction radius. These are provisional ergonomic dimensions. `KFVRAmmoSupplyVisual` loads the installed textured `KillingFloorStatics.L85Ammo` magazine cluster and measures its bounds at runtime. Its 22 cm bounding-sphere diameter and tilt are presentation settings; the pair pictured does not represent an ammunition count. No game mesh or texture is copied into the mod.

The marker appears while the active physical gun has an empty magazine well, a free tracked offhand and stored/loose ammunition. It hides while carrying a magazine, operating the slide, holstered, shopping or missing tracking. The ledger alone supplies ammunition. Taking a replacement before ejecting the inserted magazine remains unsupported because this presentation still shares the gun's single native magazine bone.

A pistol slide can be grabbed and cycled with no magazine inserted. A full pull can extract a chambered round; returning an empty well cannot feed from reserve or stored magazines. The action rail, native palm attachment and tracking-loss cancellation remain shared across pistol profiles.

The SCAR adapter inherits `SCARMK17AssaultRifle` and `SCARMK17Fire`, retaining stock ammunition, damage and Commando ancestry. Installed insertion markers .63/.66 and native magazine grasp .58 reuse the common reload pipeline. Its magazine origin lies farther from the authored palm, so only this audited profile permits a 19 cm root-to-palm bound; others retain 15 cm. This is supported by native finger/surface measurements, not a global tolerance increase. `-SCARTest` substitutes SCAR for the diagnostic Bullpup at the right shoulder. The normal VR trader catalog and multiple-rifle slot selection remain separate work.

## Chambered launchers and flip closure: planned

The [Demolitions audit](DEMOLITIONS_RELOAD_PLAN.md) specifies individual live/spent/empty chambers and chest-supplied rounds. M32 needs a sideways flick to close its native side-opening receiver while preserving the separately indexed cylinder and each round's identity. M79 and BoomStick need a shared break-action flip-close gesture with their own measured hinge basis. Closing empty, partly loaded or full must only close the mechanism; it cannot refill ammunition, reset chamber selection or bypass stock firing/reload timing. These adapters and gestures are not enabled in the current build.
