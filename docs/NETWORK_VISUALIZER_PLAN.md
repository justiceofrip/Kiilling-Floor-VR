# Networked VR body and flat-client visualizer plan

Status: source-backed design, 2026-10-01. No server was launched, no network mode was enabled, and existing standalone guards remain in place. The immediate deliverable is a cosmetic body/gesture visualizer. Multiplayer weapon, reload, melee and ammunition authority are separate work.

A stock KF server executable plus an UnrealScript package is the first candidate. The installed SDK exposes the required replication and bone-pose APIs; it does not yet establish that every player skeleton can be posed at the correct animation boundary. A custom native server is therefore **conditional**, not a prerequisite. If script animation timing proves insufficient, a client-side pose addon may be needed; that would not by itself require a native server.

## What a flat player needs

| Server/client combination | Expected result and evidence boundary |
| --- | --- |
| Ordinary KF server, ordinary flat client, local VR addon | No new remote gesture stream. The local addon cannot make another executable reconstruct poses it never receives. |
| KFVR-enabled server, stock flat executable that loads the visualizer package | Intended first target: stock locomotion/gameplay, visible VR head/arm gestures. Package transfer and actual rendering still require a private two-client test. |
| KFVR-enabled server, client refusing/missing the required package | Not a supported visualizer participant. Do not promise a transparent mixed connection without proving package negotiation. |
| Flat client with a native addon | Possible fallback for animation ordering or performance only; not assumed necessary. |

`Mutator.PreBeginPlay` calls `AddToPackageMap` for `bAddToServerPackages`; `Actor` declares that native API. The installed `Default.ini` lists `AllowDownloads=True`, `IpDrv.HTTPDownload` and `Engine.ChannelDownload`. These establish an existing package path, not proof that a particular server's download/redirect configuration works. Ship authored `.u`/source only; resolve existing character meshes/materials from each installation. Do not require the local-only converted first-person hand package on flat viewers.

## What already exists, and what is missing

- `PoseWire` encodes exactly 58 bytes: 16-byte version/flags/actor-epoch/sequence header plus three 14-byte position/quaternion targets. Positions are millimetres in a bounded +/-3 metre component range; quaternions are signed normalized 16-bit components.
- `PoseGate` checks epoch, sequence wrap/duplicates, receive-time rate, finite transforms and translation speed. It is a portable policy, not an Unreal RPC, owner authentication layer or remote-pose renderer.
- The codec has no declared head/left/right flag meanings, body-frame identifier, body yaw, tracking-reference lifetime, sample time, fingers or held-object state. A live protocol must specify those fields instead of treating 58 bytes as a complete network feature.
- The current native-to-script tracking packet is 132 bytes. It provides head position and body yaw, but **no head rotation**. Its hand targets combine controller grip position with **aim orientation**. They must not be relabeled as raw grip poses.
- The accepted hands can move from controller tracking to measured weapon/support/magazine/action anchors. A remote arm should reach the resolved displayed palm, not pull the hand away from that anchor.
- Existing rig, reload and visual actors are local (`ROLE_None` and/or standalone guards). Existing `KFVRDevMutator` is a private test entry point, not a multiplayer mutator to unlock by deleting one guard.

## Shared body/pose contract

The body and network audits agreed on this contract. It applies to cosmetic presentation; it never supplies authoritative pawn movement or damage.

