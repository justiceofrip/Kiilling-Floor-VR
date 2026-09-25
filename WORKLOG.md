# Worklog

## 2026-09-25: installed-game audit and working foundation

User asked for original KF1 native VR, using the 2142/BC2 lessons, and authorized unattended local development with the game open. Later confirmed windowed display is acceptable and explicitly authorized starting another local match after death. No public-server testing is authorized.

Created separate G:\KFVR source/build workspace. The original install is on F:, while the task's starting Unity directory is unrelated. Windows sandbox process/image tools initially failed with a named-security-info error; approved elevated shell calls were used for the actual work. This is a workstation tooling issue, not a KF mod requirement.

Read the 2142 v35b handoff and BC2 transfer/architecture/handoff. Retained portable math/comfort/action/rig-restoration/frame/presenter modules and licenses, with original SHA-256 records. Changed IPC names and protocol identities for KFVR. No previous project files were changed.

A standard-library package reader found embedded UE2 TextBuffers: 2,381 class sources across eleven installed packages, version 128/licensee 29, zero source parse errors. Normalized CRLF export handling. TextBuffers omit compiled defaultproperties, so a separate read-only live UClass default-buffer audit was added. Weapon ledger: 96 subclasses, including bases/legacy variants; 83 live default buffers verified. UCC.exe and official native SDK headers remain absent.

Confirmed x86 game/modules, D3D9RenderDevice active. D3D8 in the executable's imports was not treated as renderer evidence. Derived UObject index/name/class/outer and class-super/size access from exported native instruction sequences. The observer follows the local PlayerController.Player viewport link rather than mistaking retained old-map actors for the active player. It observed KF-Manor NM_Standalone, local pawn, machete and 100 health. A later process was refreshed and revalidated on KF-Biohazard; the previous PID was rejected before injection.

Implemented magazine/chamber/tube accounting, atomic mechanically timed operations, ownership epochs/sequences, lifecycle cancellation, trusted snapshot correction, fire cooldown, physical melee eligibility, compact pose codec and receive-time validation. Added meaningful tests including 100,000 randomized ammo actions and 10,000 waggle samples. Fixed near-unit quaternion quantization overflow by normalizing before encoding.

Native trace: compiled a separate x86 hook ABI regression; resolved Tick/Draw/FPlayerSceneNode::Render/D3D9 Present by decorated exports. First signature comparison correctly declined to proceed but the tool had not accounted for DLL relocation; corrected expected bytes with the PE relocation table. Subsequent 180-frame traces recorded one render/present per Draw on one thread, no engine Tick inside Draw, and verified restoration of original entry bytes. No camera writes or rendering replay occurred. Inert DLLs/trampolines remain resident only until those game processes exit.

Native capture initially declined a private-memory COM vtable. Read-only inspection showed Windows apphelp supplied the copied table/QueryInterface while the called graphics methods belong to d3d9.dll. Revised validation to check method ownership. Next capture declined the 8x multisampled backbuffer; added a private non-MSAA resolve before readback. Capture succeeded at 1024x768 X8R8G8B8, confirmed non-Ex D3D9, and was visually inspected. No render-target bindings were changed.

Captured six native scene matrices. Added UE2 camera adapter and a real-matrix regression. The first adapter failed the test because native inverse-rotation quantizes the negated angle before its lookup; inspected that constructor and matched it instead of weakening the test. Camera and asymmetric-eye tests now pass.

Real GPU verification: eight x86-to-x64 D3D11 pairs with all pixels checked; four cross-process synthetic pairs with exact tracking identity, head translation, pair cancellation/recreation, peer exit and abandoned mutex. Added standard OpenXR controller profiles, grip/aim poses, analog/button actions and bounded haptics. No HMD acceptance or KF controller binding is claimed.

Latest native capture/trace evidence: reports/native-trace-20260925-112424-593706. Earlier successful trace: reports/native-trace-20260925-100652-186480. Intermediate capture failures are retained. Original game files/settings/assets were not modified by this project.

## Current remaining work

Native stereo is not enabled. Need to audit render-side animation, culling/LOD/occlusion, temporal and first-person weapon state; prove separate eye targets/restoration; bridge non-Ex D3D9 eye textures; wire controller samples into KF; compile the gameplay package with a compatible SDK compiler; then validate headset and multiplayer behavior. Full body IK, all reload families, scopes, VR UI and a reversible player installer remain future integration work.

Controller regression increment: x86 16/16 and x64 15/15 suites pass. A fake OpenXR API exercises all five profiles, matching predicted times/reference generations, analog normalization, digital fallback, invalid pose/input neutralization, focus loss, runtime errors, haptic bounds, unsupported profiles and partial-start cleanup. Fixed an error being hidden during loss of focus and kept invalid poses neutral. Rebuilt GPU/IPC probes pass again; synthetic scene verifies distinct eyes and translated head. This remains mock/GPU validation, not physical-controller acceptance.

Render audit increment: added exact installed Engine/Core/D3D9Drv SHA gates and bounded CFG listings. Added native measurements for FLevelSceneNode, actor/skeletal animation, GetFrame and script ProcessEvent. Expanded x86 ABI test validates return values, reference/pointer arguments and restoration. Two further 180-frame traces passed nine-hook restoration: KF-Offices lobby and the solo match after clicking Ready. Both recorded two level renders per Draw; GetFrame ran 720/2340 times, and the instrumented animation-advance functions zero times inside Draw. Script callbacks include HUD/UI; source confirms weapon RenderOverlays writes location/rotation. This narrows the render boundary, but does not authorize unqualified replay. Closed an observer handle leak on initialization failure and bounded launcher config validation before loading the probe.

Controller transport increment: channel v2 contains independent 208-byte controller and 64-byte haptic mailboxes. No C++ bools/pointers cross processes. A 100 ms deadline releases stale holds; invalid samples publish a neutral frame; focus/recenter cancels previous feedback. OpenXR IPC mode now uses this input consumer. A real 64-bit host and 32-bit child verified all controller fields, bounded one-shot haptics, host-stall expiry, focus loss with no eye image, recenter and peer close. The image IPC still passes. Latest CTest: x86 17/17, x64 16/16. Native game input binding remains pending.

Version-control checkpoints now exist. Initial staging exposed an overly broad runtime/ ignore that hid runtime source; anchored generated-directory ignores to the repository root before committing. This machine had no git identity; the repository alone uses Codex Development <codex@localhost>, clearly identifying generated checkpoints. No global identity was changed.
