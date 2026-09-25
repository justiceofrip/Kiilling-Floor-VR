# Feasibility assessment

Provisional engineering judgement after the installed-game audit, not a measured probability:

| Target | Rating | Basis / remaining risk |
| --- | ---: | --- |
| Native stereo + head tracking + controller aiming, a small weapon subset | 7/10 | Exported scene/camera/render APIs, exact scripts, native trace and GPU transport are available. Render-side animation/culling and D3D9-to-XR integration remain substantial work. |
| Manual reloads and balanced physical melee | 7/10 | Game fire/reload/perk logic is script-visible; core state machines now conserve ammo and bound melee. Real interaction zones, server integration and every weapon mechanism still need work. |
| VR + flat on a KFVR-enabled server | 7/10 | Existing UnrealScript replication/package mechanisms fit capability-based players. Requires a custom gameplay/client package and actual dedicated-server/flat-client tests. |
| Full-body local and remote IK on all original player models | 5/10 | Native bone APIs exist, but skeleton/space/animation/LOD behavior and script performance are not yet validated. |
| Entire brief across every weapon, map, mod and server with unchanged balance | 4/10 | Broad compatibility, unusual weapons, legacy networking, progression eligibility and 32-bit performance are the long tail. |
| Full custom VR posture on clients receiving no new code/package at all | 1/10 | Vanilla code has no interpreter for the added tracking data or custom IK. A downloaded script package keeps the executable unchanged but is still the shared KFVR client package. |

The original game is the right base. The architectural change from the prompt is to treat the x64 OpenXR presenter and legacy graphics transport as explicit components. Script-visible bone/camera functions are useful, but do not eliminate native stereo work. The current result is a tested development foundation, not a playable total conversion or a release estimate.
