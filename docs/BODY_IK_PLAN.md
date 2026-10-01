# Local body IK prototype plan

October 1, 2026. This is a source and installed-asset audit, not a working full-body renderer or headset acceptance. The next prototype should add an owner-only native KF body that follows the existing accepted hands. It must not enable the old ArmTest path in place of those hands. The multiplayer/flat-client contract is coordinated in [NETWORK_VISUALIZER_PLAN.md](NETWORK_VISUALIZER_PLAN.md); its implementation remains separate.

## What already exists

| Evidence | Reusable result | Limit for a full body |
|---|---|---|
| `unrealscript/KFVRGameplay/Classes/KFVRArmMath.uc`, `KFVRArmMathTests.uc` | Analytic two-link solve, stable fallback pole, authored roll-frame reconstruction; existing engine commandlet has 4,326 checks | Assumes validated lengths; returns false for coincident/too-distant targets. Its allowed shoulder shift is up to 0.3 m, too broad to adopt as a finished body policy. No pelvis, torso or legs. |
| `KFVRHandVisual.InitializeHand`, `PoseArm` | Hidden/visible legacy first-person arm chain, measured parent-space translation basis across distinct Ticks, wrist and sleeve helper rotation | ArmTest loads original weapon arms and bypasses `InitializeGlove`. `Start-KFVRDev.ps1` explicitly rejects ArmTest together with NativeHands. Replacing the accepted hand path would lose its anatomy/grasp behavior. |
| `KFVRHandVisual.PoseGlove`, `KFVRWeaponRig.UpdateHands` | Accepted KF hand geometry and fingers; final palms follow gun, support, magazine or action contact as appropriate | Raw controller positions are intentionally separate from displayed palms. Body IK must not move the displayed hands or derive game input from inferred elbows. |
| `reports/native-bridge-20260927-191415/arm-validation.json` | Legacy arms: measured shoulder/elbow/wrist maximum errors 0.000386/0.003499/0.006895 UU; tracking and unreachable-pose fallbacks exercised | Synthetic controllers, no headset presentation; applies to weapon-arm meshes, not the soldier body or current accepted NativeHands. |
| `src/interaction/RigPose.cpp` | Topology validation and generation-keyed restoration that restores only an unchanged previous write | Engine-independent cache; not connected to native KF pawn animation arrays. Do not present this as an existing native full-body hook. |

The first-person `Hands_*` skeleton, converted native hand `Palm`/finger skeleton, and third-person `CHR_*` soldier skeleton are different profiles. They must not share bone names, import scales, parent translation bases or wrist offsets by assumption. The prior 2142 work supplies architectural experience only; no offsets or anatomical calibration are imported.

## Installed native body and exact chain

The installed SDK `KFMod/Classes/KFHumanPawn.uc` defaults to `KF_Soldier_Trip.British_Soldier1`; `System/Corporal_Lewis.upl` selects the same mesh and uniform. `KFPawn.Setup` may replace the mesh through the selected player record/species. The runtime profile must therefore key on the actual live Pawn.Mesh, skins and generation, not only class or display name.

A headless, read-only UE Viewer export is in ignored `local/body-ik-audit/KF_Soldier_Trip/SkeletalMesh/British_Soldier1.psk`. The compact repeatable source/hash/hierarchy evidence is `reports/body-ik-native-asset-audit.json`. No game was launched, and no extracted body geometry was added to tracked source.

| Part | Verified parent chain |
|---|---|
| Torso | `CHR_Pelvis` (root) → `CHR_Spine1` → `CHR_Spine2` → `CHR_Spine3` → `CHR_Ribcage` |
| Head | Ribcage → `CHR_Neck` → `CHR_Head` |
| Left arm | Ribcage → `CHR_LArmCollar` → `CHR_LArmUpper` → `CHR_LArmForeArm` → `CHR_LArmPalm` |
| Right arm | Ribcage → `CHR_RArmCollar` → `CHR_RArmUpper` → `CHR_RArmForeArm` → `CHR_RArmPalm` |
| Left leg | Pelvis → `CHR_LThigh` → `CHR_LCalf` → `CHR_LAnkle` → `CHR_LToe1` |
| Right leg | Pelvis → `CHR_RThigh` → `CHR_RCalf` → `CHR_RAnkle` → `CHR_RToe1` |
| Weapon attachments | `WeaponL_Bone` / `WeaponR_Bone` are children of the corresponding palm |

The mesh has 55 bones, 3,142 shared points, 5,740 base-LOD triangles and four LODs. UE Viewer reports import scale 1.25, origin Z=44.5 and yaw=-16384. Its base LOD uses one material, `KF_Soldier_Trip_T.Uniforms.brit_soldier_I_cmb`. These export values describe the asset; runtime calibration must still measure actor-relative native bone origins and bases before writing overrides. The arm bind translations are 16.5093 and 12.0705 raw units; legs 23.0237 and 20.3712. Do not interpret these as metres or use the first-person weapon scale of 300/60.

