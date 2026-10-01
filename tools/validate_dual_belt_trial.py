"""Read-only acceptance of physical dual magazines in the actual KF match."""
import json,pathlib

def capacity_evidence(rows):
 """Validate per-hand limits without treating retained magazines as new supply."""
 slots=(('RightInserted','RightInsertedCapacity'),('LeftInserted','LeftInsertedCapacity'),
        ('RightPresented','RightPresentedCapacity'),('LeftPresented','LeftPresentedCapacity'))
 counts={name:dict(absent=0,present=0,limits=set()) for name,_ in slots}
 samples=0;missing={};failures=[];supply_ok=identity_ok=loaded_ok=changes_ok=True;previous_changes={};supply_limits=set()
 def integer(value):return type(value) is int
 for line,row in enumerate(rows,1):
  if row.get('rig_sample_consistent') is not True:continue
  objs={o['address']:o for o in row['objects'] if not o['values'].get('bDeleteMe')}
  for address,obj in objs.items():
   if obj['type']!='KFVRDualBeltReload':continue
   b=obj['values'];gun=objs.get(b.get('Gun'),{});g=gun.get('values',{});samples+=1;bad=[]
   for key in ('CurrentCapacity','RightInsertedCapacity','LeftInsertedCapacity','RightPresentedCapacity','LeftPresentedCapacity','CapacityChanges'):
    if key not in b:missing[key]=missing.get(key,0)+1
   current=b.get('CurrentCapacity');stock=g.get('MagCapacity')
   good=gun.get('type')=='KFVRTrackedDualies' and integer(current) and 1<=current<=100 and integer(stock) and stock%2==0 and current==stock//2
   supply_ok &= good
   if not good:bad.append('supply')
   if integer(current):supply_limits.add(current)
   for name,limit_name in slots:
    identity=b.get(name);limit=b.get(limit_name)
    good=integer(identity) and identity>=0 and integer(limit) and (limit==0 if identity==0 else 1<=limit<=100)
    identity_ok &= good
    if not good:bad.append(limit_name)
    if integer(identity) and identity>=0:counts[name]['present' if identity else 'absent']+=1
    if integer(limit):counts[name]['limits'].add(limit)
   for side in ('Right','Left'):
    inserted=b.get(side+'Inserted');limit=b.get(side+'InsertedCapacity');loaded=b.get(side+'Loaded');chamber=b.get('b'+side+'Chamber')
    good=integer(inserted) and inserted>=0 and integer(limit) and (limit==0 if inserted==0 else 1<=limit<=100) and integer(loaded) and type(chamber) is bool and int(chamber)<=loaded<=(limit if inserted else 0)+int(chamber)
    loaded_ok &= good
    if not good:bad.append(side+'Loaded')
   changed=b.get('CapacityChanges');good=integer(changed) and changed>=previous_changes.get(address,0)
   changes_ok &= good
   if not good:bad.append('change_counter')
   if integer(changed):previous_changes[address]=changed
   if bad and len(failures)<8:failures.append(dict(line=line,uptime_ms=row['uptime_ms'],belt=address,fields=bad))
 coverage=samples>=10
 identity_coverage=all(c['absent']>=5 and c['present']>=5 for c in counts.values())
 checks=dict(supply_capacity_matches_live_dual_gun=coverage and supply_ok,
             magazine_identities_have_recorded_capacities=coverage and identity_coverage and identity_ok,
             loaded_rounds_fit_recorded_inserted_capacities=coverage and loaded_ok,
             capacity_change_counter_is_valid=coverage and changes_ok)
 metrics=dict(coherent_samples=samples,required_samples=10,required_slot_state_samples=5,missing_fields=missing,
              supply_capacities=sorted(supply_limits),slots={name:dict(absent=c['absent'],present=c['present'],capacities=sorted(c['limits'])) for name,c in counts.items()},last_change_counters=previous_changes,failures=failures)
 return checks,metrics


