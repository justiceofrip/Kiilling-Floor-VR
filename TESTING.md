# Testing and evidence limits

Build.ps1 runs CTest for each architecture. Current deterministic coverage includes stereo math/culling, exact pose/image matching, action focus/ownership, rig-write restoration, UI smoothing, arm-pole math, ammo conservation and interruption, melee gating, pose decoding, UE2 camera conversion, and an x86-only MinHook calling-convention/restore regression.

New ammo tests exercise tactical full-mag-plus-chamber behavior, empty reload/charging, racking a live round, fire cadence, remove/stow/discard/partial magazines, switching/death/drop cancellation, stale ownership, replay/high-latency rejection, correction validation, tube feeding, manual cycling and 100,000 randomized actions with conservation checks. Melee tests include 10,000 wrist-waggle samples, retraction, cooldown, invalid tracking, teleports and range limits. Pose tests include all truncated packet lengths, invalid quaternions, quantization overflow, sequence wrap and server-clock gates.

OpenXR controller tests use a fake API to cover bindings, predicted timing, focus/tracking loss, invalid samples, haptic limits, runtime errors and cleanup after partial initialization. Latest CTest results: x86 16/16, x64 15/15.

Hardware checks are separate from unit tests:

```powershell
& G:\KFVR\build\x86\KFVRBridgeProbe.exe --consumer-exe G:\KFVR\build\x64\KFVRBridgeProbe.exe
& G:\KFVR\build\x64\KFVRIpcProbe.exe --producer-exe G:\KFVR\build\x86\KFVRIpcProbe.exe
& G:\KFVR\build\x64\KFVRSceneProbe.exe G:\KFVR\reports\synthetic-scene
```

The first probe checks all pixels in eight real-GPU stereo pairs across formats, including backpressure and stale tickets. The IPC probe checks four synthetic pairs, changing head poses, discarded-pair recreation, peer exit and abandoned mutex. The scene is a clearly labeled synthetic room, not KF gameplay.

For an already-running **standalone** KF match, after compiling x86:

```powershell
py -3.12 G:\KFVR\tools\trace_kf.py --pid GAME_PID --game 'F:\SteamLibrary\steamapps\common\KillingFloor'
```

This is an explicit native injection/hook experiment. It rechecks the executable, reflected local controller/viewport/level, NM_Standalone, exported function bytes with relocations and device/Present instruction relationship. It samples at most 180 frames/12 seconds, never changes cameras or replays rendering, disables its hooks and checks restored entry bytes. It captures one native color image and camera matrix set. DLL/trampolines stay inert until process exit. Reports include exact DLL/launcher hashes. No public/VAC server experiments.

Observed native traces passed 180 frames on KF-Manor and KF-Biohazard. A native image capture succeeded at 1024x768, X8R8G8B8, 8x MSAA after resolving into a private non-MSAA surface. Tests prove a known native boundary and readback, **not stereo replay safety**. The unit camera fixture comes from a real FCameraSceneNode camera matrix.

Headset checks require an available HMD/runtime. No headset-visible KF stereo or physical controller acceptance has happened. Build success, fake runtime tests, GPU probes and a desktop/native image do not establish VR comfort, motion-to-photon latency, multiplayer correctness or performance.
