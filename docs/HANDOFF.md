# Developer handoff — October 9, 2026

This is a public source checkpoint of an unfinished native VR conversion for original Killing Floor. Forks can continue from the existing implementation; development by the original project owner is currently on hold. The checkpoint preserves the development history and license notices.

## Exact checkpoint

- `88f99da`: magnetic magazine guidance, rifle pull removal and fixed slide-hand pose; last gameplay change.
- `2b413d2`: wearer acceptance and the newly reported pistol shot-alignment issue.
- Tag `v0.1.0-prealpha-handoff.1`: those changes plus the public handoff documentation.

The game target is the audited Windows KF 1065 installation. Native hash checks are in `tools/native_profile.py`; do not bypass a mismatch to use an unexamined engine build. The game process is x86, the OpenXR host is x64, and gameplay experiments currently require standalone mode.

## Build and reproduce

Follow [BUILDING.md](../BUILDING.md), then [PRE_ALPHA_TEST.md](PRE_ALPHA_TEST.md). The short version is: install the game/official SDK and C++/Python tools, supply an x64 OpenXR loader, build x86 and x64, compile the authored UnrealScript package, generate native hands locally if desired, and launch the private fixture.

Authored glove/status assets are tracked and reproducible from `tools/generate_hand_mesh.py` and `tools/generate_status_mesh.py`. Native KF hands are different: `tools/prepare_native_hands.py` reads the owner's installed assets using an independently obtained UE Viewer and creates an ignored local package. Nothing from another BF2142/BC2 checkout is needed.

The default private fixture contains seven inventory weapons. MK23 and SCAR have separate `-MK23Test` and `-SCARTest` substitutions; dual pistols use `-DualTest`. Use a fresh fixture for scripted reload tests that assume untouched ammunition. Do not run synthetic controller trials during a wearer session.

## What the last wearer test established

On October 1, the wearer accepted magnetic magazine insertion, Bullpup magazine pull-out and the fixed overhand pistol-slide grasp. Earlier tests accepted native hand grips, capacitive thumb behavior and removal of unwanted weapon movement while walking.

The final three-minute session submitted 5,436 fresh native stereo pairs at 2500x2676 per eye, plus 30 retained submissions; there were no rejected pairs or host error. These totals are not a claim of headset-rate fresh rendering. The wearer still reported pistol shot mismatch and slight pistol holding-orientation discomfort. Acceptance does not extend to every weapon, headset, pose or scene.

The last gameplay build passed 25 x86 and 24 x64 native tests. Official SDK commandlets include 1,683 magazine-guide and 140 pull checks alongside the existing ammo, pose, action, melee, dual and throw suites. Actual desktop game trials passed reload/grasp/status checks for 9mm, Deagle, MK23, Bullpup and SCAR. Those trials used synthetic controller input; they complement wearer feedback.

Detailed receipts and failed attempts are summarized in [WORKLOG.md](../WORKLOG.md). Raw reports/captures remain private and are not in Git. Historical report IDs are provenance, not files a fresh clone can open. Some one-off asset audit scripts also lived in ignored reports; their planning documents describe findings, but reproducing those audits may require rebuilding the helper. Tracked runtime, conversion and primary test tools are included.

## First gameplay issue: pistol shot alignment

**Observed:** pistol bullets appeared to land away from the visible aim; Bullpup aim appeared correct. The exact affected pistol has not been confirmed by the wearer. Both 9mm and Deagle were used during the recorded session.

**Already checked:** pistol/rifle `DoFireEffect` implementations use the rig's controller-driven muzzle and aim; inherited traces consume the supplied direction. Native Idle muzzle-bone +X is essentially model +X. Saved coherent samples show the displayed firearm rotation equals rig aim, and muzzle/palm positions remain attached.

**Missing evidence:** `LastTraceStart` and `LastTraceDirection` persist after a shot. Comparing them with a later hand pose confounds movement and recoil. There is no immutable shot-time pose/serial in the current recording. These checks do not disprove the wearer report.

