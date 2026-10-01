class KFVRMagazineInsertionTests extends Commandlet;
var int Checks,Failures;
var KFVRMagazineInsertion G;

function Check(bool B,string Message)
{
    Checks++;
    if (!B) { Failures++; Log("KFVR_MAG_INSERT_FAIL " $ Message); }
}

function SmoothPush()
{
    G.Reset();
    Check(!G.Update(vect(0,0,-0.06),1,1,1,7,True) && G.bApproached,"observe approach");
    Check(!G.Update(vect(0,0,-0.045),1,2,1.02,7,True) && G.Progress>0,"partial travel");
    Check(!G.Update(vect(0,0,-0.03),1,3,1.04,7,True),"middle of stroke");
    Check(!G.Update(vect(0,0,-0.015),1,4,1.06,7,True),"short stroke cannot seat");
    Check(G.Update(vect(0,0,0),1,5,1.08,7,True) && G.bSeated && G.Progress==1,"full push seats");
    Check(!G.Update(vect(0,0,0),1,6,1.1,7,True),"one event at seat");
    Check(!G.Update(vect(0,0,-0.06),1,7,1.12,7,True),"same magazine cannot rearm by withdrawal");
    Check(!G.Update(vect(0,0,0),1,8,1.14,7,True),"same magazine cannot seat twice");
    Check(!G.Update(vect(0,0,0),1,9,1.16,7,False),"release after seating");
    Check(!G.Update(vect(0,0,-0.06),1,10,1.18,7,True),"same identity remains consumed after release");
    Check(!G.Update(vect(0,0,0),1,11,1.2,7,True),"released identity cannot seat twice");
}

function AcquisitionAndWithdrawal()
{
    G.Reset();
    Check(!G.Update(vect(0,0,0),1,1,1,1,True) && !G.bApproached,"acquisition at seat cannot insert");
    Check(!G.Update(vect(0,0,0),1,2,1.02,1,True),"stationary acquired seat cannot insert");
    Check(!G.Update(vect(0,0,-0.04),1,3,1.04,1,True) && !G.bApproached,"insufficient withdrawal does not approach");
    Check(!G.Update(vect(0,0,-0.06),1,4,1.06,1,True) && G.bApproached,"full withdrawal permits fresh approach");
    Check(!G.Update(vect(0,0,-0.025),1,5,1.08,1,True) && G.Progress>0.5,"partial insertion advances progress");
    Check(!G.Update(vect(0,0,-0.055),1,6,1.1,1,True) && G.Progress<0.2,"withdrawal reduces progress");
    Check(G.Update(vect(0,0,-0.005),1,7,1.12,1,True),"new continuous push can finish");
    G.Reset();
    Check(!G.Update(vect(0,0,-0.005),1,1,2,1,True) && !G.bApproached,"explicit reset requires another approach");
    Check(!G.Update(vect(0,0,0),1,2,2.02,1,True),"reset cannot preserve prior stroke");
    Check(!G.Update(vect(0,0,-0.06),1,3,2.04,1,True),"same magazine can approach after reset");
    Check(G.Update(vect(0,0,0),1,4,2.06,1,True),"explicit reset permits a new completed stroke");
    G.Reset();
    Check(!G.Update(vect(0,0,-0.2),1,1,3,1,True) && G.bApproached,"far aligned approach can arm");
    Check(!G.Update(vect(0,0,-0.12),1,2,3.02,1,True),"far-to-near continuous approach");
    Check(!G.Update(vect(0,0,-0.06),1,3,3.04,1,True),"far approach reaches nominal stroke");
    Check(G.Update(vect(0,0,0),1,4,3.06,1,True),"far approach completes without clamp or drop");
}

function PoseRejection()
{
    G.Reset();
    Check(!G.Update(vect(0,0,-0.06),1,1,1,1,True),"orientation case approach");
    Check(!G.Update(vect(0,0,-0.03),0,2,1.02,1,True) && !G.bApproached,"sideways cancels approach");
    Check(!G.Update(vect(0,0,0),1,3,1.04,1,True),"alignment recovery at seat cannot insert");
    Check(!G.Update(vect(0,0,-0.06),1,4,1.06,1,True),"approach after sideways rejection");
    Check(!G.Update(vect(0,0,-0.03),-1,5,1.08,1,True) && !G.bApproached,"upside down cancels approach");
    Check(!G.Update(vect(0,0,0),1,6,1.1,1,True),"roll recovery at seat cannot insert");
    Check(!G.Update(vect(0,0,-0.06),1,7,1.12,1,True),"approach after upside-down rejection");
    Check(!G.Update(vect(0.036,0,-0.03),1,8,1.14,1,True) && !G.bApproached,"radial escape cancels approach");
    Check(!G.Update(vect(0,0,0),1,9,1.16,1,True),"side entry cannot seat");
    Check(!G.Update(vect(0,0,-0.06),1,10,1.18,1,True),"approach before overshoot");
    Check(!G.Update(vect(0,0,0.025),1,11,1.21,1,True) && !G.bApproached,"overtravel cancels approach");
    Check(!G.Update(vect(0,0,0),1,12,1.23,1,True),"return from far side cannot seat");
    Check(!G.Update(vect(0,0,-0.06),0.81,13,1.25,1,True),"aligned tolerance approach");
    Check(G.Update(vect(0.02,0.02,0),0.81,14,1.28,1,True),"aligned push inside radial tolerance seats");
    G.Reset();
    Check(!G.Update(vect(0,0,-0.06),0.79,1,2,1,True) && !G.bApproached,"below angular tolerance cannot arm");
    Check(!G.Update(vect(0,0,0),1,2,2.02,1,True),"improved angle at seat lacks approach");
    G.Reset();
    Check(!G.Update(vect(0.026,0.026,-0.06),1,1,3,1,True) && !G.bApproached,"radial circle rejects diagonal corner");
    Check(!G.Update(vect(0,0,-0.06),1.01,2,3.02,1,True) && !G.bApproached,"invalid angular cosine cannot arm");
}

