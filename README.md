# Kiilling Floor VR

An experimental native OpenXR VR mod for **original Killing Floor 1** on Windows. Published as an unfinished **pre-alpha source checkpoint** so others can fork it, study it, or continue the conversion.

**Start here: [developer handoff](docs/HANDOFF.md) · [build from source](BUILDING.md) · [launch and controls](docs/PRE_ALPHA_TEST.md) · [checkpoint notes](docs/PRE_ALPHA_RELEASE.md).**

Development resumed on October 11 after the October 9 source handoff. The historical `v0.1.0-prealpha-handoff.1` tag remains available; current source includes subsequent fixes. This is a standalone development prototype, with explicitly launched sessions of up to three minutes. It requires your own game and SDK installation. There is no ready-to-install runtime bundle attached to this checkpoint.

**Maintenance status: active development resumed.** Forks and contributions remain welcome. This is still an unfinished pre-alpha.

**October 11 renderer checkpoint:** optional GPU-only eye transfer and a desktop eye mirror improved matched-view desktop throughput from **41.55 to 91.45 stereo pairs/second** at **2500x2676 per eye** (about **2.2x**, one run per mode). Two native eye renders remain; headset pacing is unverified. See [measurements and limits](WORKLOG.md#gpu-renderer-checkpoint-2026-10-11) and [experimental launch instructions](docs/PRE_ALPHA_TEST.md#experimental-gpu-renderer).

## What works

- Two native game-world eye views through OpenXR, head tracking, controller locomotion and snap turns.
- Independent 6DoF weapon aiming, capacitive finger poses, holsters, rifle support grip, an offhand status display and an initial VR trader.
- Optional native KF hand geometry, generated locally from the player's installation. The latest wearer feedback accepted thumb touch, weapon grips and steady weapon presentation while walking.
- Physical pistol reloads: chest ammunition, magnetic magazine insertion, empty-magwell slide operation and a consistent left-hand slide grasp using left grip only.
- Bullpup/SCAR magazine pull-out alongside button ejection, with partial-pull cancellation and retained magazine ammunition.
- Additional implemented prototypes: dual-pistol belt reloads, physical knife swings, syringe/welder interactions and grenade throws.

The October 1 wearer test accepted magnetic insertion, Bullpup pull-out and the pistol slide grasp. Desktop trials also cover 9mm, Deagle, MK23, Bullpup and SCAR reload profiles. This is not exhaustive headset acceptance of every weapon or interaction.

## Known issues and unfinished work

**Pistol hip-fire spread corrected; headset confirmation pending.** Stationary desktop bursts reproduced almost 5 degrees of extra 9mm dispersion. Tracked pistols now use their physical aim directly; the corrected 9mm/Deagle trial recorded zero difference between shot-time aim and firing direction. Sight geometry and pistol grip comfort still need wearer confirmation. See the [measurement and remaining limits](docs/HANDOFF.md#first-gameplay-issue-pistol-shot-alignment).

- Standalone only. Multiplayer and a flatscreen gesture visualizer are not implemented.
- Ordinary launches retain CPU image transfer. An optional D3D9On12 GPU transport and desktop eye mirror now pass private game trials; headset acceptance is pending. Two native eye world renders remain. See [renderer trials](docs/PRE_ALPHA_TEST.md#experimental-gpu-renderer).
- Incomplete weapons, menus, scopes, pickup/drop flows, model surfaces and comfort settings. Some flat-game effects, such as intentionally flickering lights and ceiling rays, need VR presentation work.
- Full-body IK, two-handed pistol support and shotgun/launcher flip reloads are still planned work.
- Native-hand conversion is opt-in. Launching without `-NativeHands` uses authored placeholder gloves.
- The native bridge is bounded; map travel requires another launch. Original game-build identity checks remain enabled.

## Build and try

Clone or fork this repository into a short path without spaces, such as `C:\KFVR`. Follow [BUILDING.md](BUILDING.md) for Visual Studio, Python, the OpenXR loader, script compilation and local native-hand preparation. No sibling Battlefield project checkout is required.

After completing those steps, close any existing KF process and run from the repository root, replacing the example game path:

```powershell
$kfGame = 'F:\SteamLibrary\steamapps\common\KillingFloor'
.\Check-KFVR.ps1 -Game $kfGame
.\Start-KFVRDev.ps1 -Game $kfGame -Unattended -NativeHands -VRProfile -PhysicalReloads
```

Wait until the private match has spawned. With SteamVR running, the headset connected and its dashboard closed:

```powershell
.\Play-KFVR.ps1 -Game $kfGame -Weapons -Seconds 180
```

The development fixture uses god mode, holds zed spawning and marks progression as using cheats. [The test guide](docs/PRE_ALPHA_TEST.md) explains controls, extra weapon fixtures and cleanup.

## Continue development

[HANDOFF.md](docs/HANDOFF.md) maps the code, accepted behavior, known bugs and next tasks. [ARCHITECTURE.md](ARCHITECTURE.md) records engine boundaries; [TESTING.md](TESTING.md) explains reproducible checks; [WORKLOG.md](WORKLOG.md) preserves the development history, including failed experiments. Historical references to publication being on hold describe earlier checkpoints.

Existing plans cover [body IK](docs/BODY_IK_PLAN.md), [multiplayer visualization](docs/NETWORK_VISUALIZER_PLAN.md), [Demolitions and break-action reloads](docs/DEMOLITIONS_RELOAD_PLAN.md), and [weapon coverage](docs/WEAPONS.md). These documents distinguish implemented systems from proposals.

Forks and contributions are welcome; see [CONTRIBUTING.md](CONTRIBUTING.md). Private reports, screenshots, extracted game scripts, native game geometry and developer runtime inputs are excluded. Some historical audit helpers lived under ignored `reports/` and are not distributed; they are not required to build the runtime.

## License and credits

Authored mod code is available under the retained [MIT license](LICENSE), with BFVR/BF2142VR/BC2VR lineage and third-party notices in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md). OpenXR and MinHook retain their respective licenses. Killing Floor belongs to its owners; this repository does not distribute the game or grant rights to its assets. The local native-hand converter requires assets from your own installation.
