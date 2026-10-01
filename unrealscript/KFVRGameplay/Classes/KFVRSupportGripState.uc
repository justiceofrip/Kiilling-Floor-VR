// Offhand support ownership only. The rig retains measured geometry and posing.
class KFVRSupportGripState extends Object;
var bool bHeld,bArmed,bPrevious;
var int Grabs,Releases,Frames;

function Reset()
{
    if (bHeld) Releases++;
    bHeld=False; bArmed=False; bPrevious=False;
}

// Reload reservation is evaluated before support can consume a squeeze edge.
// A release while reserved/untracked does not arm a later support acquisition.
function bool Update(bool Eligible,bool Reserved,bool Down,bool NearSupport,bool WithinReach)
{
    if (!Eligible || Reserved) { Reset(); return False; }
    if (!Down) bArmed=True;
    if (bHeld && (!Down || !WithinReach)) { bHeld=False; Releases++; }
    if (bArmed && Down && !bPrevious && NearSupport) { bHeld=True; Grabs++; }
    if (bHeld) Frames++;
    bPrevious=Down;
    return bHeld;
}