function ContinuityRejection()
{
    G.Reset();
    Check(!G.Update(vect(0,0,-0.06),1,1,1,1,True),"jump case approach");
    Check(!G.Update(vect(0,0,0),1,2,1.001,1,True) && !G.bApproached,"implausible speed cannot insert");
    Check(!G.Update(vect(0,0,0),1,3,1.021,1,True),"stable pose after jump still needs approach");
    Check(!G.Update(vect(0,0,-0.06),1,4,1.041,1,True),"fresh approach after jump");
    Check(G.Update(vect(0,0,0),1,5,1.061,1,True),"continuous recovery push");
    G.Reset();
    Check(!G.Update(vect(0,0,-0.06),1,1,2,1,True),"gap case approach");
    Check(!G.Update(vect(0,0,0),1,2,2.2,1,True) && !G.bApproached,"sample gap cannot insert");
    Check(!G.Update(vect(0,0,0),1,3,2.22,1,True),"gap recovery at seat cannot insert");
    Check(!G.Update(vect(0,0,-0.06),1,4,2.24,1,True),"gap recovery approach");
    Check(G.Update(vect(0,0,0),1,5,2.26,1,True),"gap recovery push");
    G.Reset();
    Check(!G.Update(vect(0,0,-0.06),1,1,3,1,True),"jump to approach initial sample");
    Check(!G.Update(vect(0,0,-0.2),1,2,3.001,1,True) && !G.bApproached,"jump ending outside cannot itself arm");
    Check(!G.Update(vect(0,0,0),1,3,3.021,1,True) && !G.bApproached,"return jump cannot insert");
}

function OrderingRejection()
{
    G.Reset();
    Check(!G.Update(vect(0,0,-0.06),1,10,1,1,True),"ordering case approach");
    Check(!G.Update(vect(0,0,0),1,10,1.02,1,True) && G.bApproached && G.Depth< -0.05,"same serial ignores changed pose");
    Check(G.LastTime==1 && G.LastSerial==10,"duplicate serial preserves sample baseline");
    Check(!G.Update(vect(0,0,0),1,9,1.02,1,True) && G.bApproached,"backwards serial ignored");
    Check(!G.Update(vect(0,0,0),1,11,1,1,True) && G.bApproached,"stalled clock ignored");
    Check(!G.Update(vect(0,0,0),1,11,0.99,1,True) && G.bApproached,"backwards clock ignored");
    Check(G.LastSerial==10 && G.LastTime==1 && G.Progress==0,"replays cannot advance diagnostics");
    Check(G.Update(vect(0,0,0),1,11,1.02,1,True),"fresh sample after duplicates finishes pending push");
    G.Reset();
    Check(!G.Update(vect(0,0,-0.06),1,1,2,1,True),"frozen serial case approach");
    Check(!G.Update(vect(0,0,-0.06),1,1,2.1,1,True),"frozen serial does not refresh time");
    Check(!G.Update(vect(0,0,0),1,2,2.2,1,True) && !G.bApproached,"late fresh serial cannot bridge frozen producer");
    Check(!G.Update(vect(0,0,-0.06),1,-1,2.22,1,True) && G.LastSerial==2,"negative serial rejected");
    Check(!G.Update(vect(0,0,-0.06),1,3,-1,1,True) && G.LastSerial==2,"negative time rejected");
}

