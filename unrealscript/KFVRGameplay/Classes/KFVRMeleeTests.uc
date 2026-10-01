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

function CheckBladeHistoryAndWithdrawal()
{
    local KFVRMeleeState S;
    local float Deadline;
    local int I;
    S=new(None) class'KFVRMeleeState';
    Check(!S.Sample(vect(0.2,0,0),0,0,0.6,vect(0,0,0)),"paired blade baseline");
    Check(!S.Sample(vect(0.3,0,0),0.04,0.04,0.6,vect(0.1,0,0)),"paired blade partial stroke");
    Check(S.Sample(vect(0.45,0,0),0.08,0.08,0.6,vect(0.25,0,0)),"paired blade eligible stroke");
    Check(S.SweepFrom==vect(0.2,0,0) && S.SweepBase==vect(0,0,0)
        && Abs(S.SweepSeconds-0.08)<0.00001,"sweep retains base and tip from the same stroke origin");
    Check(S.ContactEligible(),"forward strike permits contact");
    Check(!S.Sample(vect(0.50,0,0),0.10,0.10,0.6,vect(0.30,0,0))
        && S.ContactEligible() && S.PreviousBase==vect(0.25,0,0)
        && Abs(S.StepSeconds-0.02)<0.00001,"forward follow-through has paired previous blade frame");
    Check(!S.Sample(vect(0.46,0,0),0.12,0.12,0.6,vect(0.26,0,0))
        && S.Speed>1.2 && S.bLocked && !S.ContactEligible(),"fast reversal cannot damage through prior miss window");
    Check(!S.Sample(vect(0.25,0,0),0.16,0.16,0.6,vect(0.05,0,0))
        && !S.bLocked && !S.ContactEligible(),"full withdrawal rearms without contact");
    Check(S.Origin==vect(0.25,0,0) && S.OriginBase==vect(0.05,0,0)
        && Abs(S.OriginTime-0.16)<0.00001,"retraction preserves paired new origin");
    Check(!S.Sample(vect(0.35,0,0),0.20,0.20,0.6,vect(0.15,0,0)),"retraction cannot bypass stock cooldown");
    Check(!S.Sample(vect(0.35,0,0),0.75,0.75,0.6,vect(0.15,0,0)),"gap creates fresh paired origin");
    Check(!S.Sample(vect(0.47,0,0),0.79,0.79,0.6,vect(0.27,0,0)),"second partial stroke");
    Check(S.Sample(vect(0.60,0,0),0.83,0.83,0.6,vect(0.40,0,0))
        && S.SweepBase==vect(0.15,0,0) && Abs(S.SweepSeconds-0.08)<0.00001,"new strike cannot sweep stale blade history");
    Deadline=S.NextStrike;
    for (I=1;I<=100;I++)
        Check(!S.Sample(vect(0.60,0,0),0.83+I*0.02,0.83+I*0.02,0.6,vect(0.40,0,0))
            && !S.ContactEligible() && S.Strikes==2 && S.NextStrike==Deadline,"lingering blade never creates contact eligibility");
    Check(!S.Sample(vect(0.7,0,0),3,3,0.6,vect(4.1,0,0)) && !S.bValid
        && !S.ContactEligible() && S.NextStrike==Deadline,"invalid blade base clears history but preserves stock deadline");
    Check(!S.Sample(vect(0,0,0),3.1,3.1,0,vect(0,0,0)) && !S.bValid,"invalid zero cooldown cannot authorize strike");
    Check(!S.Sample(vect(0,0,0),-1,3.1,0.6,vect(0,0,0)) && !S.bValid,"invalid source time cannot authorize strike");
    Check(!S.Sample(vect(0,0,0),3.1,-1,0.6,vect(0,0,0)) && !S.bValid,"invalid game time cannot authorize strike");
}
function CheckCooldownBladeHistory()
{
    local KFVRMeleeState S;
    local vector P,Base;
    local float SourceNow,Deadline;
    local int I,TraceIndex;
    S=new(None) class'KFVRMeleeState';
    Check(!S.Sample(vect(0,0,0),0,0,0.6,vect(-0.2,0,0)),"cooldown history baseline");
    Check(S.Sample(vect(0.24,0,0),0.04,0.04,0.6,vect(0.04,0,0)),"cooldown history first swing");
    Deadline=S.NextStrike;
    Check(!S.Sample(vect(0,0,0),0.08,0.08,0.6,vect(-0.2,0,0)) && !S.bLocked,"cooldown history retracts");
    // Continuous 6 m/s short motions during cooldown previously left a stale
    // origin and diluted the later accepted contact below minimum speed.
    for (I=1;I<=27;I++)
    {
        SourceNow=0.08+I*0.02;
        P=vect(0.06,0,0); if ((I%2)==0) P.X=-0.06;
        Base=P-vect(0.2,0,0);
        Check(!S.Sample(P,SourceNow,SourceNow,0.6,Base) && S.Strikes==1,
            "cooldown oscillation cannot strike");
        Check(S.NextStrike==Deadline && S.Origin==P && S.OriginBase==Base
            && S.OriginTime==SourceNow,"cooldown keeps coherent fresh origin without moving deadline");
    }
    Check(!S.Sample(vect(0.24,0,0),0.65,0.65,0.6,vect(0.04,0,0)),
        "old pre-cooldown distance cannot complete a new stroke");
    Check(S.Sample(vect(0.30,0,0),0.68,0.68,0.6,vect(0.10,0,0)) && S.Strikes==2,
        "fresh post-cooldown stroke completes");
    Check(S.SweepFrom==vect(0.06,0,0) && Abs(S.SweepSeconds-0.06)<0.00001,
        "new contact interval excludes old cooldown motion");
    for (TraceIndex=0;TraceIndex<=4;TraceIndex++)
        Check(class'KFVRMeleeGeometry'.static.MovingContact(
            class'KFVRMeleeGeometry'.static.BladePoint(S.SweepBase,S.SweepFrom,TraceIndex,4),
            class'KFVRMeleeGeometry'.static.BladePoint(S.LastBase,S.Last,TraceIndex,4),
            S.SweepSeconds,1,1.2),"accepted fresh stroke keeps all moving blade points eligible");
    Check(S.ContactEligible() && Abs(S.NextStrike-1.28)<0.00001,
        "fresh strike retains stock cooldown and forward contact eligibility");
}

