class KFVRReloadPoseTests extends Commandlet;
var int Checks, Failures;
function Check(bool Condition, string Message)
{
    Checks++;
    if (!Condition) { Failures++; Log("KFVR_POSE_TEST_FAIL:" @ Message); }
}
event int Main(string Params)
{
    local KFVRReloadPoseProfile P;
    local name Sequence;
    local float Frame;
    local int I;
    P = new class'KFVRReloadPoseProfile';
    Check(!P.Resolve(0, 0.5, Sequence, Frame), "Unconfigured profiles stay inactive");
    Check(!P.SetStage(4, 'Reload', 0.0, 1.0), "Reject extra stage");
    Check(!P.SetStage(0, '', 0.0, 1.0), "Require sequence");
    Check(!P.SetStage(0, 'Reload', -0.1, 1.0), "Reject negative frame");
    Check(!P.SetStage(0, 'Reload', 0.0, 1.1), "Reject past-end frame");
    for (I=0; I<4; I++)
    {
        Check(P.SetStage(I, 'Reload', float(I)*0.2, float(I+1)*0.2), "Configure authored range");
        Check(P.Resolve(I, 0.0, Sequence, Frame) && Sequence=='Reload'
            && Abs(Frame-float(I)*0.2)<0.0001, "Physical start maps to authored start");
        Check(P.Resolve(I, 0.5, Sequence, Frame)
            && Abs(Frame-(float(I)*0.2+0.1))<0.0001, "Physical travel scrubs range");
        Check(P.Resolve(I, 1.0, Sequence, Frame)
            && Abs(Frame-float(I+1)*0.2)<0.0001, "Physical completion maps to authored end");
    }
    Check(P.SetStage(3, 'Slide', 0.8, 0.4), "Allow reversed slide range");
    Check(P.Resolve(3, 0.25, Sequence, Frame) && Sequence=='Slide'
        && Abs(Frame-0.7)<0.0001, "Reverse travel follows physical action");
    Check(!P.Resolve(2, -0.1, Sequence, Frame), "Reject invalid physical progress");
    Check(!P.Resolve(2, 1.1, Sequence, Frame), "Reject excessive physical progress");
    Log("KFVR_POSE_TEST_RESULT checks=" $ Checks $ " failures=" $ Failures);
    return Failures;
}
defaultproperties
{
    IsClient=False
    IsServer=False
    IsEditor=True
    LogToStdout=True
}