The installed full SDK confirms `KFPawn` defaults `RootBone=CHR_Pelvis`, `HeadBone=CHR_Head`, `SpineBone1=CHR_Spine2`, `SpineBone2=CHR_Spine3`, `FireRootBone=CHR_Spine1`. The extracted TextBuffers under `local/sdk` omit defaultproperties, so the full installed SDK and mesh export supply those particular facts. Other character meshes, including robot and special DLC bodies, require separate profile validation; matching some names alone is insufficient.

## Pose contract and current missing inputs

Keep the shared body root at the current stock `Pawn.Location` collision centre. Express cosmetic targets in metres in a yaw-only body frame, UE X forward/Y right/Z up. Include the explicit body yaw captured with the sample and the pawn/mesh generation. This is a cosmetic coordinate system, not a request to reposition the pawn.

`NativeCameraTrial.ReadTrackedBody` currently uses Pawn.Location plus native EyeHeight and PlayerController yaw, omitting WalkBob, desktop pitch/roll and camera shake. `NativeWeaponRig` transforms HMD/controller targets through the same reference. Preserve this common reference and the accepted walking alignment. An HMD recenter removes the original standing reference translation: the current world HeadPosition alone is not a measurement of the wearer's physical height or floor. Height/posture calibration needs an explicit XR floor/reference-space measurement or a clearly labelled avatar-size setting.

The current 132-byte `TrackingPacket` provides HeadPosition, BodyRotation, and left/right presentation transforms, but **no head orientation or raw grip orientation**. `ControllerPresentationPose` explicitly combines grip position with aim orientation. Reinterpreting LeftRotation/RightRotation as raw grip would silently change accepted controls. A new versioned body target packet should add true HMD orientation and true grip poses, while retaining the existing fields unchanged.

Proposed cosmetic sample fields:

- Pawn identity/epoch, mesh/profile generation, monotonic input sequence and sample time, reference-space epoch, explicit body yaw, world units per metre, validity per tracked point.
- Head and raw left/right grip positions/orientations in the same body frame. Head/hand validity is independent; no inferred joints are marked tracked.
- Resolved left/right presentation palm positions and orthonormal bases, contact role and visibility. Locally read these after `UpdateHands` from the actual native palm, not an earlier controller target; for the flat visualizer either transmit these bounded cosmetic targets or deterministically reproduce the same weapon/contact policy.
- Estimated pelvis, elbow and foot values belong in solver/debug output; they are not authoritative tracking or gameplay/network hit locations.

Head, grip and display-palm transforms must be named separately. A native right palm has the existing conversion roll convention, and contact overrides may replace its frame; do not apply another guessed 180-degree correction in the body solver. Measure the body wrist-to-palm relation in its own profile.

## Minimal visible local prototype

1. Add a separately opted-in `KFVRBodyVisual` actor owned by the local pawn. Start from that pawn's native mesh/materials and import/actor transform. Keep collision disabled, role local-only, and animation notifications disabled on the clone. Do not spawn another Pawn, inventory or third-person weapon attachment. Stock Pawn movement, capsule, damage and server animation remain authoritative.
2. Measure the selected body's idle chain on a hidden calibration actor across distinct ordinary Ticks. Record lengths, base axes, parent-space translation responses, wrist contact offsets and valid head-mask bones. Reject missing/aliased/zero-length bones. Keep profile data per actual mesh generation. Mesh import metadata alone cannot substitute for these measurements.
3. First render the native torso/legs and debug shoulder/elbow/foot targets without modifying accepted hands. Obtain a fresh authored/base pose each Tick. `Actor.bAnimByOwner`, `GetAnimParams`, `SetAnimFrame`, `LinkMesh`, `SetBoneDirection`, `SetBoneLocation`, `SetBoneScale` and `EnableChannelNotify` exist in the installed UE2 SDK; whether owner animation copying composes correctly with clone overrides still requires a small live probe. Do not claim `bAnimByOwner` is a verified full pose-copy API.
4. Add a pure `KFVRBodyMath` estimator and tests once the profile/target schema is settled. Then apply its torso/arm outputs on the cosmetic actor after gameplay/contact updates and before the rig's presentation completion stamp. Queue all overrides before the first skeletal evaluation of that Tick. Neither stereo eye should advance the body again. Root handles the integration point after the current reload regressions.
5. Add stance/foot grounding last. Hide or reduce only the affected cosmetic limb on invalid/unreachable tracking; the authoritative native hand remains at its accepted target. A hidden body must not steal offhand ownership from support, magazine, slide, grenade, tool or trader operations.

The file proposal for that next implementation is `KFVRBodyProfile.uc`, `KFVRBodyMath.uc`, `KFVRBodyMathTests.uc`, and `KFVRBodyVisual.uc`; no such classes are enabled by this audit. Keep packet, rig, startup and observer changes as an explicit later integration diff. A desktop view of real KF body geometry and target axes is the first acceptance gate; a mannequin-style preview alone is insufficient.

## Honest three-point estimate

Three tracked points do not determine pelvis, elbows, knees or feet. Expose estimated status in diagnostic output and use the native animation as a stable prior.

