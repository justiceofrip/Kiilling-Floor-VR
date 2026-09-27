# Physical headset checkpoint — 2026-09-25

The x64 OpenXR diagnostic now works with the user's connected headset and controllers. This is synthetic-scene hardware acceptance; subsequent native KF stereo and movement acceptance is recorded below.

SteamVR reports `SteamVR/OpenXR in Meta compatibility mode`, system `SteamVR/OpenXR : oculus`, with recommended eye images of 2064 x 2208 and an sRGB RGBA8 swapchain. This runtime identification does not establish the precise headset model.

| Run | Submitted stereo pairs | Rejected pairs | Evidence |
| --- | ---: | ---: | --- |
| 45-second visible room | 3,781 | 0 | headset-scene-20260925-180826 |
| 60-second controller check | 5,393 | 0 | headset-input-20260925-181137 |
| 30-second stick follow-up | 2,397 | 0 | headset-sticks-20260925-181313 |

All three sessions created and shut down successfully without a reported runtime or diagnostic error. Frame counts are observations, not measurements of display latency or sustained KF performance.

The wearer confirmed that the cube/grid room was visible and stayed fixed while turning/leaning, and that both trigger presses caused a short vibration. The controller run recorded 4,127 valid grip and aim samples per hand, full trigger and grip values, five trigger press/release cycles per hand, and five/six grip cycles. Both controllers requested five 20%-amplitude, 50 ms haptic pulses.

The initial controller run had no right-stick movement and only partial left-stick motion. A targeted follow-up recorded both axes in both directions: left X [-0.906, 0.992], Y [-0.992, 1.000]; right X [-0.898, 0.992], Y [-0.945, 0.992]. The user confirmed moving both sticks. No binding change was necessary.

Raw machine reports under reports/ deliberately keep headset_visuals_verified=false because the program cannot judge the wearer's experience. The separate reports/headset-acceptance-20260925.json records their confirmations. Killing Floor remained running; the diagnostic did not inject into it, change settings, or drive gameplay.

Remaining acceptance: native game-to-XR transport, sustained native eye rendering, actual HMD-driven KF views, world-scale/comfort/latency checks, controller gameplay, tracking/focus-loss behavior on hardware, map/device transitions and multiplayer. The earlier WirelessHmdNotConnected failure was resolved by connecting the user's headset.

## 2026-09-27: accepted movement, native snap turning and recommended resolution

The wearer accepted left-stick movement in native-headset-20260925-193726 / native-trace-20260925-193728-003583: "worked great, left works but no snap turning or right input. but it worked great". Evidence records 440 fresh pairs, 281 retained projections, 887/887 movement restorations and zero restoration failures. The owner declined a further wearer test for turning and is now asleep; continue local development without headset prompts.

Right-stick snaps now use the audited Core.dll UObject::CallFunction boundary for KFPlayerController.UpdateRotation, inside the existing paired PlayerTick. The adapter adds a one-shot rotation demand after stock weapon/FOV sensitivity, calls the normal function once, and restores the previous aTurn. It does not replay simulation. Reflected PlayerTick.DeltaTime, rotation, exact module hashes and relocated entry bytes are checked on each launch. Input ABI is v9. Separate left/right policies allow one tracked hand to work if the other is lost; neutral rearming protects focus/tracking/ownership changes.

Combined native-bridge-20260927-131134 / native-trace-20260927-131134-427935 passed 175 consumed pairs at 2064 x 2208 per eye, 330/330 movement restorations, three requested/measured/restored 30-degree snaps, every interruption phase, and all hook/resource cleanup. This was synthetic controller input with real native world rendering, without HMD presentation. A preceding run rendered 191 full-size pairs but correctly failed the movement criterion when the player died; a lobby attempt was rejected before injection. Readied a new standalone BioticsLab match for the passing run.

The previous bridge dimension limit of 2048 blocked the runtime's recommended size. It is now 4096 per dimension with the existing 256 MiB aggregate transport cap. The XR launcher defaults to 100% of runtime recommendations, not hardcoded Quest dimensions. Full-size CPU transfer averaged about 16.7 ms per rendered pair in this test; the receiver consumed about 23 fresh pairs/sec and 18 pairs missed their deadline. This is working full-resolution transport, not accepted shipping performance. GPU transport and rendering cadence still need work.

Play-KFVR.ps1 combines movement, snap turning and recommended resolution with a default 120-second session (15..180 configurable). Longer duration support compiles, but this checkpoint exercised the 15-second native window, not a two-minute HMD session. PE parsing is cached per module during preflight while retaining every binary/prologue check, reducing measured startup from roughly 20 seconds to roughly two. x86 18/18 and x64 17/17 pass. No original game files/settings changed.
