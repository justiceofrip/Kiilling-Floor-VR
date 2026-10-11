# Contributing

Forks and contributions to this unfinished source checkpoint are welcome. Start with [the handoff](docs/HANDOFF.md), [build instructions](BUILDING.md) and [architecture](ARCHITECTURE.md). Development resumed on October 11, 2026. Review turnaround and a completion date are not guaranteed.

Keep changes focused and describe the concrete behavior, affected weapons and remaining limits. For gameplay changes, distinguish commandlet tests, desktop trials with synthetic controllers and actual wearer feedback. Include only the checks relevant to the change; bone-math success alone does not prove a comfortable headset interaction.

Use standalone/private game sessions. Preserve stock ammunition, damage, cadence and perk behavior unless the change explicitly proposes otherwise. Keep accepted raw input and visual presentation separate.

Do not commit original game packages, extracted SDK scripts, native game meshes/textures, private INIs, tokens or raw memory/capture logs. Native-hand geometry must be regenerated from each player's installation. Retain the MIT and third-party license notices.

For a bug report, include the source commit, weapon, action sequence, headset/controller/runtime and whether it also reproduces in a desktop diagnostic. Review logs before sharing local paths or identifiers. Further pistol shot-alignment reports should include the new shot-time evidence, described in [HANDOFF.md](docs/HANDOFF.md#first-gameplay-issue-pistol-shot-alignment).
