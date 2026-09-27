# Tracked weapon and hand development

This increment enables the **9mm, dual 9mm, Deagle, Bullpup and knife** in an explicitly loaded standalone mutator. Both controllers drive separate KF hand/forearm actors with six-degree-of-freedom poses. Oculus Touch capacitance supplies index/thumb contact, with trigger/grip analog values and fallback on profiles without capacitance. This is controller-driven hand animation, not optical bare-hand tracking.

## Run

Close an existing KF process before staging a changed package:

```powershell
& G:\KFVR\Start-KFVRDev.ps1
# Spawn in the private match, connect the headset, close its dashboard:
& G:\KFVR\Play-KFVR.ps1 -Weapons
```

The normal launcher retains runtime-recommended eye resolution, left-stick locomotion, right-stick snap turns and the bounded 120-second session. Weapon mode requires the installed package to match this build. No wearer test was performed during unattended development.

- Move/rotate the right controller to aim the pistol independently of the head and desktop camera. Right trigger fires. The pistol starts in hand.
- Hip/shoulder slots follow body heading: 9mm at the right waist, Deagle at the left waist, Bullpup over the right shoulder, knife over the left shoulder, syringe on the left chest, welder on the right chest. Squeeze the right grip near a slot to draw that owned weapon. Release it near the active slot to stow. Stowed weapons cannot fire. Left X cycles supported inventory weapons.
- The knife uses physical swings after a short settle outside its holster. At least 20 cm of travel and 1.2 m/s are required; retract before another strike. Walking/snap turns do not count. Its blade endpoint is a provisional 20 cm offset from the installed knife bone and still needs wearer calibration. No trigger attack is generated while the physical knife is active.
- Dual 9mm pistols use independent controller poses, triggers, recoil, muzzle rays and loaded-round counts. Right trigger fires the right pistol; left trigger fires the left. Right B performs the stock paired timed reload until physical mode is enabled. The pair draws/stows from the right hip as one inventory weapon; the offhand pistol docks at the left-front waist. Left Y toggles physical belt reloads. Point the pistols down and press right B to retain their old magazines and present replacements at the belt. Bring each well to its magazine, push toward the body to seat, then withdraw before firing. Empty chambers receive belt slide assistance; tactical chambers remain intact. Each side works independently.
- Right A targets a pointed-at door while retaining its stock trigger proximity and lock rules; otherwise it calls KF's normal nearby Use action once per press. Real Offices door opening/closing passes; trader and VR menu navigation remain unverified.
- Outside physical mode, right B performs the stock timed reload. Its ammunition/perk timing remains the normal KF path. The idle world proxy does not yet display those timed reload animations.
- With the Bullpup, squeeze the left grip near the foregrip to support the rifle. Both hand positions then determine its aim, with dominant-hand roll retained. Release the left grip to return to one-hand aiming.
- A free offhand near you shows a native-world status card with health, armor, loaded/spare rounds, and reload prompts. It follows the hand, faces your head position, and hides while that hand grips/fires or tracking is lost. Dual pistols show separate left/right loaded counts. Its size/placement remain provisional until wearer review.
- Held weapons retain a resting finger grasp after the grip button is released. A free hand can open; capacitive thumb/index input still layers onto the weapon grasp. Support-hand and held-magazine poses use their own grasp role.
- Moving the left controller moves the offhand independently. Capacitive index/thumb contact and analog grip animate the real KF finger bones.
- Focus/tracking loss stops firing and hides the affected presentation. Return the trigger and grip to neutral before continuing. Mouse movement is not used to aim the tracked pistol.
- With the 9mm or Deagle selected, left Y toggles physical reload mode. The mode has its own haptic confirmation. Toggle it off only with a magazine seated, the offhand empty and the slide closed. While enabled, timed/keyboard reload callbacks are blocked.
- In physical mode, hold the left grip beneath the pistol and press right B to catch the magazine. Ejecting without catching discards its remaining rounds. A chambered round remains usable.
- Move a held magazine to the left-front belt and release grip to retain it. Squeeze there again to take a replacement. Bring it to the pistol's magazine well, align it with the gun, and release grip to insert. Releasing elsewhere discards it.
- An empty pistol requires cycling: pinch the slide with left trigger plus grip, pull back about 5 cm, then release. A short pull does not chamber a round; pulling a live chamber ejects that round. Tactical reloads retain the chamber for 15+1 on the 9mm or 8+1 on the Deagle.
- Switching or holstering retains a magazine held in the offhand and cancels an unfinished slide grab. Tracking loss cancels the grab without completing a reload.
- Full arm IK, left-handed weapon dominance, the remaining dual-pistol families and weapons and complete pickup/trader integration are unfinished.

