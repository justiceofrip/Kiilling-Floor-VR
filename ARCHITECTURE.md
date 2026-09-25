# Architecture based on the installed game

## Verified engine boundaries

KillingFloor.exe and its Engine/Core/D3D9Drv modules are x86. The active render device is D3D9Drv.D3D9RenderDevice, confirmed by live UObject inspection and native callbacks. D3D8 is also imported by the launcher; that import does not identify the active renderer.

Packages use UE2 version 128/licensee 29. The package reader recovered 2,381 TextBuffers and class inheritance records with no source decoding errors. These TextBuffers contain script declarations and bodies, **not a complete defaultproperties export**. The weapon audit separately reads verified live UClass default buffers; 83 of 96 weapon-derived classes were loaded with usable defaults. The initial install lacked UCC; the official Steam SDK is now installed and provides the compatible compiler/script sources. Native C++ SDK headers remain absent.

Script-visible mechanisms include PlayerCalcView, WeaponFire.GetFireStart and AdjustAim, KFWeapon.ReloadMeNow/ConsumeAmmo/AddReloadedAmmo, Actor.Trace, GetBoneCoords, SetBoneDirection, SetBoneLocation and SetBoneRotation. Canvas.DrawPortal exists, but a symmetric FOV portal alone does not provide OpenXR asymmetric projections, resource ownership or frame submission.

Native exports include UGameEngine::Tick/Draw, FPlayerSceneNode::Render, FCameraSceneNode::UpdateMatrices, and D3D9RenderDevice::Lock/Unlock/Present/ReadPixels. Export names supply concrete x86 signatures. Core accessor instructions and object index/class/name relationships validate the read-only reflection profile. Native addresses are resolved per process; no Refractor or Frostbite offsets were imported.

## Selected direction

`KF x86 native adapter -> private eye surfaces -> D3D11 pair transport -> x64 OpenXR host`, with KFVR UnrealScript gameplay/replication layered on the existing KF classes. The x64 host avoids depending on a 32-bit headset runtime. The installed runtime registry points to SteamVR's x64 manifest; no WOW6432Node runtime registration was observed.

An engine scene/camera integration plus a graphics transport is the current candidate. A full custom UE2 render device would require an unaudited legacy native ABI and much more renderer work. Intercepting D3D9 Present alone supplies one already-rendered image and cannot implement correct stereo, culling or physical aiming.

**Not yet solved:** the verified FPlayerSceneNode callback must still be audited for render-side animation, visibility/LOD, occlusion, particles, temporal effects, first-person weapon and post-processing ownership before it can be replayed. The successful trace proves no *engine Tick* occurs within Draw; it does not prove all rendering is free of simulation-like side effects. No render callback has been replayed. An expanded nine-hook trace found script HUD/UI callbacks and skeletal GetFrame calls during Draw; the two instrumented animation-advance functions had zero calls inside Draw in the sampled lobby and match. KFWeapon.RenderOverlays changes weapon location/rotation in the installed script. FLevelSceneNode::Render ran twice per Draw, so nested world/sky views must be distinguished before selecting a stereo boundary. GetFrame also writes pose caches; zero animation-update calls does not mean it is side-effect free.

