"""Live three-weapon switching, timed reload and focus-loss assertions."""
import json,math,pathlib
from validate_grip_pose import rotate

def support_recovery_evidence(controls,rows):
 """Require completed rig presentations and the actual native offhand pose."""
 checks={};metrics={};samples={}
 modes=[o['values'].get('bNativeHandTest') for row in rows if row.get('rig_sample_consistent') is True
        for o in row['objects'] if o['type']=='KFVRWeaponRig' and not o['values'].get('bDeleteMe')]
 native=bool(modes) and modes[0] is True
 checks['support_requested_hand_mode_consistent']=bool(modes) and all(type(mode) is bool and mode==native for mode in modes)
 # held, valid tracking bits, squeeze down, grabs delta, releases delta
 expected={33:(False,3,False,0,0),34:(True,3,True,1,0),35:(True,3,True,1,0),
           36:(False,1,True,1,1),37:(False,3,True,1,1),38:(False,3,False,1,1),
           39:(True,3,True,2,1),40:(True,3,True,2,1),41:(False,3,False,2,2)}
 def distance(a,b):
  if not isinstance(a,(list,tuple)) or not isinstance(b,(list,tuple)) or len(a)!=3 or len(b)!=3:return math.inf
  if not all(isinstance(x,(int,float)) and math.isfinite(x) for x in [*a,*b]):return math.inf
  return math.dist(a,b)
 for n,(held,flags,down,grabs,releases) in expected.items():
  sent=[c for c in controls if c.get('phase')==n and c.get('sent')]
  times=[c['uptime_ms'] for c in sent];records=[];missing=0
  checks[f'support_phase_{n}_sent_input']=len(sent)>=3 and all(c.get('focused') is True
   and c.get('left_grip_valid') is True and c.get('right_grip_valid')==(flags==3)
   and isinstance(c.get('left_squeeze'),(int,float))
   and ((c['left_squeeze']>.7) if down else (c['left_squeeze']<.2)) for c in sent)
  for row in rows:
   if not times or not min(times)+150<=row['uptime_ms']<=max(times) or row.get('rig_sample_consistent') is not True:continue
   objs={o['address']:o for o in row['objects'] if not o['values'].get('bDeleteMe')}
   rigs=[o for o in objs.values() if o['type']=='KFVRWeaponRig']
   if len(rigs)!=1:missing+=1;continue
   obj=rigs[0];r=obj['values'];gun=objs.get(r.get('Gun'),{});hand=objs.get(next((value for key,value in r.items() if key.casefold()=='lefthand'),None),{});visual=objs.get(r.get('Visual'),{})
   h=hand.get('values',{});v=visual.get('values',{})
   if (gun.get('type')!='KFVRTrackedBullpup' or gun.get('values',{}).get('VRRig')!=obj['address']
       or hand.get('type')!='KFVRHandVisual' or h.get('bLeftHand') is not True
       or visual.get('type')!='KFVRWeaponVisual' or v.get('SourceWeapon')!=gun.get('address')):
    missing+=1;continue
   records.append((r,h,v,visual['address']))
  samples[n]=records;checks[f'support_phase_{n}_coherent_samples']=len(records)>=3 and missing==0
  checks[f'support_phase_{n}_ownership']=len(records)>=3 and all(r.get('bTracking') is True
   and (r.get('Tracking.Flags',0)&3)==flags and r.get('bSupportHeld')==held
   and not r.get('bHolstered',True) and not r.get('bFiring',True)
   and r.get('ManualReload')=='0x0' for r,h,v,address in records)
  palm_errors=[];hand_valid=[]
  for r,h,v,address in records:
   valid=(r.get('bNativeHandTest') is native and h.get('bNativeHandMesh') is native
          and h.get('bHidden') is False and h.get('PoseFrames',0)>0)
   if held and native:
    valid=valid and h.get('bGripPose') is True and h.get('bGripSourceLeft') is True and h.get('GripRole')==3 and h.get('GripVisual')==address
    point=v.get('GripPose.0.Origin');rotation=v.get('Rotation');scale=v.get('DrawScale');location=v.get('Location')
    if distance(point,point)<math.inf and distance(rotation,rotation)<math.inf and distance(location,location)<math.inf and isinstance(scale,(int,float)) and math.isfinite(scale):
     target=[a+b for a,b in zip(rotate([x*scale for x in point],rotation),location)]
    else:target=None
   elif held:
    valid=valid and h.get('bGripPose') is False and h.get('GripRole')==3 and h.get('GripVisual')==address
    target=r.get('Tracking.LeftPosition')
   else:
    valid=valid and h.get('bGripPose') is False and h.get('GripRole')==0 and h.get('GripVisual')=='0x0'
    target=r.get('Tracking.LeftPosition')
   error=distance(h.get('PalmWorld'),target);palm_errors.append(error);hand_valid.append(valid and error<(.005 if held and native else .05))
  checks[f'support_phase_{n}_actual_hand']=len(hand_valid)>=3 and all(hand_valid) and len({h.get('PoseFrames') for r,h,v,address in records})>=3
  if held:checks[f'support_phase_{n}_accepted_alignment']=len(records)>=3 and all(r.get('SupportDirectionDot',0)>.999 for r,h,v,address in records)
  if n in (36,37):checks[f'support_phase_{n}_fresh_release_required']=len(records)>=3 and all(r.get('bSupportArmed') is False for r,h,v,address in records)
  if n==38:checks['support_release_rearms_grab']=len(records)>=3 and all(r.get('bSupportArmed') is True for r,h,v,address in records)
  metrics[str(n)]=dict(coherent_samples=len(records),unresolved_objects=missing,sent_samples=len(sent),
   maximum_palm_error_uu=max(palm_errors) if palm_errors and all(math.isfinite(x) for x in palm_errors) else None)
 baseline=samples.get(33,[])
 for n,(_,_,_,grabs,releases) in expected.items():
  records=samples[n]
  checks[f'support_phase_{n}_exact_transitions']=len(baseline)>=3 and len(records)>=3 and all(
   r.get('SupportGrabs')==baseline[0][0].get('SupportGrabs',-100)+grabs
   and r.get('SupportReleases')==baseline[0][0].get('SupportReleases',-100)+releases
   and r.get('TraceCount')==baseline[0][0].get('TraceCount',-100) for r,h,v,address in records)
 return checks,metrics

