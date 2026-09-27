// Independent loaded feeds; reserve and actual shot consumption remain stock KF.
// This timed-reload adapter does not claim physical magazines or separate chambers.
class KFVRDualFeedState extends Object;
var int RightLoaded,LeftLoaded,RightCapacity,LeftCapacity;
function Initialize(int Capacity,int Loaded)
{
    RightCapacity=(Capacity+1)/2; LeftCapacity=Capacity/2;
    RightLoaded=Min(RightCapacity,(Loaded+1)/2); LeftLoaded=Min(LeftCapacity,Loaded/2);
}
function int Total() { return RightLoaded+LeftLoaded; }
function int Loaded(int Mode) { if (Mode==1) return LeftLoaded; return RightLoaded; }
function bool Consume(int Mode)
{
    if (Mode==1) { if (LeftLoaded<=0) return False; LeftLoaded--; }
    else if (Mode==0) { if (RightLoaded<=0) return False; RightLoaded--; }
    else return False;
    return True;
}
function Reconcile(int Count)
{
    local int Delta,N;
    Count=Clamp(Count,0,RightCapacity+LeftCapacity); Delta=Count-Total();
    if (Delta>=0)
    { N=Min(RightCapacity-RightLoaded,Delta); RightLoaded+=N; LeftLoaded+=Delta-N; }
    else
    { N=Min(RightLoaded,-Delta); RightLoaded-=N; LeftLoaded+=Delta+N; }
}
function bool Valid() { return RightLoaded>=0 && LeftLoaded>=0 && RightLoaded<=RightCapacity && LeftLoaded<=LeftCapacity; }
