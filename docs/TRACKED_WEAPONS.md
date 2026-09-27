# Tracked weapon and hand development

This increment enables the **9mm only** in an explicitly loaded standalone mutator. Both controllers drive separate KF hand/forearm actors with six-degree-of-freedom poses. Oculus Touch capacitance supplies index/thumb contact, with trigger/grip analog values and fallback on profiles without capacitance. This is controller-driven hand animation, not optical bare-hand tracking.

## Run

Close an existing KF process before staging a changed package:

```powershell
& G:\KFVR\Start-KFVRDev.ps1
# Spawn in the private match, connect the headset, close its dashboard:
& G:\KFVR\Play-KFVR.ps1 -Weapons
```

The normal launcher retains runtime-recommended eye resolution, left-stick locomotion, right-stick snap turns and the bounded 120-second session. Weapon mode requires the installed package to match this build. No wearer test was performed during unattended development.

- Move/rotate the right controller to aim the pistol independently of the head and desktop camera. Right trigger fires. The pistol starts in hand.
- At the right waist, release the grip after squeezing it to stow. Squeeze near the waist to draw. The pistol cannot fire while stowed.
- Moving the left controller moves the offhand independently. Capacitive index/thumb contact and analog grip animate the real KF finger bones.
- Focus/tracking loss stops firing and hides the affected presentation. Return the trigger and grip to neutral before continuing. Mouse movement is not used to aim the tracked pistol.
- Stock keyboard reload still exists. Physical magazine manipulation, controller reload bindings, offhand weapon support and the other weapon families are unfinished.

## Implementation

Native request ABI v11 validates the installed gameplay package, current private player, rig ownership, every member of the 124-byte tracking structure and 20-byte feedback structure. The publisher writes only its own tracking field at the existing game-thread rig Tick. No additional simulation tick is invoked. Body yaw, the camera reference and current pawn displacement align both hands with the native stereo world.

Separate world presentation actors are necessary because the stereo cameras do not run KF's ordinary first-person overlay. The installed 9mm mesh is linked at runtime; the gun and opposing hand branches are hidden on the appropriate instances. The authored palm offset is measured, rotated and subtracted so the actual palm bone follows the controller. The first-person mesh's provisional scale is 300 asset units/metre, separate from the explicit world scale; hand ergonomics and visual polish still need wearer review.

The derived Single/SingleFire retain normal native WeaponFire ticking, cadence, perk-modified spread/damage and ammunition consumption. Only the shot origin/direction and VR recoil presentation are replaced. The native AWeapon tick normally releases fire when desktop bFire is zero; the derived ClientStopFire defers that release only while a fresh, armed VR trigger demands fire. Focus loss/holstering invokes the normal StopFire. No persistent writes to desktop fire input are used.

Shots and holster/draw transitions increment a reflected feedback serial. The native bridge sends bounded, one-shot haptic pulses tied to fresh controller input; a newly launched bridge does not replay old events. A stopped or unfocused bridge drops feedback.

## Private evidence and limits

`tools/native_bridge_trial.py --pid <current> --game <KF> --weapon` runs synthetic controls against actual KF native rendering. `weapon-validation.json` checks three intended shots/three rounds, normal weapon ticking, aim changes with controller rotation, stow/draw, focus loss, held-trigger rearming, palm placement, finger input changes, feedback and presentation cleanup. Receiver images are genuine native eye renders; this is not a headset comfort/visual acceptance test.

`Start-KFVRDev.ps1 -Unattended` additionally auto-readies, enables god mode, delays zed spawning, and marks the local Steam-stat actor as using cheats. These are explicit test-mode behavior, never multiplayer behavior. The mutator refuses non-standalone worlds. Do not treat this development replacement as a finished pickup/trader/drop/dual-wielding adapter.

Only the generated KFVRGameplay.u is staged; original game packages/assets remain untouched. No autoload configuration is added. Normal KF launches without this mutator remain available. The native probe stays inert and resident after teardown until KF exits.