- **Torso and pelvis:** begin at stock body heading and native pelvis height. Apply a bounded cosmetic horizontal offset for room-scale head displacement, with a small calibrated neck-to-HMD offset. Head pitch may contribute limited upper-spine bend, but does not rotate the pelvis directly. Apply yaw deadband/turn smoothing to the torso only; never feed it back into camera, PlayerController, holsters or the accepted hand transforms. Standing/crouching/leaning cannot be uniquely distinguished from HMD position without a calibrated floor/height baseline. The first prototype should keep native leg state and report its assumed posture explicitly.
- **Shoulders and arms:** derive shoulders from the posed ribcage/collar bones, solve each measured two-link arm to the resolved visible palm through a calibrated wrist frame. Reuse the analytic solver behind validated finite inputs, but replace its broad shoulder-shift acceptance with a profile limit and explicit unreachable fallback. Prefer an elbow pole below/outside the shoulder with bounded history-based continuity; crossed hands, high reaches and points parallel to the pole require stable fallbacks. Torso/clavicle compensation must never translate a controller or game muzzle.
- **Legs and feet:** native locomotion selects the gait. Read animated ankle/toe targets and probe the actual floor with bounded `Actor.Trace` from those positions. Solve cosmetic knees/ankles only against valid contacts, with measured sole offsets and limited pelvis adjustment. During jump/fall, release foot locks and retain native airborne pose; no invented floor below a ledge. Moving-platform contact needs base-relative lock points and invalidation when the base changes. Until this exists, animated legs are a stated approximation rather than tracked lower limbs.
- **Calibration:** derive bone lengths from the chosen live mesh. Derive body scale from the user's explicit calibration; use the native avatar size as a labelled fallback. Numeric reach, yaw and lean limits are tunable authored policy, not measured anthropometry. Avoid forcing a body to reach impossible controllers by stretching the upper arm or moving the accepted hand.

## Owner head hiding and duplicate-hand boundary

The full installed `Engine/Classes/Pawn.uc` defaults `bOwnerNoSee=True`. `Actor` exposes both owner visibility flags. A separate owner-only body permits local head masking while the stock/remote body retains its head. Confirm that the actual private stereo scene honours owner visibility in both eyes; do not just clear Pawn.bOwnerNoSee globally. Disable the local clone when the view target leaves the bound living pawn, on death/ragdoll, mesh change, tracking loss, pause, recenter or actor destruction; restore only state actually owned by this presentation.

Use clone-only bone overrides for an initial head/palm mask, and keep all head/hand masks off the remote visualizer. **Never call KFPawn.HideBone for cosmetic clipping:** it allocates severed limb/head actors and effects before scaling bones. The body's single base material prevents simply hiding a separate head material.

The native export exposes a real mask-quality risk: 819 points have CHR_Head influence; 71 are also influenced outside that subtree. Each palm subtree affects 291 points, with 43 shared outside it. Zero scale therefore is not equivalent to removing all head/hand triangles. Inspect head/neck and wrist boundaries at every LOD, looking down, leaning and turning. If collapsed geometry intrudes into the near plane or doubles the accepted hands, build a local-only native body variant with explicitly clipped/capped head and wrist boundaries, using the existing native-hand conversion/provenance workflow. Keep derived geometry in ignored local storage, never in the distributable source package. That fallback needs its own seam/skin/LOD validation.

## Required evidence before enabling by default

| Layer | Minimum checks |
|---|---|
| Pure math | Finite bounds and profile rejection; equivalent transforms under translated roots, yaw turns and multiple UnitsPerMetre values; measured upper/lower lengths preserved; fixed wrist endpoints; poles at parallel/antiparallel and crossed-hand cases; bounded pelvis/clavicle; sequence/time reset without stale interpolation. |
| Native skeleton | Independent actual readback of shoulder/elbow/wrist and spine/foot axes in coherent completed samples; profile measured across Ticks; no chassis/root alias or same-frame calibration; at least ten settled samples per normal pose plus transient samples during turns. Preserve current grip/muzzle strict tolerances and report body tolerances separately. |
| Gameplay isolation | No changes to native palm/muzzle/grip roles, stock ammo, shot trace count, support ownership or reload transitions. Repeat the chest reload, held-magazine tracking loss and attached-slide cases with body enabled. |
| Presentation | Both eyes and a genuine flat observer: local chest/legs visible, local head/duplicate hands hidden, remote head/hands intact; no doubled shadow/weapon, visible spikes, wrist seam or near-plane head intrusion. Validate all four British_Soldier1 LODs and at least one changed mesh/profile rejection. |
| Locomotion/lifecycle | Standing, strafe, turn, crouch, stairs, landing, jumping/fall, edge/no-floor, moving base, room-scale lean and recenter; hand-only loss, total loss, pause, view-target change, pawn replacement, death/ragdoll and cleanup. Stock collision/root remains unchanged. |
| Frame cost | One solve/pose update per ordinary game Tick, shared by both eyes; no extra animation notifications or gameplay work during rendering. Record CPU/frame cost and culling; numerical success does not prove stable stereo. |

Headset comfort and physical reach remain a separate wearer acceptance step. This audit establishes a concrete native asset/profile and integration boundary; it does not validate a full-body render, IK comfort or multiplayer synchronization.
