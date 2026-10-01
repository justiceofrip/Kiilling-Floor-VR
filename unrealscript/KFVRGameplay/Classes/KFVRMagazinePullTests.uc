class KFVRMagazinePullTests extends Commandlet;
var int Checks,Failures;
var KFVRMagazinePull P;
var KFVRMagazineState Ammo;
function Check(bool B,string Message)
{
    Checks++;
    if (!B) { Failures++; Log("KFVR_MAG_PULL_FAIL " $ Message); }
}
function Cases()
{
    local vector Palm;
    local int I,Id,Total;
    P=new class'KFVRMagazinePull';
    Ammo=new class'KFVRMagazineState';
    Check(Ammo.Initialize(30,30,160),"initial ledger");
    Id=Ammo.Inserted; Total=Ammo.Total();
    Check(P.Begin(vect(0,0,0),1,1,Id),"grasp");
    Check(!P.Update(vect(0,0,-0.025),2,1.02,Id,True) && P.Progress>0,"partial pull");
    Check(Ammo.Inserted==Id && Ammo.Held==0 && Ammo.Total()==Total,"partial geometry no ammo authority");
    Check(!P.Update(vect(0,0,-0.04),3,1.04,Id,True),"short pull");
    Check(P.Update(vect(0,0,-0.07),4,1.06,Id,True) && P.bComplete,"full pull event");
    Check(Ammo.Eject(True) && Ammo.Held==Id && Ammo.Inserted==0,"retain exact extracted magazine");
    Check(Ammo.Total()==Total && Ammo.Discarded==0 && Ammo.bChamber && Ammo.Valid(),"tactical round conservation");
    Check(!P.Update(vect(0,0,-0.1),5,1.08,Id,True) && P.CompleteCount==1,"no duplicate removal");
    Check(Ammo.Seat(),"return retained magazine");
    Check(P.Begin(vect(0,0,0),10,2,Id),"new fresh grasp");
    Check(!P.Update(vect(0,0,-0.02),11,2.02,Id,True),"partial before release");
    Check(!P.Update(vect(0,0,-0.07),12,2.04,Id,False) && !P.bActive,"release cancels before removal");
    Check(Ammo.Inserted==Id && Ammo.Held==0,"release retains seat");
    Check(P.Begin(vect(0,0,0),20,3,Id),"teleport grasp");
    Check(!P.Update(vect(0,0,-0.1),21,3.001,Id,True) && !P.bActive,"teleport rejected");
    Check(P.Begin(vect(0,0,0),30,4,Id),"gap grasp");
    Check(!P.Update(vect(0,0,-0.07),31,4.2,Id,True) && !P.bActive,"gap rejected");
    Check(P.Begin(vect(0,0,0),40,5,Id),"duplicate grasp");
    Check(!P.Update(vect(0,0,-0.07),40,5.02,Id,True) && P.bActive && P.Progress==0,"duplicate cannot move");
    Check(!P.Update(vect(0,0,-0.07),41,5,Id,True) && P.Progress==0,"stalled time cannot move");
    Check(!P.Update(vect(0,0,-0.07),39,5.04,Id,True) && !P.bActive,"rollback cancels");
    Check(P.Begin(vect(0,0,0),50,6,Id),"wrong identity grasp");
    Check(!P.Update(vect(0,0,-0.07),51,6.02,Id+1,True) && !P.bActive,"identity change cancels");
    Check(P.Begin(vect(0,0,0),60,7,Id),"sideways grasp");
    Check(!P.Update(vect(0.07,0,0),61,7.02,Id,True),"small lateral error tolerated");
    Check(!P.Update(vect(0.13,0,0),62,7.04,Id,True) && !P.bActive,"lateral escape cancels");
    Check(P.Begin(vect(0,0,0),70,8,Id),"wrong way grasp");
    Check(!P.Update(vect(0,0,0.05),71,8.02,Id,True) && !P.bActive,"inward pull cannot eject");
    Check(P.Begin(vect(0,0,0),80,9,Id),"stationary grasp");
    for (I=1;I<=100;I++) Check(!P.Update(vect(0,0,0),80+I,9+float(I)*0.02,Id,True) && P.Progress==0,"stationary grip cannot eject");
    Check(P.Begin(vect(0,0,0),200,12,Id),"fast valid pull");
    Check(!P.Update(vect(0,0,-0.07),201,12.02,Id,True) && P.Progress==1,"single fresh displacement awaits confirmation");
    Check(P.Update(vect(0,0,-0.07),202,12.04,Id,True),"fresh stable confirmation completes");
    Check(!P.Begin(vect(5,0,0),1,1,Id),"invalid vector");
    Check(!P.Begin(vect(0,0,0),1,1,0),"missing magazine");
    Check(!P.Begin(vect(0,0,0),1,-1,Id),"invalid time");
    // Translation and rotation belong to the caller's socket transform. The
    // same relative palm stroke must preserve outcomes at a tilted world pose.
    Palm=(vect(0,0,-0.035)*50)>>rot(4096,16384,8192);
    Palm=(Palm<<rot(4096,16384,8192))/50;
    Check(P.Begin(vect(0,0,0),300,13,Id),"rotated socket grasp");
    Check(!P.Update(Palm,301,13.02,Id,True) && Abs(P.Distance-0.035)<0.0001,"transformed partial pull");
    Check(P.Update(Palm*2,302,13.04,Id,True),"transformed full pull");
    Check(Ammo.Valid() && Ammo.Total()==Total && Ammo.Inserted==Id,"canceled/repeated gestures leave ledger intact");
}
event int Main(string Params)
{
    Cases();
    Log("KFVR_MAG_PULL_TEST_RESULT checks=" $ Checks $ " failures=" $ Failures);
    if (Failures>0) return 1;
    return 0;
}
