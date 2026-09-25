# Physical headset checkpoint — 2026-09-25

The x64 OpenXR diagnostic now works with the user's connected headset and controllers. This is synthetic-scene hardware acceptance; actual Killing Floor images have not been submitted to the headset.

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
