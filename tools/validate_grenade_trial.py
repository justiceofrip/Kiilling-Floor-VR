"""Physical release gestures and actual installed grenade projectiles/damage."""
import json,pathlib,math

def validate(folder):
 folder=pathlib.Path(folder);rs=[json.loads(x) for x in (folder/'weapon-observations.jsonl').read_text().splitlines()];cs=[json.loads(x) for x in (folder/'controls.jsonl').read_text().splitlines()]
 def objects(typ,rows=rs):return [o['values'] for r in rows for o in r['objects'] if o['type']==typ]
 def phase(n,typ='KFVRGrenadeControl'):
  times=[c['uptime_ms'] for c in cs if c['phase']==n and c['sent']]
  return objects(typ,[r for r in rs if times and min(times)+850<=r['uptime_ms']<=max(times)])
 def last(n):
  d=phase(n);return d[-1] if d else {}
 gs=objects('KFVRGrenadeControl');rigs=objects('KFVRWeaponRig');targets=objects('KFVRMeleeTarget');checks={}
 checks['grenade_holster_draw']=bool(phase(2)) and any(g['AmmoCount']==5 and g['MotionSamples']>=3 for g in phase(2))
 checks['arm_before_release']=bool(phase(3)) and all(g['bPinRemoved'] and g['ThrowCount']==0 for g in phase(3))
 checks['first_physical_throw']=last(5).get('ThrowCount')==1 and last(5).get('AmmoAfter')==4 and last(5).get('ReleaseSpeed',0)>30
 checks['focus_cancels_armed_grenade']=last(12).get('ThrowCount')==1 and not last(12).get('bPinRemoved',True)
 checks['focus_release_does_not_throw']=last(13).get('ThrowCount')==1
 checks['movement_and_snap_do_not_throw']=last(19).get('ThrowCount')==1 and last(19).get('bPinRemoved')==True
 checks['stationary_drop_after_movement']=last(21).get('ThrowCount')==2 and last(21).get('ReleaseSpeed',999)<3
 checks['lost_hand_cancels']=last(28).get('ThrowCount')==2 and not last(28).get('bPinRemoved',True)
 checks['tracking_recovery_held_trigger_blocks']=last(29).get('ThrowCount')==2
 checks['tracking_jump_cancels']=last(33).get('ThrowCount')==2 and not last(33).get('bPinRemoved',True)
 checks['third_throw_after_rearm']=last(37).get('ThrowCount')==3 and last(37).get('ReleaseSpeed',0)>30
 checks['one_ammo_debit_per_throw']=bool(gs) and gs[-1]['ThrowCount']==3 and gs[-1]['AmmoAfter']==2 and all(g['AmmoAfter']==g['AmmoBefore']-1 for g in gs if g['ThrowCount']>0)
 checks['independent_stock_ammo']=bool(objects('FragAmmo')) and objects('FragAmmo')[0]['AmmoAmount']==5 and objects('FragAmmo')[-1]['AmmoAmount']==2
 checks['spawn_succeeded']=bool(gs) and gs[-1]['SpawnFailures']==0 and gs[-1]['BlockedCount']==0
 projectiles=[o for r in rs for o in r['objects'] if o['type'] in ['Nade','MedicNade','FlameNade']]
 checks['actual_stock_projectiles_observed']=len({o['address'] for o in projectiles})>=3
 checks['stock_fuse_observed']=bool(projectiles) and all(abs(o['values']['ExplodeTimer']-2)<0.001 for o in projectiles)
 checks['native_projectile_motion']=any(math.sqrt(sum(x*x for x in o['values']['Velocity']))>30 for o in projectiles)
 checks['three_stock_explosions']=len({o['address'] for o in projectiles if o['values'].get('bHasExploded')})==3
 checks['actual_explosion_damage']=bool(targets) and targets[-1]['HitCount']>=2 and targets[-1]['DamageReceived']>0
 before=phase(17,'KFHumanPawn');after=phase(19,'KFHumanPawn')
 checks['actual_locomotion_and_snap']=bool(before and after) and math.dist(before[-1]['Location'],after[-1]['Location'])>100 and before[-1]['Rotation'][1]!=after[-1]['Rotation'][1]
 checks['no_firearm_or_use_actions']=rigs[-1]['TraceCount']==rigs[0]['TraceCount'] and rigs[-1]['UseCount']==rigs[0]['UseCount']
 # Bracketed snapshots independently verify the live source-clock boundary.
 # Reset epochs may admit a new host's serials after clearing all motion history.
 motion_rows=[];motion_errors=[];motion_pairs=[];previous=None
 for row in rs:
  if not row.get('rig_sample_consistent'):continue
  objs={o['address']:o for o in row['objects'] if not o['values'].get('bDeleteMe')}
  active=next((o for o in objs.values() if o['type']=='KFVRWeaponRig'),None)
  if not active:continue
  r=active['values']
  if not r['bTracking'] or r['bHolstered'] or not r['Tracking.Flags']&2:continue
  selected=[o for o in objs.values() if o['type']=='KFVRGrenadeControl'
            and o['values']['Rig']==active['address'] and o['values']['GrenadeWeapon']==r['Gun']]
  if len(selected)!=1:continue
  obj=selected[0];g=obj['values']
  if g['MotionSamples']<=0 or g['LastSerial']<=0:continue
  source=r['Tracking.InputSeconds'];sample=g['MotionSourceSeconds']
  motion_errors.append(abs(source-sample) if math.isfinite(source) and math.isfinite(sample) else math.inf)
  motion_rows.append((g['LastSerial'],r['Tracking.InputSerial']))
  current=(obj['address'],g['MotionResets'],g['LastSerial'],g['MotionUpdates'])
  if previous and current[:2]==previous[:2]:
   motion_pairs.append((current[2]-previous[2],current[3]-previous[3]))
  previous=current
 checks['grenade_motion_uses_controller_time']=len(motion_rows)>=10 and max(motion_errors,default=math.inf)<0.00001 and all(a==b for a,b in motion_rows)
 checks['grenade_motion_consumes_each_source_at_most_once']=len(motion_pairs)>=10 and all(0<=updates<=samples for samples,updates in motion_pairs) and any(updates>0 for _,updates in motion_pairs)
 checks['clean_release']=bool(gs) and not gs[-1]['bPinRemoved'] and not rigs[-1]['bTracking'] and all(o['values']['bHidden'] for o in rs[-1]['objects'] if o['type'] in ['KFVRHandVisual','KFVRWeaponVisual'])
 report=dict(passed=all(checks.values()),checks=checks,grenade=gs[-1] if gs else {},damage=targets[-1] if targets else {},motion_clock=dict(samples=len(motion_rows),successive_pairs=len(motion_pairs),max_source_time_error_seconds=max(motion_errors,default=None)),live_game=True,synthetic_controllers=True,headset_presented=False)
 (folder/'grenade-validation.json').write_text(json.dumps(report,indent=2)+'\n')
 if not report['passed']:raise ValueError('Grenade checks failed: '+', '.join(k for k,v in checks.items() if not v))
 return report
if __name__=='__main__':
 import sys
 print(json.dumps(validate(sys.argv[1]),indent=2))
