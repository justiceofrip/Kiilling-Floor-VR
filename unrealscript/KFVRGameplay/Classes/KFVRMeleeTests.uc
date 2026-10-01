class KFVRMeleeTests extends Commandlet;
var int Checks,Failures;
function Check(bool B,optional string Message) { Checks++; if (!B) { Failures++; Log("KFVR_MELEE_FAIL " $ Message); } }

function CheckPhysicalClock()
{
    local KFVRMeleeState S,Scaled;
    local int Trial,Step,Repeat;
    local float SourceNow,GameNow,Dilation,SavedCooldown;
    local vector P,SavedLast;
    local bool Struck,ScaledStruck;
    // A 24 cm / 40 ms physical stroke stays below the existing 15 m/s jump
    // limit at every game speed. Feeding scaled time incorrectly exceeds it.
    for (Trial=0;Trial<3;Trial++)
    {
        S=new(None) class'KFVRMeleeState'; Scaled=new(None) class'KFVRMeleeState';
        SourceNow=20; GameNow=100;
        Check(!S.Sample(vect(0,0,0),SourceNow,GameNow,0.6),"source motion baseline");
        Check(!Scaled.Sample(vect(0,0,0),GameNow,GameNow,0.6),"scaled comparison baseline");
        for (Step=1;Step<=2;Step++)
        {
            Dilation=1.1;
            if (Trial==1 || (Trial==2 && Step==1)) Dilation=0.22;
            SourceNow+=0.02; GameNow+=0.02*Dilation;
            P=vect(0,0,0); P.X=0.12*Step;
            Struck=S.Sample(P,SourceNow,GameNow,0.6);
            ScaledStruck=Scaled.Sample(P,GameNow,GameNow,0.6);
            Check(Struck==(Step==2),"physical trajectory strike independent of dilation");
            Check(Abs(S.Speed-6.0)<0.002 && S.RejectedJumps==0,"physical speed independent of dilation");
            SavedCooldown=S.NextStrike; SavedLast=S.Last;
            for (Repeat=0;Repeat<16;Repeat++)
            {
                Check(!S.Sample(P,SourceNow,GameNow+0.001,0.6),"duplicate sample cannot strike");
                Check(!S.bMoved && S.Last==SavedLast && S.LastTime==SourceNow
                    && S.NextStrike==SavedCooldown,"duplicate cannot reuse movement or cooldown");
            }
        }
        Check(S.Strikes==1 && S.bLocked,"physical stroke locks until retraction");
        Check(ScaledStruck==(Trial==0),"scaled clock reproduces slow-motion jump rejection");
        Check(Abs(S.NextStrike-(GameNow+0.6))<0.0001,"stock cooldown remains game time");
    }

    // A slow hand must not become a damaging swing merely because the game
    // slows. Travel exceeds the 20 cm stroke threshold, isolating the existing
    // 1.2 m/s minimum-speed rule rather than passing on insufficient distance.
    for (Trial=0;Trial<3;Trial++)
    {
        S=new(None) class'KFVRMeleeState'; Scaled=new(None) class'KFVRMeleeState';
        SourceNow=30; GameNow=200;
        Check(!S.Sample(vect(0,0,0),SourceNow,GameNow,0.6),"slow source baseline");
        Check(!Scaled.Sample(vect(0,0,0),GameNow,GameNow,0.6),"slow scaled baseline");
        for (Step=1;Step<=30;Step++)
        {
            Dilation=1.1;
            if (Trial==1 || (Trial==2 && Step<=26)) Dilation=0.22;
            else if (Trial==2) Dilation=0.22+0.22*(Step-26);
            SourceNow+=0.02; GameNow+=0.02*Dilation;
            P=vect(0,0,0); P.X=0.008*Step;
            Check(!S.Sample(P,SourceNow,GameNow,0.6),"slow physical hand never strikes");
            Scaled.Sample(P,GameNow,GameNow,0.6);
            Check(Abs(S.Speed-0.4)<0.002 && S.RejectedJumps==0,"slow speed stays physical in ZED time");
        }
        Check(S.Strikes==0 && !S.bLocked && S.NextStrike==0,"slow motion creates no strike or cooldown");
        Check(Scaled.Strikes==int(Trial!=0),"scaled clock reproduces false slow-hand strike");
    }

    S=new(None) class'KFVRMeleeState';
    Check(!S.Sample(vect(0,0,0),100,10,0.6),"pre-rebase baseline");
    Check(!S.Sample(vect(0.10,0,0),100.04,10.04,0.6),"pre-rebase partial");
    Check(S.Sample(vect(0.25,0,0),100.08,10.08,0.6),"pre-rebase strike");
    SavedCooldown=S.NextStrike;
    Check(!S.Sample(vect(0.4,0,0),99,10.09,0.6),"backward source cannot strike");
    Check(S.NextStrike==SavedCooldown && !S.bMoved,"backward source preserves cooldown");
    S.ResetTracking();
    Check(S.NextStrike==SavedCooldown && !S.bValid && !S.bLocked,"source reset retains game cooldown");
    Check(!S.Sample(vect(0,0,0),0,10.09,0.6),"new source epoch baseline");
    Check(!S.Sample(vect(0.12,0,0),0.02,10.11,0.6),"new epoch partial");
    Check(!S.Sample(vect(0.24,0,0),0.04,10.13,0.6),"reset cannot bypass stock cooldown");
    Check(S.Strikes==1 && S.NextStrike==SavedCooldown,"cooldown rejection retains deadline");
    Check(!S.Sample(vect(0.24,0,0),0.60,10.69,0.6),"stationary gap starts a fresh stroke");
    Check(!S.Sample(vect(0.36,0,0),0.62,10.71,0.6),"post-cooldown partial");
    Check(S.Sample(vect(0.49,0,0),0.64,10.73,0.6),"game cooldown expires despite lower source epoch");
    Check(S.Strikes==2 && Abs(S.NextStrike-11.33)<0.0001,"new strike sets stock game deadline");
}