def validate(folder):
 folder=pathlib.Path(folder);rows=[json.loads(x) for x in (folder/'weapon-observations.jsonl').read_text().splitlines()];controls=[json.loads(x) for x in (folder/'controls.jsonl').read_text().splitlines()]
 def objects(typ,selected=rows):return [o['values'] for r in selected for o in r['objects'] if o['type']==typ and not o['values'].get('bDeleteMe')]
 def phase(n,typ='KFVRDualBeltReload'):
  times=[c['uptime_ms'] for c in controls if c['phase']==n and c['sent']]
  return objects(typ,[r for r in rows if times and min(times)+250<=r['uptime_ms']<=max(times)])
 belts=objects('KFVRDualBeltReload');guns=objects('KFVRTrackedDualies');rigs=objects('KFVRWeaponRig');checks={}
 if not belts:raise ValueError('Dual physical belt mode was never observed')
 start,end=guns[0],guns[-1];r0=start['RightShots'];l0=start['LeftShots'];budget=belts[0]['OwnedRounds']+belts[0]['SpentRounds']
 def at(name,n,predicate,typ='KFVRDualBeltReload'):
  data=phase(n,typ);checks[name]=bool(data) and all(predicate(x) for x in data)
 checks['every_physical_sample_conserves_ammo']=all(b['bValid'] and b['OwnedRounds']+b['SpentRounds']==budget for b in belts)
 capacity_checks,capacity_metrics=capacity_evidence(rows);checks.update(capacity_checks)
 at('forward_ejection_rejected',6,lambda b:b['EjectCount']==0 and b['RightInserted']>0 and b['LeftInserted']>0)
 at('downward_ejection_preserves_chambers',8,lambda b:b['EjectCount']==2 and b['RightLoaded']==b['LeftLoaded']==1 and b['RightInserted']==b['LeftInserted']==0)
 at('two_distinct_belt_magazines',9,lambda b:b['RightPresented']>0 and b['LeftPresented']>0 and b['RightPresented']!=b['LeftPresented'])
 at('tactical_seating_retains_both_plus_ones',11,lambda b:b['RightLoaded']==b['LeftLoaded']==16 and b['bRightLatched'] and b['bLeftLatched'] and b['SpentRounds']==0)
 at('right_can_fire_while_left_latched',13,lambda b:b['SpentRounds']==1 and b['RightLoaded']==15 and b['LeftLoaded']==16 and not b['bRightLatched'] and b['bLeftLatched'])
 at('both_fire_after_withdrawal',16,lambda b:b['SpentRounds']==3 and b['RightLoaded']==14 and b['LeftLoaded']==15 and not b['bRightLatched'] and not b['bLeftLatched'])
 at('retained_chambers_fire_without_magazines',19,lambda b:b['SpentRounds']==5 and b['RightLoaded']==b['LeftLoaded']==0)
 at('dry_triggers_cannot_borrow_belt_supply',21,lambda b:b['SpentRounds']==5)
 at('empty_reload_slide_assists_both',24,lambda b:b['AssistCount']==2 and b['RightLoaded']==b['LeftLoaded']==15 and b['bRightLatched'] and b['bLeftLatched'] and b['SpentRounds']==5)
 at('independent_latch_blocks_only_left',26,lambda b:b['SpentRounds']==6 and b['RightLoaded']==14 and b['LeftLoaded']==15 and b['bLeftLatched'])
 at('both_empty_reloads_fire',29,lambda b:b['SpentRounds']==8 and b['RightLoaded']==13 and b['LeftLoaded']==14)
 at('focus_loss_releases',32,lambda r:not r['bTracking'] and not r['bFiring'],'KFVRWeaponRig')
 at('focus_return_inside_cannot_seat',34,lambda b:b['SeatCount']==4 and b['RightPresented']>0 and b['LeftPresented']>0)
 at('left_tracking_return_inside_cannot_seat',37,lambda b:b['SeatCount']==5 and b['RightInserted']>0 and b['LeftInserted']==0 and b['LeftPresented']>0)
 at('fresh_left_approach_seats',40,lambda b:b['SeatCount']==6 and not b['bRightLatched'] and b['bLeftLatched'])
 at('recovered_pair_fires',43,lambda b:b['SpentRounds']==10)
 at('holster_retains_presented_magazines',47,lambda b:b['StowCount']==1 and b['RightPresented']==b['LeftPresented']==0 and b['SpentRounds']==10)
 at('redraw_preserves_chambers',50,lambda b:b['RightLoaded']==b['LeftLoaded']==1 and b['SpentRounds']==10)
 at('reload_after_holster_seats_both',54,lambda b:b['SeatCount']==8 and b['PresentCount']==10 and b['EjectCount']==8 and b['SpentRounds']==10)
 at('completed_physical_cycle',57,lambda b:b['SpentRounds']==12 and b['RightLoaded']==b['LeftLoaded']==15 and b['ClearCount']==8 and b['AssistCount']==2)
 at('mode_exit_preserves_stock_ammo',60,lambda g:g['Belt']=='0x0' and g['RightLoaded']==g['LeftLoaded']==14,'KFVRTrackedDualies')
 checks['fourteen_actual_stock_shots']=end['RightShots']-r0==8 and end['LeftShots']-l0==6 and rigs[-1]['TraceCount']-rigs[0]['TraceCount']==14
 checks['stock_total_decrements_once_per_shot']=start['VRTotalAmmo']-end['VRTotalAmmo']==14
 checks['no_timed_reload_during_physical_sequence']=not any(g['bIsReloading'] for g in guns)
 displayed=[b for n in [8,9,31,34,35,52] for b in phase(n) if b['RightPresented'] and b['LeftPresented']]
 checks['actual_magazine_bones_follow_belt']=bool(displayed) and max(max(b['RightMagazineError'],b['LeftMagazineError']) for b in displayed)<.05
 checks['stop_releases_both']=not end['bLeftFiring'] and not end['bRightFiring'] and not rigs[-1]['bTracking']
 checks['stop_hides_presentation']=all(o['values']['bHidden'] for o in rows[-1]['objects'] if o['type'] in ['KFVRWeaponVisual','KFVRHandVisual'] and not o['values'].get('bDeleteMe'))
 receiver=json.loads((folder/'receiver.json').read_text())
 report=dict(capacity_evidence=capacity_metrics,passed=all(checks.values()),checks=checks,right_shots=end['RightShots']-r0,left_shots=end['LeftShots']-l0,ammo_used=start['VRTotalAmmo']-end['VRTotalAmmo'],maximum_belt_magazine_error_uu=max((max(b['RightMagazineError'],b['LeftMagazineError']) for b in displayed),default=-1),haptic_pulses=receiver['haptic_pulses'],live_game=True,synthetic_controllers=True,headset_presented=False)
 (folder/'dual-belt-validation.json').write_text(json.dumps(report,indent=2)+'\n')
 if not report['passed']:raise ValueError('Dual belt checks failed: '+', '.join(k for k,v in checks.items() if not v))
 return report
if __name__=='__main__':
 import sys
 print(json.dumps(validate(sys.argv[1]),indent=2))
