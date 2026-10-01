"""Assert live physical magazine, slide, stock-shot and bone behavior."""
import json,pathlib,math


def clock_evidence(controls,rows,native,gun_addresses):
 """Read-only clock evidence; absent new fields yield failed checks on old builds."""
 checks={
  'clock_events_published_neutral':native.get('source_clock_resets')==2 and native.get('last_clock_fault')==7 and native.get('failed')==0 and native.get('clock_fault_overflow')==0 and len(native.get('clock_faults',[]))==2 and all(e.get('published') and e.get('flags')==0 for e in native.get('clock_faults',[])),
  'clock_trial_preserved_reference_space':native.get('reference_space_resets')==0,
 }
 for phase_id,fault,label in [(19,6,'clock_rollback_forced_neutral_tick'),(36,7,'focus_gap_recovery_forced_neutral_tick')]:
  times=[c['uptime_ms'] for c in controls if c['sent'] and c['phase']==phase_id]
  events=[e for e in native.get('clock_faults',[]) if e.get('fault')==fault]
  checks[label]=len(events)==1 and bool(times) and min(times)<=events[0].get('uptime_ms',0)<=max(times) and events[0].get('published') and events[0].get('flags')==0
 fixtures=[o for r in rows for o in r['objects'] if o['type']=='KFVRReloadTimeTest' and not o['values'].get('bDeleteMe')]
 fixture_values=[o['values'] for o in fixtures];last=fixture_values[-1] if fixture_values else {}
 def finite(value):return isinstance(value,(float,int)) and math.isfinite(value)
 applied=[v for v in fixture_values if v.get('bInitialized') and v.get('bApplied') and v.get('AppliedCount',0)>0 and v.get('BoundGun') in gun_addresses]
 checks['time_fixture_applied']=bool(applied) and all(finite(v.get('SavedDilation')) and v['SavedDilation']>0 and finite(v.get('AppliedDilation')) and abs(v['AppliedDilation']-.2*v['SavedDilation'])<.00001 for v in applied)
 checks['time_fixture_restored']=len({o['address'] for o in fixtures})==1 and last.get('AppliedCount',0)>0 and last.get('RestoredCount')==last.get('AppliedCount') and not last.get('bApplied',True) and not last.get('bExpired',True) and finite(last.get('CurrentDilation')) and finite(last.get('SavedDilation')) and abs(last['CurrentDilation']-last['SavedDilation'])<.00001
 slopes={}
 for n in (12,13,14,15):
  times=[c['uptime_ms'] for c in controls if c['phase']==n and c['sent']];groups={}
  for row in rows:
   if not times or not min(times)+120<=row['uptime_ms']<=max(times) or not row.get('rig_sample_consistent'):continue
   objs={o['address']:o for o in row['objects'] if not o['values'].get('bDeleteMe')}
   rig_obj=next((o for o in objs.values() if o['type']=='KFVRWeaponRig'),None)
   if not rig_obj:continue
   rig=rig_obj['values']
   if rig.get('Gun') not in gun_addresses or not rig.get('bTracking') or rig.get('bHolstered') or rig.get('Tracking.Flags',0)&3!=3:continue
   fixture=next((o for o in objs.values() if o['type']=='KFVRReloadTimeTest' and o['values'].get('Rig')==rig_obj['address']),None)
   if not fixture:continue
   f=fixture['values'];source=rig.get('Tracking.InputSeconds');game=f.get('GameTime');dilation=f.get('AppliedDilation')
   if not f.get('bApplied') or f.get('BoundGun')!=rig.get('Gun') or f.get('BoundManual')!=rig.get('ManualReload') or not all(finite(v) for v in (source,game,dilation)):continue
   group=groups.setdefault((fixture['address'],f.get('AppliedCount')),[])
   if not group or source!=group[-1][0]:group.append((source,game,dilation))
  points=max(groups.values(),key=len,default=[]);span=points[-1][0]-points[0][0] if points else 0;slope=None
  monotonic=all(b[0]>a[0] for a,b in zip(points,points[1:]))
  if len(points)>=3 and span>=.3 and monotonic:
   xmean=sum(p[0] for p in points)/len(points);ymean=sum(p[1] for p in points)/len(points)
   slope=sum((x-xmean)*(y-ymean) for x,y,_ in points)/sum((x-xmean)**2 for x,_,_ in points)
  slopes[str(n)]=dict(samples=len(points),source_span_seconds=span,game_to_source_slope=slope,applied_dilation_min=min((p[2] for p in points),default=None),applied_dilation_max=max((p[2] for p in points),default=None))
  checks['unscaled_input_during_slow_motion_phase_'+str(n)]=slope is not None and .18<=slope<=.26 and all(.18<=p[2]<=.26 and abs(slope-p[2])<.04 for p in points)
 sent=[c for c in controls if c['sent']];stamped=[c for c in sent if isinstance(c.get('predicted_ns'),int)]
 rollbacks=[(a,b) for a,b in zip(stamped,stamped[1:]) if b['predicted_ns']<a['predicted_ns']]
 checks['sent_source_clock_rollback_measured']=len(stamped)==len(sent) and len(rollbacks)==1 and rollbacks[0][1]['phase']==19 and all(isinstance(c.get('source_ns'),int) for c in sent) and all(b['source_ns']>a['source_ns'] for a,b in zip(sent,sent[1:])) and rollbacks[0][1]['predicted_ns']-rollbacks[0][0]['predicted_ns']-(rollbacks[0][1]['source_ns']-rollbacks[0][0]['source_ns'])==-2000000000
 spaces={c.get('space_generation') for c in sent}
 checks['sent_clock_trial_used_one_space']=len(spaces)==1 and next(iter(spaces),0) not in (0,None)
 stroke=[c for c in sent if c['phase']==20]
 complete=[c for c in stroke if isinstance(c.get('predicted_ns'),int) and finite(c.get('reload_phase_ms')) and isinstance(c.get('left_grip_position'),list) and len(c['left_grip_position'])==3 and all(finite(v) for v in c['left_grip_position'])]
 hold=[c for c in complete if c['reload_phase_ms']<=400];dwell=[c for c in complete if c['reload_phase_ms']>=480]
 moving=[]
 for a,b in zip(complete,complete[1:]):
  distance=math.dist(a['left_grip_position'],b['left_grip_position'])
  if distance>.001:moving.append(dict(delta=(b['predicted_ns']-a['predicted_ns'])*1e-9,distance=distance,first_ms=a['reload_phase_ms'],last_ms=b['reload_phase_ms']))
 travel=math.dist(hold[-1]['left_grip_position'],dwell[0]['left_grip_position']) if hold and dwell else None
 duration=(moving[-1]['last_ms']-moving[0]['first_ms'])/1000 if moving else None
 stationary=bool(hold) and bool(dwell) and all(math.dist(c['left_grip_position'],hold[-1]['left_grip_position'])<.00001 for c in hold) and all(math.dist(c['left_grip_position'],dwell[0]['left_grip_position'])<.00001 for c in dwell)
 checks['fast_stroke_sent_with_hold_and_dwell']=len(complete)==len(stroke) and len(hold)>=3 and len(dwell)>=3 and stationary and any(400<c['reload_phase_ms']<480 for c in complete) and travel is not None and .095<=travel<=.105 and duration is not None and .04<=duration<=.16
 checks['fast_stroke_valid_in_source_time']=bool(moving) and all(0<p['delta']<=.15 and p['distance']<=4*p['delta']+.005 for p in moving)
 checks['fast_stroke_exposes_scaled_clock_bug']=any(p['delta']>0 and p['distance']>4*p['delta']*.22+.005 for p in moving)
 return checks,dict(native_source_clock_resets=native.get('source_clock_resets'),native_last_clock_fault=native.get('last_clock_fault'),fixture_samples=len(fixtures),fixture_final=last,slow_motion_slopes=slopes,sent_rollback_count=len(rollbacks),stroke_sent_samples=len(stroke),stroke_travel_metres=travel,stroke_duration_seconds=duration,moving_segments=moving)

