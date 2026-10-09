# Pre-alpha source handoff — October 9, 2026

Tag: `v0.1.0-prealpha-handoff.1`.

This release publishes the current source and development history for forks and continuation. Development by the original owner is currently on hold. Gameplay is the `88f99da` checkpoint, with wearer feedback in `2b413d2`; handoff changes update documentation. This is an unfinished standalone prototype for the audited Windows Killing Floor 1065 build.

## Included

- Native stereo world rendering and head tracking through OpenXR.
- Controller movement/snap turns, tracked weapons, capacitive hands, holsters, rifle support and initial status/trader interfaces.
- Chest ammunition, magnetic magazine insertion, empty-magwell pistol slide cycling and a fixed left-hand overhand slide grip.
- Bullpup/SCAR grab-and-pull magazine removal alongside button ejection.
- Additional dual-belt reload, melee, healing, welding and grenade prototypes.
- Local native-hand converter, build/test tools and plans for broader weapon mechanics, body IK and multiplayer visualization.

The October 1 wearer session accepted the new reload interactions. The final desktop reload checks cover 9mm, Deagle, MK23, Bullpup and SCAR; native builds passed 25 x86 and 24 x64 tests. These are recorded checkpoint results, not a new October 9 headset test.

## Important open issues

- **Pistol shots reportedly miss the visible aim; cause unresolved.** Bullpup aim was reported correct. Pistol comfort orientation needs later tuning.
- Standalone only, sessions bounded to three minutes, incomplete game/weapon/menu coverage.
- CPU frame-transfer bottleneck and remaining rendering/model polish.
- Two-handed pistol support, full-body IK, launcher/shotgun flip reloads and multiplayer visualization are not implemented.

## Getting started

Read [HANDOFF.md](HANDOFF.md), then [BUILDING.md](../BUILDING.md) and [PRE_ALPHA_TEST.md](PRE_ALPHA_TEST.md). GitHub's source archives contain the repository only. No prebuilt runtime ZIP, original game files, SDK, extracted native assets or private logs are attached. Build the native/script components and generate optional native hands from your own installation.

The older private `0.1.0-prealpha.1-rc1` runtime archive and historical packaging manifest do not represent this checkpoint. `tools/package_prealpha.py` is retained as historical tooling and needs a refreshed file list/metadata before another runtime release.

Authored source retains its [MIT license](../LICENSE); see [third-party notices](../THIRD_PARTY_NOTICES.md). Forks are welcome, with no promised completion date or support schedule.
