# Physical reload pipeline

The **9mm and Deagle now have live physical reloads** in the standalone development mutator: catch or discard its magazine, retain a partial magazine at the belt, take and insert a replacement, then physically cycle the slide when needed. It uses a game-thread script ledger and the installed model's magazine/slide bones. See [controls and coverage](TRACKED_WEAPONS.md). This has live native-game synthetic-controller evidence, separate from wearer acceptance.

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

The live standalone pistol adapter now supports both 9mm and Deagle through KFVRWeaponProfile. Independent inventory ledgers preserve each chamber and partial magazines across switches. Bullpup physical cycling is deferred because its installed skeleton has no separate action bone; it retains stock timed reload. See WORKLOG.md for consecutive same-process Deagle/9mm acceptance evidence.

The live dual belt gate requires a fresh aligned approach and continuous movement through seating depth; a hand appearing inside the dock after focus/tracking loss cannot seat. The chosen convenience rule assists any empty chamber with one actual magazine round, including a chamber fired after its magazine was removed. A latched pistol cannot fire until withdrawn. Ejection retains partial magazines, and holstering stores presented magazines without adding ammunition. Disabling physical mode requires both magazines seated and both guns clear; it clamps any plus-one loaded count to the stock capacity while preserving the total owned rounds.


Native-hand presentation now has a measured detached-magazine grasp for 9mm and Deagle. It samples the installed reload pose at runtime and anchors its palm at the controller; catch/insertion proximity follows the resulting magazine root. This is independent of the optional four-stage whole-weapon animation scrubber above. The active gun remains in its tracked idle presentation, and physical state continues to come from the live script ledger. The magazine-specific pose has no wearer acceptance yet.


October 1 headset retest rejected magazine clipping/attachment; it did not establish a successful physical catch. Repeated mode toggles exposed a shared presentation bug: an inserted magazine retained an absolute world rotation after physical mode was disabled. The corrected visual path clears that override for seated magazines and clears all mechanical overrides on mode exit. This applies through the common weapon visual rather than per-pistol offset patches. Updated wearer acceptance remains pending.


Tracking interruption while holding a single-pistol magazine now has an explicit recovery rule. Returning with the grip held resumes the same magazine; returning with the hand open stores it with its remaining rounds. Reappearing beside the magwell cannot itself insert or chamber a round. This shared rule applies to native and fallback hand presentation.

The live insertion gate measures complete magazine orientation against the installed socket frame. Forward aim alone was insufficient: a magazine rolled 180 degrees could previously seat. Sideways/upside-down strokes now fail the shared alignment gate; releasing an unseated magazine retains the existing belt-store/drop behavior. The current geometric tolerances still need ergonomic assessment. The socket reference is the magazine bone root, not a measured feed tip or mesh collision surface.


## Shared continuous insertion calibration

`KFVRMagazineSocket` privately samples two authored reload frames and Idle on separate ordinary game ticks. It measures magazine travel relative to the gun chassis, derives the insertion axis toward the installed root, and verifies that the sampled movement approaches that root. Per-weapon profiles provide bone names and animation markers; extracted asset coordinates remain local. Hidden owned weapon visuals preload this measurement before the first tracked reload. Existing palm/grip transforms are unchanged.

The current single-pistol defaults arm at least 5 cm outside the seated root, finish within 1 cm, permit 3.5 cm radial error and require full-frame cosine at least 0.8. Travel above 4 m/s plus 5 mm observation allowance or a sample gap above 150 ms cancels the pending stroke. These are provisional interaction bounds, not collision detection. Magazine roots lie within the magazine body, so the gesture measures the final portion of insertion rather than first contact between the feed tip and well.

The 132-byte tracking packet separates game/presentation `Serial`, fresh-controller `InputSerial`, and unscaled pose timestamp `InputSeconds`. Both single-pistol and belt insertion use controller time, so duplicate game Ticks and ZED-time dilation cannot shorten physical travel intervals. Pose/focus/expiry loss and source-clock faults reach the normal rig Tick and suspend the interaction; a reference-generation change delivers a neutral Tick even when new eye/controller frames arrive together. Delivery uses request ABI15, with reflected package/layout verification before native writes. The source clock subtracts an epoch before float conversion and rejects rollback, mutated/replayed timestamps and large jumps; no input expiry was extended.

After seating, release the offhand grip before a fresh slide grab. Seating does not chamber a round; a retained chamber remains retained, and an empty chamber still needs the existing full slide stroke. No timed reload or animation completion credits ammunition.


## Extending the pistol mechanism

The MK23 development fixture reuses the same magazine ledger, insertion gate, socket sampler and hand-pose path. Its profile adds the installed Magazine/Slide/MK23 bones, reload insertion markers .61/.63, a stable magazine grasp at .51 and a provisional 46 mm mechanical slide stroke measured from the full reload. Its stock-derived weapon/fire adapters retain the installed fire trace implementation and add the existing tracked-muzzle and successful-consumption hooks. No new ammunition algorithm or magazine position offset is needed.

Launch with `Start-KFVRDev.ps1 -Unattended -NativeHands -MK23Test -VRProfile`. This isolated seven-weapon fixture substitutes MK23 for Deagle at the left hip; the normal loadout and conversion list remain unchanged. It does not implement general sidearm-slot selection, MK23 shop/dual conversion, multiplayer or achievement credit for physical reloads. Use `--mk23-reload` with the normal, insertion, interruption or alignment receiver variant. Mixing it with `--deagle-reload` is rejected before startup.


Mechanical translations now project model-space offsets into the measured installed parent frame before applying the engine's measured translation scale. Magazine rotation continues to follow the hand independently of that rest parent frame. Separate magazine/action parent selectors avoid assuming identical hierarchies on future profiles. The legacy dual-belt path remains unchanged until separately audited.


## Magazine capacity changes

Each physical magazine records its own capacity. The weapon's current perk-adjusted capacity controls newly supplied magazines and refilled empty stored shells; a capacity notification does not resize an inserted, held, presented, stored or discarded magazine, change its rounds, or complete a mechanical action. Repeated notifications are no-ops. Invalid capacities leave the ledger untouched.

Binding an existing stock weapon preserves all observed loaded rounds, including a chambered plus-one or a larger load retained after a perk decrease. The initial identity limit is the larger of current supply capacity and observed magazine rounds, within the ledger's existing hard bounds. Later supply uses the current stock capacity. Ordinary mode exit keeps its existing policy: loaded rounds are clamped to the current timed-feed capacity while the total remains conserved as loaded plus reserve.

Live single/dual adapters reconcile capacity after stock weapon updates and publish current and per-slot limits for read-only inspection. Script magazine limits remain 1–100 rounds (up to 101 loaded including chamber); the separate portable prototype retains its 1–1000 bound. The portable identity ledger and engine-run single/dual ledgers have separate regression suites; the existing AmmoMachine parity stream is a different model and is not evidence of identity-ledger parity.