The native device is non-Ex D3D9 with 8x MSAA in the captured session. A future D3D9Ex promotion path needs its own resource-pool/reset compatibility work. A non-Ex D3D9 CPU bridge now resolves and transports two supplied eye surfaces into the D3D11 pair stream. An isolated 32-bit D3D9 producer/64-bit consumer verified all pixels, RGBA/BGRA order, alpha, MSAA, backpressure, discarded-stream recreation and device reset. It never renders the eyes itself or changes native render-target bindings. Callers must release it before native device reset. The 256 MiB aggregate working-resource cap excludes caller-owned source surfaces and driver overhead. Measured transfer cost was 4-6 ms at 1024x768 and 16-25 ms at 2048x2048 with 8x MSAA in the second probe run; this is a prototype fallback, not a validated VR performance path. GPU sharing must use same-adapter, compatible non-MSAA shared resources. See [Microsoft's sharing requirements](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11device-opensharedresource).

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

OpenXR Actions supports Khronos Simple, Oculus Touch, Valve Index, Microsoft motion controllers and Vive bindings, with separate grip and aim poses. Only focused, valid tracking produces controls. Haptics are capped at 250 ms and suppressed without focus. The x64 host publishes controller samples over a separate mailbox inside the version-2 frame channel. Input delivery and neutral releases do not require an eye pair. Samples expire after 100 ms; focus/recenter transitions cancel old haptics. Reverse feedback uses one-shot sequence IDs. Both directions passed a real x64/x86 process test. Binding these controls to KF movement and weapons remains pending. Standard bindings follow the [OpenXR specification](https://registry.khronos.org/OpenXR/specs/1.0-khr/html/xrspec.html).

Bone APIs make a script-first remote-pose prototype plausible, but KF skeleton topology, bone spaces, animation timing, dedicated-server behavior and render-thread ownership still need verification. The imported restoration cache avoids reusing previous IK results as authored animation and keys state by actor/skeleton generation. It is not a complete KF full-body solver.

Menus/HUD need their own VR plane or quad layer, controller pointer and focus handling. Existing pickups/trader rules remain the baseline. Scopes and procedural legs/feet follow working stereo, input and authoritative weapons.

## Crossplay decision

Flat and VR players should share a KF server and normal KF game rules. An unchanged flat executable can load UnrealScript packages supplied by the server; Engine.Mutator.bAddToServerPackages and AddToPackageMap exist in the installed source. A completely vanilla package set has no consumer for new head/hand targets and cannot invent custom IK.

The target is therefore **B: a KFVR gameplay/client package shared by VR and flat clients**, with no HMD requirement for flat users. Automatic package download still needs an actual server/client test. If script bone controls prove insufficient, full remote body rendering may additionally require a small native flat-client addon. This is an unresolved acceptance item, not a promise of zero installation. Existing unmodified public servers cannot validate new manual reload/melee rules. Perk/stat eligibility and whitelist behavior must be tested separately; local progression compatibility is not proof of official online stat compatibility.

D3D9 transport follows the documented [GetRenderTargetData constraints](https://learn.microsoft.com/en-us/windows/win32/api/d3d9/nf-d3d9-idirect3ddevice9-getrendertargetdata), [MSAA resolve path](https://learn.microsoft.com/en-us/windows/win32/api/d3d9/nf-d3d9-idirect3ddevice9-stretchrect) and [adapter LUID mapping](https://learn.microsoft.com/en-us/windows/win32/api/d3d9/nf-d3d9-idirect3d9ex-getadapterluid). A temporary D3D9Ex enumerator identifies the adapter; it does not replace/promote the original device. Copying bytes with explicit channel order does not calibrate KF gamma/exposure.

The ammunition policy also compiles as KFVRGameplay.KFVRAmmoState inside the actual KF scripting engine. A commandlet passed 30,059 checks; 20 rolling state/result checkpoints agree with the C++ reference over 20,000 actions for detachable/self-loading and tube/manual profiles with ownership changes. This is regression evidence, not proof of complete behavioral equivalence. The script boundary uses nonnegative 31-bit sequence numbers and bounded monotonic integer milliseconds; a native/client adapter must convert this explicitly. Neither policy accepts client-supplied counts. KF inventory/fire/reload notifications are not bound to the policy yet.

The native target-management experiment now passes on the audited build: a native FAuxRenderTarget is bound, cleared, popped and destroyed; original GPU bindings and all 0x594 cached RHI bytes match afterwards. Pixel readback occurs from an AddRef-retained private surface at Present, outside scene submission. This is a bounded developer probe, not a persistent eye renderer. Camera construction, independent culling and repeated render-side effects remain separate work.

A bounded native stereo diagnostic now produces two actual world images using fresh FCameraSceneNode instances with parallel camera offsets and separate FAuxRenderTargets. It calls FCameraSceneNode::Render, not the full FPlayerSceneNode/HUD/weapon-overlay loop. The original player scene is never edited. The observed world-render path still invokes HUD.WorldSpaceOverlays and skeletal GetFrame; their repeated-frame behavior remains unresolved. Images are read at Present. No persistent render loop or XR game-image submission is connected.
