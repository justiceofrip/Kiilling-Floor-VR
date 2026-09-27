"""Body-anchored arm geometry measured from actual native KF bones."""
import json,pathlib

def validate(folder):
 folder=pathlib.Path(folder);rows=[json.loads(x) for x in (folder/'weapon-observations.jsonl').read_text().splitlines()];controls=[json.loads(x) for x in (folder/'controls.jsonl').read_text().splitlines()]
 def objects(typ,selected=rows):return [o['values'] for r in selected for o in r['objects'] if o['type']==typ]
 def phase(n):
  times=[c['uptime_ms'] for c in controls if c['phase']==n and c['sent']]
  return objects('KFVRHandVisual',[r for r in rows if times and min(times)+900<=r['uptime_ms']<=max(times)])
 hands=objects('KFVRHandVisual');rigs=objects('KFVRWeaponRig');checks={};settled=[]
 for n in [0,1,2,3,4,5,6,8,11,13]:
  data=phase(n);settled.extend(data)
  checks['pose_'+str(n)+'_calibrated']=bool(data) and {h['bLeftHand'] for h in data}=={False,True} and all(h['bArmValid'] and h['bArmSolved'] and h['ArmFrames']>0 for h in data)
  checks['pose_'+str(n)+'_joint_attachment']=bool(data) and all(max(h['ArmShoulderError'],h['ArmElbowError'],h['ArmWristError'])<0.15 for h in data)
  checks['pose_'+str(n)+'_bounded_clavicle']=bool(data) and all(h['ArmAnchorShift']<0.3 for h in data)
 checks['unreachable_pose_retains_controller_palms']=bool(phase(12)) and all(not h['bArmSolved'] and h['ArmFallbacks']>0 and h['PalmError']<0.05 for h in phase(12))
 checks['focus_hides_both_hands']=bool(phase(7)) and all(h['bHidden'] for h in phase(7))
 checks['left_tracking_loss_hides_only_left']=bool(phase(9)) and all(h['bHidden']==h['bLeftHand'] for h in phase(9))
 checks['right_tracking_loss_hides_only_right']=bool(phase(10)) and all(h['bHidden']!=h['bLeftHand'] for h in phase(10))
 checks['no_gameplay_from_arm_motion']=rigs[-1]['TraceCount']==rigs[0]['TraceCount'] and rigs[-1]['UseCount']==rigs[0]['UseCount']
 checks['stop_hides_presentation']=all(h['bHidden'] for h in objects('KFVRHandVisual',rows[-1:])) and not rigs[-1]['bTracking']
 report=dict(passed=all(checks.values()),checks=checks,max_settled_errors={k:max((h[k] for h in settled),default=None) for k in ['ArmShoulderError','ArmElbowError','ArmWristError']},calibration=[{k:h[k] for k in ['bLeftHand','UpperAxisDot','LowerAxisDot','UpperArmLength','LowerArmLength','ArmTranslationScale','ArmFrames']} for h in objects('KFVRHandVisual',rows[-1:])],live_game=True,synthetic_controllers=True,headset_presented=False)
 (folder/'arm-validation.json').write_text(json.dumps(report,indent=2)+'\n')
 if not report['passed']:raise ValueError('Arm checks failed: '+', '.join(k for k,v in checks.items() if not v))
 return report
if __name__=='__main__':
 import sys
 print(json.dumps(validate(sys.argv[1]),indent=2))
