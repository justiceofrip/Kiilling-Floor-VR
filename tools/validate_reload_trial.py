"""Assert live physical magazine, slide, stock-shot and bone behavior."""
import json,pathlib,math

def validate(folder,weapon="single",interrupted=False,insertion=False):
 folder=pathlib.Path(folder)
 controls=[json.loads(x) for x in (folder/'controls.jsonl').read_text().splitlines()]
 rows=[json.loads(x) for x in (folder/'weapon-observations.jsonl').read_text().splitlines()]
 gun_type={'single':'KFVRTrackedSingle','deagle':'KFVRTrackedDeagle'}[weapon]
 gun_addresses={o['address'] for row in rows for o in row['objects'] if o['type']==gun_type}
 def objects(typ,selected=rows):return [o['values'] for row in selected for o in row['objects'] if o['type']==typ and not o['values'].get('bDeleteMe') and (typ!='KFVRManualReload' or o['values']['Gun'] in gun_addresses)]
 def phase(n,typ='KFVRManualReload'):
  times=[c['uptime_ms'] for c in controls if c['phase']==n and c['sent']]
  return objects(typ,[r for r in rows if times and min(times)+(700 if interrupted and n==10 else 120)<=r['uptime_ms']<=max(times)])
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
 at('replacement_has_new_identity',11,lambda m:m['HeldMagazine']==2 and m['GrabCount']==1)
 at('insertion_does_not_feed',21 if insertion else 14,lambda m:m['InsertedMagazine']==2 and not m['bChambered'] and m['SpentRounds']==1)
 if not insertion:at('partial_slide_pull_does_not_feed',19,lambda m:m['RackCount']==0 and not m['bChambered'] and m['SpentRounds']==1)
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
   if obj['type']=='KFVRManualReload' and not obj['values'].get('bDeleteMe') and obj['values']['Gun'] not in gun_addresses:
    m=obj['values'];other_states.setdefault(m['Gun'],[]).append(tuple(m[k] for k in ['OwnedRounds','LoadedRounds','SpentRounds','DiscardedRounds','HeldMagazine','InsertedMagazine','Revision']))
 checks['other_physical_weapon_states_unchanged']=all(len(set(states))==1 for states in other_states.values())
 checks['input_released_at_stop']=not rigs[-1]['bTracking'] and not rigs[-1]['bFiring']
 if interrupted:
  checks['offhand_loss_exercised']=any(r['Tracking.Flags']&3==2 for r in phase(5,'KFVRWeaponRig'))
  times=[c['uptime_ms'] for c in controls if c['phase']==10 and c['sent']]
  checks['release_focus_loss_exercised']=any(not o['values']['bTracking'] for row in rows if times and min(times)<=row['uptime_ms']<min(times)+500 for o in row['objects'] if o['type']=='KFVRWeaponRig')
  at('recovery_never_auto_inserts_or_chambers',10,lambda m:m['InsertedMagazine']==0 and m['SeatCount']==0 and not m['bChambered'] and m['HeldMagazine']==0)
  at('interrupted_open_hand_retains_magazine_once',10,lambda m:m.get('RecoveryStowCount')==1 and m.get('HeldInterruptions',0)>=2 and not m.get('bRecoveryPending',True))

 if insertion:
  for n,label in [(12,'acquisition_at_seat'),(14,'teleport_to_seat'),(15,'partial_stroke'),(17,'dominant_tracking_return'),(19,'recenter_inside_socket')]:
   at(label+'_cannot_insert',n,lambda m:m['HeldMagazine']==2 and m['InsertedMagazine']==0 and m['SeatCount']==0 and not m['bChambered'])
  at('outside_approach_observed',13,lambda m:m['bInsertionApproached'] and m['InsertionPosition'][2]<-.15)
  before_jump=phase(13);after_jump=phase(14)
  checks['teleport_exceeded_measured_speed_limit']=bool(before_jump) and bool(after_jump) and min(m['InsertionSpeedRejects'] for m in after_jump)>max(m['InsertionSpeedRejects'] for m in before_jump)
  partial=phase(15);checks['partial_push_advances_measured_progress']=any(.15<m['InsertionProgress']<.9 for m in partial)
  checks['dominant_tracking_loss_exercised']=any(r['Tracking.Flags']&3==1 for r in phase(16,'KFVRWeaponRig'))
  at('fresh_approach_after_tracking_loss',18,lambda m:m['bInsertionApproached'] and m['HeldMagazine']==2)
  summary=json.loads((folder/'trace-summary.json').read_text())
  native=json.loads((pathlib.Path(summary['report']).parent/'native-weapon.json').read_text())
  checks['reference_change_forced_neutral_tick']=native.get('reference_space_resets')==1 and native['failed']==0
  delivered=native['packets']
  checks['fresh_controller_sequence_is_distinct_from_ticks']=0<native['input_samples']<native['updates'] and any(a['input_serial']>0 and a['input_serial']==b['input_serial'] and a['serial']!=b['serial'] for a,b in zip(delivered,delivered[1:]))
  at('complete_push_seats_while_grip_stays_held',21,lambda m:m['InsertedMagazine']==2 and m['HeldMagazine']==0 and m['SeatCount']==1)
  times=[c['uptime_ms'] for c in controls if c['phase']==20 and c['sent']]
  seated_held=[]
  for row in rows:
   if not times or not min(times)<=row['uptime_ms']<=max(times) or not row.get('rig_sample_consistent'):continue
   objs={o['address']:o for o in row['objects'] if not o['values'].get('bDeleteMe')}
   rig=next((o['values'] for o in objs.values() if o['type']=='KFVRWeaponRig'),None)
   manual=objs.get(rig.get('ManualReload'),{}).get('values') if rig else None
   if manual:seated_held.append(manual['InsertedMagazine']==2 and manual['SeatCount']==1 and rig['Tracking.LeftGrip']>.7)
  checks['seat_event_precedes_grip_release']=any(seated_held)
  # Independently reconstruct socket depth from the actual rendered magazine
  # root and actual weapon transform, not the gate's progress/depth claims.
  from validate_grip_pose import rotate
  socket_errors=[];depths={};tilted=[]
  for n in (12,13,14,15,17,18,19,20):
   times=[c['uptime_ms'] for c in controls if c['phase']==n and c['sent']]
   depths[n]=[]
   for row in rows:
    if not times or not min(times)+120<=row['uptime_ms']<=max(times) or not row.get('rig_sample_consistent'):continue
    objs={o['address']:o for o in row['objects'] if not o['values'].get('bDeleteMe')}
    rig=next((o['values'] for o in objs.values() if o['type']=='KFVRWeaponRig'),None)
    if not rig or not rig['bTracking'] or rig['Tracking.Flags']&3!=3:continue
    visual=objs.get(rig['Visual'],{}).get('values');manual=objs.get(rig['ManualReload'],{}).get('values')
    if not visual or not manual or not manual['HeldMagazine'] or visual['bHidden']:continue
    rest=[a+b for a,b in zip(visual['Location'],rotate([x*visual['DrawScale'] for x in visual['MagazineLocal']],visual['Rotation']))]
    delta=[(a-b)/rig['Tracking.UnitsPerMetre'] for a,b in zip(visual['MeasuredMagazinePosition'],rest)]
    frame=[rotate(visual[k],visual['Rotation']) for k in ('InsertionX','InsertionY','InsertionAxis')]
    position=[sum(a*b for a,b in zip(delta,axis)) for axis in frame]
    socket_errors.append(math.dist(position,manual['InsertionPosition']));depths[n].append(position[2])
    tilted.append(abs(visual['Rotation'][0])>1000 and abs(visual['Rotation'][2])>1000)
  checks['actual_magazine_socket_travel_matches']=len(socket_errors)>=15 and max(socket_errors)<.001
  checks['tilted_weapon_stroke_exercised']=bool(tilted) and all(tilted)
  checks['actual_outside_approach_reached']=bool(depths[13]) and max(depths[13])<-.18
  checks['actual_socket_reentry_reached']=bool(depths[14]) and bool(depths[19]) and max(abs(x) for x in depths[14])<.005 and max(depths[19])>-.01



 # A magazine seated in the gun must retain its full bone frame when physical
 # mode is toggled off and the controller subsequently turns on several axes.
 from validate_grip_pose import rotate
 rest_metrics={}
 for n in (53,55,57,58,59):
  times=[c['uptime_ms'] for c in controls if c['phase']==n and c['sent']]
  position_errors=[];axis_errors=[]
  for row in rows:
   if not times or not min(times)+120<=row['uptime_ms']<=max(times) or not row.get('rig_sample_consistent'):continue
   objs={o['address']:o for o in row['objects'] if not o['values'].get('bDeleteMe')}
   rig=next((o['values'] for o in objs.values() if o['type']=='KFVRWeaponRig'),None)
   if not rig or rig['Gun'] not in gun_addresses or not rig['bTracking'] or rig['bHolstered']:continue
   visual=objs.get(rig['Visual'],{}).get('values')
   if not visual:continue
   expected=[a+b for a,b in zip(visual['Location'],rotate([x*visual['DrawScale'] for x in visual['MagazineLocal']],visual['Rotation']))]
   position_errors.append(math.dist(expected,visual['MeasuredMagazinePosition']))
   for axis in ('X','Y','Z'):
    axis_errors.append(math.dist(rotate(visual['Magazine'+axis],visual['Rotation']),visual['MeasuredMagazine'+axis]))
  rest_metrics[str(n)]=dict(samples=len(position_errors),max_position_error_uu=max(position_errors,default=None),max_axis_error=max(axis_errors,default=None))
  checks['inserted_magazine_frame_phase_'+str(n)]=len(position_errors)>=3 and max(position_errors)<.05 and max(axis_errors)<.002
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
 report=dict(max_socket_position_error_metres=max(socket_errors,default=None) if insertion else None,insertion_trial=insertion,interruption_trial=interrupted,inserted_magazine_frames=rest_metrics,missing_native_profiles=missing_native_profiles,native_magazine_grip_samples=native_grip_samples,max_magazine_to_palm_error_uu=max(mag_palm_errors,default=None),max_palm_tracking_error_uu=max(palm_tracking_errors,default=None),other_physical_weapons_observed=len(other_states),weapon=weapon,capacity=capacity,passed=all(checks.values()),checks=checks,shots=shots,initial_rounds=initial,remaining_rounds=end['OwnedRounds'],maximum_magazine_error_uu=max((m['MagazineError'] for m in held),default=-1),synthetic_controllers=True,live_game=True,headset_presented=False)
 (folder/'reload-validation.json').write_text(json.dumps(report,indent=2)+'\n')
 if not report['passed']:raise ValueError('Reload checks failed: '+', '.join(k for k,v in checks.items() if not v))
 return report
if __name__=='__main__':
 import argparse
 parser=argparse.ArgumentParser();parser.add_argument('folder');parser.add_argument('--weapon',choices=['single','deagle'],default='single');args=parser.parse_args()
 print(json.dumps(validate(args.folder,args.weapon),indent=2))