def validate(folder):
 folder=pathlib.Path(folder)
 controls=[json.loads(x) for x in (folder/'controls.jsonl').read_text().splitlines()]
 rows=[json.loads(x) for x in (folder/'weapon-observations.jsonl').read_text().splitlines()]
 def objects(typ,selected=rows):return [o['values'] for row in selected for o in row['objects'] if o['type']==typ]
 def phase(n):
  times=[c['uptime_ms'] for c in controls if c['phase']==n and c['sent']]
  return [r for r in rows if times and min(times)+150<=r['uptime_ms']<=max(times)]
 checks={};rigs=objects('KFVRWeaponRig');start,end=rigs[0],rigs[-1]
 workflow=objects('KFVRWeaponRig',phase(27));workflow_end=workflow[-1] if workflow else end
 checks['original_workflow_checkpoint_observed']=bool(workflow)
 single_addresses={o['address'] for row in rows for o in row['objects'] if o['type']=='KFVRTrackedSingle'}
 preface=objects('KFVRWeaponRig',phase(0))
 if not preface:raise ValueError('Inventory trial is missing its initial draw observation')
 preface_draw=preface[0]['SwitchCount']-start['SwitchCount']
 checks['preface_selects_pistol']=preface[0]['Gun'] in single_addresses and not preface[0]['bHolstered']
 checks['one_stock_use_request']=end['UseCount']-start['UseCount']==1
 checks['seven_owned_weapons']=start['InventoryCount']==end['InventoryCount']==7
 checks['three_holster_switches']=workflow_end['SwitchCount']-start['SwitchCount']==3+preface_draw and workflow_end['Gun'] in {o['address'] for r in rows for o in r['objects'] if o['type']=='KFVRTrackedSingle'}
 checks['three_reload_requests']=end['ReloadCount']-start['ReloadCount']==3
 totalshots=0
 for typ,minimum,maximum,wait,final_loaded in [('KFVRTrackedSingle',3,3,4,13),('KFVRTrackedDeagle',1,1,11,8),('KFVRTrackedBullpup',3,15,18,40)]:
  guns=objects(typ);shots=guns[-1]['VRFireAttempts']-guns[0]['VRFireAttempts'];totalshots+=shots
  checks[typ+'_shots']=minimum<=shots<=maximum
  ready=objects(typ,phase(wait))
  checks[typ+'_reload_completed']=ready and any(g['MagAmmoRemaining']==g['MagCapacity'] and not g['bIsReloading'] for g in ready)
  checks[typ+'_final_loaded']=guns[-1]['MagAmmoRemaining']==final_loaded
 checks['stock_shots_match_traces']=totalshots==end['TraceCount']-start['TraceCount']
 lost=objects('KFVRWeaponRig',phase(23));held=objects('KFVRWeaponRig',phase(24))
 checks['focus_loss_stops']=lost and all(not r['bTracking'] and not r['bFiring'] for r in lost)
 checks['held_trigger_cannot_rearm']=held and all(not r['bTriggerArmed'] and not r['bFiring'] and r['TraceCount']==end['TraceCount']-1 for r in held)
 # Appended recovery contributes two weapon draws and two support acquisitions.
 checks['feedback_per_shot_and_draw']=json.loads((folder/'receiver.json').read_text())['haptic_pulses']==totalshots+8+preface_draw
 support=objects('KFVRWeaponRig',phase(16))
 checks['support_hand_grabbed']=workflow_end['SupportGrabs']-start['SupportGrabs']==1
 checks['two_hand_aim_alignment']=bool(support) and all(r['bSupportHeld'] and r['SupportDirectionDot']>.999 for r in support)
 checks['support_hand_released']=workflow_end['SupportReleases']-start['SupportReleases']==1 and not workflow_end['bSupportHeld']
 checks['seven_calibrated_models']=len({o['address'] for row in rows for o in row['objects'] if o['type']=='KFVRWeaponVisual' and o['values']['bCalibrated']})==7
 hands=objects('KFVRHandVisual');posed=[h for h in hands if h['PoseFrames']>0]
 checks['both_palms_tracked']={h['bLeftHand'] for h in posed}=={True,False} and max(h['PalmError'] for h in posed)<.05
 checks['clean_input_release']=not end['bTracking'] and not end['bFiring']
 checks['all_models_hidden_after_stop']=all(o['values']['bHidden'] for o in rows[-1]['objects'] if o['type'] in ['KFVRWeaponVisual','KFVRHandVisual'])
 recovery_checks,recovery_metrics=support_recovery_evidence(controls,rows);checks.update(recovery_checks)
 checks['support_recovery_two_grabs_two_releases']=end['SupportGrabs']-workflow_end['SupportGrabs']==2 and end['SupportReleases']-workflow_end['SupportReleases']==2
 restored=objects('KFVRWeaponRig',[r for r in phase(44) if r.get('rig_sample_consistent') is True])
 checks['support_recovery_returns_to_pistol']=len(restored)>=3 and all(r['Gun'] in single_addresses and not r['bHolstered'] and not r['bSupportHeld'] and not r['bFiring'] and r['SwitchCount']==workflow_end['SwitchCount']+2 and r['TraceCount']==workflow_end['TraceCount'] for r in restored)
 checks['support_recovery_adds_no_shots']=end['TraceCount']==workflow_end['TraceCount']
 report=dict(passed=all(checks.values()),checks={k:bool(v) for k,v in checks.items()},support_recovery=recovery_metrics,shots=totalshots,synthetic_controllers=True,live_game=True,headset_presented=False)
 (folder/'inventory-validation.json').write_text(json.dumps(report,indent=2)+'\n')
 if not report['passed']:raise ValueError('Inventory checks failed: '+', '.join(k for k,v in checks.items() if not v))
 return report
if __name__=='__main__':
 import sys
 print(json.dumps(validate(sys.argv[1]),indent=2))