| Item | Contract |
| --- | --- |
| Actor identity | Server binds an owned channel to its current `PlayerController.Pawn`, with a fresh nonzero actor epoch on possession/respawn. Never trust a client-supplied actor pointer as the recipient identity. |
| Root | Stock `Pawn.Location`, in the engine's world frame. Keep collision, locomotion, health, weapon ownership and the authoritative root unchanged. The HMD is not the pawn root. |
| Position frame | Metres relative to that pawn root, rotated into the sample's explicit yaw-only body frame. UE model convention is X forward, Y right, Z up. `local = inverse(yaw) * (world - pawnRoot) / UnitsPerMetre`. |
| Reconstruction | `world = currently presented pawnRoot + yaw * (local * trustedUnitsPerMetre)`. Capture yaw with the sample; do not combine an old local pose with an unrelated newer yaw. Interpolate root/yaw/targets coherently. Snap/recenter starts a new reference generation. |
| Head target | Actual HMD centre/orientation transformed into the same frame. Until head orientation is explicitly delivered by the native adapter, mark that orientation unavailable and leave authored head animation; do not substitute body yaw and call it tracked head orientation. |
| Hand targets | For the first visualizer, use resolved left/right KF palm origins and measured palm bases after grip/support/magazine/action posing. Tag these as presentation-palm frames. Raw grip and aim are distinct optional future target kinds, not interchangeable rotations. |
| Validity | Separate head/left/right validity and orientation-validity masks, plus active/cosmetic capability state. Invalid targets immediately stop driving bones. Use a documented versioned envelope; do not silently reinterpret v1 flags. |
| Other presentation | Optional quantized thumb/index/grip curls and grasp-role identifiers. Weapon/magazine identity and reload stage are separate server-sanctioned cosmetic state; they cannot grant ammunition or fire permission. |
| Feet/pelvis | Feet come from stock animation and local floor-trace estimates. Pelvis adjustment is a bounded cosmetic offset. No claim of foot tracking, no root teleport, and no replicated floor-hit point accepted as gameplay truth. |

Current source is sufficient to obtain resolved palms and head position. Adding true head orientation is the next explicit ABI/source-contract change, with x86/x64 layout guards and a measured orientation check. The 58-byte codec can inform quantization, but protocol fields and UE serialization size must be designed and measured independently.

The separately audited `KF_Soldier_Trip.British_Soldier1` is the first body candidate: `CHR_Pelvis`, spine chain through `CHR_Ribcage`, neck/head, bilateral collar/upper/forearm/palm chains, and thigh/calf/ankle/toe chains. `WeaponL_Bone` and `WeaponR_Bone` descend from the corresponding palms. Its import transform differs from first-person weapon hands (scale 1.25, origin Z44.5, yaw -16384); measure it at runtime and do not reuse the weapon model's 300 units/metre conversion. Other character/DLC skeletons remain opt-in only after topology/pose checks.

## Minimal transport prototype

Use a separate authored visualizer package and mutator, leaving the standalone gameplay package's guards intact.

1. Create one server-owned channel actor per player, owned by that `PlayerController`, with owner-only relevance. A flat client needs the receiving side; a VR client also supplies tracked presentation targets. A small reliable handshake binds protocol version, capabilities, pawn identity and actor/reference epochs.
2. Send owner-to-server pose submissions as **unreliable RPCs** on that owned channel. `PlayerController` has existing unreliable client-to-server movement RPCs; this establishes the mechanism. The server checks the channel's current owner/pawn, generation, sequence, finite/range bounds and update budget before accepting data. A claimed VR flag does not grant gameplay authority.
3. Forward only the latest accepted pose to each eligible viewer through that viewer's owned channel, using unreliable cosmetic RPCs. Do not enqueue every tracked frame as reliable calls. Lifecycle bind/disable messages are reliable, small and independently bounded.
4. Start with nearby players in a private small session, then add server-chosen distance/view-target/team/spectator eligibility. Do not let senders choose their recipients. Cap per-viewer streams and send a disable/reset when leaving relevance. Owner-only recipient channels avoid relying on an unproven native relevance override for invisible noncolliding helper actors. Do not set a permanent all-player broadcast on every pose actor.
5. Each viewer keeps one small bounded record per currently bound VR pawn and creates a local, noncolliding body presenter. No custom socket, UDP port, native net-driver hook or replacement server executable is required by this proposal.

First RPCs should use a small compiler-verified fixed schema. Do not assume a C++ 58-byte array or UnrealScript struct serializes as exactly 58 network bytes. Capture actual channel traffic, loss behavior and per-client cost before choosing packed word fields. `PostNetBeginPlay`/`bNetNotify`/`PostNetReceive` and native actor/attachment replication are available, but class declarations alone do not prove ordering or delivery semantics for a new schema.

## Exact lifetime and stale-pose rules

