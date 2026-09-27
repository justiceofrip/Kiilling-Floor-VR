class KFVRThrowMotionTests extends Commandlet;
var int Checks,Failures;
function Check(bool Good,string CaseName)
{
    Checks++; if (!Good) { Failures++; Log("KFVR_THROW_FAIL " $ CaseName); }
}
function int Main(string Params)
{
    local KFVRThrowMotion M;
    local int I,J;
    local vector Direction,Position;
    local float Now,Step;
    M=new class'KFVRThrowMotion';
    for (I=0;I<72;I++)
    {
        Direction=Normal(vect(1,0,0)>>(rot(0,910,0)*I));
        // Vary speed and sample interval; constant velocity must remain invariant.
        Direction*=(1+I%11); M.Reset(); Position=vect(0,0,0); Now=0;
        Check(!M.Add(Position,Now),"first sample");
        for (J=0;J<24;J++)
        {
            Step=0.01+(J%4)*0.005; Now+=Step; Position+=Direction*Step;
            M.Add(Position,Now);
            Check(VSize(M.Velocity-Direction)<0.001,"constant speed");
            if (J>0) Check(M.Ready(Now),"continuous history");
        }
        Check(!M.Ready(Now+0.11),"expired history");
        M.Add(Position+vect(5,0,0),Now+0.016);
        Check(M.bDiscontinuity && !M.Ready(Now+0.016),"tracking jump");
        M.Add(Position,Now+0.2); M.Add(Position,Now+0.19);
        Check(M.bDiscontinuity,"time rollback");
    }
    M.Reset(); M.Add(vect(0,0,0),1); M.Add(vect(0,0,0),1.02); M.Add(vect(0,0,0),1.04);
    Check(M.Ready(1.04) && VSize(M.Velocity)==0,"stationary drop");
    M.Reset(); Check(!M.Ready(1.04),"loss resets history");
    Log("KFVR_THROW_RESULT checks=" $ Checks $ " failures=" $ Failures);
    if (Failures>0) return 1;
    return 0;
}
defaultproperties
{
    IsClient=False
    IsServer=True
    IsEditor=True
    LogToStdout=True
}