def capacity_evidence(rows,gun_type):
 """Compare coherent live gun state with each magazine's recorded limit."""
 gun_addresses={o['address'] for row in rows for o in row['objects'] if o['type']==gun_type and not o['values'].get('bDeleteMe')}
 manual_addresses={o['address'] for row in rows for o in row['objects'] if o['type']=='KFVRManualReload' and not o['values'].get('bDeleteMe') and o['values'].get('Gun') in gun_addresses}
 slots=(('InsertedMagazine','InsertedCapacity'),('HeldMagazine','HeldCapacity'))
 counts={name:dict(absent=0,present=0,limits=set()) for name,_ in slots}
 samples=0;missing={};failures=[];supply_ok=identity_ok=loaded_ok=changes_ok=True;previous_changes={};supply_limits=set()
 def integer(value):return type(value) is int
 for line,row in enumerate(rows,1):
  if row.get('rig_sample_consistent') is not True:continue
  objs={o['address']:o for o in row['objects'] if not o['values'].get('bDeleteMe')}
  for address in manual_addresses:
   obj=objs.get(address)
   if not obj or obj['type']!='KFVRManualReload':continue
   m=obj['values'];gun=objs.get(m.get('Gun'),{});g=gun.get('values',{});samples+=1;bad=[]
   for key in ('CurrentCapacity','InsertedCapacity','HeldCapacity','CapacityChanges'):
    if key not in m:missing[key]=missing.get(key,0)+1
   current=m.get('CurrentCapacity');stock=g.get('MagCapacity')
   good=gun.get('type')==gun_type and integer(current) and 1<=current<=100 and integer(stock) and current==stock
   supply_ok &= good
   if not good:bad.append('supply')
   if integer(current):supply_limits.add(current)
   for name,limit_name in slots:
    identity=m.get(name);limit=m.get(limit_name)
    good=integer(identity) and identity>=0 and integer(limit) and (limit==0 if identity==0 else 1<=limit<=100)
    identity_ok &= good
    if not good:bad.append(limit_name)
    if integer(identity) and identity>=0:counts[name]['present' if identity else 'absent']+=1
    if integer(limit):counts[name]['limits'].add(limit)
   inserted=m.get('InsertedMagazine');limit=m.get('InsertedCapacity');loaded=m.get('LoadedRounds');chamber=m.get('bChambered')
   good=integer(inserted) and inserted>=0 and integer(limit) and (limit==0 if inserted==0 else 1<=limit<=100) and integer(loaded) and type(chamber) is bool and int(chamber)<=loaded<=(limit if inserted else 0)+int(chamber)
   loaded_ok &= good
   if not good:bad.append('loaded')
   changed=m.get('CapacityChanges');good=integer(changed) and changed>=previous_changes.get(address,0)
   changes_ok &= good
   if not good:bad.append('change_counter')
   if integer(changed):previous_changes[address]=changed
   if bad and len(failures)<8:failures.append(dict(line=line,uptime_ms=row['uptime_ms'],manual=address,fields=bad))
 coverage=samples>=10
 identity_coverage=all(c['absent']>=5 and c['present']>=5 for c in counts.values())
 checks=dict(supply_capacity_matches_live_gun=coverage and supply_ok,
             magazine_identities_have_recorded_capacities=coverage and identity_coverage and identity_ok,
             loaded_rounds_fit_recorded_inserted_capacity=coverage and loaded_ok,
             capacity_change_counter_is_valid=coverage and changes_ok)
 metrics=dict(coherent_samples=samples,required_samples=10,required_slot_state_samples=5,missing_fields=missing,
              supply_capacities=sorted(supply_limits),slots={name:dict(absent=c['absent'],present=c['present'],capacities=sorted(c['limits'])) for name,c in counts.items()},last_change_counters=previous_changes,failures=failures)
 return checks,metrics