Proposed initial timings are presentation policy, to be tested rather than claimed as measured acceptance:

- Begin hidden with no valid targets. A reliable server binding is required before accepting a pose for an actor epoch/reference generation. Old bindings, duplicate/older sequences, mismatched pawn lifetimes and malformed transforms cannot refresh the last-valid receipt time.
- A new owner, pawn, character mesh/skeleton, reference generation or protocol changes clears interpolation/history and all IK overrides. Only a matching new sample can enable them again; never interpolate across those boundaries.
- Explicit tracking loss disables the affected target immediately, even if it arrives inside the ordinary rate interval. Keep its sequence watermark; a late valid packet from before that loss must not revive the limb. Rate/speed gates must distinguish validity transitions from a tracked hand teleport.
- No valid refresh for **250 ms of local monotonic elapsed time** means no target is eligible. Clear the IK layer and restore stock animation; an optional <=100 ms visual blend must not leave the stale pose held indefinitely. Never extrapolate a hand after this deadline.
- Presentation interpolation may buffer about 50 ms initially, bounded to a few samples. Delayed/reordered samples cannot extend lifetime, rewind the accepted sequence or overwrite a newer validity mask.
- Pause, world teardown, logout, death, possession loss or channel destruction immediately disable the presenter and release exact actor/bone/attachment state owned by the mod. Relevance re-entry is a fresh baseline, not a continuation of an old arm pose.
- Keep a server-owned acceptance clock and a viewer-owned presentation clock. Sender timestamps are display hints, never authorization. UE2 `Level.TimeSeconds` is affected by `TimeDilation`; `Level.TimeSeconds/currentDilation` is wrong across ramps. Script wall-clock fields and per-Tick dilation-compensated accumulation are candidates that require measurement through ZED time, pause and stalls. `Core.Time` is explicitly unimplemented/commented out in this SDK; do not call invented `GetGlobalTime`/`RealTimeSeconds` APIs. Native timing is a fallback only if a sufficient script clock cannot be demonstrated.

Current `PoseGate` does not provide stale hiding, lifecycle cleanup or validity-transition semantics. Its speed comparison also considers all three target positions regardless of the undocumented flags. A future relay must handle tracking-loss masks explicitly rather than allowing an invalid/zeroed hand position to be rejected as a large movement and leaving an old hand visible.

## Body and attachment posing

Use the shared body solver on a separate local presentation actor first. Load the viewed pawn's installed mesh/skins, capture native rest frames, solve the head/arms and keep native lower-body animation. Apply only audited bone chains and keep root transforms outside IK. A third-person weapon attachment follows the resolved weapon/palm relationship; it must not change the server's weapon trace or collision.

`Actor` provides `GetBoneCoords`, `SetBoneDirection`, `SetBoneRotation`, `SetBoneLocation`, `BoneRefresh` and `AttachToBone`. `KFWeaponAttachment.PostNetReceive` already reconnects a changed instigator, and `WeaponAttachment` replicates firing/hit effects. These are useful boundaries, not proof that render-time animation will preserve every override. Test native animation/LOD ordering, socket effects and restore behavior before altering the visible stock pawn. Stock `KFPawn` foot-adjusting bodies are commented out; their presence is not evidence of working full-body ground IK.

The first local preview should place a separate visible ghost beside the real pawn, with collision/damage disabled and original pawn visibility untouched. Feed it current resolved targets through the same declared frame conversion. A later flat visualizer may replace only its local presentation once failure cleanup is proven. Local VR head hiding and remote full-head visibility are separate presentation choices. Do not scale the actual pawn's head to hide it for every viewer.

BF2142 contributes useful disciplines: validate topology/lifetime, preserve authored wrist/weapon relations, apply from fresh native animation and restore only owned writes. Its own hand/network documentation explicitly leaves remote IK unverified. No Refractor offsets, bone counts, network format or demonstrated multiplayer capability transfers to KF.

## Bandwidth target and evidence

Start at 20 pose submissions/second, render/interpolate locally at display cadence, and cap accepted traffic. The existing gate's 30 Hz ceiling is a policy example, not a measured Unreal transport limit.