**Next measurement:** inside the firing call, record weapon identity, input/presentation serial, actual muzzle/tip frame, pre-spread aim, post-spread direction and actual trace endpoint. Compare stationary and moving shots with the visible sight line. Preserve normal stock damage, cadence and perk-modified spread. Do not hide the problem with an arbitrary grip rotation; comfort calibration is a separate task.

## Code map

| Area | Starting points |
| --- | --- |
| Native game admission and trace launcher | `tools/native_profile.py`, `tools/observe_kf.py`, `tools/trace_kf.py`, `src/native/NativeProbe.cpp` |
| Native stereo, camera and input | `src/native/NativeEyeBridge.cpp`, `NativeCameraTrial.cpp`, `NativeWeaponRig.cpp`; `src/engine/Ue2Camera.cpp` |
| Image transfer and host | `src/graphics/d3d9/D3D9CpuPair.cpp`, `src/graphics/d3d11/`, `src/ipc/`, `src/xr/OpenXrHost.cpp` |
| Script rig and native hand/weapon presentation | `unrealscript/KFVRGameplay/Classes/KFVRWeaponRig.uc`, `KFVRWeaponVisual.uc`, `KFVRHandVisual.uc`, `KFVRWeaponProfile.uc` |
| Live single-gun reload authority | `KFVRManualReload.uc`, `KFVRMagazineState.uc`, `KFVRMagazineGuide.uc`, `KFVRMagazinePull.uc`, `KFVRActionRail.uc` |
| Actual shots | `KFVRSingleFire.uc`, `KFVRDeagleFire.uc`, `KFVRMK23Fire.uc`, `KFVRBullpupFire.uc`, `KFVRSCARFire.uc`; stock-derived tracked weapon classes |
| Dual reloads, melee, tools and grenades | `KFVRDualBeltReload.uc`, `KFVRTrackedDualies.uc`, `KFVRTrackedKnife.uc`, tool adapters, `KFVRGrenadeControl.uc` |
| Portable mechanics | `src/gameplay/`, `src/interaction/`, `tests/`; distinguish these components from the live script integration |
| Builds and measurements | `Build.ps1`, `tools/build_scripts.py`, `tools/native_bridge_trial.py`, `tools/observe_weapon_rig.py`, `tools/validate_*.py` |

Unqualified `.uc` entries in the table are in `unrealscript/KFVRGameplay/Classes/`.

## Next useful contributions

1. Resolve pistol shot alignment, then implement a stable two-handed pistol support grip.
2. Expand native weapon coverage with profile-specific contact and mechanism measurements. Bullpup has no invented charging handle; SCAR's modeled handle/side-slap interaction is deferred.
3. Implement the chamber/hinge foundation in [DEMOLITIONS_RELOAD_PLAN.md](DEMOLITIONS_RELOAD_PLAN.md): individual M32 rounds and side-flick closure, plus M79/BoomStick flip closure. Closing must not refill or reset chamber identity.
4. Improve melee against real Zeds, walls and perk conditions; current knife fixtures are narrower than that acceptance scope.
5. Implement [local body IK](BODY_IK_PLAN.md), preserving the accepted hands, then [remote pose visualization](NETWORK_VISUALIZER_PLAN.md). These remain plans; no replacement native server has been justified yet.
6. Improve frame transport, renderer lifecycle, complete menus/inventory, and VR presentation of flickering lights, rays and flat-only model surfaces.

## Constraints worth preserving

- A render replay must not advance the game simulation. Tests measure this boundary but do not prove every render-side cache/effect is harmless.
- Raw tracking drives interaction travel. Snapped hands, recoil and support presentation must not manufacture reload progress or melee velocity.
- The ammunition ledger alone commits ownership transfers. Preserve ammunition, chamber state and magazine identity across release, tracking loss, switching and cancellation.
- Keep native offsets/build identity measured and per-process; do not import Battlefield offsets or guess engine APIs.
- Generated game geometry, SDK copies, private settings and raw reports stay ignored. Publish authored code and required third-party notices.
- The injected diagnostic module becomes inert when hooks end and remains resident until normal game exit. Do not turn it into an auto-loading proxy DLL without a separate lifecycle design.

There is no maintained installer, complete multiplayer implementation or promised completion date at this checkpoint. This handoff makes the working foundation and remaining gaps available for independent development.
