class KFVRActionRailTests extends Commandlet;
var int Checks,Failures;
var KFVRActionRail G;

function Check(bool B,string Message)
{
    Checks++;
    if (!B) { Failures++; Log("KFVR_ACTION_RAIL_FAIL " $ Message); }
}
function Fresh()
{
    G=new class'KFVRActionRail';
}
function PistolStroke()
{
    Fresh();
    Check(!G.Move(vect(0,0,0)),"inactive move is rejected");
    Check(G.Begin(vect(0,0,0),vect(-1,0,0),0.05),"begin existing pistol rail");
    Check(G.bActive && G.Progress==0,"begin does not invent travel");
    Check(VSize(G.SideAxis-vect(0,1,0))<0.00001 && VSize(G.UpAxis-vect(0,0,1))<0.00001,"pistol transverse axes remain Y and Z");
    Check(G.Move(vect(-0.025,0,0)) && Abs(G.Progress-0.5)<0.00001,"half pull");
    Check(G.Move(vect(-0.045,0,0)) && Abs(G.Progress-0.9)<0.00001,"feed threshold is visible to caller");
    Check(G.Move(vect(-0.05,0,0)) && Abs(G.Progress-1)<0.00001,"full pull");
    Check(G.Move(vect(-0.10,0,0)) && G.Progress==1,"rear overtravel clamps");
    Check(G.Move(vect(-0.025,0,0)) && Abs(G.Progress-0.5)<0.00001,"physical partial return");
    Check(G.Move(vect(0,0,0)) && G.Progress==0,"physical full return");
    Check(G.Move(vect(0.03,0,0)) && G.Progress==0,"forward motion clamps without reverse progress");
    Check(G.Move(vect(-0.05,0,0)) && G.Progress==1,"another held stroke can reach rear");
    G.Cancel();
    Check(!G.bActive && G.Progress==1,"cancel retains last progress instead of returning");
    Check(!G.Move(vect(0,0,0)) && G.Progress==1,"cancelled rail cannot return until explicitly begun again");
}
function PistolSquareBounds()
{
    Fresh();
    Check(G.Begin(vect(0,0,0),vect(-1,0,0),0.05),"begin diagonal boundary");
    Check(G.Move(vect(-0.025,0.12,0.12)) && Abs(G.Progress-0.5)<0.00001,"square corner accepted, not replaced by radial circle");
    Check(G.Move(vect(-0.025,-0.12,-0.12)),"negative square corner accepted");
    Check(!G.Move(vect(-0.025,0.1201,0)) && !G.bActive && Abs(G.Progress-0.5)<0.00001,"Y escape cancels without changing progress");
    Check(!G.Move(vect(-0.05,0,0)),"return inside does not revive cancelled gesture");
    Check(G.Begin(vect(0,0,0),vect(-1,0,0),0.05),"begin Z escape");
    Check(!G.Move(vect(-0.025,0,-0.1201)) && !G.bActive,"negative Z escape cancels");
    Check(G.Begin(vect(0,0,0),vect(-1,0,0),0.05),"begin negative Y escape");
    Check(!G.Move(vect(-0.025,-0.1201,0)),"negative Y escape cancels");
    Check(G.Begin(vect(0,0,0),vect(-1,0,0),0.05),"begin positive Z escape");
    Check(!G.Move(vect(-0.025,0,0.1201)),"positive Z escape cancels");
}
function InitialProgress()
{
    Fresh();
    Check(G.Begin(vect(0.3,-0.2,0.1),vect(-1,0,0),0.05,0.4),"begin at an existing partial action");
    Check(G.Move(vect(0.3,-0.2,0.1)) && Abs(G.Progress-0.4)<0.00001,"stationary regrab retains initial travel");
    Check(G.Move(vect(0.28,-0.2,0.1)) && Abs(G.Progress-0.8)<0.00001,"pull adds to initial progress");
    Check(G.Move(vect(0.32,-0.2,0.1)) && Abs(G.Progress)<0.00001,"return subtracts from initial progress");
    Check(G.Begin(vect(0,0,0),vect(-1,0,0),0.046,1),"MK23 travel and rear regrab");
    Check(G.Move(vect(0.046,0,0)) && Abs(G.Progress)<0.00001,"existing MK23 distance returns a rear regrab");
}
function GenericRails()
{
    local vector Origin,Axis,P;
    local int I;
    Origin=vect(0.1,-0.2,0.15);
    for (I=0;I<5;I++)
    {
        if (I==0) Axis=vect(0,1,0);
        else if (I==1) Axis=vect(0,-1,0);
        else if (I==2) Axis=vect(0,0,1);
        else if (I==3) Axis=vect(-2,3,4);
        else Axis=vect(0.001,1,0.001);
        Fresh();
        Check(G.Begin(Origin,Axis,0.08),"normalize generic axis, including near-Y fallback");
        Check(Abs(VSize(G.PullAxis)-1)<0.00001 && Abs(VSize(G.SideAxis)-1)<0.00001 && Abs(VSize(G.UpAxis)-1)<0.00001,"unit rail frame");
        Check(Abs(G.PullAxis dot G.SideAxis)<0.00001 && Abs(G.PullAxis dot G.UpAxis)<0.00001 && Abs(G.SideAxis dot G.UpAxis)<0.00001,"orthogonal rail frame");
        P=Origin+G.PullAxis*0.04+G.SideAxis*0.11+G.UpAxis*0.11;
        Check(G.Move(P) && Abs(G.Progress-0.5)<0.00001,"generic square lateral allowance with half pull");
        Check(G.Move(Origin+G.PullAxis*0.08) && Abs(G.Progress-1)<0.00001,"generic full pull");
        Check(G.Move(Origin) && Abs(G.Progress)<0.00001,"generic physical return");
        Check(!G.Move(Origin+G.SideAxis*0.121) && !G.bActive,"generic transverse escape cancels");
    }
}
function InvalidProfilesAndPoints()
{
    Fresh();
    Check(!G.Begin(vect(0,0,0),vect(0,0,0),0.05),"zero axis rejected");
    Check(!G.Begin(vect(0,0,0),vect(0.000001,0,0),0.05),"near-zero axis rejected before normalization");
    Check(!G.Begin(vect(0,0,0),vect(1000,0,0),0.05),"unbounded axis rejected before squared length");
    Check(!G.Begin(vect(0,0,0),vect(-1,0,0),0),"zero travel rejected");
    Check(!G.Begin(vect(0,0,0),vect(-1,0,0),-0.05),"negative travel rejected");
    Check(!G.Begin(vect(0,0,0),vect(-1,0,0),0.0099),"travel below one centimetre rejected");
    Check(!G.Begin(vect(0,0,0),vect(-1,0,0),0.2501),"travel beyond twenty-five centimetres rejected");
    Check(G.Begin(vect(0,0,0),vect(-1,0,0),0.01),"minimum travel accepted");
    Check(G.Begin(vect(0,0,0),vect(-1,0,0),0.25),"maximum travel accepted");
    Check(!G.Begin(vect(0,0,0),vect(-1,0,0),0.05,-0.001),"negative initial travel rejected");
    Check(!G.Begin(vect(0,0,0),vect(-1,0,0),0.05,1.001),"initial travel above one rejected");
    Check(!G.Begin(vect(4.01,0,0),vect(-1,0,0),0.05),"implausible starting relative point rejected");
    Check(G.Begin(vect(0,0,0),vect(-1,0,0),0.05,0.5),"valid gesture before invalid reconfiguration");
    Check(!G.Begin(vect(0,0,0),vect(0,0,0),0.05) && !G.bActive && G.Progress==0.5,"bad profile cancels an old gesture without a return");
    Check(G.Begin(vect(0,0,0),vect(-1,0,0),0.05,0.5),"valid gesture before implausible move");
    Check(!G.Move(vect(-4.01,0,0)) && !G.bActive && G.Progress==0.5,"implausible move cancels without progress mutation");
    Check(!G.BoundedNumber(1000000,4) && !G.BoundedNumber(-1000000,4),"large signed components fail finite bounded guard");
}
function ControllerFrameInvariance()
{
    local int I,J,Step;
    local float Units,Expected;
    local vector RightWorld,LeftWorld,LocalP,StartP,P;
    local rotator Aim;
    StartP=vect(0.12,-0.24,0.18);
    for (I=0;I<4;I++)
    {
        Units=25*float(I+1);
        for (J=0;J<8;J++)
        {
            Aim.Pitch=J*1700; Aim.Yaw=J*6200; Aim.Roll=J*2100;
            RightWorld=vect(100,-200,20);
            LeftWorld=RightWorld+((StartP*Units)>>Aim);
            LocalP=((LeftWorld-RightWorld)<<Aim)/Units;
            Fresh();
            Check(G.Begin(LocalP,vect(-1,0,0),0.05),"begin from translated tilted controller frame");
            for (Step=0;Step<=10;Step++)
            {
                // Both controllers can move and turn together. No weapon actor
                // transform or visual recoil enters this conversion or helper.
                RightWorld.X+=3; RightWorld.Y-=2; RightWorld.Z+=1;
                Aim.Pitch+=500; Aim.Yaw+=700; Aim.Roll-=300;
                Expected=float(Step)/10;
                P=StartP+vect(-1,0,0)*(0.05*Expected);
                LeftWorld=RightWorld+((P*Units)>>Aim);
                LocalP=((LeftWorld-RightWorld)<<Aim)/Units;
                Check(G.Move(LocalP) && Abs(G.Progress-Expected)<0.001,"local stroke independent of world units, common motion and aim tilt");
            }
        }
    }
}

event int Main(string Params)
{
    PistolStroke(); PistolSquareBounds(); InitialProgress(); GenericRails();
    InvalidProfilesAndPoints(); ControllerFrameInvariance();
    Log("KFVR_ACTION_RAIL_TEST_RESULT checks=" $ Checks $ " failures=" $ Failures);
    if (Failures>0) return 1;
    return 0;
}
