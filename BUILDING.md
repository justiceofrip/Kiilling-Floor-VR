# Building

Requires Visual Studio C++ x86/x64 tools, Windows SDK, CMake and Ninja. Build.ps1 discovers the installed VS instance and launches its developer environment. Both builds run deterministic CTest suites; failures return a nonzero status.

```powershell
& G:\KFVR\Build.ps1 -Architecture x86
& G:\KFVR\Build.ps1 -Architecture x64
```

The x86 build produces core tests, GPU/IPC probes, KFVRNativeProbe.dll and KFVRNativeTrace.exe. The x64 build additionally produces KFVRXrHost.exe and KFVRSceneProbe.exe. OpenXR headers and third-party licenses are in source. The local loader DLL is an ignored build input at runtime/openxr/win64/openxr_loader.dll; its hash/origin are recorded in the portable-source manifest. There is no dependency on a live BF2142/BC2 checkout after the recorded imports.

Python audit tools require Python 3.12. Package extraction uses only the standard library. Native inspection requires pefile==2024.8.26; disassembly uses capstone==5.0.9. Local copies are under ignored local/pydeps. On another machine install those versions there with pip or into a virtual environment.

```powershell
py -3.12 G:\KFVR\tools\ue2_package.py 'F:\SteamLibrary\steamapps\common\KillingFloor\System' --output G:\KFVR\reports\packages.json --sources G:\KFVR\local\sdk
py -3.12 G:\KFVR\tools\observe_kf.py --pid GAME_PID --game 'F:\SteamLibrary\steamapps\common\KillingFloor' --output G:\KFVR\reports\live-observation.json
py -3.12 G:\KFVR\tools\audit_weapons.py --pid GAME_PID --game 'F:\SteamLibrary\steamapps\common\KillingFloor'
```

Native inspection derives exported accessor layouts and rejects unsupported ones. Its external memory snapshot is not synchronized with garbage collection, so retry after a loading transition. Never use an old report as authority for native writes.

The official Killing Floor SDK (Steam app 1260) is now installed. It added UCC.exe and script sources without changing any of the 82 pre-existing System binaries/packages/configuration files checked before installation. Native C++ SDK headers are still absent.

Build the authored UnrealScript package and run its engine tests in an isolated compiler copy:

```powershell
py -3.12 G:\KFVR\tools\build_scripts.py --game 'F:\SteamLibrary\steamapps\common\KillingFloor'
```

The driver copies SDK runtime inputs into ignored local/compiler, builds only authored KFVR source, executes the commandlet tests and checks its action-stream digests against the built x64 C++ reference. It produces build/unrealscript/KFVRGameplay.u. It does not install the package into the running game. The package contains the standalone tracked weapon/tool, reload, status and trader adapters plus their commandlet tests; multiplayer RPC integration is not enabled. A build failure leaves its log in reports and returns nonzero.

Do not install the diagnostic DLL as a D3D proxy. Native trace tooling copies its exact payload into a private report directory and loads it explicitly for one bounded experiment; it has no startup integration. Closing that game process removes the inert resident probe modules.

Bounded native camera diagnostics (standalone game only): `py -3.12 tools/trace_kf.py --pid <current-KF-PID> --game <game-folder> --target-trial`, `--camera-trial`, `--stereo-trial`, `--asymmetric-trial`, or `--pose-trial`. Each later option includes prerequisite checks; only the final three render additional views, once. Synthetic pose/FOV/scale are explicitly diagnostic. Nothing is auto-installed into the game.

Hardware diagnostics: `build/x64/KFVRXrHost.exe --test-scene --seconds 45` displays the synthetic room. `--input-test --seconds 60` adds per-hand grip/aim sample counts, motion extents, trigger/grip press/release counts, thumbstick ranges and short trigger-confirmation haptics (20% amplitude, 50 ms, at least 300 ms apart). Input-test mode does not drive KF or change bindings. Raw host reports cannot certify what the wearer saw/felt; record their confirmation separately.

## Pre-alpha runtime archive

After both architecture checks and the UnrealScript build pass, commit the exact candidate and run:

```powershell
py -3.12 -m pip download --only-binary=:all: --no-deps pefile==2024.8.26 --dest local/release-deps
py -3.12 tools/package_prealpha.py --version 0.1.0-prealpha.1-rc1
```

The packager verifies the pinned wheel hash, includes its license, requires a clean checkout, uses an explicit runtime-file list, and checks every archived member. Outputs go to ignored `dist/`. Source publication uses the Git-tracked checkout, never the ignored local SDK or reports. The candidate stays separate from a public pre-release until wearer feedback has been recorded.
