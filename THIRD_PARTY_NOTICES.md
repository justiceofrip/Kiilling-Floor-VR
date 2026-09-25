# Source provenance

Portable math, interaction policies, frame coordination, D3D11 transport, IPC and OpenXR host code were copied from the local BC2VR work, which retained the MIT-licensed BFVR/2142 lineage. No BC2/Refractor native offsets, game assets, server configuration, voice credentials or launcher destinations were imported.

- Original BFVR: https://github.com/JayBiggsGMG/BFVR-Battlefield-1942-VR-Mod
- BF2142 adaptation: https://github.com/justiceofrip/BF2142VR
- MIT license retained in LICENSE and licenses/BFVR-MIT.txt.
- Khronos OpenXR 1.1.61 headers/loader: Apache 2.0, licenses/OpenXR-Apache-2.0.txt.
- MinHook 1.3.4: retained BSD notices in licenses/MinHook-BSD.txt and upstream dependency LICENSE.txt.

The portable-source manifest records original copied-file hashes and initial destination hashes. Subsequent modifications belong to the KFVR git history. The generic process-bootstrap launcher is adapted from the existing BC2 native launcher; all KF identity checks, script/object audit, camera conversion and gameplay modules are project-specific.

Proprietary KF binaries, extracted scripts and assets are never committed. Private logs/captures/reports and runtime binary inputs are ignored. Numerical matrix/default-value observations inform tests and documentation; no claim of owning Tripwire's code or assets is made.
