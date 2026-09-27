# Planned manual reload interactions

The user proposed four animation-guided interaction stages: release a magazine, hold/manage it, insert it, and grab/hold the charging handle or pistol slide. Reference animation poses guide hands and parts; tracked hand travel advances mechanical state. Magazine, chamber and reserve accounting remains authoritative. This interaction layer is planned, not implemented by the current ammunition-policy prototype.

Each weapon needs magwell/magazine transforms, grasp poses, insertion direction and seating depth, action grip point and travel limits, and extraction/feed thresholds. Tactical reloads preserve an existing chambered round. Interrupted actions, retained/discarded partial magazines and partially operated actions need explicit state instead of completing a timed reload automatically.

## User-selected dual-wield direction

Two upward-facing replacement magazines present at waist/belt docks. With both hands holding pistols, the player deliberately ejects the magazines while pointing down, brings each empty magwell onto a belt magazine, pushes to seat it and lifts away. Both docks operate independently or simultaneously. Add alignment assistance and a seating click/haptic response. The belt follows an estimated waist/body heading, rather than pitching with every head movement.

A dual-wield assist can release a locked-back slide after a new magazine is seated, solving the occupied-hands problem. Keep each pistol's chamber, inserted magazine and ammunition identity independent. Supply replacement magazines from the player's ammunition inventory, and preserve the contents of partial magazines. The user explicitly accepts that this belt action will be quick because it makes dual wielding practical.

Final controller weapon aiming follows tracked gun pose and physical sight alignment. Right-stick snap turning changes body heading; its native input adapter does not define controller aiming sensitivity.
