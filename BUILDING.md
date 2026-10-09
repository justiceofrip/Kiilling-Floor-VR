# Build the source checkpoint

Run commands from the checkout root. Use a short writable path without spaces, such as `C:\KFVR`; the optional VR-profile launcher depends on that restriction. All game paths below are examples. No BF2142/BC2 checkout or private developer report is needed.

## Prerequisites

- Windows x64 and your own original Killing Floor installation. Native admission currently recognizes the audited 1065 engine binaries in `tools/native_profile.py`; unsupported hashes fail rather than being patched blindly.
- The official Killing Floor SDK (Steam Tools, app 1260). `System\ucc.exe` and SDK dependencies must be present in the game directory supplied below.
- Visual Studio C++ x86/x64 tools, Windows SDK, CMake 3.24+ and Ninja. `Build.ps1` discovers the installed VS instance. The October 9 loader recipe was verified with VS 2026 using Ninja.
- Python **3.12 x64**, the Windows `py` launcher and Git.
- SteamVR configured as the active OpenXR runtime for headset use. Compiling native tests does not require a connected headset.
- Optional native KF hands: obtain UE Viewer from its [author](https://www.gildor.org/en/projects/umodel). It is an external local tool; its code/binaries and exported game assets are not distributed here.

```powershell
git clone https://github.com/justiceofrip/Kiilling-Floor-VR.git C:\KFVR
Set-Location C:\KFVR
$kfGame = 'F:\SteamLibrary\steamapps\common\KillingFloor'
py -3.12 -c "import struct; assert struct.calcsize('P') == 8, 'Use 64-bit Python'"
py -3.12 -m pip install --target local/pydeps pefile==2024.8.26 capstone==5.0.9
```

`pefile` is required by native inspection; `capstone` is used by disassembly audits. These pinned dependencies stay in ignored local storage.

## Supply the x64 OpenXR loader

The headers are tracked, but the loader DLL is an ignored runtime input. Build it from the [official OpenXR SDK 1.1.61 source](https://github.com/KhronosGroup/OpenXR-SDK/tree/release-1.1.61). Its [build instructions](https://github.com/KhronosGroup/OpenXR-SDK/blob/release-1.1.61/README.md#building) require `DYNAMIC_LOADER=ON` to produce a Windows DLL.

The following recipe was built successfully in an isolated directory for this handoff. It pins the upstream commit, uses the installed x64 C++ developer environment and does not replace a system DLL:

```powershell
$kfVsWhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$kfVsInstall = & $kfVsWhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $kfVsInstall) { throw 'Install the Visual Studio C++ tools first.' }
& (Join-Path $kfVsInstall 'Common7\Tools\Launch-VsDevShell.ps1') -VsInstallationPath $kfVsInstall -Arch amd64 -HostArch amd64 -SkipAutomaticLocation

git clone --branch release-1.1.61 --depth 1 https://github.com/KhronosGroup/OpenXR-SDK.git local/openxr-sdk
git -C local/openxr-sdk checkout --detach 5267613edf3d937e3d77556a106a65c2f82b25c6
$kfPython = & py -3.12 -c 'import sys; print(sys.executable)'
cmake -S local/openxr-sdk -B local/openxr-sdk-build -G Ninja -DCMAKE_BUILD_TYPE=Release -DDYNAMIC_LOADER=ON -DBUILD_TESTING=OFF "-DPython3_EXECUTABLE=$kfPython"
cmake --build local/openxr-sdk-build --target openxr_loader --parallel 4
New-Item -ItemType Directory -Path runtime/openxr/win64 -Force | Out-Null
Copy-Item -LiteralPath local/openxr-sdk-build/src/loader/openxr_loader.dll -Destination runtime/openxr/win64/openxr_loader.dll
```

Check each command succeeds before continuing. Ninja's output has no `Release` subdirectory. This builds the loader only; the SDK carries its own JsonCpp dependency and generated sources. Compiler-specific binary hashes may differ. The recipe was compiled and its AMD64 exports inspected; the newly built DLL was not substituted into the historical wearer session.

## Build native components and tests

```powershell
.\Build.ps1 -Architecture x86
.\Build.ps1 -Architecture x64
```

Both scripts compile and run their CTest suites, failing on errors. The x86 outputs include `KFVRNativeProbe.dll` and `KFVRNativeTrace.exe`; x64 includes `KFVRXrHost.exe`, native diagnostics and `KFVRAmmoParity.exe`.

Supply the loader **before** the x64 build so its post-build step copies it to `build/x64/runtime/openxr/win64/openxr_loader.dll`. If adding/changing a loader after an already up-to-date build, explicitly copy it there as well; the host resolves that executable-relative path, not a random DLL on PATH.

## Build authored UnrealScript

```powershell
py -3.12 tools/build_scripts.py --game $kfGame
```

This requires the x64 native build for ammo parity checks. It copies SDK inputs into ignored `local/compiler/`, compiles the authored classes and runs engine commandlet tests. Output is `build/unrealscript/KFVRGameplay.u`; the driver does not install it into a running game. Authored glove PSK/PSA and status LWO inputs are already tracked, so no model generator is needed for an unchanged clone. Failed builds save compiler logs in `reports/`.

No full game-script extraction is required to build the mod. The tracked classes derive from the owner's installed stock packages.

## Optional native KF hands

After the script build has prepared the isolated compiler, place the independently obtained UE Viewer executable at a known location and run:

```powershell
py -3.12 tools/prepare_native_hands.py --game $kfGame --umodel 'C:\Tools\umodel\umodel_64.exe'
```

This generates `local/native-hands/KFVRLocalHands.u` from the installed game. It stays local and must not be redistributed as part of the source checkpoint. [NATIVE_HANDS.md](docs/NATIVE_HANDS.md) explains geometry, provenance and installation manifests. Use `-NativeHands` at launch to reproduce the accepted hand presentation; otherwise the authored placeholder gloves are used.

## Launch

Follow [PRE_ALPHA_TEST.md](docs/PRE_ALPHA_TEST.md). With SteamVR registered, `.\Check-KFVR.ps1 -Game $kfGame` checks binaries, dependencies and the supported game hashes before launch. It does not certify headset appearance.

If reusing a game installation from a previous checkout, retain its `reports/dev-install.json` and optional `reports/native-hand-install.json`. The staging tool refuses to overwrite unknown packages. With KF closed, deliberately remove only known previously installed KFVR packages if you no longer have those manifests; do not remove stock game files.

## Optional investigation tools

```powershell
py -3.12 tools/ue2_package.py "$kfGame\System" --output reports/packages.json --sources local/sdk
py -3.12 tools/observe_kf.py --pid GAME_PID --game $kfGame --output reports/live-observation.json
py -3.12 tools/audit_weapons.py --pid GAME_PID --game $kfGame
```

The first command writes proprietary extracted scripts into ignored `local/sdk`; do not commit them. Use the actual current standalone process ID for the other commands. Native inspection validates exported accessor layouts, but external snapshots can race game loading/garbage collection. Never use an old report to authorize native writes.

[TESTING.md](TESTING.md) documents bounded render/input and weapon trials. Do not install the diagnostic DLL as a D3D proxy: tooling explicitly loads it for a bounded experiment, disables/restores hooks afterward and leaves the module inert until normal game exit.

## Runtime packaging status

This release publishes source and an annotated checkpoint tag. It has no refreshed runtime ZIP. `tools/package_prealpha.py` preserves the earlier candidate workflow, including stale candidate metadata and a curated file list that needs review before another runtime release. Do not label its old output as this handoff build. The build/launch flow above is the supported path for this checkpoint.
