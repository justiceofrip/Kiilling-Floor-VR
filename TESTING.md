# Testing and evidence limits

Build.ps1 runs CTest for each architecture. Current deterministic coverage includes stereo math/culling, exact pose/image matching, action focus/ownership, rig-write restoration, UI smoothing, arm-pole math, ammo conservation and interruption, melee gating, pose decoding, UE2 camera conversion, and an x86-only MinHook calling-convention/restore regression.

New ammo tests exercise tactical full-mag-plus-chamber behavior, empty reload/charging, racking a live round, fire cadence, remove/stow/discard/partial magazines, switching/death/drop cancellation, stale ownership, replay/high-latency rejection, correction validation, tube feeding, manual cycling and 100,000 randomized actions with conservation checks. Melee tests include 10,000 wrist-waggle samples, retraction, cooldown, invalid tracking, teleports and range limits. Pose tests include all truncated packet lengths, invalid quaternions, quantization overflow, sequence wrap and server-clock gates.

OpenXR controller tests use a fake API to cover bindings, predicted timing, focus/tracking loss, invalid samples, haptic limits, runtime errors and cleanup after partial initialization. Latest CTest results: x86 18/18, x64 17/17, including controller locomotion interruption and neutral-rearm coverage.

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

Headset checks require an available HMD/runtime. Physical synthetic-scene display and controller acceptance is now recorded in docs/HEADSET_TESTING.md. Native KF stereo, head tracking and left-stick movement have subsequently been accepted by the wearer; later entries supersede the historical unconnected-path checkpoints. Build success, fake runtime tests, GPU probes and a desktop/native image do not establish VR comfort, motion-to-photon latency, multiplayer correctness or performance.

Expanded native activity reports: `reports/native-trace-20260925-114948-065338` (KF-Offices lobby) and `reports/native-trace-20260925-115123-397460` (after readying the solo match). Both passed 180 frames and restoration of all nine hooks. Script events, nested world renders and skeletal evaluation are counted separately from the two instrumented animation-advance functions. `tools/audit_native.py` emits bounded CFG/call listings into ignored reports; indirect calls and branches beyond export bounds remain explicit unknowns.

`KFVRInputProbe` verified actual x64/x86 controller and haptic transport, 64-bit predicted time/reference IDs, host-stall expiration, focus loss without an image, recenter, invalid input neutralization and host-close release. Synthetic packets are not a physical controller test.

`KFVRD3D9Probe` verified 20 non-Ex D3D9-origin pairs with an actual x86 producer and x64 consumer, four RGBA/BGRA/sRGB formats, X8/A8 alpha handling, odd dimensions/row pitches, six 8x-MSAA cases, untouched render-target/viewport bindings, pair backpressure, discarded-generation recreation and a real D3D9 reset after releasing bridge resources. Per-resolution timings are in `reports/d3d9-transfer-timings.jsonl`; these include readback/upload/publication but exclude scene drawing and compositor latency. No in-game stereo is claimed.

One bounded `KFVRXrHost --probe` attempt failed at xrCreateInstance with XR_ERROR_RUNTIME_FAILURE (-2). It created no instance/session and submitted no frames. Logs: `reports/openxr-probe.json` and `.stderr.txt`. This establishes runtime unavailability at that attempt, not a diagnosis of headset hardware.

UnrealScript acceptance now runs with the official KF UCC engine in local/compiler. The authored package passed 30,059 assertions, including 10,000 randomized conservation checks and 20,000 actions used for C++/script comparison. Twenty rolling 16-bit checkpoints (10 per family) match; this detects regressions but is not an exhaustive equivalence proof. Tactical reload, empty/charged firing, live-round ejection, cadence, replay, interruption, active ownership transfer, corrections, tube/manual cycling and 31-bit sequence wrap have explicit assertions. The `.u` package is compiled, but not installed/enabled in live gameplay.

Native private-target trial: `tools/trace_kf.py --pid <current> --game <KF> --target-trial` explicitly enables one private target operation. The audited run native-trace-20260925-123709-130310 verified 196,608/196,608 pixels and exact original RHI state after target destruction. The launcher verifies entry bytes; the Python wrapper also rejects missing/incomplete/failed target reports. No scene replay is enabled by this option.

`--camera-trial` implies the native target test and adds five constructor/destructor cases. Evidence native-trace-20260925-124237-118548 passes original-scene byte preservation, both storage canaries, supplied fields, finite matrices and camera/view/projection algebra checks. Camera-only mode performs no scene replay. Diagnostic 3.2-world-unit separation is not physical scale calibration.

Explicit `--stereo-trial` now replays two world views once using fresh native camera nodes and private targets. It implies target/camera validation. Native pair native-trace-20260925-124727-508250 passes restoration, camera preservation, pixel diversity and eye difference checks. Images native-left.bmp/native-right.bmp were inspected side by side. The trial reports render callbacks rather than assuming rendering is pure; HUD callbacks repeat and GetFrame cache work remains. It is not a persistent mode.

`--asymmetric-trial` uses different diagnostic eye frusta; `--pose-trial` also applies a synthetic translated/rotated head pose through the portable UE2 adapter. Both remain single-pair experiments and imply camera/target checks. Saved native-eye-cameras.json contains all six matrices per eye. Wrapper validation checks native eye separation, midpoint translation, relative head rotation and horizontal projection centers against the requested diagnostic inputs. Neither test uses measured HMD tracking. Native full-payload comparison, clip-edge mapping, inverse conditioning bounds and fixed-error world roundtrips now supplement the camera tests. Successful evidence: native-trace-20260925-125425-314640 and native-trace-20260925-125821-622646.

