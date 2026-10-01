# Local native KF hand prototype

The development source can build separate VR hands from the owner's installed `KF_Weapons_Trip.9mm_Trip` mesh. It retains the game's hand geometry, UVs and current character sleeve/hand material. These are actual KF assets, locally adapted for independent controllers. The ordinary launcher still uses KFVR's authored placeholder gloves; select `-NativeHands` explicitly for this prototype.

The owner has not tested this option in the headset. Finger shapes, the flat dark wrist closure and hand-to-weapon contact remain provisional. It does not add arms, optical hand tracking or complete weapon-specific grasp animations. The existing capacitive controller inputs drive the fingers.

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
