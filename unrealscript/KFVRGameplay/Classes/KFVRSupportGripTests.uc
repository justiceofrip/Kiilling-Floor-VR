class KFVRSupportGripTests extends Commandlet;
var int Checks,Failures;
var KFVRSupportGripState G;

function Check(bool B,string Message)
{
    Checks++;
    if (!B) { Failures++; Log("KFVR_SUPPORT_GRIP_FAIL " $ Message); }
}
function Step(bool Eligible,bool Reserved,bool Down,bool NearSupport,bool WithinReach,bool Expected,string Message)
{
    Check(G.Update(Eligible,Reserved,Down,NearSupport,WithinReach)==Expected,Message);
    Check(G.Grabs-G.Releases==int(G.bHeld),"one owner, balanced transitions");
    Check(!G.bHeld || (G.bArmed && G.bPrevious),"held support consumed a release and press");
}
function Fresh()
{
    G=new class'KFVRSupportGripState';
}
function Grab()
{
    Step(True,False,False,True,True,False,"observe released grip");
    Step(True,False,True,True,True,True,"fresh squeeze acquires support");
}
function OrdinaryEdges()
{
    local int I;
    Fresh();
    Step(True,False,True,True,True,False,"startup held squeeze cannot acquire");
    Grab();
    for (I=0;I<20;I++) Step(True,False,True,True,True,True,"duplicate down level keeps one owner");
    Check(G.Grabs==1 && G.Releases==0 && G.Frames==21,"duplicate levels do not replay grab");
    for (I=0;I<20;I++) Step(True,False,False,True,True,False,"duplicate released level remains free");
    Check(G.Grabs==1 && G.Releases==1 && G.Frames==21,"one release, no duplicate events");
    Step(True,False,True,False,True,False,"squeeze outside support cannot acquire");
    Step(True,False,True,True,True,False,"moving into support while held is not a fresh edge");
    Grab();
    Check(G.Grabs==2 && G.Releases==1,"release then regrab");
}
function ReloadPriority()
{
    local int I;
    Fresh();
    Step(True,False,False,True,True,False,"support and manual observe neutral");
    Step(True,True,True,True,True,False,"overlapping belt target reserves fresh squeeze before support");
    Check(!G.bArmed && G.Grabs==0,"reserved edge disarms support");
    for (I=0;I<5;I++) Step(True,True,True,True,True,False,"held magazine owns offhand");
    for (I=0;I<5;I++) Step(True,False,True,True,True,False,"seated magazine does not hand held squeeze to support");
    Check(G.Grabs==0 && G.Releases==0,"seating never created support ownership");
    Grab();
    Step(True,True,True,True,True,False,"slide or caught magazine releases existing support");
    Check(G.Grabs==1 && G.Releases==1 && !G.bArmed,"reload preemption released exactly once");
    Step(True,True,False,True,True,False,"release during another owner's reservation cannot arm support");
    Step(True,False,True,True,True,False,"reservation ending under squeeze still requires release");
    Grab();
    Check(G.Grabs==2 && G.Releases==1,"explicit release permits support after reload");
}
function TrackingAndSwitchReset()
{
    Fresh(); Grab();
    Step(False,False,True,True,True,False,"dominant or offhand tracking loss releases support");
    Step(False,False,False,True,True,False,"untracked release is not observed input");
    Step(True,False,True,True,True,False,"tracking recovery with held squeeze cannot acquire");
    Check(G.Grabs==1 && G.Releases==1 && !G.bArmed,"tracking loss is one release");
    Grab();
    G.Reset(); G.Reset();
    Check(!G.bHeld && !G.bArmed && !G.bPrevious,"switch or holster resets all ownership edges");
    Check(G.Grabs==2 && G.Releases==2 && G.Frames==2,"repeated reset preserves lifetime counters");
    Step(True,False,True,True,True,False,"new weapon cannot inherit held squeeze");
    Grab();
    Check(G.Grabs==3 && G.Releases==2,"new weapon requires fresh release and press");
}
function DistanceRelease()
{
    Fresh(); Grab();
    Step(True,False,True,False,False,False,"exceeding retained reach releases support");
    Step(True,False,True,True,True,False,"returning within reach cannot reacquire a held squeeze");
    Check(G.Grabs==1 && G.Releases==1,"distance release occurs exactly once");
    Grab();
    Check(G.Grabs==2 && G.Releases==1,"distance recovery requires release and regrab");
}

event int Main(string Params)
{
    OrdinaryEdges(); ReloadPriority(); TrackingAndSwitchReset(); DistanceRelease();
    Log("KFVR_SUPPORT_GRIP_TEST_RESULT checks=" $ Checks $ " failures=" $ Failures);
    if (Failures>0) return 1;
    return 0;
}