## Implementation

Native request ABI v12 validates the installed gameplay package, current private player, rig ownership, every member of the 124-byte tracking structure and 28-byte feedback structure. The publisher writes only its own tracking field at the existing game-thread rig Tick. No additional simulation tick is invoked. Body yaw, the camera reference and current pawn displacement align both hands with the native stereo world.

Separate world presentation actors are necessary because the stereo cameras do not run KF's ordinary first-person overlay. The installed weapon meshes are linked at runtime; the 9mm supplies the separate hands; the gun and opposing hand branches are hidden on the appropriate instances. The authored palm offset is measured, rotated and subtracted so the actual palm bone follows the controller. The first-person mesh's provisional scale is 300 asset units/metre, separate from the explicit world scale; hand ergonomics and visual polish still need wearer review.

The derived Single, Deagle and Bullpup weapons/fire modes retain normal native WeaponFire ticking, cadence, perk-modified spread/damage and ammunition consumption. Only the shot origin/direction and VR recoil presentation are replaced. The native AWeapon tick normally releases fire when desktop bFire is zero; the derived ClientStopFire defers that release only while a fresh, armed VR trigger demands fire. Focus loss/holstering invokes the normal StopFire. No persistent writes to desktop fire input are used.

Shots and holster/draw transitions increment separate reflected left/right feedback serials. The native bridge sends bounded, one-shot haptic pulses tied to fresh controller input; a newly launched bridge does not replay old events. A stopped or unfocused bridge drops feedback. Shared-channel v3 coalesces unread hand pulses without extending their original expiry, clears consumed feedback, and never replays a previously consumed opposite-hand pulse.


Physical pistol reload authority is game-thread `KFVRMagazineState`, connected through `KFVRManualReload` and the stock `ConsumeAmmo` callback. It retains up to 64 magazine identities, separate loose reserve and chamber state. Stock shots remain responsible for rate, damage and native consumption; the ledger mirrors each successful shot once, then publishes the mechanically loaded count. Pickups adjust the owned budget without silently chambering a round. The standalone script adapter is separate from the portable single/dual C++ prototype and makes no multiplayer authority claim.

Each inventory weapon retains its own physical ledger across switching. Magazine and slide bones come from a shared profile of the installed model. The Bullpup remains on timed reload: its shipped skeleton has magazine/release/trigger bones but no independent charging-handle or bolt bone. A rendered calibration step measures UE2's extra import scale for bone translation. A held magazine follows offhand position and orientation; skeletal changes become visible through the ordinary animation cache. The support-hand solver uses the model's measured left/right palm offsets and a shortest-rotation alignment, independently of head direction.

## Private evidence and limits

`tools/native_bridge_trial.py --pid <current> --game <KF> --weapon` runs synthetic controls against actual KF native rendering. `weapon-validation.json` checks three intended shots/three rounds, normal weapon ticking, aim changes with controller rotation, stow/draw, focus loss, held-trigger rearming, palm placement, finger input changes, feedback and presentation cleanup. Receiver images are genuine native eye renders; this is not a headset comfort/visual acceptance test.

`--melee` exercises physical knife contacts against an explicit private collision fixture; launch with `Start-KFVRDev.ps1 -Unattended -MeleeTest`. This validates contact dispatch, not live zed/perk outcomes.

