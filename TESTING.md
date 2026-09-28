# Testing and evidence limits

Build.ps1 runs CTest for each architecture. Current deterministic coverage includes stereo math/culling, exact pose/image matching, action focus/ownership, rig-write restoration, UI smoothing, arm-pole math, ammo conservation and interruption, melee gating, pose decoding, UE2 camera conversion, and an x86-only MinHook calling-convention/restore regression.

New ammo tests exercise tactical full-mag-plus-chamber behavior, empty reload/charging, racking a live round, fire cadence, remove/stow/discard/partial magazines, switching/death/drop cancellation, stale ownership, replay/high-latency rejection, correction validation, tube feeding, manual cycling and 100,000 randomized actions with conservation checks. Melee tests include 10,000 wrist-waggle samples, retraction, cooldown, invalid tracking, teleports and range limits. Pose tests include all truncated packet lengths, invalid quaternions, quantization overflow, sequence wrap and server-clock gates.

OpenXR controller tests use a fake API to cover bindings, predicted timing, focus/tracking loss, invalid samples, haptic limits, runtime errors and cleanup after partial initialization. Latest CTest results: x86 22/22, x64 21/21, including controller locomotion interruption and neutral-rearm coverage.

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

UnrealScript acceptance now runs with the official KF UCC engine in local/compiler. The authored package passed 30,059 assertions, including 10,000 randomized conservation checks and 20,000 actions used for C++/script comparison. Twenty rolling 16-bit checkpoints (10 per family) match; this detects regressions but is not an exhaustive equivalence proof. Tactical reload, empty/charged firing, live-round ejection, cadence, replay, interruption, active ownership transfer, corrections, tube/manual cycling and 31-bit sequence wrap have explicit assertions. At this historical checkpoint the package was not installed. The September 27 standalone weapon increment below stages our generated package; its physical reload authority remains unbound.

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

## Physical reload and weapon pose foundation

Both architecture suites include PhysicalReload, ReloadGestures, BeltReloadSession and TrackedWeapon. Coverage includes persistent partial magazine contents, shared dual reserve, independent chambers, short/full action strokes, slide assist, 30,000 randomized conserved operations, cadence/replay/ownership, geometry alignment and withdrawal, stale/focus-loss recovery, head-independent weapon rays and upright belt placement. Integrated tests drive real ledger changes from synthetic physical pose sequences.

The isolated official KF script compiler builds KFVRReloadVisual/KFVRReloadPoseProfile and runs 25 four-stage mapping checks. Existing 30,059 ammo-policy checks and 20 cross-language checkpoints remain passing. These prove script compatibility and profile arithmetic; the visual actor has not been rendered in a live game and the new physical ledger is not yet an UnrealScript authority.

Read-only reload-assets-20260927.json contains actual default asset references for Single/Dualies, Deagle/DualDeagle and MK23Pistol/DualMK23Pistol. Neither a resolved mesh name nor a successful build establishes grip/magwell/action calibration. No physical headset test was requested in this batch.

## Live tracked weapon, hands and holster acceptance

Current suites: x86 23/23, x64 22/22; official UCC 30,059 ammo assertions, 20 cross-language checkpoints and 25 reload-pose checks. InputIpcProbe also verifies real cross-process capacitive contact and face buttons with all existing release conditions.

The final `--weapon` receiver run is reports/native-bridge-20260927-145949, paired with native-trace-20260927-145949-998734. All 14 checks in weapon-validation.json pass, including three shots/three rounds, five haptic events, holster/draw, focus-loss recovery, both actual palm bone positions and controller-directed aim. 259 native eye pairs were consumed. `weapon_pose_delivery_passed` alone only verifies mailbox writes; the independent read-only script observer/validator establishes actual gameplay behavior. Captures were visually inspected; no headset was worn.

The script observation utility never writes to game memory. It runs in an explicitly verified installed game process, discovers reflected owned rig/hand fields, records observations to a private JSONL file, and exits via its own stop marker before validation. Synthetic-controller native trials are separately bounded and retain the existing binary/prologue, world, render-state and teardown checks. Full weapon coverage and physical reload tests remain pending.

## Live inventory, support hand and physical reload acceptance

The current `--inventory` receiver trial passes all 23 checks in reports/native-bridge-20260927-161020 (786 actual stereo pairs): three supported weapons, stock firing/reloading, hip/shoulder switching, measured two-hand rifle aim and focus/cleanup behavior. `--reload` passes all 23 checks in reports/native-bridge-20260927-161347 (1,677 pairs over 51 seconds): magazine identity/round conservation, short/full slide pulls, dry/open-slide fire rejection, tactical plus-one, retained magazine on switching and five stock shots. Magazine position and rotated orientation are independently read from actual game bones. No wearer was involved.

