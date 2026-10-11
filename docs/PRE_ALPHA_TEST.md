# Launching the source checkpoint

Complete [BUILDING.md](../BUILDING.md) first. GitHub's source archive is not an installed runtime build. This checkpoint targets original Killing Floor 1065 on Windows, with SteamVR/OpenXR and your own game/SDK installation.

## Start the accepted development setup

Use a short checkout path without spaces. Close any existing KF process, keep Steam running, and run from the repository root:

```powershell
$kfGame = 'F:\SteamLibrary\steamapps\common\KillingFloor'
.\Check-KFVR.ps1 -Game $kfGame
.\Start-KFVRDev.ps1 -Game $kfGame -Unattended -NativeHands -VRProfile -PhysicalReloads
```

`-NativeHands` requires the locally generated hand package from the build guide. Omit it to use authored placeholder gloves, which differ from the accepted native-hand presentation. `-PhysicalReloads` enables the new physical reloads by default and requires `-Unattended`. The fixture automatically readies, grants its test inventory, enables god mode, holds zed spawning and marks progression as using cheats.

`-VRProfile` creates separate INIs under ignored `local/play-profile/`; its legacy game parser requires a checkout path without spaces. It requests desktop VSync off and a 144 FPS game limit. It does not establish headset-rate frame delivery.

Wait until the player has spawned in the world. Connect the headset, start SteamVR, close its dashboard and face forward with centered sticks:

```powershell
.\Play-KFVR.ps1 -Game $kfGame -Weapons -Scale 100 -Seconds 180
```

The session ends after three minutes. Run the command again for another pass; map travel requires a new session. `-Scale 75` reduces the eye dimensions. The launcher reports the actual size and fits runtime recommendations to the bridge's current memory budget without changing SteamVR settings.

## Controls and reloads

| Input | Action |
| --- | --- |
| Left stick / right stick | Walk / snap turn |
| Right trigger | Fire or use the held tool |
| Left X | Cycle available weapons |
| Right grip at a holster | Draw; release near its slot to stow |
| Right A | Use nearby door / open an available trader |
| Right B, physical single-gun reload | Eject magazine; catching is optional |
| Left grip near chest supply | Take replacement ammunition |
| Held magazine near magwell | Magnetic capture, then push along the rail to seat |
| Left grip at pistol slide | Grab, pull and return/release; no left trigger required |
| Left grip at Bullpup/SCAR magazine | Pull to remove; a partial canceled pull returns it to the seat |
| Left grip near rifle foregrip | Support grip when the hand is free |
| Free left palm turned toward face | Reveal status display |

The slide can be operated with no magazine inserted. Reloads preserve a tactical chamber and magazine identity. Without `-PhysicalReloads`, supported single pistols can toggle physical mode with left Y; right B otherwise requests stock reload. Physical reload mode in the fixed fixture stays enabled.

Holsters: right hip single/dual 9mm, left hip Deagle, right shoulder Bullpup, left shoulder knife, left chest syringe, right chest welder, rear-right waist grenade. Syringe: touch chest and squeeze right trigger. Welder: right trigger at a closed door; hold left trigger before starting to unweld. Grenade: hold grip, press trigger to arm, release grip to throw. Knife: draw clear of the shoulder and swing deliberately.

## Additional fixtures

Restart the game with `-SCARTest` or `-MK23Test` added to the development launch to substitute the respective rifle/pistol. These require `-Unattended`. The standard fixture does not spawn every supported weapon at once.

For dual pistols use `-DualTest`. Each trigger fires its own pistol. Left Y enables belt reload mode; point down and press right B, bring each well to the corresponding belt magazine, seat and lift away. Empty chambers use belt slide assistance. Dual presentation/hip firing has an unresolved wearer report; it is not included in the latest single-gun reload acceptance.

## Limits and cleanup

The October 11 pistol hip-fire spread correction passes desktop firing checks; physical sight/impact alignment still needs a headset retest. Native hand orientation, weapon model surfaces, lighting/rays, complete menus, item flows and performance need more work. Full-body IK and multiplayer are not implemented. [HANDOFF.md](HANDOFF.md) distinguishes working features from planned systems.

The launcher stages generated `System/KFVRGameplay.u` and, when requested, `System/KFVRLocalHands.u`. It records them in `reports/dev-install.json` and `reports/native-hand-install.json`. Keep those manifests when reusing an installation. An unrecorded/changed destination package is deliberately rejected; do not overwrite it blindly. To undo a known KFVR installation, close KF and remove only the corresponding recorded generated packages. Original game packages are not replaced, and a normal game launch does not auto-load VR.

Closing the game normally removes the inert diagnostic module left after hook shutdown. Do not install the diagnostic DLL as a D3D proxy. Reports under `reports/native-headset-*` and `reports/native-trace-*` include local paths and game observations; review them before sharing. Never run automated synthetic controller trials over a wearer session.

## Experimental GPU renderer

Build both architectures from current source. Keep SteamVR ready before starting this short trial, close an existing KF process normally, then launch:

```powershell
.\Start-KFVRDev.ps1 -Game $kfGame -Unattended -NativeHands -VRProfile -PhysicalReloads -GpuRendererTrial
```

Once the private match has spawned, run:

```powershell
.\Play-KFVR.ps1 -Game $kfGame -Weapons -Mirror -Seconds 120
```

`-GpuRendererTrial` selects the Windows non-Ex D3D9On12 factory before KF creates its render device. The native bridge confirms the actual device and chooses GPU-only image transfer automatically. There is no silent CPU fallback after an On12 transfer error. The unchanged ordinary launch remains available for comparison and compatibility. This path has desktop game evidence on the development machine; it has not received headset acceptance or broad driver coverage.

The GPU launcher requests normal game closure after 180 seconds from startup (`-GpuTrialSeconds 15..180`). Start the headset session promptly; its duration must fit inside the remaining game lifetime. Startup receipts are under ignored `reports/on12-launch-*`. Original game binaries and runtime registration are unchanged; the existing development-package staging still applies. Normal process exit removes the resident startup module.

`-Mirror` can also be used with the ordinary CPU launch. After a successful mirror preflight, fresh stereo frames replace the extra desktop world draw with a cropped left-eye mirror. Both native eye world renders remain. When no complete focused pair is available, stock desktop rendering continues. The mirror replaces the desktop HUD image; VR world-space status/trader displays are unaffected by that composition policy. Scene correctness and headset pacing still need broader testing.
