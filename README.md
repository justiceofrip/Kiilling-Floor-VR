# Killing Floor VR

Native VR mod development for the original Killing Floor. Source/builds live in `G:\KFVR`; the installed game is `F:\SteamLibrary\steamapps\common\KillingFloor`. This does not use the unrelated Unity workspace.

**Development checkpoint: Killing Floor is not yet playable in VR.** Real native tracing and a D3D9 frame capture have succeeded in standalone gameplay. The compiled OpenXR host, stereo transport, camera adapter and gameplay policies are foundations; they are not connected into a complete native stereo game loop.

Implemented and verified:

- Read-only extraction of 2,381 embedded UnrealScript classes from the installed packages. Proprietary source stays in ignored `local/sdk`.
- Read-only native object/property inspection, local player/viewport ownership and standalone-mode verification.
- Bounded x86 native probes: 180 ticks, 180 player-view renders and 180 presents on one thread; no engine tick inside Draw; hooks disabled and original entry bytes verified afterward.
- Native D3D9 multisample-resolved image capture, verified visually. The game uses ordinary D3D9, not D3D9Ex.
- UE2 camera conversion tested against a captured native matrix, plus asymmetric per-eye transforms and explicit world-scale input.
- Prototype non-Ex D3D9-to-D3D11 CPU pair bridge with MSAA resolve and real x86-to-x64 pixel verification. High-resolution transfer cost still needs improvement.
- x86-to-x64 D3D11 stereo texture/pose transport, actual GPU pixel checks, resource generations, partial-pair cancellation and peer-loss recovery.
- Conserved magazine/chamber/hand-magazine/reserve accounting, tactical and empty reloads, manual cycling, tube shells, replay rejection, mechanical timings and interrupted-action handling.
- Physical melee eligibility with travel, speed, retraction and cooldown; 58-byte quantized tracked-pose packets with validation.
- Portable comfort, recenter, weapon/UI math, rig topology checks and exact previous-write restoration retained from the 2142/BC2 work.
- x64 OpenXR lifecycle, headset/eye poses and projection swapchains. Added grip/aim poses, trigger/grip/stick/menu actions and bounded haptics for standard controller profiles. Controller/haptic IPC also passes a real x64/x86 process test, including lost-focus/stale-input release. KF input binding and hardware controller acceptance remain pending.

Start with [BUILDING](BUILDING.md), [TESTING](TESTING.md), [ARCHITECTURE](ARCHITECTURE.md), [feasibility](docs/FEASIBILITY.md), [weapon coverage](docs/WEAPONS.md), and [WORKLOG](WORKLOG.md).

Original game binaries, maps, settings and assets have not been changed by this project. Probe DLLs are copied into private report directories and remain inert in the tested game process until it exits. Nothing auto-loads on a later game launch. Do not copy this diagnostic DLL into System as a mod installer.