Official UCC additionally runs 20,012 live-magazine ledger checks and 720 two-hand alignment checks. Existing ammo parity/pose and native x86/x64 suites remain passing. The real InputIpcProbe forces mailbox contention and verifies bounded neutral failure with unchanged expiry/recenter/focus/peer-close behavior.

The observer caches reflected field layouts and bulk-reads each object, retries only transient read snapshots, and takes a final snapshot after the existing script heartbeat has released input. A missing observation is a failed acceptance check; it is not treated as a pass.

Physical profile regression: `native_bridge_trial.py --deagle-reload`, then `--reload` in the same process verifies 8+1/15+1 and independent inactive weapon state. `validate_reload_trial.py <report> --weapon deagle` can recheck saved Deagle evidence. Controller right-A Use is counted in the inventory trial, but actual trader/door interaction requires a separate acceptance case.

Physical knife: `Start-KFVRDev.ps1 -Unattended -MeleeTest`, then `native_bridge_trial.py --pid <current> --game <KF> --melee`. Current evidence is native-bridge-20260927-170139 (18/18); its two fixture damage calls are real game collision dispatch, not zed/perk acceptance. UCC runs 10,075 knife-motion checks. Current four-weapon inventory evidence is 165952 (25/25), and physical pistol regression 170043 (24/24). InputIpcProbe additionally covers pending opposite-hand feedback and no replay after consumption with shared channel v3.

Independent dual 9mm: `Start-KFVRDev.ps1 -Unattended -DualTest`, then `native_bridge_trial.py --dual`. Evidence native-bridge-20260927-171823 passes 27 checks, including left-feed exhaustion with right-side firing, 26 total stock shots, simultaneous haptic pulses and two timed reloads. UCC runs 37,160 feed-allocation/conservation checks. Physical belt reload is still separate at this checkpoint.

Physical dual belt: `native_bridge_trial.py --dual-belt` on the explicit unattended DualTest match. Current evidence native-bridge-20260927-173833 passes 28 checks over 62 seconds. It includes interrupted insertion, independent belt latches, actual mag-bone attachment, shared-ammo conservation, holstering and mode exit. Both architecture suites pass; UCC includes randomized dual-magazine accounting plus directed insertion-gate cases.

Offhand status and grasp checks now supplement each `--weapon` native receiver trial. `--inventory` additionally compares real pawn vitals, gun changes, completed reload values and support/focus visibility. Current final inventory evidence 180740 passes 25 gameplay + 22 display/grasp checks; physical reload 180929 passes 24 + 10. Text rendering counters prove callbacks, not legibility: final 180740 captures were independently viewed to confirm panel orientation/aspect/depth.

Tracked healing: explicit `Start-KFVRDev.ps1 -Unattended -HealTest`, then `native_bridge_trial.py --heal`. Evidence 182659 passes 17 healing + ten display/grasp assertions, actual health 40 -> 90, one completed stock injection and one focus-cancelled injection, 500 charge consumed/recovered, four pulses, 977 stereo pairs and clean native teardown. Five-item inventory evidence 181842 passes 25 inventory + 22 status assertions; the pistol reload display check now samples the actual reload/completion interval, avoiding an unrelated short phase with no observer snapshot. Teammate healing and wearer contact calibration remain open.

Tracked welder and real door: launch the explicit unattended DoorTest fixture and run `native_bridge_trial.py --weld`. Evidence 184225 / trace184226-092559 passes 19 tool/door + ten status/grasp checks, 32 weld hits, 14 unweld hits, stock charge recovery, actual door open/close, and all interruption/cleanup checks. 1,150 stereo pairs, 2,163 valid tracking packets, 49 hand pulses. The native receiver now holds the initial contact-observation phase for two seconds; a one-second phase had no snapshot in the preceding otherwise-successful run.

Six-item inventory regression with both tools installed: 184338 passes 25 inventory and 22 display/grasp checks, twelve shots and 828 stereo pairs.

Experimental arms: launch `Start-KFVRDev.ps1 -Unattended -ArmTest`, then `py -3.12 tools/native_bridge_trial.py --pid <PID> --game <game> --arms`. The 28-second sequence covers both joint attachment and tracking/unreachable fallback. See WORKLOG for known sleeve deformation; normal gameplay does not enable it.

VR trader: `Start-KFVRDev.ps1 -Unattended -TraderTest`, then `native_bridge_trial.py --trader`. The 104-second sequence uses actual shop/pawn transactions; fixture-only left-Y commands set zero cash, restrict carry capacity and close the map shop for negative checks. Current evidence 193902 passes 27 assertions. Stock healing is checked both while pending and after completion; an already-full ammo request must leave cash unchanged. The fixture's cash/weight edits are unavailable in an ordinary match.


