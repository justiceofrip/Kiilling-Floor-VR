"""Read-only acceptance of actual game traces into a private collision fixture."""
import json,pathlib,math


def motion_clock_evidence(controls,rows):
 """Compare consumed knife samples with coherent delivered controller packets."""
 checks={};phases={};distinct_serials=0
 for n in (4,5,7,12,13,20):
  times=[c['uptime_ms'] for c in controls if c['phase']==n and c['sent']]
  samples=0;missing=0;serial_mismatches=0;time_mismatches=0;unsampled=0
  for row in rows:
   if not times or not min(times)+250<=row['uptime_ms']<=max(times) or not row.get('rig_sample_consistent'):continue
   objs={o['address']:o for o in row['objects'] if not o['values'].get('bDeleteMe')}
   rig=next((o['values'] for o in objs.values() if o['type']=='KFVRWeaponRig'),None)
   if not rig or not rig.get('bTracking') or rig.get('bHolstered') or rig.get('Tracking.Flags',0)&2==0:continue
   knife=objs.get(rig.get('Gun'),{})
   if knife.get('type')!='KFVRTrackedKnife':continue
   values=knife['values'];samples+=1
   if any(k not in rig for k in ('Tracking.InputSerial','Tracking.InputSeconds','Tracking.Serial')) or any(k not in values for k in ('LastMotionSerial','LastMotionTime','bMotionSampled')):
    missing+=1;continue
   if not values['bMotionSampled']:unsampled+=1
   if rig['Tracking.InputSerial']<=0 or values['LastMotionSerial']!=rig['Tracking.InputSerial']:serial_mismatches+=1
   source=rig['Tracking.InputSeconds'];consumed=values['LastMotionTime']
   if not all(isinstance(v,(int,float)) and math.isfinite(v) and v>=0 for v in (source,consumed)) or consumed!=source:time_mismatches+=1
   if rig['Tracking.InputSerial']!=rig['Tracking.Serial']:distinct_serials+=1
  phases[str(n)]=dict(samples=samples,missing_fields=missing,unsampled=unsampled,serial_mismatches=serial_mismatches,time_mismatches=time_mismatches)
  checks['knife_uses_fresh_source_clock_phase_'+str(n)]=samples>=3 and missing==unsampled==serial_mismatches==time_mismatches==0
 checks['knife_source_serial_distinct_from_presentation']=distinct_serials>0
 return checks,dict(phases=phases,distinct_source_serial_samples=distinct_serials)

def validate(folder):
 folder=pathlib.Path(folder)
 controls=[json.loads(x) for x in (folder/'controls.jsonl').read_text().splitlines()]
 rows=[json.loads(x) for x in (folder/'weapon-observations.jsonl').read_text().splitlines()]
 def objects(typ,selected=rows):return [o['values'] for row in selected for o in row['objects'] if o['type']==typ]
 def phase(n,typ='KFVRTrackedKnife'):
  times=[c['uptime_ms'] for c in controls if c['phase']==n and c['sent']]
  return objects(typ,[r for r in rows if times and min(times)+250<=r['uptime_ms']<=max(times)])
 checks={};knives=objects('KFVRTrackedKnife');targets=objects('KFVRMeleeTarget');rigs=objects('KFVRWeaponRig')
 if not knives or not targets:raise ValueError('Live knife and explicit collision fixture must be observed')
 start,end=knives[0],knives[-1]
 def at(name,n,predicate,typ='KFVRTrackedKnife'):
  data=phase(n,typ);checks[name]=bool(data) and all(predicate(x) for x in data)
 # The current private mutator registers seven slots, also established by
 # the existing inventory regression and the captured complete fixture.
 checks['seven_owned_weapons']=rigs[0]['InventoryCount']==rigs[-1]['InventoryCount']==7
 at('slow_motion_no_damage',5,lambda k:k['HitCount']==start['HitCount'] and k['SwingCount']==start['SwingCount'])
 at('small_waggle_no_damage',6,lambda k:k['HitCount']==start['HitCount'] and k['SwingCount']==start['SwingCount'])
 at('first_swing_single_hit',8,lambda k:k['HitCount']==start['HitCount']+1 and k['SwingCount']==start['SwingCount']+1)
 at('focus_loss_stops',9,lambda r:not r['bTracking'] and not r['bFiring'],'KFVRWeaponRig')
 at('held_contact_and_trigger_no_repeat',10,lambda k:k['HitCount']==start['HitCount']+1)
 at('retraction_no_damage',11,lambda k:k['HitCount']==start['HitCount']+1)
 at('second_swing_single_hit',13,lambda k:k['HitCount']==start['HitCount']+2 and k['SwingCount']==start['SwingCount']+2)
 at('locomotion_no_swing',13,lambda k:k['SwingCount']==start['SwingCount']+2)
 at('snap_turn_no_swing',14,lambda k:k['SwingCount']==start['SwingCount']+2)
 at('knife_stowed',17,lambda r:r['bHolstered'],'KFVRWeaponRig')
 at('holstered_motion_no_swing',17,lambda k:k['SwingCount']==start['SwingCount']+2)
 at('knife_redrawn',20,lambda r:not r['bHolstered'],'KFVRWeaponRig')
 checks['exactly_two_contact_damage_calls']=targets[-1]['HitCount']==2 and targets[-1]['DamageReceived']==38
 checks['contact_instigator_is_local_pawn']=targets[-1]['LastInstigator']==rigs[-1]['BoundPawn']
 checks['exactly_two_total_swings']=end['SwingCount']-start['SwingCount']==2
 checks['stop_releases_motion']=end['WindowUntil']==0 and not rigs[-1]['bTracking'] and not rigs[-1]['bFiring']
 checks['presentation_hidden_at_stop']=all(o['values']['bHidden'] for o in rows[-1]['objects'] if o['type'] in ['KFVRWeaponVisual','KFVRHandVisual'])
 clock_checks,clock_metrics=motion_clock_evidence(controls,rows);checks.update(clock_checks)
 report=dict(motion_clock_evidence=clock_metrics,passed=all(checks.values()),checks=checks,swings=end['SwingCount']-start['SwingCount'],contact_hits=targets[-1]['HitCount'],damage=targets[-1]['DamageReceived'],synthetic_controllers=True,live_game=True,headset_presented=False,collision_fixture=True,zed_perks_tested=False)
 (folder/'melee-validation.json').write_text(json.dumps(report,indent=2)+'\n')
 if not report['passed']:raise ValueError('Melee checks failed: '+', '.join(k for k,v in checks.items() if not v))
 return report
if __name__=='__main__':
 import sys
 print(json.dumps(validate(sys.argv[1]),indent=2))
