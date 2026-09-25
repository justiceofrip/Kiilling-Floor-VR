# KFVR development

Read README.md, ARCHITECTURE.md, WORKLOG.md and TESTING.md before modifying this project.

- The target is original Killing Floor, not a remake. Source root is G:\KFVR.
- Preserve the game installation and the existing BF2142VR/BC2VR projects.
- Native game experiments are limited to standalone/private development sessions. Revalidate the exact process, local viewport and world at each run.
- No borrowed native offsets, invented UE4 APIs, replayed simulation or claims that a single frame capture is stereo VR.
- Treat render-side animation, culling, state caches and temporal resources as unresolved until measured.
- Keep game scripts/assets in ignored local storage and retain copied-code licenses/provenance.
- Build/test x86 and x64 after relevant changes. Runtime/headset acceptance is separate from compilation and synthetic tests.
- Native probe modules must remain inert/resident after hooks are disabled unless callback-unload safety has been proven.
- Document concrete failures and remaining integration gaps. The owner authorized local match restarts after death and autonomous development; do not repeatedly ask routine permission.