def validate(folder,weapon="single",interrupted=False,insertion=False,clock=False):
 insertion=insertion or clock
 folder=pathlib.Path(folder)
 controls=[json.loads(x) for x in (folder/'controls.jsonl').read_text().splitlines()]
 rows=[json.loads(x) for x in (folder/'weapon-observations.jsonl').read_text().splitlines()]
 gun_type={'single':'KFVRTrackedSingle','deagle':'KFVRTrackedDeagle','mk23':'KFVRTrackedMK23'}[weapon]
 gun_addresses={o['address'] for row in rows for o in row['objects'] if o['type']==gun_type}
 def objects(typ,selected=rows):return [o['values'] for row in selected for o in row['objects'] if o['type']==typ and not o['values'].get('bDeleteMe') and (typ!='KFVRManualReload' or o['values']['Gun'] in gun_addresses)]
 def phase(n,typ='KFVRManualReload'):
  times=[c['uptime_ms'] for c in controls if c['phase']==n and c['sent']]
  return objects(typ,[r for r in rows if times and min(times)+(700 if interrupted and n==10 else 120)<=r['uptime_ms']<=max(times)])
 checks={};clock_metrics={};rigs=objects('KFVRWeaponRig');manuals=objects('KFVRManualReload');guns=objects(gun_type);capacity=guns[0]['MagCapacity']
 if not manuals:raise ValueError('Physical mode was never observed')
 start,end=manuals[0],manuals[-1];shots=rigs[-1]['TraceCount']-rigs[0]['TraceCount'];initial=start['OwnedRounds']+start['SpentRounds']+start['DiscardedRounds']
 def at(name,n,predicate,typ='KFVRManualReload'):
  data=phase(n,typ);checks[name]=bool(data) and all(predicate(x) for x in data)
 checks['every_sample_conserves_ammo']=all(m['bValid'] and m['OwnedRounds']+m['SpentRounds']+m['DiscardedRounds']==initial for m in manuals)
 capacity_checks,capacity_metrics=capacity_evidence(rows,gun_type);checks.update(capacity_checks)
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
  for n,label in [(12,'acquisition_at_seat'),(14,'teleport_to_seat'),(15,'partial_stroke'),(17,'dominant_tracking_return'),(19,'clock_rollback_inside_socket' if clock else 'recenter_inside_socket')]:
   at(label+'_cannot_insert',n,lambda m:m['HeldMagazine']==2 and m['InsertedMagazine']==0 and m['SeatCount']==0 and not m['bChambered'])
  at('outside_approach_observed',13,lambda m:m['bInsertionApproached'] and m['InsertionPosition'][2]<-.15)
  before_jump=phase(13);after_jump=phase(14)
  checks['teleport_exceeded_measured_speed_limit']=bool(before_jump) and bool(after_jump) and min(m['InsertionSpeedRejects'] for m in after_jump)>max(m['InsertionSpeedRejects'] for m in before_jump)
  partial=phase(15);checks['partial_push_advances_measured_progress']=any(.15<m['InsertionProgress']<.9 for m in partial)
  checks['dominant_tracking_loss_exercised']=any(r['Tracking.Flags']&3==1 for r in phase(16,'KFVRWeaponRig'))
  at('fresh_approach_after_tracking_loss',18,lambda m:m['bInsertionApproached'] and m['HeldMagazine']==2)
  summary=json.loads((folder/'trace-summary.json').read_text())
  native=json.loads((pathlib.Path(summary['report']).parent/'native-weapon.json').read_text())
  if clock:
   clock_checks,clock_metrics=clock_evidence(controls,rows,native,gun_addresses);checks.update(clock_checks)
  else:checks['reference_change_forced_neutral_tick']=native.get('reference_space_resets')==1 and native['failed']==0
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



 # Compare the rendered action bone against a model-space pull, independently
 # of the parent-bone translation basis used to apply the override in script.
 from validate_grip_pose import rotate
 action_metrics={}
 for n in ((23,24) if insertion else (17,23,24)):
  times=[c['uptime_ms'] for c in controls if c['phase']==n and c['sent']]
  action_errors=[];action_travel=[];missing_action_samples=0
  for row in rows:
   if not times or not min(times)+120<=row['uptime_ms']<=max(times) or not row.get('rig_sample_consistent'):continue
   objs={o['address']:o for o in row['objects'] if not o['values'].get('bDeleteMe')}
   rig=next((o['values'] for o in objs.values() if o['type']=='KFVRWeaponRig'),None)
   if not rig or rig['Gun'] not in gun_addresses or not rig['bTracking'] or rig['bHolstered'] or rig['Tracking.Flags']&3!=3:continue
   visual=objs.get(rig['Visual'],{}).get('values');manual=objs.get(rig['ManualReload'],{}).get('values')
   if not visual or visual['bHidden'] or not manual or manual['Gun']!=rig['Gun']:continue
   if 'MeasuredActionPosition' not in visual:
    missing_action_samples+=1;continue
   rest=[a+b for a,b in zip(visual['Location'],rotate([x*visual['DrawScale'] for x in visual['SlideLocal']],visual['Rotation']))]
   travel=manual['SlideTravel'];distance=rig['Tracking.UnitsPerMetre']*visual['ActionTravelMetres']*travel
   expected=[a-b*distance for a,b in zip(rest,rotate([1,0,0],visual['Rotation']))]
   action_errors.append(math.dist(expected,visual['MeasuredActionPosition']));action_travel.append(travel)
  action_metrics[str(n)]=dict(samples=len(action_errors),missing_samples=missing_action_samples,max_position_error_uu=max(action_errors,default=None),min_slide_travel=min(action_travel,default=None),max_slide_travel=max(action_travel,default=None))
  checks['actual_action_position_phase_'+str(n)]=len(action_errors)>=3 and missing_action_samples==0 and all(math.isfinite(error) and error<.05 for error in action_errors)
 # A magazine seated in the gun must retain its full bone frame when physical
 # mode is toggled off and the controller subsequently turns on several axes.
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
 report=dict(capacity_evidence=capacity_metrics,clock_trial=clock,clock_evidence=clock_metrics,max_socket_position_error_metres=max(socket_errors,default=None) if insertion else None,insertion_trial=insertion,interruption_trial=interrupted,inserted_magazine_frames=rest_metrics,action_bone_positions=action_metrics,missing_native_profiles=missing_native_profiles,native_magazine_grip_samples=native_grip_samples,max_magazine_to_palm_error_uu=max(mag_palm_errors,default=None),max_palm_tracking_error_uu=max(palm_tracking_errors,default=None),other_physical_weapons_observed=len(other_states),weapon=weapon,capacity=capacity,passed=all(checks.values()),checks=checks,shots=shots,initial_rounds=initial,remaining_rounds=end['OwnedRounds'],maximum_magazine_error_uu=max((m['MagazineError'] for m in held),default=-1),synthetic_controllers=True,live_game=True,headset_presented=False)
 (folder/'reload-validation.json').write_text(json.dumps(report,indent=2)+'\n')
 if not report['passed']:raise ValueError('Reload checks failed: '+', '.join(k for k,v in checks.items() if not v))
 return report
if __name__=='__main__':
 import argparse
 parser=argparse.ArgumentParser();parser.add_argument('folder');parser.add_argument('--weapon',choices=['single','deagle','mk23'],default='single');variant=parser.add_mutually_exclusive_group();variant.add_argument('--reload-interrupt',action='store_true');variant.add_argument('--reload-insertion',action='store_true');variant.add_argument('--reload-clock',action='store_true');args=parser.parse_args()
 print(json.dumps(validate(args.folder,args.weapon,interrupted=args.reload_interrupt,insertion=args.reload_insertion,clock=args.reload_clock),indent=2))
