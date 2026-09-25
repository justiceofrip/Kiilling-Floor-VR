# Architecture based on the installed game

## Verified engine boundaries

KillingFloor.exe and its Engine/Core/D3D9Drv modules are x86. The active render device is D3D9Drv.D3D9RenderDevice, confirmed by live UObject inspection and native callbacks. D3D8 is also imported by the launcher; that import does not identify the active renderer.

Packages use UE2 version 128/licensee 29. The package reader recovered 2,381 TextBuffers and class inheritance records with no source decoding errors. These TextBuffers contain script declarations and bodies, **not a complete defaultproperties export**. The weapon audit separately reads verified live UClass default buffers; 83 of 96 weapon-derived classes were loaded with usable defaults. No official native C++ headers or UCC executable were found in the install.

Script-visible mechanisms include PlayerCalcView, WeaponFire.GetFireStart and AdjustAim, KFWeapon.ReloadMeNow/ConsumeAmmo/AddReloadedAmmo, Actor.Trace, GetBoneCoords, SetBoneDirection, SetBoneLocation and SetBoneRotation. Canvas.DrawPortal exists, but a symmetric FOV portal alone does not provide OpenXR asymmetric projections, resource ownership or frame submission.

Native exports include UGameEngine::Tick/Draw, FPlayerSceneNode::Render, FCameraSceneNode::UpdateMatrices, and D3D9RenderDevice::Lock/Unlock/Present/ReadPixels. Export names supply concrete x86 signatures. Core accessor instructions and object index/class/name relationships validate the read-only reflection profile. Native addresses are resolved per process; no Refractor or Frostbite offsets were imported.

## Selected direction

`KF x86 native adapter -> private eye surfaces -> D3D11 pair transport -> x64 OpenXR host`, with KFVR UnrealScript gameplay/replication layered on the existing KF classes. The x64 host avoids depending on a 32-bit headset runtime. The installed runtime registry points to SteamVR's x64 manifest; no WOW6432Node runtime registration was observed.

An engine scene/camera integration plus a graphics transport is the current candidate. A full custom UE2 render device would require an unaudited legacy native ABI and much more renderer work. Intercepting D3D9 Present alone supplies one already-rendered image and cannot implement correct stereo, culling or physical aiming.

**Not yet solved:** the verified FPlayerSceneNode callback must still be audited for render-side animation, visibility/LOD, occlusion, particles, temporal effects, first-person weapon and post-processing ownership before it can be replayed. The successful trace proves no *engine Tick* occurs within Draw; it does not prove all rendering is free of simulation-like side effects. No render callback has been replayed.

The native device is non-Ex D3D9 with 8x MSAA in the captured session. A future D3D9Ex promotion path needs its own resource-pool/reset compatibility work. A CPU readback path is suitable for bounded proof/capture and could transport early correct stereo, but is not assumed to meet VR latency. GPU sharing must use same-adapter, compatible non-MSAA shared resources. See [Microsoft's sharing requirements](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11device-opensharedresource).

## Frame and camera invariants

- One native simulation/animation advance supplies both eyes. Pair identity includes frame, resource epoch, reference space and predicted display time.
- Both images must exist and be restored to a clean native state before publication. An incomplete pair is dropped.
- UE world axes are X forward, Y right, Z up. Camera matrices use row vectors, with right/up/forward rows. Native near plane observed as 5 UU; perspective depth approximately 65536 UU. Values are observation, not globally fixed constants.
- KF's inverse-rotation constructor negates rotator units before its 14-bit lookup. The camera adapter matches this quantization and the captured native basis.
- Scale is explicit, never borrowed from Battlefield. Physical calibration remains pending.
- Keep head, body heading, locomotion, weapon aim and recoil separate. Do not teleport the server pawn to follow room-scale movement.
- Native stereo capability remains disabled until eye targets, culling, render side effects and complete restoration are demonstrated.

## Gameplay and ammunition

KF AmmoAmount already includes loaded rounds. Stock ConsumeAmmo decrements the native ammo total and magazine count; AddReloadedAmmo selects a loaded count from that total rather than adding rounds. KFVR must take authority over reload transitions when enabled so classic reload notifications cannot also credit ammo.

AmmoMachine owns chamber, inserted magazine/tube, retained hand magazine, reserve, spent and discarded rounds. Every accepted transition conserves the initial budget. Clients request actions without supplying round counts. Sequence/ownership epoch and trusted server time reject stale/replayed commands. Switching, death and drop cancel pending gestures without reversing completed physical operations. Fire cadence survives suspend/re-equip.

This accounting currently supports closed-bolt detachable magazines and tube-fed manual/self-loading weapons. Revolvers, break actions, dual independent guns, magazines as inventory objects, pickups/trader integration and special weapons require additional authoritative adapters. Profile timings must be derived from the native weapon/perk state; example defaults are not a final balance decision. Snapshot correction is a trusted-server API, not a client RPC.

MeleeGate returns bounded attack eligibility and a segment, not damage or a trusted hit. Integration still needs authoritative swept traces, weapon-specific arcs/range/damage/cooldowns and velocity evidence that accounts for packet loss. The 58-byte pose codec is an engine-independent transport design, not yet a KF replication implementation.

## Controllers, IK and UI

OpenXR Actions supports Khronos Simple, Oculus Touch, Valve Index, Microsoft motion controllers and Vive bindings, with separate grip and aim poses. Only focused, valid tracking produces controls. Haptics are capped at 250 ms and suppressed without focus. The input-consumer contract is local C++; controller samples are not yet added to the existing head/eye IPC channel or KF input bridge. Standard bindings follow the [OpenXR specification](https://registry.khronos.org/OpenXR/specs/1.0-khr/html/xrspec.html).

Bone APIs make a script-first remote-pose prototype plausible, but KF skeleton topology, bone spaces, animation timing, dedicated-server behavior and render-thread ownership still need verification. The imported restoration cache avoids reusing previous IK results as authored animation and keys state by actor/skeleton generation. It is not a complete KF full-body solver.

Menus/HUD need their own VR plane or quad layer, controller pointer and focus handling. Existing pickups/trader rules remain the baseline. Scopes and procedural legs/feet follow working stereo, input and authoritative weapons.

## Crossplay decision

Flat and VR players should share a KF server and normal KF game rules. An unchanged flat executable can load UnrealScript packages supplied by the server; Engine.Mutator.bAddToServerPackages and AddToPackageMap exist in the installed source. A completely vanilla package set has no consumer for new head/hand targets and cannot invent custom IK.

The target is therefore **B: a KFVR gameplay/client package shared by VR and flat clients**, with no HMD requirement for flat users. Automatic package download still needs an actual server/client test. If script bone controls prove insufficient, full remote body rendering may additionally require a small native flat-client addon. This is an unresolved acceptance item, not a promise of zero installation. Existing unmodified public servers cannot validate new manual reload/melee rules. Perk/stat eligibility and whitelist behavior must be tested separately; local progression compatibility is not proof of official online stat compatibility.