Physical grenade acceptance: native-bridge-20260927-200326 / native-trace-20260927-200327-030073 passes 22 checks, including three actual stock projectiles/explosions, independently observed velocity and FragAmmo 5 -> 2, real damage (two hits / 317 damage; the third throw lands outside the stationary target's radius), actual locomotion/snap followed by a stationary drop, focus/hand loss, held recovery and tracking discontinuity. 2,731 consumed stereo pairs, 4,683 rig packets and twelve hand pulses; zero invalid/busy input reads and restored hooks/resources. The external observer follows the reflected LastProjectile pointer with an independent UObject index check so short flight is sampled before landing. 3,674 official-engine throw-estimator assertions pass. Synthetic controllers in the real standalone game; no wearer/server/perk acceptance.

Trader extension 201202 / trace201202-807038 passes 32 real-game checks, including dual-sale shared ammo/right-feed preservation and a fired replacement single, stock-priced grenade refill, held/full-ammo rejection and fixture shop reopening. Both native suites and all UCC suites pass.


Full-resolution cadence: `tools/native_bridge_trial.py --pid <current> --game <KF> --cadence --width 2064 --height 2208` runs a stationary 20-second native receiver benchmark. Read per-stage timings and no-request callback counts in the trace's native-bridge.json. It uses no headset and is not an OpenXR frame-rate acceptance test. Controlled V3/V4 comparison with identical SIMD and no receiver success delay measured 31.4 -> 50.3 fresh pairs/sec (202643 versus 202524). The regular inventory scenario retains its existing diagnostic pacing; full-resolution 202813 passes 25 inventory plus 22 HUD/grasp checks. Core tests exercise delayed completion versus queued requests; real x64/x86 IPC and input probes pass on v4.


Pre-alpha candidate: see docs/PRE_ALPHA_TEST.md for the three-minute wearer checklist. Restored renderer 204850 passes 25 live gameplay + 22 HUD/grasp checks at 2064 x 2208. The reload display check compares against independently read actual magazine state, since a held-trigger phase can contain one or two stock-cadence shots. Profile startup, runtime and exit preserve original INI hashes; optional profile uses a direct executable launch because Steam re-quotes INI arguments. Candidate headset acceptance remains separate and pending.


Wearer session205619 failed head/hand alignment acceptance. Corrective startup reference tests cover 48 tilted starts with vertical-motion and controller-alignment invariants. Runtime resolution selection checks the real 2528 x 2704 recommendation (99% fits) and the tested 82% selection. No corrective wearer acceptance yet; publication stays on hold.


## Sleeve-free hands and vertical head poses

Normal gameplay now uses original glove meshes. Run `tools/native_bridge_trial.py --pid <current> --game <KF> --hands --width 1280 --height 960` in an unattended private match. No headset is required. The 36-second sequence includes a translated/rotated OpenXR reference, head-independent palm positions, exact up/down gaze, open/contact/fist poses, wrist rotations, snap turning, focus loss and single-hand loss. `hand-validation.json` compares actual bone axes to independently computed input expectations; `hand-eye-pairs.json` requires fresh stereo frames at both vertical poses. Final evidence 213108 passes 60 checks and native cleanup. Visually inspect captures separately; this is not wearer acceptance. Existing x86 24/24 and x64 23/23 suites now include 210 camera roundtrips at and around the vertical singularities. Full-resolution inventory regression 212843 passes 25 gameplay plus 22 HUD/grasp checks.

Physical-pistol regression after the glove and vertical-gaze corrections: native-bridge-20260927-213209 passes 24 reload + ten HUD/grasp checks, five stock shots and clean teardown. No corrective wearer acceptance is claimed.


## Deliberate wrist HUD

Run `tools/native_bridge_trial.py --pid <current> --game <KF> --hud --width 1280 --height 960` in an unattended private match. The 27-second sequence exercises ordinary/palm-facing poses, reading distance and height, grip/trigger suppression, focus/tracking loss, recovery and angle hysteresis. `wrist-display-validation.json` checks reflected actor visibility, 20 cm actual draw width, wrist offset, head-facing orientation and actual pawn vitals. This uses synthetic controllers without headset presentation; inspect captured eye images separately.

Evidence native-bridge-20260928-114709 passes all 26 checks and clean native teardown, with 1,124 stereo pairs. Native-bridge-20260928-114754 passes 25 inventory plus 22 display/grasp checks, twelve shots, three reloads and 1,069 pairs. The inventory driver now turns the free palm toward the eyes. Generic status validation reports revealed_samples and requires native texture rendering when the card was revealed; a run with no deliberate reveal is not visual evidence. Dual-wield/occupied-hand status data can still be validated while hidden, but there is no dual-wield reveal gesture yet.

Both architecture builds (24/24 x86, 23/23 x64), official UCC suites and Python parse checks pass. The two runs record 16/13 bridge invalid reads, with zero busy reads; the HUD checks do not establish the cause of those counters. Final game log is wrist-hud-final-game.log, with normal exit and no script errors. No corrective wearer acceptance is claimed. Stopped after this checkpoint at the owner's request.
