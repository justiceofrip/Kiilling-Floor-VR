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

The installed game currently lacks UCC.exe and official native SDK headers. Extracted TextBuffers do not replace the compiler or native headers. Script compilation and the packaged gameplay layer remain blocked on a compatible compiler. No speculative `.u` build or package is advertised as working.

Do not install the diagnostic DLL as a D3D proxy. Native trace tooling copies its exact payload into a private report directory and loads it explicitly for one bounded experiment; it has no startup integration. Closing that game process removes the inert resident probe modules.
