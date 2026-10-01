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

def blade_contact_evidence(controls,rows):
 """Independent blade span and per-swing dispatch evidence, on completed poses."""
 from validate_grip_pose import rotate
 def finite(value):return type(value) in (float,int) and math.isfinite(value)
 def vector(value):return isinstance(value,(tuple,list)) and len(value)==3 and all(finite(x) for x in value)
 def distance(a,b):return math.dist(a,b) if vector(a) and vector(b) else math.inf
 samples=0;missing=0;span_errors=[];base_errors=[];tip_errors=[];tokens=[];trace_counts=[];sound_counts=[];damage=[]
 okay=True;rates=True;last_counters={}
 wanted={4,5,7,8,12,13,20}
 windows=[(min(c['uptime_ms'] for c in controls if c['sent'] and c['phase']==n)+250,
           max(c['uptime_ms'] for c in controls if c['sent'] and c['phase']==n))
          for n in wanted if any(c['sent'] and c['phase']==n for c in controls)]
 for row in rows:
  if not row.get('rig_sample_consistent') or not any(a<=row['uptime_ms']<=b for a,b in windows):continue
  objs={o['address']:o for o in row['objects'] if not o['values'].get('bDeleteMe')}
  rig=next((o['values'] for o in objs.values() if o['type']=='KFVRWeaponRig'),None)
  if not rig or not rig.get('bTracking') or rig.get('bHolstered') or rig.get('Tracking.Flags',0)&2==0:continue
  obj=objs.get(rig.get('Gun'),{});k=obj.get('values',{});visual=objs.get(rig.get('Visual'),{});v=visual.get('values',{})
  if obj.get('type')!='KFVRTrackedKnife':continue
  samples+=1
  required=('BladeBase','BladeTip','BladeLengthMetres','LastHitSwing','SwingSoundCount','ContactTraceCount','StockMeleeDamage','StockFireSpeed','StockFireInterval','LastDamage','bLastBackstab','bSwingSoundReady')
  if any(key not in k for key in required) or visual.get('type')!='KFVRWeaponVisual' or v.get('SourceWeapon')!=obj.get('address'):
   missing+=1;continue
  try:
   units=rig['Tracking.UnitsPerMetre'];length=k['BladeLengthMetres']
   expected_tip=[x+y for x,y in zip(v['Location'],rotate([x*v['DrawScale'] for x in v['TipLocal']],v['Rotation']))]
   axis=rotate(v['BladeAxis'],v['Rotation'])
   expected_base=[x-y*length*units for x,y in zip(expected_tip,axis)]
   span_errors.append(abs(distance(k['BladeBase'],k['BladeTip'])/units-length))
   base_errors.append(distance(k['BladeBase'],expected_base));tip_errors.append(distance(k['BladeTip'],expected_tip))
   hits=k['HitCount']+k['WorldHitCount'];swings=k['SwingCount'];token=k['LastHitSwing']
   current=(swings,hits,k['ContactTraceCount'],k['SwingSoundCount'])
   previous=last_counters.get(obj['address'],(0,0,0,0));last_counters[obj['address']]=current
   okay &= (all(type(x) is int and x>=old for x,old in zip(current,previous)) and 0<=hits<=swings
            and type(token) is int and 0<=token<=swings and (token>0 if hits>0 else token==0))
   tokens.append(token);trace_counts.append(k['ContactTraceCount']);sound_counts.append(k['SwingSoundCount'])
   rates &= finite(k['StockFireSpeed']) and k['StockFireSpeed']>0 and finite(k['StockFireInterval']) and k['StockFireInterval']>0
   if hits>0:
    expected_damage=k['StockMeleeDamage']*(2 if k['bLastBackstab'] else 1)
    damage.append(k['LastDamage']==expected_damage and k['StockMeleeDamage']>0)
  except (KeyError,TypeError,ValueError,ZeroDivisionError):missing+=1
 all_knives=[o['values'] for row in rows for o in row['objects'] if o['type']=='KFVRTrackedKnife' and not o['values'].get('bDeleteMe')]
 final=all_knives[-1] if all_knives else {}
 covered=samples>=10 and missing==0
 checks=dict(actual_blade_base_and_tip_match_installed_axis=covered and bool(span_errors) and max(span_errors)<.0001 and max(base_errors)<.005 and max(tip_errors)<.005,
  one_contact_dispatch_per_accepted_swing=covered and okay and len(tokens)>=10 and final.get('LastHitSwing')==final.get('SwingCount')==2,
  temporal_blade_traces_were_executed=covered and bool(trace_counts) and max(trace_counts)>=2,
  stock_base_damage_preserved_at_contact=covered and bool(damage) and all(damage),
  stock_perk_fire_interval_is_available=covered and rates,
  stock_swing_sound_once_per_accepted_strike=final.get('bSwingSoundReady') is True and final.get('SwingSoundCount')==final.get('SwingCount')==2)
 def maximum(values):return max(values) if values and all(finite(x) for x in values) else None
 return checks,dict(coherent_samples=samples,missing_fields_or_transforms=missing,max_blade_span_error_metres=maximum(span_errors),max_base_position_error_uu=maximum(base_errors),max_tip_position_error_uu=maximum(tip_errors),contact_trace_count=final.get('ContactTraceCount'),blocked_contacts=final.get('BlockedContacts'),cancelled_windows=final.get('CancelledWindows'),swing_sounds=final.get('SwingSoundCount'),stock_base_damage=final.get('StockMeleeDamage'),stock_fire_speed=final.get('StockFireSpeed'),stock_fire_interval=final.get('StockFireInterval'),wall_occlusion_exercised=False,zed_perks_tested=False)

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
 contact_checks,contact_metrics=blade_contact_evidence(controls,rows);checks.update(contact_checks)
 report=dict(blade_contact_evidence=contact_metrics,motion_clock_evidence=clock_metrics,passed=all(checks.values()),checks=checks,swings=end['SwingCount']-start['SwingCount'],contact_hits=targets[-1]['HitCount'],damage=targets[-1]['DamageReceived'],synthetic_controllers=True,live_game=True,headset_presented=False,collision_fixture=True,zed_perks_tested=False)
 (folder/'melee-validation.json').write_text(json.dumps(report,indent=2)+'\n')
 if not report['passed']:raise ValueError('Melee checks failed: '+', '.join(k for k,v in checks.items() if not v))
 return report
if __name__=='__main__':
 import sys
 print(json.dumps(validate(sys.argv[1]),indent=2))
