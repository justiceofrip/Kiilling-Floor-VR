"""Live three-weapon switching, timed reload and focus-loss assertions."""
import json,pathlib

def validate(folder):
 folder=pathlib.Path(folder)
 controls=[json.loads(x) for x in (folder/'controls.jsonl').read_text().splitlines()]
 rows=[json.loads(x) for x in (folder/'weapon-observations.jsonl').read_text().splitlines()]
 def objects(typ,selected=rows):return [o['values'] for row in selected for o in row['objects'] if o['type']==typ]
 def phase(n):
  times=[c['uptime_ms'] for c in controls if c['phase']==n and c['sent']]
  return [r for r in rows if times and min(times)+150<=r['uptime_ms']<=max(times)]
 checks={};rigs=objects('KFVRWeaponRig');start,end=rigs[0],rigs[-1]
 single_addresses={o['address'] for row in rows for o in row['objects'] if o['type']=='KFVRTrackedSingle'}
 preface=objects('KFVRWeaponRig',phase(0))
 if not preface:raise ValueError('Inventory trial is missing its initial draw observation')
 preface_draw=preface[0]['SwitchCount']-start['SwitchCount']
 checks['preface_selects_pistol']=preface[0]['Gun'] in single_addresses and not preface[0]['bHolstered']
 checks['one_stock_use_request']=end['UseCount']-start['UseCount']==1
 checks['seven_owned_weapons']=start['InventoryCount']==end['InventoryCount']==7
 checks['three_holster_switches']=end['SwitchCount']-start['SwitchCount']==3+preface_draw and end['Gun'] in {o['address'] for r in rows for o in r['objects'] if o['type']=='KFVRTrackedSingle'}
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
 checks['feedback_per_shot_and_draw']=json.loads((folder/'receiver.json').read_text())['haptic_pulses']==totalshots+4+preface_draw
 support=objects('KFVRWeaponRig',phase(16))
 checks['support_hand_grabbed']=end['SupportGrabs']-start['SupportGrabs']==1
 checks['two_hand_aim_alignment']=bool(support) and all(r['bSupportHeld'] and r['SupportDirectionDot']>.999 for r in support)
 checks['support_hand_released']=end['SupportReleases']-start['SupportReleases']==1 and not end['bSupportHeld']
 checks['seven_calibrated_models']=len({o['address'] for row in rows for o in row['objects'] if o['type']=='KFVRWeaponVisual' and o['values']['bCalibrated']})==7
 hands=objects('KFVRHandVisual');posed=[h for h in hands if h['PoseFrames']>0]
 checks['both_palms_tracked']={h['bLeftHand'] for h in posed}=={True,False} and max(h['PalmError'] for h in posed)<.05
 checks['clean_input_release']=not end['bTracking'] and not end['bFiring']
 checks['all_models_hidden_after_stop']=all(o['values']['bHidden'] for o in rows[-1]['objects'] if o['type'] in ['KFVRWeaponVisual','KFVRHandVisual'])
 report=dict(passed=all(checks.values()),checks={k:bool(v) for k,v in checks.items()},shots=totalshots,synthetic_controllers=True,live_game=True,headset_presented=False)
 (folder/'inventory-validation.json').write_text(json.dumps(report,indent=2)+'\n')
 if not report['passed']:raise ValueError('Inventory checks failed: '+', '.join(k for k,v in checks.items() if not v))
 return report
if __name__=='__main__':
 import sys
 print(json.dumps(validate(sys.argv[1]),indent=2))
