# Killing Floor VR

Native VR mod development for the original Killing Floor. Source/builds live in `G:\KFVR`; the installed game is `F:\SteamLibrary\steamapps\common\KillingFloor`. This does not use the unrelated Unity workspace.

**Development checkpoint: native KF stereo, head tracking and left-stick movement are accepted in the headset.** Right-stick snap turns and full 2064 x 2208 images per eye now pass a combined live-game receiver trial. The launcher uses the runtime's recommended resolution (100%) and a 120-second session by default. The standalone development mutator now adds controller-aimed 9mm, dual 9mm, Deagle, Bullpup, knife, syringe and welder tools/weapons, independent tracked hands with capacitive finger input, hip/shoulder holsters, an offhand health/ammunition display, and a controller-operated world-space trader for the supported firearm subset. Controller reloads, physical 9mm/Deagle magazines and slides, dual-9mm belt reloads, Bullpup support-hand aiming, and physical knife sweeps are implemented. Live synthetic-controller checks pass; these additions have not had a wearer test. Wider physical reload/weapon coverage, sustained rendering and the complete mod remain unfinished. Full-resolution CPU transport still limits fresh frame cadence.

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

Original game binaries, maps and assets are preserved. The development launcher adds only our generated `System/KFVRGameplay.u`, tracked by `reports/dev-install.json`. KF may save its normal settings on exit; this installer does not edit INI files. Probe DLLs are copied into private report directories and remain inert in the tested game process until it exits. Nothing auto-loads on a later game launch. Do not copy this diagnostic DLL into System as a mod installer.

For tracked weapons, close KF, run `G:\KFVR\Start-KFVRDev.ps1`, enter the private match, then run `G:\KFVR\Play-KFVR.ps1 -Weapons`. See [tracked weapons and hands](docs/TRACKED_WEAPONS.md) for controls, exact coverage and unattended testing.
