"""Assert live physical magazine, slide, stock-shot and bone behavior."""
import json,pathlib,math

def validate(folder,weapon="single"):
 folder=pathlib.Path(folder)
 controls=[json.loads(x) for x in (folder/'controls.jsonl').read_text().splitlines()]
 rows=[json.loads(x) for x in (folder/'weapon-observations.jsonl').read_text().splitlines()]
 gun_type={'single':'KFVRTrackedSingle','deagle':'KFVRTrackedDeagle'}[weapon]
 gun_addresses={o['address'] for row in rows for o in row['objects'] if o['type']==gun_type}
 def objects(typ,selected=rows):return [o['values'] for row in selected for o in row['objects'] if o['type']==typ and (typ!='KFVRManualReload' or o['values']['Gun'] in gun_addresses)]
 def phase(n,typ='KFVRManualReload'):
  times=[c['uptime_ms'] for c in controls if c['phase']==n and c['sent']]
  return objects(typ,[r for r in rows if times and min(times)+120<=r['uptime_ms']<=max(times)])
 checks={};rigs=objects('KFVRWeaponRig');manuals=objects('KFVRManualReload');guns=objects(gun_type);capacity=guns[0]['MagCapacity']
 if not manuals:raise ValueError('Physical mode was never observed')
 start,end=manuals[0],manuals[-1];shots=rigs[-1]['TraceCount']-rigs[0]['TraceCount'];initial=start['OwnedRounds']+start['SpentRounds']+start['DiscardedRounds']
 def at(name,n,predicate,typ='KFVRManualReload'):
  data=phase(n,typ);checks[name]=bool(data) and all(predicate(x) for x in data)
 checks['every_sample_conserves_ammo']=all(m['bValid'] and m['OwnedRounds']+m['SpentRounds']+m['DiscardedRounds']==initial for m in manuals)
 at('catch_keeps_chamber',5,lambda m:m['HeldMagazine']==1 and m['InsertedMagazine']==0 and m['LoadedRounds']==1 and m['bChambered'])
 at('retained_chamber_fires_once',7,lambda m:m['SpentRounds']==1 and m['LoadedRounds']==0 and not m['bChambered'])
 at('empty_trigger_cannot_fire',8,lambda m:m['SpentRounds']==1)
 at('partial_magazine_stowed',10,lambda m:m['StowCount']==1 and m['HeldMagazine']==0)
 at('replacement_has_new_identity',12,lambda m:m['HeldMagazine']==2 and m['GrabCount']==1)
 at('insertion_does_not_feed',14,lambda m:m['InsertedMagazine']==2 and not m['bChambered'] and m['SpentRounds']==1)
 at('partial_slide_pull_does_not_feed',19,lambda m:m['RackCount']==0 and not m['bChambered'] and m['SpentRounds']==1)
 at('slide_open_blocks_fire',23,lambda m:m['SlideTravel']>.9 and not m['bChambered'] and m['SpentRounds']==1)
 at('full_return_feeds',24,lambda m:m['RackCount']==1 and m['bChambered'] and m['LoadedRounds']==capacity)
 at('stock_shot_consumes_chamber_and_feeds',26,lambda m:m['SpentRounds']==2 and m['LoadedRounds']==capacity-1 and m['bChambered'])
 at('tactical_reload_retains_plus_one',33,lambda m:m['LoadedRounds']==capacity+1 and m['bChambered'] and m['InsertedMagazine']==3)
 at('focus_loss_releases',35,lambda r:not r['bTracking'] and not r['bFiring'],'KFVRWeaponRig')
 at('held_trigger_needs_neutral',36,lambda m:m['SpentRounds']==3)
 checks['five_actual_stock_shots']=shots==5 and end['SpentRounds']==5 and guns[-1]['VRFireAttempts']-guns[0]['VRFireAttempts']==5
 checks['no_stock_timed_reload']=not any(g['bIsReloading'] for g in guns)
 held=[m for n in [5,6,7,11,12,28,29,31,32] for m in phase(n) if m['HeldMagazine']]
 checks['magazine_follows_offhand']=bool(held) and max(m['MagazineError'] for m in held)<.05
 rotated=phase(5)+phase(6)+phase(7)
 checks['magazine_rotates_with_offhand']=bool(rotated) and min(m['MagazineDirectionDot'] for m in rotated)>.995
 checks['two_reload_exchanges_plus_retained_switch']=end['EjectCount']==end['StowCount']==3 and end['GrabCount']==end['SeatCount']==2 and end['DropCount']==0
 at('switch_retains_held_magazine',44,lambda m:m['HeldMagazine']==0 and m['InsertedMagazine']==0 and m['StowCount']==3 and m['bChambered'])
 at('return_draw_retains_chamber',48,lambda m:m['bChambered'] and m['InsertedMagazine']==0 and m['LoadedRounds']==1)
 at('chamber_usable_after_switch',50,lambda m:m['SpentRounds']==5 and m['LoadedRounds']==0)
 other_states={}
 for row in rows:
  for obj in row['objects']:
   if obj['type']=='KFVRManualReload' and obj['values']['Gun'] not in gun_addresses:
    m=obj['values'];other_states.setdefault(m['Gun'],[]).append(tuple(m[k] for k in ['OwnedRounds','LoadedRounds','SpentRounds','DiscardedRounds','HeldMagazine','InsertedMagazine','Revision']))
 checks['other_physical_weapon_states_unchanged']=all(len(set(states))==1 for states in other_states.values())
 checks['input_released_at_stop']=not rigs[-1]['bTracking'] and not rigs[-1]['bFiring']
 # Independent actual magazine-bone to actual hand-palm comparison, including
 # sharp transitions. Only completed, coherent rig updates can establish this.
 native=any(r.get('bNativeHandTest') for r in rigs);mag_palm_errors=[];palm_tracking_errors=[];native_grip_samples=0;missing_native_profiles=0
 if native:
  from validate_grip_pose import rotate
  for row in rows:
   if not row.get('rig_sample_consistent'):continue
   objs={o['address']:o for o in row['objects'] if not o['values'].get('bDeleteMe')}
   rig=next((o['values'] for o in objs.values() if o['type']=='KFVRWeaponRig'),None)
   if not rig or not rig['bTracking'] or rig['bHolstered'] or rig['Tracking.Flags']&3!=3:continue
   hand=next((o['values'] for o in objs.values() if o['type']=='KFVRHandVisual' and o['values']['bLeftHand']),None)
   if not hand or hand['bHidden']:continue
   for obj in objs.values():
    m=obj['values']
    if obj['type']!='KFVRManualReload' or m['Gun'] not in gun_addresses or m['Gun']!=rig['Gun'] or not m['HeldMagazine']:continue
    native_grip_samples+=1
    profile=objs.get(hand.get('GripVisual'),{})
    if profile.get('type')!='KFVRMagazineGrip' or not hand['bGripPose']:
     missing_native_profiles+=1;continue
    g=profile['values']
    delta=rotate([x*g['DrawScale'] for x in g['GripPose.0.Origin']],g['Rotation'])
    expected=[x+y for x,y in zip(m['MagazineWorld'],delta)]
    mag_palm_errors.append(math.dist(expected,hand['PalmWorld']))
    palm_tracking_errors.append(math.dist(hand['PalmWorld'],rig['Tracking.LeftPosition']))
  checks['native_magazine_grasp_is_measured_and_attached']=native_grip_samples>=5 and not missing_native_profiles and bool(mag_palm_errors) and max(mag_palm_errors)<.05
  checks['magazine_grasp_keeps_controller_palm']=bool(palm_tracking_errors) and max(palm_tracking_errors)<.005
 report=dict(missing_native_profiles=missing_native_profiles,native_magazine_grip_samples=native_grip_samples,max_magazine_to_palm_error_uu=max(mag_palm_errors,default=None),max_palm_tracking_error_uu=max(palm_tracking_errors,default=None),other_physical_weapons_observed=len(other_states),weapon=weapon,capacity=capacity,passed=all(checks.values()),checks=checks,shots=shots,initial_rounds=initial,remaining_rounds=end['OwnedRounds'],maximum_magazine_error_uu=max((m['MagazineError'] for m in held),default=-1),synthetic_controllers=True,live_game=True,headset_presented=False)
 (folder/'reload-validation.json').write_text(json.dumps(report,indent=2)+'\n')
 if not report['passed']:raise ValueError('Reload checks failed: '+', '.join(k for k,v in checks.items() if not v))
 return report
if __name__=='__main__':
 import argparse
 parser=argparse.ArgumentParser();parser.add_argument('folder');parser.add_argument('--weapon',choices=['single','deagle'],default='single');args=parser.parse_args()
 print(json.dumps(validate(args.folder,args.weapon),indent=2))