function OwnershipRejection()
{
    G.Reset();
    Check(!G.Update(vect(0,0,-0.06),1,1,1,1,True),"identity case approach");
    Check(!G.Update(vect(0,0,0),1,2,1.02,2,True) && !G.bApproached,"new identity cannot inherit stroke");
    Check(!G.Update(vect(0,0,-0.06),1,3,1.04,2,True),"new identity approach");
    Check(G.Update(vect(0,0,0),1,4,1.06,2,True),"new identity push");
    Check(!G.Update(vect(0,0,-0.06),1,5,1.08,3,True) && !G.bSeated,"changed identity clears consumed event");
    Check(G.Update(vect(0,0,0),1,6,1.1,3,True),"another magazine can seat");
    G.Reset();
    Check(!G.Update(vect(0,0,-0.06),1,1,2,1,True),"grip loss case approach");
    Check(!G.Update(vect(0,0,-0.03),1,2,2.02,1,False) && !G.bApproached,"grip loss cancels stroke");
    Check(!G.Update(vect(0,0,0),1,3,2.04,1,True),"regrip at seat cannot insert");
    Check(!G.Update(vect(0,0,-0.06),1,4,2.06,1,True),"fresh approach after regrip");
    Check(G.Update(vect(0,0,0),1,5,2.08,1,True),"push after regrip can finish");
    G.Reset();
    Check(!G.Update(vect(0,0,-0.06),1,1,3,0,True) && !G.bApproached,"empty hand cannot approach");
    Check(!G.Update(vect(0,0,0),1,2,3.02,1,True),"new magazine at seat lacks approach");
}

function TransformedSocket()
{
    local rotator R;
    local vector Socket,World,LocalP;
    // The gate only receives metres in socket space. This exercises the UE2
    // caller transform with a translated, tilted, rolled gun and nonunit scale.
    R=rot(8192,16384,4096); Socket=vect(250,-310,80);
    G.Reset();
    World=Socket+((vect(0,0,-0.065)*50)>>R);
    LocalP=((World-Socket)<<R)/50;
    Check(!G.Update(LocalP,1,1,1,1,True) && G.bApproached,"rotated socket approach");
    World=Socket+((vect(0,0,-0.03)*50)>>R);
    LocalP=((World-Socket)<<R)/50;
    Check(!G.Update(LocalP,1,2,1.02,1,True) && G.Progress>0,"rotated socket partial push");
    World=Socket+((vect(0,0,0)*50)>>R);
    LocalP=((World-Socket)<<R)/50;
    Check(G.Update(LocalP,1,3,1.04,1,True),"rotated socket full push");
}

function DiagnosticClassification()
{
    local KFVRMagazineInsertion D;
    D=new class'KFVRMagazineInsertion';
    Check(!D.Update(vect(0,0,-0.06),1,1,1,1,True),"diagnostic approach");
    Check(D.LastDelta==0 && D.LastDistance==0 && D.SpeedRejects==0 && D.GapRejects==0,"first observation has no discontinuity");
    Check(!D.Update(vect(0,0,-0.06),1,2,1.02,1,True),"diagnostic stationary sample");
    Check(Abs(D.LastDelta-0.02)<0.0001 && D.LastDistance==0 && D.SpeedRejects==0 && D.GapRejects==0,"continuous sample metrics");
    Check(!D.Update(vect(0,0,0),1,3,1.021,1,True),"diagnostic fast jump rejected");
    Check(D.SpeedRejects==1 && D.GapRejects==0 && Abs(D.LastDistance-0.06)<0.0001 && Abs(D.LastDelta-0.001)<0.0001,"speed-only rejection classified");
    Check(!D.Update(vect(1,0,0),1,3,1.2,1,True),"diagnostic replay ignored");
    Check(D.SpeedRejects==1 && D.GapRejects==0 && Abs(D.LastDistance-0.06)<0.0001 && Abs(D.LastDelta-0.001)<0.0001,"replay preserves metrics and counters");
    D.Reset();
    Check(D.SpeedRejects==1 && D.GapRejects==0 && D.LastDelta==0 && D.LastDistance==0,"reset clears pose metrics but retains lifetime counts");
    Check(!D.Update(vect(0,0,-0.06),1,1,2,1,True),"gap diagnostic approach");
    Check(!D.Update(vect(0,0,-0.06),1,2,2.2,1,True),"stationary late sample rejected");
    Check(D.SpeedRejects==1 && D.GapRejects==1 && D.LastDistance==0 && Abs(D.LastDelta-0.2)<0.0001,"gap-only rejection classified");
    Check(!D.Update(vect(0,0,-0.2),1,3,2.201,2,True),"different identity starts separate observation");
    Check(D.SpeedRejects==1 && D.GapRejects==1 && D.LastDelta==0 && D.LastDistance==0,"identity transition is not a speed fault");
    Check(!D.Update(vect(1,0,0),1,4,2.401,2,True),"combined gap and speed rejection");
    Check(D.SpeedRejects==2 && D.GapRejects==2,"independent fault categories both recorded");
}

event int Main(string Params)
{
    G=new class'KFVRMagazineInsertion';
    SmoothPush(); AcquisitionAndWithdrawal(); PoseRejection();
    ContinuityRejection(); OrderingRejection(); OwnershipRejection(); TransformedSocket(); DiagnosticClassification();
    Log("KFVR_MAG_INSERT_TEST_RESULT checks=" $ Checks $ " failures=" $ Failures);
    if (Failures>0) return 1;
    return 0;
}