OpenXR failure clarified from the runtime logs: xrclient_KFVRXrHost and vrserver report VRInitError_Driver_WirelessHmdNotConnected / no connected devices at the earlier probe. The loader did locate and start SteamVR; the wireless headset was not connected. No runtime setting was changed or repeated launch attempted. Report: reports/openxr-availability.json. Physical headset/controller acceptance requires that connection, independently of the remaining native transport/gameplay integration.

Physical headset acceptance: connected SteamVR/OpenXR Meta compatibility runtime passed three synthetic-room sessions. User confirmed a stable visible room and left/right haptics. Per-hand telemetry verifies grip/aim tracking, trigger/grip press and release, and both thumbstick axes after a targeted follow-up. Reports and exact counts are in docs/HEADSET_TESTING.md; these do not certify the unconnected native KF presentation path.

Native-to-x64 acceptance: `tools/native_bridge_trial.py --pid <current> --game <KF>` launches a hidden x64 receiver and a separately bounded 15-second native bridge. It verifies twelve fresh exact-time/space/tracking pairs, native geometry, differing eyes, restoration and graphics-thread cleanup. Reports native-bridge-20260925-183300 and native-trace-20260925-183300-522835 pass. `tools/native_headset_trial.py` exercises the same bridge through OpenXR at an initial 40% eye-dimension scale (100% since September 27). Subsequent physical acceptance is recorded below. The generic host reports that it cannot verify native producer identity; the native launcher and combined trial evidence establish provenance.

User-confirmed native HMD flicker fix: native-headset-20260925-184412 produced 279 fresh pairs plus 172 retained projections; user reported flashing gone, but setup consumed the viewing window. --wait-first-pair fixes that timing with a separate bounded startup deadline. Full follow-up native-headset-20260925-184625 produced 445 fresh pairs plus 192 retained projections over a 15-second native run; zero rejected pairs and native/reset cleanup passed. Head-movement acceptance is recorded separately in reports/native-headset-acceptance-20260925.json.

Read-only native input timing: `tools/trace_kf.py --pid <current> --game <KF> --input-trial` adds a bounded UViewport::ReadInput hook and measures axis values before/after reading and PlayerTick. Native-trace-20260925-190036-910911 passes 240 samples / 117 paired tick observations, finite values, ten-hook entry restoration and no input writes. --input-trial is an audit, not controller locomotion.

Controller movement implementation checkpoint: both architecture builds pass, Python launchers compile, and KFVRInputProbe again verifies real x64/x86 controller transport/release with game_connected=false and headset_used=false. The opt-in native movement exercise and result checker are ready; no live gameplay movement is claimed. `reports/movement-validator-offline/verification.json` records synthetic checker acceptance/rejection cases, not engine results. See docs/INPUT_INTEGRATION.md for the next live commands and acceptance boundaries.

Live movement increment: native-bridge-20260925-193320 passes all twelve control/release phases; native-trace-20260925-193320-142487 verifies 382/382 temporary raw-input restorations, zero failures and complete core/input/reset hook/resource cleanup. The independent position monitor records corresponding pawn displacement. Receiver movement mode checks first-pair pixels and every pair's identity/GPU copy, preventing exhaustive CPU pixel counting from expiring controller samples. The original failing run remains recorded. Physical headset acceptance is separate and pending.

## 2026-09-27: accepted movement, native snap turning and recommended resolution

The wearer accepted left-stick movement in native-headset-20260925-193726 / native-trace-20260925-193728-003583: "worked great, left works but no snap turning or right input. but it worked great". Evidence records 440 fresh pairs, 281 retained projections, 887/887 movement restorations and zero restoration failures. The owner declined a further wearer test for turning and is now asleep; continue local development without headset prompts.

Right-stick snaps now use the audited Core.dll UObject::CallFunction boundary for KFPlayerController.UpdateRotation, inside the existing paired PlayerTick. The adapter adds a one-shot rotation demand after stock weapon/FOV sensitivity, calls the normal function once, and restores the previous aTurn. It does not replay simulation. Reflected PlayerTick.DeltaTime, rotation, exact module hashes and relocated entry bytes are checked on each launch. Input ABI is v9. Separate left/right policies allow one tracked hand to work if the other is lost; neutral rearming protects focus/tracking/ownership changes.

Combined native-bridge-20260927-131134 / native-trace-20260927-131134-427935 passed 175 consumed pairs at 2064 x 2208 per eye, 330/330 movement restorations, three requested/measured/restored 30-degree snaps, every interruption phase, and all hook/resource cleanup. This was synthetic controller input with real native world rendering, without HMD presentation. A preceding run rendered 191 full-size pairs but correctly failed the movement criterion when the player died; a lobby attempt was rejected before injection. Readied a new standalone BioticsLab match for the passing run.

The previous bridge dimension limit of 2048 blocked the runtime's recommended size. It is now 4096 per dimension with the existing 256 MiB aggregate transport cap. The XR launcher defaults to 100% of runtime recommendations, not hardcoded Quest dimensions. Full-size CPU transfer averaged about 16.7 ms per rendered pair in this test; the receiver consumed about 23 fresh pairs/sec and 18 pairs missed their deadline. This is working full-resolution transport, not accepted shipping performance. GPU transport and rendering cadence still need work.

Play-KFVR.ps1 combines movement, snap turning and recommended resolution with a default 120-second session (15..180 configurable). Longer duration support compiles, but this checkpoint exercised the 15-second native window, not a two-minute HMD session. PE parsing is cached per module during preflight while retaining every binary/prologue check, reducing measured startup from roughly 20 seconds to roughly two. x86 18/18 and x64 17/17 pass. No original game files/settings changed.