`--inventory` exercises the firearm slots alongside the knife, timed reloads and rifle support; `--reload` and `--deagle-reload` exercise the physical pistol stages, stock shot counts, conservation and magazine bone attachment. Their validators distinguish actual script behavior from packet delivery.

`Start-KFVRDev.ps1 -Unattended` additionally auto-readies, provides the three diagnostic weapons, enables god mode, delays zed spawning, and marks the local Steam-stat actor as using cheats. These are explicit test-mode behavior, never multiplayer behavior. The mutator refuses non-standalone worlds. Do not treat this development replacement as a finished pickup/trader/drop/dual-wielding adapter.

Only the generated KFVRGameplay.u is staged; original game packages/assets remain untouched. No autoload configuration is added. Normal KF launches without this mutator remain available. The native probe stays inert and resident after teardown until KF exits.

Dual acceptance: `Start-KFVRDev.ps1 -Unattended -DualTest` supplies the stock pair before conversion, and `native_bridge_trial.py --dual` checks independent and simultaneous shots, left-feed exhaustion, reloads, holstering and tracking recovery. It retains one stock ammo pool; KFVRDualFeedState only divides the mechanically loaded count for this timed-reload adapter. When physical mode is enabled, KFVRDualMagazineState replaces that loaded-feed allocation with a shared magazine identity pool and separate chamber/latch state. KF normally replaces the single 9mm when the pair is acquired.

Dual belt acceptance: `native_bridge_trial.py --dual-belt` drives four reload exchanges, independent withdrawal/fire, empty-chamber assistance, focus/hand interruption, retained magazines across holstering, and return to normal timed mode. Both magazine bones are measured in the real rendered game. Current evidence: native-bridge-20260927-173833, 28 checks, fourteen stock shots and eight successful insertions. No wearer ergonomics or online authority is implied.

The status card is an authored two-triangle static mesh with a private pooled ScriptedTexture and stock runtime font. Tick captures the text/vitals snapshot; RenderTexture only draws that snapshot. The texture revision changes only with displayed information, not for each eye. The material writes opaque depth and respects world occlusion. Destroyed display actors release their pooled surface and clear client/material references. No game art is redistributed.

The syringe uses its own chest slot. Bring the needle toward your chest and squeeze right trigger to inject; contact and trigger must remain valid through the stock injection delay. Stock health, charge cost, recharge and perk handling remain in the installed healing functions. Focus loss, hand loss, stowing or leaving contact cancels an unfinished injection. The needle endpoint is provisional (15 cm from the installed Syringe bone); the actual bone axis determines its direction. Teammate contact is implemented but has no live acceptance yet. Outside active VR, the normal desktop healing path remains available.

`Start-KFVRDev.ps1 -Unattended -HealTest` creates the explicit health-40 fixture; `native_bridge_trial.py --heal` tests it. Evidence native-bridge-20260927-182659 passes 17 healing and ten display/grasp checks, including actual health 40 -> 90, one 500-charge spend, stock recharge, an interrupted injection, and chest draw/stow/rearm. Native images were viewed; no wearer test occurred.

The welder draws from the right chest. Hold right trigger with the flame tip near a closed door to weld; hold left trigger before beginning the right-trigger stroke to select unwelding. The installed flame_base/flame_tip bones define its working ray. Head-to-tip and head-to-hand clearance include moving collision, and stock range still bounds contact. Native charge consumption/recovery, fire cadence, perk modifier, door locks and shared door-group weld rules remain in effect. A cancelled welder stroke retains the stock up-front charge cost. Focus/tracking loss and stowing cancel remaining work and require neutral right trigger before resuming.

`Start-KFVRDev.ps1 -Unattended -DoorTest` explicitly positions the local pawn beside an existing closed map door; it creates no fake door and edits no map asset. `native_bridge_trial.py --weld` passes 19 door/tool and ten display checks in 184225, including weld, unweld to unsealed, controller open/close, interruptions and charge recovery. Captures were viewed. This is standalone synthetic-control acceptance, not a wearer or server test.