event int Main(string Params)
{
    local KFVRMeleeState S;
    local int I;
    local vector P;
    S=new(None) class'KFVRMeleeState';
    Check(!S.Sample(vect(0,0,0),0,0,0.6));
    Check(!S.Sample(vect(0.1,0,0),0.04,0.04,0.6));
    // Duplicate game ticks must not reset a physically continuing stroke.
    Check(!S.Sample(vect(0.1,0,0),0.05,0.05,0.6));
    Check(S.Sample(vect(0.25,0,0),0.08,0.08,0.6));
    for (I=1;I<=60;I++) Check(!S.Sample(vect(0.25,0,0),0.08+I*0.02,0.08+I*0.02,0.6));
    Check(!S.Sample(vect(0.05,0,0),1.32,1.32,0.6));
    Check(!S.Sample(vect(0.15,0,0),1.36,1.36,0.6));
    Check(S.Sample(vect(0.30,0,0),1.40,1.40,0.6));
    S.ResetTracking(); Check(!S.Sample(vect(0,0,0),1.41,1.41,0.6));
    Check(!S.Sample(vect(0.3,0,0),1.47,1.47,0.6));
    S=new(None) class'KFVRMeleeState';
    for (I=0;I<10000;I++) { P.X=0.02*Sin(I); Check(!S.Sample(P,I*0.01,I*0.01,0.6)); }
    Check(S.Strikes==0);
    S.ResetTracking(); Check(!S.Sample(vect(0,0,0),101,101,0.6));
    Check(!S.Sample(vect(1,0,0),101.001,101.001,0.6)); Check(S.RejectedJumps>0);
    Check(!S.Sample(vect(1,0,0),102,102,0.6));
    Check(!S.Sample(vect(1.3,0,0),101,101,0.6));
    CheckPhysicalClock();
    Log("KFVR_MELEE_TEST_RESULT checks=" $ Checks $ " failures=" $ Failures);
    if (Failures>0) return 1;
    return 0;
}
