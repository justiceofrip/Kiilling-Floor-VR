# Local native KF hand prototype

The development source can build separate VR hands from the owner's installed `KF_Weapons_Trip.9mm_Trip` mesh. It retains the game's hand geometry, UVs and current character sleeve/hand material. These are actual KF assets, locally adapted for independent controllers. The ordinary launcher still uses KFVR's authored placeholder gloves; select `-NativeHands` explicitly for this prototype.

The October 1 follow-up wearer session accepted the corrected thumb touch and weapon grips on build `20a6bd4`, and confirmed steady weapon presentation while walking. The wearer described the thumb as perfect and the grips as great. Individual weapon/art polish, the flat dark wrist closure and detached-magazine poses remain open. The existing capacitive controller inputs drive the fingers; this does not add arms or optical hand tracking.

## Build and launch

Use the installed official KF SDK and the existing KFVR native/script build prerequisites. Obtain UE Viewer from its [author](https://www.gildor.org/en/projects/umodel); the executable is a local build dependency, not bundled with KFVR. Its [source and license](https://github.com/gildor2/UEViewer) document the ActorX export convention. No exporter code is embedded in the converter.

From the source checkout, with KF closed:

```powershell
py -3.12 tools/build_scripts.py --game 'F:\SteamLibrary\steamapps\common\KillingFloor'
py -3.12 tools/prepare_native_hands.py --game 'F:\SteamLibrary\steamapps\common\KillingFloor' --umodel 'local/asset-tools/umodel/umodel_64.exe'
.\Start-KFVRDev.ps1 -NativeHands
```

After joining the private match, the existing `Play-KFVR.ps1 -Weapons` starts the bounded headset session. `-Unattended -NativeHands` starts the existing desktop diagnostic fixture; it does not itself launch OpenXR. `-NativeHands` cannot be combined with the legacy `-ArmTest`.

The converter reads the installed package and writes only under ignored `local/`. It exports no textures. The generated `KFVRLocalHands.u` contains proprietary derived geometry and is local-only. It is excluded from Git and the curated release ZIP. The normal `KFVRGameplay.u` references the optional mesh by a runtime string and contains no extracted KF geometry.

The source-package, exported-mesh, exporter and generated-package hashes are recorded in `local/native-hands/manifest.json`. Staging verifies the prepared package against this game's source-package hash, then checks ownership of **both** destination files before copying either. Changed/unrecorded packages are rejected. A game update requires rebuilding the local package. Missing native assets fail this explicit option; they do not silently substitute placeholder hands.

`reports/native-hand-install.json` records the added `System/KFVRLocalHands.u`; the ordinary gameplay package has its own `reports/dev-install.json`. Neither auto-loads during a normal game launch. To remove this option, close KF and remove only the recorded local `KFVRLocalHands.u`. Keep the manifests when reusing the same installation.

## Conversion and evidence limits

Each hand has a palm root and fifteen finger bones. The converter preserves native bind geometry with one rigid coordinate change, folds helper/metacarpal influences into the palm, clips at the wrist and adds a palm-weighted cap. Native model scale is 60 raw units/metre, derived from the installed mesh's scale of five and the existing weapon scale. Native finger roll is retained while the runtime drives finger pointing/curl.

Use shared skin points and descending influence weights. The first triangle-expanded exports passed numerical bone checks and round-tripped through the SDK, but rendered torn palms in held poses. Frozen animation and a plain material did not fix them. Shared points plus descending influences removed that visible defect in the controlled private comparison. This isolates the working representation; the individual contribution of those two changes was not separated.

The rig preloads hidden hands after a living local pawn is available. Tracking still controls when they appear. This moves asset loading out of the first tracking packet; no measured headset latency improvement is claimed.

The `--hands`, `--inventory` and `--reload` desktop trials exercise the same native game/renderer with synthetic controller poses. Their numeric reports and eye images are complementary: a passing bone/interaction report alone does not establish correct mesh rendering or wearer comfort. Exact run IDs and failures are in `WORKLOG.md`.


## Weapon contact and thumb touch

With native hands enabled, the weapon proxy measures both palms and fifteen finger joints from its installed idle pose before hiding the original arms. The separate VR hand follows that grasp after the weapon has finished support/recoil and mechanical updates. The rifle support palm follows the authored foregrip; it releases when the controller separation differs from the weapon grip separation by more than the existing 18 cm grab radius. This presentation adjustment does not move the tracked interaction coordinates.

Thumb touch blends independently into the native resting thumb pose, or the held weapon's thumb pose. Release raises the three thumb joints along separate arcs; holding a gun no longer forces thumb curl. The gun index finger still layers trigger input over the grasp. All hand joint overrides are queued before skeletal evaluation, preventing the previous pose from surviving for a frame after a sharp wrist turn.

Coverage includes single/dual 9mm, Deagle, Bullpup, knife, syringe and welder. The grenade's opposite-hand toss pose and detached magazine grasp retain provisional poses. The common 9mm hand anatomy differs slightly from other weapons, so matching joint frames is not exact surface contact. The follow-up wearer session accepted the new thumb/grip behavior overall; it does not establish exhaustive acceptance of every weapon, pose or controller profile. Some paper-like polygons appeared near holstered weapons in the office fixture; their source is unresolved and may be unrelated map geometry/effects. A sleeve material experiment did not change them and was discarded.
