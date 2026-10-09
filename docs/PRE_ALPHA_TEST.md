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

Pistol shot alignment is an open issue. Native hand orientation, weapon model surfaces, lighting/rays, complete menus, item flows and performance need more work. Full-body IK and multiplayer are not implemented. [HANDOFF.md](HANDOFF.md) distinguishes working features from planned systems.

The launcher stages generated `System/KFVRGameplay.u` and, when requested, `System/KFVRLocalHands.u`. It records them in `reports/dev-install.json` and `reports/native-hand-install.json`. Keep those manifests when reusing an installation. An unrecorded/changed destination package is deliberately rejected; do not overwrite it blindly. To undo a known KFVR installation, close KF and remove only the corresponding recorded generated packages. Original game packages are not replaced, and a normal game launch does not auto-load VR.

Closing the game normally removes the inert diagnostic module left after hook shutdown. Do not install the diagnostic DLL as a D3D proxy. Reports under `reports/native-headset-*` and `reports/native-trace-*` include local paths and game observations; review them before sharing. Never run automated synthetic controller trials over a wearer session.
