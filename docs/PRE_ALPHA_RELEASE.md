# Kiilling Floor VR 0.1.0-prealpha.1

Release candidate for original Killing Floor 1065 on Windows. Publication is on hold after the first wearer pass reported broken head/hand placement. Corrective work is underway; no public release has been created.

Included: native stereo/head tracking, controller locomotion and snap turns, independent 6DoF weapon aim, capacitive hand poses, hip/shoulder/chest holsters, supported firearms and tools, physical single-pistol reloads, dual-pistol belt reloads, rifle support grip, physical knife/grenade interactions, offhand status and an initial world-space trader.

Native test evidence: original candidate x86 23/23 and x64 22/22 suites; full-resolution standalone receiver passes 25 weapon and 22 HUD/grasp checks. Separate physical reload, dual reload, grenade, trader, syringe and door trials are recorded in WORKLOG.md. These use synthetic controllers in the real game and do not substitute for wearer acceptance.

Known limits: standalone only, three-minute sessions, provisional hand/arm art and calibration, incomplete weapons/menus/inventory/networking, and a CPU image-transfer bottleneck. Rough completion estimate is about 30% of the intended total conversion, not a release-readiness score. Use the [test guide](PRE_ALPHA_TEST.md).

The runtime ZIP contains only KFVR build outputs, the redistributable OpenXR loader, pinned pefile Python package, launch/audit tools and licenses. Original KF files, SDK/compiler files, personal INIs, machine logs and source extraction are excluded. Each member is recorded by SHA-256 in release-manifest.json, with a checksum for the complete archive.

## First wearer pass and corrective candidate

At 2072 x 2216 per eye the user confirmed native VR was visible but reported badly broken head/hand placement and deferred further troubleshooting. This fails wearer acceptance regardless of the passing synthetic checks. The game then exited normally before the native test deadline, so final probe/XR totals were not available.

Corrective candidate: startup captures position and yaw only, once headset focus is active. Initial head pitch/roll no longer tilt the tracking reference. Bounded incremental head/controller pose logs survive early exits. The launcher reports actual dimensions and fits oversized runtime recommendations to the bridge budget before injection. Early game exit produces a specific error instead of a missing-file traceback. These changes need a wearer retest; they do not establish that all placement/art issues are solved.

Corrective build checks: 24/24 x86 and 23/23 x64, including 48 tilted-startup camera/controller cases. No corrective wearer pass yet.
