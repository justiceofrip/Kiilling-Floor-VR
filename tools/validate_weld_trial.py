"""Actual installed map door welding, unwelding and Use through tracked controls."""
import json,pathlib

def validate(folder):
 folder=pathlib.Path(folder);rows=[json.loads(x) for x in (folder/'weapon-observations.jsonl').read_text().splitlines()];controls=[json.loads(x) for x in (folder/'controls.jsonl').read_text().splitlines()]
 def objects(typ,selected=rows):return [o['values'] for r in selected for o in r['objects'] if o['type']==typ]
 def phase(n,typ='KFVRTrackedWelder'):
  times=[c['uptime_ms'] for c in controls if c['phase']==n and c['sent']]
  return objects(typ,[r for r in rows if times and min(times)+300<=r['uptime_ms']<=max(times)])
 guns=objects('KFVRTrackedWelder');rigs=objects('KFVRWeaponRig');doors=objects('KFVRDoorTest');checks={}
 if not guns or not doors:raise ValueError('Missing welder or explicit real-map door fixture')
 start,end=guns[0],guns[-1]
 def at(name,n,predicate,typ='KFVRTrackedWelder'):
  data=phase(n,typ);checks[name]=bool(data) and all(predicate(x) for x in data)
 at('existing_map_door_fixture_ready',2,lambda x:x['bReady'] and x['InitialStrength']==0,'KFVRDoorTest')
 at('pointing_away_does_not_weld',3,lambda x:x['WeldHits']==start['WeldHits'] and x['UnweldHits']==start['UnweldHits'])
 at('tip_contact_detects_real_door',4,lambda x:x['ContactDoor']!='0x0')
 at('weld_changes_actual_door_strength',7,lambda x:x['CurrentStrength']>0 and x['bDoorSealed'],'KFVRDoorTest')
 at('stock_weld_damage_dispatches',7,lambda x:x['WeldHits']>start['WeldHits'])
 at('stock_unweld_damage_dispatches',9,lambda x:x['UnweldHits']>start['UnweldHits'])
 at('focus_return_requires_neutral',11,lambda x:not x['bWeldApproved'] and not x['bWeldFiring'])
 at('hand_loss_releases',14,lambda x:not x['bWeldApproved'] and not x['bWeldFiring'])
 at('held_hand_return_requires_neutral',15,lambda x:not x['bWeldApproved'])
 at('unweld_clears_seal',19,lambda x:x['CurrentStrength']<=0 and not x['bDoorSealed'],'KFVRDoorTest')
 at('use_opens_real_door',22,lambda x:not x['bDoorClosed'],'KFVRDoorTest')
 at('next_press_closes_real_door',25,lambda x:x['bDoorClosed'],'KFVRDoorTest')
 at('held_use_does_not_repeat',22,lambda x:x['UseCount']==rigs[0]['UseCount']+1,'KFVRWeaponRig')
 at('holstered_trigger_cannot_weld',30,lambda x:not x['bWeldApproved'] and not x['bWeldFiring'])
 checks['charge_consumed_and_recovers']=end['ChargeSpent']>start['ChargeSpent'] and 0<end['VRTotalAmmo']<=end['MaxCharge']
 checks['pointed_door_and_proximity_uses']=rigs[-1]['DoorUseCount']>rigs[0]['DoorUseCount'] and rigs[-1]['UseCount']-rigs[0]['UseCount']==2
 checks['no_gunshot_traces']=rigs[-1]['TraceCount']==rigs[0]['TraceCount']
 checks['draw_stow_redraw']=rigs[-1]['DrawCount']-rigs[0]['DrawCount']==2 and rigs[-1]['HolsterCount']-rigs[0]['HolsterCount']==1
 checks['stop_releases_owned_input']=not end['bVRControlled'] and not end['bWeldFiring'] and not rigs[-1]['bTracking']
 report=dict(passed=all(checks.values()),checks=checks,weld_hits=end['WeldHits']-start['WeldHits'],unweld_hits=end['UnweldHits']-start['UnweldHits'],charge_spent=end['ChargeSpent']-start['ChargeSpent'],final_charge=end['VRTotalAmmo'],final_strength=doors[-1]['CurrentStrength'],door_uses=rigs[-1]['DoorUseCount']-rigs[0]['DoorUseCount'],live_map_door=True,synthetic_controllers=True,headset_presented=False)
 (folder/'weld-validation.json').write_text(json.dumps(report,indent=2)+'\n')
 if not report['passed']:raise ValueError('Welder checks failed: '+', '.join(k for k,v in checks.items() if not v))
 return report
if __name__=='__main__':
 import sys
 print(json.dumps(validate(sys.argv[1]),indent=2))