function CheckStaleBladeHistory()
{
    local KFVRMeleeState S;
    local vector P;
    local float SourceNow;
    local int I;
    S=new(None) class'KFVRMeleeState';
    Check(!S.Sample(vect(0,0,0),0,10,0.6,vect(-0.2,0,0)),"stale history starts without cooldown");
    for (I=1;I<=30;I++)
    {
        SourceNow=I*0.02; P=vect(0.06,0,0); if ((I%2)==0) P.X=-0.06;
        Check(!S.Sample(P,SourceNow,10+SourceNow,0.6,P-vect(0.2,0,0))
            && S.Strikes==0 && S.NextStrike==0,"fast short oscillation does not strike");
    }
    Check(!S.Sample(vect(0.24,0,0),0.64,10.64,0.6,vect(0.04,0,0))
        && S.Strikes==0 && S.NextStrike==0,"diluted whole stroke cannot consume sound or stock cooldown");
    Check(S.Origin==vect(0.24,0,0) && S.OriginBase==vect(0.04,0,0)
        && Abs(S.OriginTime-0.64)<0.00001,"diluted stroke rebases coherent blade history");
    Check(!S.Sample(vect(0.36,0,0),0.68,10.68,0.6,vect(0.16,0,0)),"fresh movement still needs complete stroke");
    Check(S.Sample(vect(0.49,0,0),0.72,10.72,0.6,vect(0.29,0,0)) && S.Strikes==1,
        "ordinary fast stroke succeeds after history rebase");
    Check(S.SweepFrom==vect(0.24,0,0) && S.SweepBase==vect(0.04,0,0)
        && Abs(S.SweepSeconds-0.08)<0.00001 && Abs(S.NextStrike-11.32)<0.00001,
        "accepted stroke and contact share source interval and stock deadline");
}

