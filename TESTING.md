# Testing and evidence limits

Build.ps1 runs CTest for each architecture. Current deterministic coverage includes stereo math/culling, exact pose/image matching, action focus/ownership, rig-write restoration, UI smoothing, arm-pole math, ammo conservation and interruption, melee gating, pose decoding, UE2 camera conversion, and an x86-only MinHook calling-convention/restore regression.

New ammo tests exercise tactical full-mag-plus-chamber behavior, empty reload/charging, racking a live round, fire cadence, remove/stow/discard/partial magazines, switching/death/drop cancellation, stale ownership, replay/high-latency rejection, correction validation, tube feeding, manual cycling and 100,000 randomized actions with conservation checks. Melee tests include 10,000 wrist-waggle samples, retraction, cooldown, invalid tracking, teleports and range limits. Pose tests include all truncated packet lengths, invalid quaternions, quantization overflow, sequence wrap and server-clock gates.

OpenXR controller tests use a fake API to cover bindings, predicted timing, focus/tracking loss, invalid samples, haptic limits, runtime errors and cleanup after partial initialization. Latest CTest results: x86 17/17, x64 16/16.

Hardware checks are separate from unit tests:

```powershell
& G:\KFVR\build\x86\KFVRD3D9Probe.exe --consumer-exe G:\KFVR\build\x64\KFVRD3D9Probe.exe
& G:\KFVR\build\x86\KFVRBridgeProbe.exe --consumer-exe G:\KFVR\build\x64\KFVRBridgeProbe.exe
& G:\KFVR\build\x64\KFVRInputProbe.exe --producer-exe G:\KFVR\build\x86\KFVRInputProbe.exe
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

Expanded native activity reports: `reports/native-trace-20260925-114948-065338` (KF-Offices lobby) and `reports/native-trace-20260925-115123-397460` (after readying the solo match). Both passed 180 frames and restoration of all nine hooks. Script events, nested world renders and skeletal evaluation are counted separately from the two instrumented animation-advance functions. `tools/audit_native.py` emits bounded CFG/call listings into ignored reports; indirect calls and branches beyond export bounds remain explicit unknowns.

`KFVRInputProbe` verified actual x64/x86 controller and haptic transport, 64-bit predicted time/reference IDs, host-stall expiration, focus loss without an image, recenter, invalid input neutralization and host-close release. Synthetic packets are not a physical controller test.

`KFVRD3D9Probe` verified 20 non-Ex D3D9-origin pairs with an actual x86 producer and x64 consumer, four RGBA/BGRA/sRGB formats, X8/A8 alpha handling, odd dimensions/row pitches, six 8x-MSAA cases, untouched render-target/viewport bindings, pair backpressure, discarded-generation recreation and a real D3D9 reset after releasing bridge resources. Per-resolution timings are in `reports/d3d9-transfer-timings.jsonl`; these include readback/upload/publication but exclude scene drawing and compositor latency. No in-game stereo is claimed.

One bounded `KFVRXrHost --probe` attempt failed at xrCreateInstance with XR_ERROR_RUNTIME_FAILURE (-2). It created no instance/session and submitted no frames. Logs: `reports/openxr-probe.json` and `.stderr.txt`. This establishes runtime unavailability at that attempt, not a diagnosis of headset hardware.

UnrealScript acceptance now runs with the official KF UCC engine in local/compiler. The authored package passed 30,059 assertions, including 10,000 randomized conservation checks and 20,000 actions used for C++/script comparison. Twenty rolling 16-bit checkpoints (10 per family) match; this detects regressions but is not an exhaustive equivalence proof. Tactical reload, empty/charged firing, live-round ejection, cadence, replay, interruption, active ownership transfer, corrections, tube/manual cycling and 31-bit sequence wrap have explicit assertions. The `.u` package is compiled, but not installed/enabled in live gameplay.

Native private-target trial: `tools/trace_kf.py --pid <current> --game <KF> --target-trial` explicitly enables one private target operation. The audited run native-trace-20260925-123709-130310 verified 196,608/196,608 pixels and exact original RHI state after target destruction. The launcher verifies entry bytes; the Python wrapper also rejects missing/incomplete/failed target reports. No scene replay is enabled by this option.
