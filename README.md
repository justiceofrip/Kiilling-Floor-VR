# Kiilling Floor VR

Pre-alpha candidate: see [test instructions and controls](docs/PRE_ALPHA_TEST.md) and [candidate release notes](docs/PRE_ALPHA_RELEASE.md). Publication is on hold after the first wearer pass reported head/hand alignment problems. Corrective work is underway.

Native VR mod development for the original Killing Floor. Source/builds live in `G:\KFVR`; the installed game is `F:\SteamLibrary\steamapps\common\KillingFloor`. This does not use the unrelated Unity workspace.

**Current status: the October 1 follow-up headset test accepts thumb touch, weapon grips and steady aiming while walking on build `20a6bd4`.** The wearer described the thumb as perfect and the grips as great, and confirmed that the gun stayed steady while walking. This follows the earlier acceptance of basic movement and the actual KF hands as a proof of concept. Remaining work includes individual weapon/art polish and rendering issues. Detached 9mm/Deagle magazines have measured native hand grasps. A later wearer test rejected magazine attachment/clipping; shared rotation-lifecycle, interrupted-grip recovery and continuous full-frame insertion corrections now pass desktop regression. Updated wearer acceptance remains pending. An opt-in MK23 fixture now reuses the same reload pipeline and passes desktop checks. The Bullpup now uses its installed magazine-only reload: seating finishes the reload without an invented charging handle. The shared action rail remains for installed pistol slides and future audited mechanisms. Private `-PhysicalReloads` sessions automatically enable supported physical reloads, with B releasing the magazine and left grip alone grabbing a slide; this revision needs wearer acceptance. Physical insertion, knife motion and grenade velocity now use unscaled controller pose time. Insertion passes a live slow-motion regression; knife contact and actual grenade projectile regressions also pass. Tool alignment, head/HUD comfort, complete weapon coverage and release readiness were not separately accepted. Publication remains on hold.

**New local hand option:** actual KF hand geometry now has a separate conversion/animation path with capacitive fingers and closed wrists. It is opt-in while appearance and wearer comfort are evaluated. See [native hand build and launch](docs/NATIVE_HANDS.md). Hands now preload hidden during match startup rather than waiting for the first tracking packet.

The development mutator includes controller-aimed 9mm, dual 9mm, Deagle, Bullpup, knife, syringe, welder and stock grenades, capacitive finger poses, holsters, an offhand status display and an initial VR trader. Single-pistol physical reloads, dual belt reloads, rifle support and physical melee/throws are implemented. This remains a standalone prototype with incomplete weapon/menu/network coverage. The launcher requests the runtime's recommended resolution, fits it to the current transport budget and defaults to a bounded 120-second session. Full-resolution CPU transport still limits fresh frame cadence; stationary receiver measurements have reached about 50 fresh stereo pairs/sec at 2064 x 2208 per eye, without headset presentation.

Implemented and verified:

- Read-only extraction of 2,381 embedded UnrealScript classes from the installed packages. Proprietary source stays in ignored `local/sdk`.
- Read-only native object/property inspection, local player/viewport ownership and standalone-mode verification.
- Bounded x86 native probes: 180 ticks, 180 player-view renders and 180 presents on one thread; no engine tick inside Draw; hooks disabled and original entry bytes verified afterward.
- Native stereo world-render trials with fresh private cameras/targets, asymmetric frusta, synthetic head translation/rotation, visually verified parallax and measured extra callbacks. The accepted full-length headset run submitted 445 fresh pairs plus 192 retained-frame submissions; sustained operation remains open.
- Native D3D9 multisample-resolved image capture, verified visually. The game uses ordinary D3D9, not D3D9Ex.
- UE2 camera conversion tested against a captured native matrix, plus asymmetric per-eye transforms and explicit world-scale input.
- Prototype non-Ex D3D9-to-D3D11 CPU pair bridge with MSAA resolve and real x86-to-x64 pixel verification. High-resolution transfer cost still needs improvement.
- x86-to-x64 D3D11 stereo texture/pose transport, actual GPU pixel checks, resource generations, partial-pair cancellation and peer-loss recovery.
- A compiled KFVRGameplay UnrealScript package, with 30,059 engine-run checks and C++/script action-stream comparison. Seven standalone weapon/tool adapters are live; broader weapon coverage remains pending.
- Physical detachable-magazine pipeline for single/dual guns: magazine identities, shared belt supply, continuous insertion/action travel and independent chambers. Controller weapon/muzzle transforms and a four-stage UE2 pose driver also compile and pass headless tests; a separate game-thread script ledger now binds physical pistol reloads to actual KF ammunition. Dual-9mm belt reloads now also run in the standalone game with shared supply, separate chambers, retained magazines and belt slide assistance. See [manual reloads](docs/MANUAL_RELOAD_INTERACTIONS.md).
- Conserved magazine/chamber/hand-magazine/reserve accounting, tactical and empty reloads, manual cycling, tube shells, replay rejection, mechanical timings and interrupted-action handling.
- Physical melee eligibility with travel, speed, retraction and cooldown; 58-byte quantized tracked-pose packets with validation.
- Portable comfort, recenter, weapon/UI math, rig topology checks and exact previous-write restoration retained from the 2142/BC2 work.
- x64 OpenXR lifecycle, headset/eye poses and projection swapchains. Added grip/aim poses, trigger/grip/stick/menu actions and bounded haptics for standard controller profiles. Controller/haptic IPC also passes a real x64/x86 process test, including lost-focus/stale-input release. The physical diagnostic now verifies headset presentation and both controllers' triggers, grips, sticks and haptics. The bounded native left-stick binding passes a live engine movement/release trial; the wearer accepted left-stick movement. See [headset testing](docs/HEADSET_TESTING.md).

Start with [BUILDING](BUILDING.md), [TESTING](TESTING.md), [ARCHITECTURE](ARCHITECTURE.md), [feasibility](docs/FEASIBILITY.md), [weapon coverage](docs/WEAPONS.md), and [WORKLOG](WORKLOG.md).

For the current development build, start a solo match with a living player, connect the headset and close the SteamVR dashboard, then run:

```powershell
& G:\KFVR\Play-KFVR.ps1
```

Optional `-Scale 75`, `-Seconds 120` and `-SnapDegrees 30` control resolution, session length and right-stick angle. This remains an explicitly launched, bounded development session; map travel requires a new launch. No headset test is needed during the owner's current unattended session.

Original game binaries, maps and assets are preserved. The ordinary development launcher adds our generated `System/KFVRGameplay.u`, tracked by `reports/dev-install.json`. Explicit `-NativeHands` also stages the separately recorded local-only `KFVRLocalHands.u`; original KF packages remain untouched. KF may save its normal settings on exit; this installer does not edit INI files. Probe DLLs are copied into private report directories and remain inert in the tested game process until it exits. Nothing auto-loads on a later game launch. Do not copy this diagnostic DLL into System as a mod installer.

For tracked weapons, close KF, run `G:\KFVR\Start-KFVRDev.ps1`, enter the private match, then run `G:\KFVR\Play-KFVR.ps1 -Weapons`. See [tracked weapons and hands](docs/TRACKED_WEAPONS.md) for controls, exact coverage and unattended testing.
