"""Read-only acceptance of actual game traces into a private collision fixture."""
import json,pathlib

def validate(folder):
 folder=pathlib.Path(folder)
 controls=[json.loads(x) for x in (folder/'controls.jsonl').read_text().splitlines()]
 rows=[json.loads(x) for x in (folder/'weapon-observations.jsonl').read_text().splitlines()]
 def objects(typ,selected=rows):return [o['values'] for row in selected for o in row['objects'] if o['type']==typ]
 def phase(n,typ='KFVRTrackedKnife'):
  times=[c['uptime_ms'] for c in controls if c['phase']==n and c['sent']]
  return objects(typ,[r for r in rows if times and min(times)+250<=r['uptime_ms']<=max(times)])
 checks={};knives=objects('KFVRTrackedKnife');targets=objects('KFVRMeleeTarget');rigs=objects('KFVRWeaponRig')
 if not knives or not targets:raise ValueError('Live knife and explicit collision fixture must be observed')
 start,end=knives[0],knives[-1]
 def at(name,n,predicate,typ='KFVRTrackedKnife'):
  data=phase(n,typ);checks[name]=bool(data) and all(predicate(x) for x in data)
 checks['four_owned_weapons']=rigs[-1]['InventoryCount']==4
 at('slow_motion_no_damage',5,lambda k:k['HitCount']==start['HitCount'] and k['SwingCount']==start['SwingCount'])
 at('small_waggle_no_damage',6,lambda k:k['HitCount']==start['HitCount'] and k['SwingCount']==start['SwingCount'])
 at('first_swing_single_hit',8,lambda k:k['HitCount']==start['HitCount']+1 and k['SwingCount']==start['SwingCount']+1)
 at('focus_loss_stops',9,lambda r:not r['bTracking'] and not r['bFiring'],'KFVRWeaponRig')
 at('held_contact_and_trigger_no_repeat',10,lambda k:k['HitCount']==start['HitCount']+1)
 at('retraction_no_damage',11,lambda k:k['HitCount']==start['HitCount']+1)
 at('second_swing_single_hit',13,lambda k:k['HitCount']==start['HitCount']+2 and k['SwingCount']==start['SwingCount']+2)
 at('locomotion_no_swing',13,lambda k:k['SwingCount']==start['SwingCount']+2)
 at('snap_turn_no_swing',14,lambda k:k['SwingCount']==start['SwingCount']+2)
 at('knife_stowed',17,lambda r:r['bHolstered'],'KFVRWeaponRig')
 at('holstered_motion_no_swing',17,lambda k:k['SwingCount']==start['SwingCount']+2)
 at('knife_redrawn',20,lambda r:not r['bHolstered'],'KFVRWeaponRig')
 checks['exactly_two_contact_damage_calls']=targets[-1]['HitCount']==2 and targets[-1]['DamageReceived']==38
 checks['contact_instigator_is_local_pawn']=targets[-1]['LastInstigator']==rigs[-1]['BoundPawn']
 checks['exactly_two_total_swings']=end['SwingCount']-start['SwingCount']==2
 checks['stop_releases_motion']=end['WindowUntil']==0 and not rigs[-1]['bTracking'] and not rigs[-1]['bFiring']
 checks['presentation_hidden_at_stop']=all(o['values']['bHidden'] for o in rows[-1]['objects'] if o['type'] in ['KFVRWeaponVisual','KFVRHandVisual'])
 report=dict(passed=all(checks.values()),checks=checks,swings=end['SwingCount']-start['SwingCount'],contact_hits=targets[-1]['HitCount'],damage=targets[-1]['DamageReceived'],synthetic_controllers=True,live_game=True,headset_presented=False,collision_fixture=True,zed_perks_tested=False)
 (folder/'melee-validation.json').write_text(json.dumps(report,indent=2)+'\n')
 if not report['passed']:raise ValueError('Melee checks failed: '+', '.join(k for k,v in checks.items() if not v))
 return report
if __name__=='__main__':
 import sys
 print(json.dumps(validate(sys.argv[1]),indent=2))
