class KFVRMagazineGuideTests extends Commandlet;
var int Checks,Failures;
var KFVRMagazineGuide G;
function Check(bool B,string Message)
{
    Checks++;
    if (!B) { Failures++; Log("KFVR_MAG_GUIDE_FAIL " $ Message); }
}
function Fresh() { G=new(None) class'KFVRMagazineGuide'; }
function Capture()
{
    Check(!G.Update(vect(0,0,-0.10),vect(0,0,-0.06),1,1,1,7,True)
        && G.bCaptured && !G.bSeated && G.Progress==0,"capture observes entrance without seating");
}
function SmoothPush()
{
    Fresh();
    Check(!G.Update(vect(0.2,0.1,-0.2),vect(0.07,0.02,-0.08),0.55,1,1,7,True)
        && G.bCaptured && Abs(G.RailDepth+0.08)<0.00001,"forgiving off-axis/angle entrance captures");
    Check(!G.Update(vect(0.202,0.1,-0.175),vect(0.3,0.2,0.2),-1,2,1.02,7,True)
        && G.bCaptured && Abs(G.RailDepth+0.055)<0.00001 && G.Progress>0,
        "rail uses captured root/palm offset despite changed free root and wrist");
    Check(!G.Update(vect(0.202,0.1,-0.15),vect(-0.3,0.2,0.1),0,3,1.04,7,True)
        && G.Progress>0.5,"continuous partial push advances rail");
    Check(G.Update(vect(0.202,0.1,-0.12),vect(0,0,0.2),-1,4,1.06,7,True)
        && G.bSeated && !G.bCaptured && G.RailDepth==0 && G.Progress==1,
        "deliberate inward palm stroke seats once");
    Check(G.SeatCount==1 && G.CaptureCount==1 && G.CancelCount==0,"success counters exclude cancellation");
    Check(!G.Update(vect(0,0,-0.1),vect(0,0,-0.06),1,5,1.08,7,True),"consumed identity cannot recapture");
    Check(!G.Update(vect(0,0,0),vect(0,0,0),1,6,1.10,7,False),"opening after seat does not return event");
    Check(!G.Update(vect(0,0,-0.1),vect(0,0,-0.06),1,7,1.12,7,True)
        && G.SeatCount==1 && !G.bCaptured,"regrip cannot duplicate seat");
    Check(!G.Update(vect(0,0,-0.1),vect(0,0,-0.06),1,8,1.14,8,True)
        && G.bCaptured && !G.bSeated,"changed identity starts a separate capture");
}
function AcquisitionBounds()
{
    Fresh();
    Check(!G.Update(vect(0,0,0),vect(0,0,0),1,1,1,1,True) && !G.bCaptured,"interior acquisition cannot capture");
    Check(!G.Update(vect(0,0,0.03),vect(0,0,0.03),1,2,1.02,1,True) && !G.bCaptured,"pushing farther inside cannot seat");
    Fresh();
    Check(!G.Update(vect(0,0,-0.1),vect(0.081,0,-0.06),1,1,1,1,True) && !G.bCaptured,"capture radial limit");
    Fresh();
    Check(!G.Update(vect(0,0,-0.1),vect(0.06,0.06,-0.06),1,1,1,1,True) && !G.bCaptured,"circle rejects diagonal corner");
    Fresh();
    Check(!G.Update(vect(0,0,-0.1),vect(0,0,-0.06),0.49,1,1,1,True) && !G.bCaptured,"sideways entrance lacks moderate alignment");
    Fresh();
    Check(!G.Update(vect(0,0,-0.1),vect(0,0,-0.06),-1,1,1,1,True) && !G.bCaptured,"upside-down entrance rejected");
    Fresh();
    Check(!G.Update(vect(0,0,-0.3),vect(0,0,-0.181),1,1,1,1,True) && !G.bCaptured,"far root is outside capture range");
    Fresh();
    Check(!G.Update(vect(0,0,-0.06),vect(0,0,-0.034),1,1,1,1,True) && !G.bCaptured,"too-deep acquisition lacks deliberate stroke");
    Fresh();
    Check(!G.Update(vect(0,0,-0.1),vect(0.079,0,-0.036),0.51,1,1,1,True) && G.bCaptured,"near shallow edge remains forgiving");
    Check(!G.Update(vect(0,0,-0.075),vect(0,0,0),1,2,1.02,1,True) && !G.bSeated,"less than minimum push cannot seat");
    Check(G.Update(vect(0,0,-0.066),vect(0,0,0),1,3,1.04,1,True),"shallow capture still requires actual complete inward travel");
}
function WristAndDuplicates()
{
    local vector FreeRoot;
    local int I;
    local float Now;
    Fresh(); Capture();
    for (I=1;I<=800;I++)
    {
        Now=1+I*0.01;
        FreeRoot.X=0.2*Sin(I); FreeRoot.Y=0.2*Cos(I); FreeRoot.Z=0.1*Sin(I*2);
        Check(!G.Update(vect(0,0,-0.1),FreeRoot,Cos(I),I+1,Now,7,True)
            && G.bCaptured && G.RawRailDepth==G.CaptureRootDepth && G.InwardTravel==0 && G.Progress==0,
            "wrist-only free-root changes cannot move rail or seat");
        Check(!G.Update(vect(0,0,0.1),vect(0,0,0),1,I+1,Now,7,True)
            && G.bCaptured && G.Progress==0,"duplicate source cannot manufacture inward motion");
    }
    Check(G.SeatCount==0 && G.SpeedRejects==0 && G.InwardSamples==0,"wrist motion does not count as translation or teleport");
    Check(!G.Update(vect(0,0,-0.075),vect(0,0,0),-1,802,9.02,7,True) && G.bCaptured,"real push starts after wrist movement");
    Check(G.Update(vect(0,0,-0.035),vect(0,0,0),-1,803,9.04,7,True),"real translation still seats after wrist movement");
    Fresh(); Capture();
    Check(!G.Update(vect(0,0,-0.02),vect(0,0,0),1,2,1.03,7,True)
        && G.bCaptured && G.RailDepth==0 && G.SeatCount==0,"single quick push clamps at end but does not immediately seat");
    Check(!G.Update(vect(0,0,-0.02),vect(0,0,0),1,2,1.03,7,True) && G.SeatCount==0,"repeated arrival is not confirmation");
    Check(G.Update(vect(0,0,-0.02),vect(0,0,0),1,3,1.05,7,True),"fresh stable sample confirms valid quick push");
}
function CancellationAndRecovery()
{
    Fresh(); Capture();
    Check(!G.Update(vect(0,0,-0.075),vect(0,0,-0.035),1,2,1.02,7,True) && G.Progress>0,"partial push before retreat");
    Check(!G.Update(vect(0,0,-0.085),vect(0,0,-0.045),1,3,1.04,7,True)
        && G.bCaptured && G.Progress>0,"small retreat reduces travel without detaching");
    Check(!G.Update(vect(0,0,-0.12),vect(0,0,-0.08),1,4,1.06,7,True)
        && !G.bCaptured && G.CancelCount==1 && G.SeatCount==0,"real retreat cancels rail");
    Fresh(); Capture();
    Check(!G.Update(vect(0.07,0.07,-0.1),vect(0,0,-0.06),1,2,1.04,7,True) && G.bCaptured,"lateral margin uses circle");
    Check(!G.Update(vect(0.09,0.09,-0.1),vect(0,0,-0.06),1,3,1.06,7,True)
        && !G.bCaptured && G.CancelCount==1,"lateral escape cancels independently of free-root orientation");
    Check(!G.Update(vect(0.09,0.09,-0.04),vect(0,0,0),1,4,1.08,7,True) && !G.bCaptured,"escape recovery at seat cannot reacquire");
    Fresh(); Capture();
    Check(!G.Update(vect(0,0,-0.1),vect(0,0,-0.06),1,1,1,7,False)
        && !G.bCaptured && G.CancelCount==1,"release cancels even on duplicate packet");
    Check(!G.Update(vect(0,0,-0.04),vect(0,0,0),1,2,1.02,7,True) && !G.bCaptured,"regrip inside requires new entrance");
    Fresh(); Capture(); G.Reset();
    Check(!G.bCaptured && !G.bSeated && G.CancelCount==1 && G.CaptureCount==1,"tracking/reference reset clears stroke but preserves counts");
    Check(!G.Update(vect(0,0,-0.04),vect(0,0,0),1,1,0,7,True) && !G.bCaptured,"new clock epoch at seat cannot auto-seat");
    Check(!G.Update(vect(0,0,-0.1),vect(0,0,-0.06),1,2,0.02,7,True) && G.bCaptured,"new epoch can freshly capture entrance");
    Fresh(); Capture();
    Check(!G.Update(vect(0,0,-0.04),vect(0,0,0),1,2,1.02,8,True)
        && !G.bCaptured && !G.bSeated && G.CancelCount==1,"different magazine cannot inherit pending push");
}
function Discontinuities()
{
    Fresh(); Capture();
    Check(!G.Update(vect(0,0,-0.04),vect(0,0,0),1,2,1.001,7,True)
        && !G.bCaptured && G.SpeedRejects==1 && G.SeatCount==0,"palm teleport cannot complete insertion");
    Check(!G.Update(vect(0,0,-0.04),vect(0,0,0),1,3,1.021,7,True) && !G.bCaptured,"teleport destination remains ineligible");
    Fresh(); Capture();
    Check(!G.Update(vect(0,0,-0.04),vect(0,0,0),1,2,1.2,7,True)
        && !G.bCaptured && G.GapRejects==1,"source gap cancels pending travel");
    Fresh();
    Check(!G.Update(vect(0,0,-0.3),vect(0,0,-0.26),1,1,1,7,True),"outside entrance baseline");
    Check(!G.Update(vect(0,0,-0.1),vect(0,0,-0.06),1,2,1.001,7,True)
        && !G.bCaptured && G.SpeedRejects==1,"teleport into entrance cannot itself capture");
    Check(!G.Update(vect(0,0,-0.1),vect(0,0,-0.06),1,3,1.021,7,True)
        && G.bCaptured && G.Progress==0,"stable fresh entrance can recover without inherited travel");
    Fresh(); Capture();
    Check(!G.Update(vect(0,0,-0.04),vect(0,0,0),1,1,1.02,7,True)
        && !G.bCaptured && G.ClockRejects==1,"same serial with changed source time cancels");
    Fresh(); Capture();
    Check(!G.Update(vect(0,0,-0.04),vect(0,0,0),1,2,1,7,True)
        && !G.bCaptured && G.ClockRejects==1,"fresh serial with stalled time cancels");
    Fresh(); Capture();
    Check(!G.Update(vect(0,0,-0.04),vect(0,0,0),1,2,0.99,7,True)
        && !G.bCaptured && G.ClockRejects==1,"backwards source time cancels");
    Fresh(); Capture();
    Check(!G.Update(vect(0,0,-0.08),vect(0,0,-0.04),1,2,1.02,7,True),"serial rollback partial");
    Check(!G.Update(vect(0,0,-0.04),vect(0,0,0),1,1,1.04,7,True)
        && !G.bCaptured && G.ClockRejects==1,"backwards serial cancels");
}
function InvalidInput()
{
    Fresh(); Capture();
    Check(!G.Update(vect(5,0,0),vect(0,0,0),1,2,1.02,7,True)
        && !G.bCaptured && G.InvalidRejects==1,"invalid raw palm cancels");
    Fresh(); Capture();
    Check(!G.Update(vect(0,0,-0.1),vect(0,5,0),1,2,1.02,7,True)
        && !G.bCaptured && G.InvalidRejects==1,"invalid raw root cancels even while guided");
    Fresh(); Capture();
    Check(!G.Update(vect(0,0,-0.1),vect(0,0,-0.06),1.01,2,1.02,7,True)
        && !G.bCaptured && G.InvalidRejects==1,"invalid cosine rejected");
    Fresh(); Capture();
    Check(!G.Update(vect(0,0,-0.1),vect(0,0,-0.06),1,0,1.02,7,True)
        && !G.bCaptured && G.InvalidRejects==1,"invalid source serial rejected");
    Fresh(); Capture();
    Check(!G.Update(vect(0,0,-0.1),vect(0,0,-0.06),1,2,-1,7,True)
        && !G.bCaptured && G.InvalidRejects==1,"invalid source time rejected");
    Fresh(); Capture();
    Check(!G.Update(vect(0,0,-0.1),vect(0,0,-0.06),1,2,1.02,0,True)
        && !G.bCaptured && G.SeatCount==0,"empty hand cannot retain guide");
}
function TransformedSocket()
{
    local vector Socket,WorldPalm,WorldRoot,Palm,Root;
    local rotator Frame;
    local float Units;
    local int I;
    Socket=vect(312,-221,71); Frame=rot(8192,16384,4096);
    for (I=0;I<3;I++)
    {
        Units=25+25*I; Fresh();
        WorldPalm=Socket+((vect(0.03,-0.06,-0.1)*Units)>>Frame);
        WorldRoot=Socket+((vect(0.05,-0.03,-0.07)*Units)>>Frame);
        Palm=((WorldPalm-Socket)<<Frame)/Units; Root=((WorldRoot-Socket)<<Frame)/Units;
        Check(!G.Update(Palm,Root,0.6,1,1,1,True) && G.bCaptured,"caller rotated/scaled socket captures");
        WorldPalm=Socket+((vect(0.03,-0.06,-0.065)*Units)>>Frame);
        Palm=((WorldPalm-Socket)<<Frame)/Units;
        Check(!G.Update(Palm,vect(0.2,0.1,0.1),-1,2,1.02,1,True)
            && Abs(G.RailDepth+0.035)<0.00001,"rotated socket preserves rail-only depth");
        WorldPalm=Socket+((vect(0.03,-0.06,-0.025)*Units)>>Frame);
        Palm=((WorldPalm-Socket)<<Frame)/Units;
        Check(G.Update(Palm,vect(0.1,0.2,0.1),0,3,1.04,1,True),"rotated socket seats from actual inward translation");
    }
}
event int Main(string Params)
{
    SmoothPush(); AcquisitionBounds(); WristAndDuplicates(); CancellationAndRecovery();
    Discontinuities(); InvalidInput(); TransformedSocket();
    Log("KFVR_MAG_GUIDE_TEST_RESULT checks=" $ Checks $ " failures=" $ Failures);
    if (Failures>0) return 1;
    return 0;
}
