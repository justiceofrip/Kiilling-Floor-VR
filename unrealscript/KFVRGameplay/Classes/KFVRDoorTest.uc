// Explicit unattended fixture: position the local pawn beside an existing map door.
// Door collision, damage, trigger and welding remain the actual installed classes.
class KFVRDoorTest extends Actor;
var KFDoorMover TestDoor;
var PlayerController PC;
var bool bReady,bDoorClosed,bDoorSealed;
var float InitialStrength,CurrentStrength;
var int SetupAttempts;
function bool Initialize(PlayerController Player)
{
    local KFDoorMover D;
    local vector Axis,Center,Eye,HitLocation,HitNormal;
    local int I;
    PC=Player;
    if (PC==None || PC.Pawn==None || Level.NetMode!=NM_Standalone) return False;
    foreach DynamicActors(class'KFDoorMover',D)
    {
        if (!D.bClosed || D.bHidden || D.bDisallowWeld || D.MyTrigger==None || D.WeldStrength!=0) continue;
        Center=D.MyTrigger.Location+vect(0,0,40);
        for (I=0;I<4;I++)
        {
            Axis=vect(1,0,0); if (I==1) Axis=vect(-1,0,0); if (I==2) Axis=vect(0,1,0); if (I==3) Axis=vect(0,-1,0);
            Eye=Center+Axis*55; SetupAttempts++;
            if (PC.Pawn.Trace(HitLocation,HitNormal,Center-Axis*10,Eye,True)!=D) continue;
            if (!PC.Pawn.SetLocation(Eye-PC.Pawn.EyePosition())) continue;
            PC.SetRotation(rotator(-Axis)); PC.Pawn.SetRotation(rotator(-Axis));
            TestDoor=D; InitialStrength=D.WeldStrength; bReady=True;
            Log("KFVR_DOOR_TEST_READY door=" $ D $ " pawn=" $ PC.Pawn.Location $ " view=" $ PC.Rotation);
            return True;
        }
    }
    Log("KFVR_DOOR_TEST_FAILED attempts=" $ SetupAttempts); return False;
}
function Tick(float Delta)
{
    if (TestDoor==None) return;
    CurrentStrength=TestDoor.WeldStrength; bDoorClosed=TestDoor.bClosed; bDoorSealed=TestDoor.bSealed;
}
defaultproperties
{
    bHidden=True
    RemoteRole=ROLE_None
}
