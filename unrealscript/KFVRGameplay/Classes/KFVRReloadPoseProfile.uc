// Four authored animation ranges. Fractions use Actor.SetAnimFrame's normalized
// convention (UnitFlag=0). This contains presentation data, never ammo authority.
class KFVRReloadPoseProfile extends Object;

var name Sequences[4];
var float BeginFrames[4], EndFrames[4];
var byte Configured[4];

function bool SetStage(byte Stage, name Sequence, float BeginFrame, float EndFrame)
{
    if (Stage >= 4 || Sequence == '' || !(BeginFrame >= 0.0 && BeginFrame <= 1.0)
        || !(EndFrame >= 0.0 && EndFrame <= 1.0))
        return false;
    Sequences[Stage] = Sequence;
    BeginFrames[Stage] = BeginFrame;
    EndFrames[Stage] = EndFrame;
    Configured[Stage] = 1;
    return true;
}

// 0 release, 1 held magazine, 2 insertion, 3 slide/charging handle.
// Reversed ranges support returning a slide along its physical stroke.
function bool Resolve(byte Stage, float Progress, out name Sequence, out float Frame)
{
    if (Stage >= 4 || Configured[Stage] == 0 || !(Progress >= 0.0 && Progress <= 1.0))
        return false;
    Sequence = Sequences[Stage];
    Frame = BeginFrames[Stage] + (EndFrames[Stage] - BeginFrames[Stage]) * Progress;
    return true;
}
