# Kiilling Floor VR - pre-alpha test

This is a Windows 64-bit, standalone test build for original Killing Floor 1065. It renders two real native world views through OpenXR. It requires your own installed game; no original game binaries, maps or textures are included.

## Start

1. Install Python 3.12 **64-bit** with the Windows `py` launcher. The ZIP includes the small pinned Python dependency and the OpenXR loader. Native C++ runtimes are statically linked.
2. Extract the complete ZIP to a short writable path, for example `C:\KFVR`. Keep Steam running. Connect the headset and start SteamVR with SteamVR selected as the OpenXR runtime.
3. Open PowerShell in the extracted folder. Replace the example game path with your installed KillingFloor directory:

```powershell
$kfGame = 'F:\SteamLibrary\steamapps\common\KillingFloor'
.\Check-KFVR.ps1 -Game $kfGame
.\Start-KFVRDev.ps1 -Game $kfGame -Unattended
```

Close any previous KF process before starting this match. The test automatically spawns you, grants the supported test firearms, enables god mode and holds zed spawning. It marks the session as using cheats. Wait until you are in the world.

4. Put on the headset, close the SteamVR dashboard, face forward and center both sticks. Start:

```powershell
.\Play-KFVR.ps1 -Game $kfGame -Weapons -Scale 100 -Seconds 180
```

The session intentionally stops after three minutes. Run the same command again for another pass. A map change requires a new session. `-Scale 75` reduces each eye dimension if fresh-frame motion is too uneven; the default requests 100% of the runtime's recommended dimensions. The launcher prints the actual dimensions and automatically chooses the largest whole percentage that fits the current 256 MiB bridge budget. It never changes SteamVR settings.

If Windows blocks local PowerShell scripts, launch the command in a new process with `powershell -NoProfile -ExecutionPolicy Bypass -File .\Play-KFVR.ps1 ...` and the same arguments. This does not change the machine's saved execution policy.

## First pass: three minutes

| Time | Try | Report |
| --- | --- | --- |
| 0:00-0:30 | Turn and lean; look at nearby wall detail | World stability, flicker, clarity, scale |
| 0:30-1:00 | Move with left stick, snap with right stick | Direction and turn comfort |
| 1:00-1:45 | Aim the pistol independently of your head; shoot; press right B | Gun alignment, haptics, reload |
| 1:45-2:30 | Open/close hands, touch/release trigger and thumb controls; draw at hip/shoulder slots | Finger response and reachable holsters |
| 2:30-3:00 | Raise the free left hand; try the Bullpup support grip | HUD readability and two-hand aiming |

Quick controls: right trigger fires, right B reloads, left X cycles available weapons, right A uses a nearby door or opens a currently available trader. Squeeze right grip at a holster to draw; release at the slot to stow. Holsters follow body heading. Right hip holds single/dual 9mm, left hip Deagle, right shoulder Bullpup, left shoulder knife, left chest syringe, right chest welder, rear-right waist grenades.

## Optional second pass

With single 9mm or Deagle equipped, left Y toggles physical reload. Right B ejects the magazine; left grip handles/catches a magazine or takes one from the left belt supply, then seats it in the gun. Grip plus trigger at the slide, pull back and return to chamber an empty gun. Tactical chambered ammunition is retained. These gestures need wearer calibration.

For dual 9mm, restart the private match with `-Unattended -DualTest`. Each trigger fires its own pistol. Left Y enables physical mode; aim the pistols down and eject, continuously bring their magazine wells onto the two belt magazines, then lift away. Empty guns use intentional slide assistance. This build does not require dropping one pistol to reload the other.

Syringe: draw from left chest, touch the needle to the chest and squeeze right trigger. Welder: right trigger at a closed door; hold left trigger before starting to unweld. Grenade: draw from rear-right waist, keep grip held, press trigger to arm, release grip to throw. The fuse begins when thrown; there is no cooking or physical pin pull yet. Knife uses deliberate swings after leaving the shoulder slot.

## Known limits

- Standalone only; no multiplayer support or complete flat-player compatibility.
- Three-minute sessions, development launch flow, and incomplete VR menus/pickup/drop coverage.
- CPU texture transfer still limits fresh frame delivery. Full resolution is not a claim of headset-rate fresh rendering. Current native receiver measurements are roughly 45-50 pairs/sec at 2064 x 2208 per eye; headset presentation is a separate measurement.
- Supported weapons are 9mm, dual 9mm, Deagle, Bullpup, knife, syringe, welder and stock frag grenades. Bullpup reload is timed. Full weapon/perk/server coverage is unfinished.
- Legacy hands/sleeves are provisional; experimental arm IK is off. World scale, palm/needle alignment and holster ergonomics need wearer feedback.
- Trader is an initial supported-firearm panel. It requires being at an actually open shop. Full catalog and general inventory/drop continuity are unfinished.

Only `System/KFVRGameplay.u` is staged into the game. Normal launches do not auto-load VR. Close KF to remove the inactive native probe modules. To remove the staged mod, close KF and remove that KFVRGameplay.u file; original game files are not replaced. Keep the extracted folder's `reports/dev-install.json` with the package when reusing the same installation.

The optional `Start-KFVRDev.ps1 -VRProfile` uses separate INIs outside the game and launches the executable directly to avoid Steam's legacy INI quoting issue. It requires a project path without spaces. It is not the default or a promised performance fix.

For feedback, describe which pass/control failed, headset and connection method, and whether clarity, aim alignment or motion changed. Logs are in `reports/native-headset-*` and `reports/native-trace-*`; review them before sharing because they contain local paths and game observations.
