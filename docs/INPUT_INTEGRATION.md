# Native input integration audit

The headset and controller hardware work, but native gameplay control writes are not enabled yet.

The installed Engine.dll exports UViewport::ReadInput(float), which calls UInput::ReadInput(float,FOutputDevice&). Its audited code obtains the input object from viewport+0x78; that object's +0xea4 points back to the viewport. The viewport's +0x34 controller and the script input object's Outer were verified against the one local KFPlayerController. These are this installed build's observed relationships, not portable UE2 offsets.

Live reflected controller axes are ten consecutive FloatProperty values at offsets 1348 through 1384: aBaseX, aBaseY, aBaseZ, aMouseX, aMouseY, aForward, aTurn, aStrafe, aUp, aLookUp. PlayerInput is an ObjectProperty at 2160. Fire/alternate fire belong to the inherited Controller class (ByteProperty at 889/890), not PlayerController. Descriptor metadata is evidence; an external snapshot does not authorize unsynchronized writes.

Read-only native trial native-trace-20260925-190036-910911 observed 240 input calls on thread 53768: 120 positive-delta passes and 120 sentinel passes with DeltaTime=-1. 117 complete PlayerTick observations followed positive-delta reads; no sentinel pass was consumed as a PlayerTick sample. All axis values were finite, all game input values were left unchanged, and the extra ReadInput hook plus all original trace hooks restored. Early samples precede completion of the other instrumentation hooks and are not counted as full tick observations.

The native ReadInput function scales FloatProperty input values by 20/DeltaTime for a normal pass and zero for the sentinel. Default forward/strafe aliases specify Speed 300. Thus controllers must be inserted at a verified positive-delta boundary, not both passes. Actual nonzero input scaling and motion still need acceptance; do not infer physical movement success from idle zero samples.

KFPlayerController.PlayerTick calls the inherited PlayerTick. Engine.PlayerController then calls PlayerInput.PlayerInput, Pawn.RawInput and PlayerMove. KFPlayerInput remaps aBaseY into aForward, aBaseX/mouse into aTurn, applies weapon/FOV sensitivity, and clears raw base axes. KF UpdateRotation uses 32*DeltaTime*aTurn and applies recoil. Movement uses pawn forward/right axes and the existing native movement/replication path.

Next implementation must validate local pawn ownership/liveness, consume focused non-stale controller samples, handle neutral/recenter/respawn transitions, and preserve normal input outside VR. HMD view remains independent of weapon aim. Controller weapon transforms, server-authoritative firing and full physical manipulation require separate adapters.

No custom script input class was installed: KFPlayerController.InitInputSystem explicitly resets InputClass to KFPlayerInput, so a naive class-setting approach would not persist. The SDK source and binary audit stay in ignored local/report storage.
