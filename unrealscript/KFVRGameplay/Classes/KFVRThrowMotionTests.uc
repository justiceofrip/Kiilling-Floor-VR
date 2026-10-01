class KFVRThrowMotionTests extends Commandlet;
var int Checks,Failures;
function Check(bool Good,string CaseName)
{
    Checks++; if (!Good) { Failures++; Log("KFVR_THROW_FAIL " $ CaseName); }
}
function SourceClockTrajectories()
{
    local KFVRThrowMotion M,GameClock;
    local int Trial,I,J,Serial,LastSerial,GameSerial,Count,Rejects;
    local float SourceNow,GameNow,Dilation,LastTime;
    local vector Position,Expected,Velocity;
    local bool ScaledClockRejected;
    Expected=vect(3,0,4); // Five physical metres per second in two axes.
    for (Trial=0;Trial<3;Trial++)
    {
        M=new class'KFVRThrowMotion'; GameClock=new class'KFVRThrowMotion';
        LastSerial=0; GameSerial=0; GameNow=0; ScaledClockRejected=False;
        Check(!M.AddSource(vect(0,0,0),0,0,LastSerial) && M.Samples==0,"zero source serial ignored");
        for (I=0;I<=24;I++)
        {
            SourceNow=I*0.02; Position=Expected*SourceNow; Serial=I+1;
            Dilation=1.1;
            if (Trial==1) Dilation=0.22;
            else if (Trial==2 && I<8) Dilation=0.22;
            else if (Trial==2 && I<16) Dilation=0.55;
            if (I>0) GameNow+=0.02*Dilation;
            Check(M.AddSource(Position,SourceNow,Serial,LastSerial),"fresh source sample consumed");
            GameClock.AddSource(Position,GameNow,Serial,GameSerial);
            if (GameClock.bDiscontinuity) ScaledClockRejected=True;
            Check(M.Samples==I+1 && M.Rejected==0,"source history independent of game speed");
            if (I>0) Check(VSize(M.Velocity-Expected)<0.001,"same trajectory has physical velocity");
            if (I>1) Check(M.Ready(SourceNow),"source history becomes ready");
            Velocity=M.Velocity; Count=M.Samples; Rejects=M.Rejected; LastTime=M.LastTime;
            // Extra game ticks may present the same mailbox repeatedly. Even a
            // changed duplicate payload must not enter the physical trajectory.
            for (J=0;J<3;J++)
            {
                Check(!M.AddSource(Position+vect(1,0,0),SourceNow+0.005*J,Serial,LastSerial),"duplicate game ticks ignored");
                Check(M.Samples==Count && M.Rejected==Rejects && M.LastTime==LastTime
                    && VSize(M.Velocity-Velocity)==0 && LastSerial==Serial,"duplicate preserves complete motion history");
            }
            Check(!M.AddSource(vect(5,5,5),SourceNow+1,Serial-1,LastSerial)
                && M.Samples==Count && M.LastTime==LastTime,"older source serial ignored");
        }
        Check(M.Ready(SourceNow) && VSize(M.Velocity-Expected)<0.001,"normal slow and ramping clocks agree");
        // The same physical 10 cm/20 ms segment exceeds 12 m/s if measured
        // against slowed game time. This is the concrete pre-fix failure.
        if (Trial>0) Check(ScaledClockRejected,"game-clock counterexample rejects legitimate throw");
        Check(!M.Ready(SourceNow+0.11),"source history expires independently of game clock");
    }
}

function SourceRecovery()
{
    local KFVRThrowMotion M;
    local int LastSerial,Count,Rejects;
    M=new class'KFVRThrowMotion';
    M.AddSource(vect(0,0,0),30,1,LastSerial);
    M.AddSource(vect(0.1,0,0),30.02,2,LastSerial);
    M.AddSource(vect(0.2,0,0),30.04,3,LastSerial);
    Check(M.Ready(30.04),"initial source history ready");
    // Native clock faults deliver neutral input, which resets the caller and
    // velocity history before a later valid timestamp starts a new epoch.
    M.Reset(); LastSerial=0;
    Check(!M.Ready(0) && M.Samples==0 && VSize(M.Velocity)==0,"neutral clock fault clears velocity");
    Check(M.AddSource(vect(2,0,0),0.02,4,LastSerial) && !M.Ready(0.02),"rebased source needs new baseline");
    M.AddSource(vect(2.1,0,0),0.04,5,LastSerial);
    Check(!M.Ready(0.04),"one rebased interval cannot throw");
    M.AddSource(vect(2.2,0,0),0.06,6,LastSerial);
    Check(M.Ready(0.06) && Abs(VSize(M.Velocity)-5)<0.001,"fresh rebased history recovers");
    Check(M.AddSource(vect(2.3,0,0),0.05,7,LastSerial) && M.bDiscontinuity,"advancing serial with rollback is a fault");
    Rejects=M.Rejected; M.Reset(); // Local faults preserve the consumed serial.
    Check(!M.AddSource(vect(2.3,0,0),0.05,7,LastSerial) && M.Samples==0
        && M.Rejected==Rejects,"fault duplicate cannot restart history");
    M.AddSource(vect(3,0,0),0.08,8,LastSerial);
    M.AddSource(vect(3.1,0,0),0.10,9,LastSerial);
    M.AddSource(vect(3.2,0,0),0.12,10,LastSerial);
    Check(M.Ready(0.12) && Abs(VSize(M.Velocity)-5)<0.001,"fault recovery requires fresh physical intervals");
    Count=M.Samples;
    Check(!M.AddSource(vect(3.2,0,0),0.12,10,LastSerial) && M.Samples==Count,"ready history survives duplicate");
    Check(M.AddSource(vect(3.2,0,0),0.24,11,LastSerial) && M.bDiscontinuity
        && !M.Ready(0.24),"source gap cancels ready throw");
    // A new bounded native host restarts input serials; lifecycle reset permits
    // that only after clearing the old physical history.
    M.Reset(); LastSerial=0;
    M.AddSource(vect(0,0,0),0,1,LastSerial);
    M.AddSource(vect(0,0,0),0.02,2,LastSerial);
    M.AddSource(vect(0,0,0),0.04,3,LastSerial);
    Check(M.Ready(0.04) && VSize(M.Velocity)==0,"new host can rebuild stationary drop history");
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
    SourceClockTrajectories(); SourceRecovery();
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