| 58-byte payload only | 20 Hz | 30 Hz |
| --- | ---: | ---: |
| One VR sender upload | 1,160 B/s | 1,740 B/s |
| One viewer receiving five VR players | 5,800 B/s | 8,700 B/s |
| Server forwarding six VR players to the other five | 34,800 B/s | 52,200 B/s |

These are lower-bound arithmetic, not wire bandwidth or latency results. Protocol extensions, actor/RPC headers, acknowledgements, packet framing and ordinary game traffic add cost. Reliable pose queuing under loss is specifically excluded. Measure both listen and dedicated behavior later; a dedicated server should validate/route data and avoid running character IK or loading render-only assets unnecessarily.

## Concrete next checkpoints

1. **Current authorized standalone work:** complete the local native-body presenter; establish explicit true head orientation and resolved-palm frame conversion; verify known 90-degree turns, translations, grip/action hand locks and root preservation. No networking enabled.
2. **Isolated script prototype, after that contract is fixed:** compile a separate minimal visualizer channel/receiver package. Commandlet cases cover version/binding changes, reordered/duplicated packets, loss-mask transitions, timeout cleanup and bounded interpolation. A local ghost exercise verifies received coordinates without claiming network delivery.
3. **Future separately coordinated private network trial:** one pose sender and one ordinary flat executable loading the `.u`; verify actual received sequences and visible waving/head turns, no native addon on the flat process, package download or explicit staged install, and ordinary stock movement/fire behavior.
4. Extend that trial with 10-30% packet loss, jitter, delayed old epochs, malformed/rate-excess submissions, tracking loss, ZED-time ramps, pause, death/respawn, character switch, map travel and relevance exit/re-entry. Each target must clear by its deadline; no collision/root/ammo/damage state may change from cosmetic packets. Test both listen and dedicated configurations before any compatibility claim.
5. Only after a demonstrated script limitation, decide whether a native client render hook or server timing/transport helper is required. Full VR gameplay multiplayer remains a separate server-authority project.

## Source audit index

Tracked source:

- `include/kfvr/gameplay/PoseWire.h`, `src/gameplay/PoseWire.cpp`, `tests/PoseWireTests.cpp`: existing codec/admission policy and present coverage.
- `src/native/NativeWeaponRig.cpp` packet declaration and `UpdateWeaponRig`; `src/native/NativeCameraTrial.cpp::ReadTrackedBody`; `src/interaction/TrackedWeapon.cpp::ControllerPresentationPose`: actual target contents and yaw/body framing.
- `include/kfvr/interaction/RigPose.h`, `src/interaction/RigPose.cpp`: topology validation and generation-scoped restoration.
- `unrealscript/KFVRGameplay/Classes/KFVRWeaponRig.uc::UpdateHands`, `KFVRHandVisual.uc`, `KFVRDevMutator.uc`: current resolved palms, presentation ownership and standalone boundary.

Owner-installed SDK, retained only in ignored local storage:

- `local/sdk/Engine/Classes/Mutator.uc::PreBeginPlay`, `Actor.uc` networking flags/replication/bone APIs/`AddToPackageMap`, `PlayerController.uc` replication block.
- `local/sdk/Engine/Classes/WeaponAttachment.uc`, `InventoryAttachment.uc`; `local/sdk/KFMod/Classes/KFWeaponAttachment.uc::PostNetReceive/PostNetBeginPlay`.
- `local/sdk/Engine/Classes/LevelInfo.uc` time fields; `local/sdk/Core/Classes/Time.uc` unimplemented declarations.
- `local/sdk/KFMod/Classes/KFPawn.uc::SetFootHeight/UpdateFeetCoords`, `KFPawnFootAdjuster.uc`, `SPECIES_KFMaleHuman.uc::LoadResources`.
- Installed `System/Default.ini` download/package declarations. No installed configuration was changed.

Read-only comparison: `G:\BF2142VR\source\docs\BF2142_HANDS.md` and `BF2142_ALPHA_READINESS.md`. Prior engine work supplies design lessons, not KF network evidence.
