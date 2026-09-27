"""Checks real script behavior, separately from native pose delivery/render checks."""
import json,math,pathlib

def validate(folder):
 folder=pathlib.Path(folder)
 controls=[json.loads(l) for l in (folder/'controls.jsonl').read_text().splitlines()]
 observations=[json.loads(l) for l in (folder/'weapon-observations.jsonl').read_text().splitlines()]
 def objects(typ,rows=observations):return [o['values'] for row in rows for o in row['objects'] if o['type']==typ]
 def phase(n):
  times=[c['uptime_ms'] for c in controls if c['phase']==n and c['sent']]
  if not times:raise ValueError('No delivered controls in phase '+str(n))
  return [r for r in observations if min(times)+150<=r['uptime_ms']<=max(times)]
 def rigphase(n):return objects('KFVRWeaponRig',phase(n))
 checks={}
 def check(name,passed):checks[name]=bool(passed)
 rigs=objects('KFVRWeaponRig');guns=objects('KFVRTrackedSingle');hands=objects('KFVRHandVisual')
 if not rigs or not guns or not hands:raise ValueError('Missing live rig, gun or hand observations')
 start,end=rigs[0],rigs[-1];shots=end['TraceCount']-start['TraceCount']
 check('three_deliberate_shots',shots==3)
 check('shot_and_holster_feedback',json.loads((folder/'receiver.json').read_text()).get('haptic_events')==5)
 check('one_round_per_shot',guns[0]['MagAmmoRemaining']-guns[-1]['MagAmmoRemaining']==shots)
 check('stock_fire_tick_used',guns[-1]['VRFireAttempts']-guns[0]['VRFireAttempts']==shots and guns[-1]['VRModeTicks']>guns[0]['VRModeTicks'])
 check('holster_and_draw_once',end['HolsterCount']-start['HolsterCount']==1 and end['DrawCount']-start['DrawCount']==1)
 check('poses_applied',end['AppliedFrames']-start['AppliedFrames']>100)
 lost=rigphase(8);held=rigphase(9)
 check('focus_loss_releases',lost and all(not r['bTracking'] and not r['bFiring'] for r in lost))
 check('held_trigger_does_not_rearm',held and all(not r['bTriggerArmed'] and not r['bFiring'] and r['TraceCount']==start['TraceCount']+2 for r in held))
 holstered=rigphase(4);check('holstered_fire_blocked',holstered and all(r['bHolstered'] and not r['bFiring'] for r in holstered))
 check('stop_releases_input',not end['bTracking'] and not end['bFiring'])
 check('stop_hides_presentation',all(o['values']['bHidden'] for o in observations[-1]['objects'] if o['type'] in ['KFVRWeaponVisual','KFVRHandVisual']))
 activehands=[h for h in hands if h['PoseFrames']>0]
 check('both_palms_follow_controllers',{h['bLeftHand'] for h in activehands}=={True,False} and max(h['PalmError'] for h in activehands)<.05)
 check('finger_inputs_vary',len({round(h['IndexCurl'],2) for h in activehands})>=3 and len({h['ThumbCurl'] for h in activehands})>=2 and max(h['GripCurl'] for h in activehands)>=1)
 first=[r for r in rigs if r['TraceCount']==start['TraceCount']+1]
 second=[r for r in rigs if r['TraceCount']==start['TraceCount']+2]
 if first and second:
  dot=sum(a*b for a,b in zip(first[-1]['LastTraceDirection'],second[-1]['LastTraceDirection']))
 else:dot=1
 check('aim_changes_with_controller',dot<.98)
 report=dict(passed=all(checks.values()),checks=checks,shots=shots,applied_frames=end['AppliedFrames']-start['AppliedFrames'],max_palm_error_uu=max(h['PalmError'] for h in activehands),synthetic_controllers=True,live_game=True,headset_presented=False,aim_direction_dot=dot)
 (folder/'weapon-validation.json').write_text(json.dumps(report,indent=2)+'\n')
 if not report['passed']:raise ValueError('Live weapon checks failed: '+', '.join(k for k,v in checks.items() if not v))
 return report
if __name__=='__main__':
 import sys
 print(json.dumps(validate(sys.argv[1]),indent=2))