function CheckBladeGeometry()
{
    local int I,Intervals,ScaleIndex;
    local float Units,LengthMetres,Radius;
    local vector A,B,From,To,Previous;
    Check(class'KFVRMeleeGeometry'.static.TraceIntervals(0.20,0.03)==4,"knife contact intervals preserve overlapping 3 cm extent");
    Check(class'KFVRMeleeGeometry'.static.TraceIntervals(0,0.03)==0
        && class'KFVRMeleeGeometry'.static.TraceIntervals(0.2,0)==0
        && class'KFVRMeleeGeometry'.static.TraceIntervals(1.6,0.03)==0,"invalid blade profiles cannot issue traces");
    for (ScaleIndex=0;ScaleIndex<3;ScaleIndex++)
    {
        Units=25+ScaleIndex*25;
        A=vect(0,0,0); B=vect(0,0,0.2)*Units;
        Check(class'KFVRMeleeGeometry'.static.BladePoint(A,B,0,4)==A
            && class'KFVRMeleeGeometry'.static.BladePoint(A,B,4,4)==B,"trace endpoints are blade base and tip");
        for (I=0;I<=4;I++)
        {
            From=class'KFVRMeleeGeometry'.static.BladePoint(A,B,I,4);
            To=class'KFVRMeleeGeometry'.static.BladePoint(A+vect(0.24,0,0)*Units,B+vect(0.24,0,0)*Units,I,4);
            Check(class'KFVRMeleeGeometry'.static.MovingContact(From,To,0.10,Units,1.2),"translated blade contact is independent of world scale");
            Check(!class'KFVRMeleeGeometry'.static.MovingContact(From,From,0.10,Units,1.2),"stationary blade point is not a damage ray");
        }
        From=class'KFVRMeleeGeometry'.static.BladePoint(A,B,1,4);
        To=class'KFVRMeleeGeometry'.static.BladePoint(A,B+vect(0.24,0,0)*Units,1,4);
        Check(!class'KFVRMeleeGeometry'.static.MovingContact(From,To,0.10,Units,1.2),"slow hilt-side pivot cannot inherit fast tip damage");
        From=B; To=B+vect(0.24,0,0)*Units;
        Check(class'KFVRMeleeGeometry'.static.MovingContact(From,To,0.10,Units,1.2),"same pivot can qualify moving tip contact");
    }
    // Longer future blades need more overlapping traces, not a hand-to-tip ray.
    for (I=1;I<=100;I++)
    {
        LengthMetres=0.01+I*0.014; Radius=0.005+(I%10)*0.005;
        Intervals=class'KFVRMeleeGeometry'.static.TraceIntervals(LengthMetres,Radius);
        Check(Intervals>0 && LengthMetres/Intervals<=2*Radius,"bounded profile stencil leaves no longitudinal gaps");
    }
    Check(!class'KFVRMeleeGeometry'.static.MovingContact(vect(0,0,0),vect(1,0,0),0,50,1.2)
        && !class'KFVRMeleeGeometry'.static.MovingContact(vect(0,0,0),vect(1,0,0),0.1,0,1.2),"invalid contact clock/scale cannot trace");
    Check(class'KFVRKnifeFire'.default.MeleeDamage==class'KnifeFire'.default.MeleeDamage
        && class'KFVRKnifeFire'.default.MeleeDamage>0,"physical adapter inherits installed primary damage");
    Check(class'KFVRKnifeFire'.default.hitDamageClass==class'KnifeFire'.default.hitDamageClass
        && ClassIsChildOf(class'KFVRKnifeFire'.default.hitDamageClass,class'DamTypeMelee'),"stock melee damage type still reaches Berserker victim rules");
    Check(class'KFVRKnifeFire'.default.FireRate==class'KnifeFire'.default.FireRate
        && class'KFVRKnifeFire'.default.FireRate>0,"physical adapter inherits stock cooldown before perk modifier");
    Check(Sound(DynamicLoadObject(class'KFVRKnifeFire'.default.PhysicalSwingSoundRef,class'Sound',True))!=None,
        "physical swing profile resolves an installed knife sound");
    Check(class'KFVRKnifeFire'.default.FireSound==class'KnifeFire'.default.FireSound
        && class'KFVRKnifeFire'.default.FireSoundRef==class'KnifeFire'.default.FireSoundRef,
        "physical sound resolver does not change flat fire sound defaults");
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
    CheckBladeHistoryAndWithdrawal();
    CheckCooldownBladeHistory();
    CheckStaleBladeHistory();
    CheckBladeGeometry();
    Log("KFVR_MELEE_TEST_RESULT checks=" $ Checks $ " failures=" $ Failures);
    if (Failures>0) return 1;
    return 0;
}
