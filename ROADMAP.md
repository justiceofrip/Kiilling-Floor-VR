# Roadmap

## Completed foundations

- [x] Inspect installed packages/binaries and previous VR work; preserve originals.
- [x] Extract exact installed script declarations/bodies and build a weapon coverage ledger.
- [x] Recover/read UObject metadata and identify local standalone ownership.
- [x] Compile portable VR math, frame policies, graphics transport and x64 XR host.
- [x] Validate native x86 hook ABI, record real render callbacks and verify hook removal.
- [x] Capture an actual native D3D9 frame, including MSAA resolve and compatibility-vtable handling.
- [x] Match native UE2 camera math and construct asymmetric eye matrices.
- [x] Implement/test conserved magazine/chamber and tube reload logic, melee gating and compact poses.
- [x] Implement OpenXR actions and controller/haptic IPC; verify x64/x86 focus/expiry/recenter behavior. Hardware acceptance pending.
- [x] Verify non-Ex D3D9 CPU pair transport, MSAA resolve, pixel formats and reset; high-resolution performance remains insufficient.
- [x] Install the official SDK, compile a KFVR gameplay policy package and run real-engine tests/C++ parity checks.

## First playable vertical slice

- [ ] Audit render-side animation/culling/temporal ownership and prove state restoration.
- [ ] Render two actual KF eye views from one simulation step into independent surfaces.
- [ ] Connect D3D9 eye surfaces to the verified D3D11/x64 presentation transport.
- [ ] Submit an actual KF stereo pair to OpenXR and verify it in the headset.
- [ ] Add controller samples and semantic actions to the native IPC/gameplay bridge.
- [ ] Bind head/collision/locomotion and physical aim for one supported pistol.
- [ ] One authoritative magazine/chamber reload family integrated with KF ammo, perks and inventory.

## Gameplay expansion

- [ ] Shotgun shell insertion/manual pump; one melee weapon with server swept traces.
- [ ] Reload interaction zones, dominant hand, two-hand support and haptic event dispatch.
- [ ] Dedicated reload mechanisms for revolvers, break/bolt/lever actions and unusual weapons.
- [ ] Weapon switching/death/drop/pickup/trader behavior with authoritative state correction.
- [ ] KF skeleton profiles, local body IK, remote interpolation and actor-lifecycle handling.
- [ ] VR HUD/trader/menu presentation, scopes and accessibility reload options.

## Multiplayer and delivery

- [ ] Private dedicated server, server-owned VR capability and validated pose/weapon requests.
- [ ] Flat client package download and remote-body acceptance without a headset.
- [ ] Perk progression/whitelist and custom map/mod compatibility testing.
- [ ] Reversible installer/launcher, device loss/loading/reconnect, performance profiling.
- [ ] Exact staged payload and real headset/multiplayer acceptance before release.
