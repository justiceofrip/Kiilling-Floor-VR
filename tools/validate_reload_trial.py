"""Assert live physical magazine, slide, stock-shot and bone behavior."""
import json,pathlib

def validate(folder):
 folder=pathlib.Path(folder)
 controls=[json.loads(x) for x in (folder/'controls.jsonl').read_text().splitlines()]
 rows=[json.loads(x) for x in (folder/'weapon-observations.jsonl').read_text().splitlines()]
 def objects(typ,selected=rows):return [o['values'] for row in selected for o in row['objects'] if o['type']==typ]
 def phase(n,typ='KFVRManualReload'):
  times=[c['uptime_ms'] for c in controls if c['phase']==n and c['sent']]
  return objects(typ,[r for r in rows if times and min(times)+120<=r['uptime_ms']<=max(times)])
 checks={};rigs=objects('KFVRWeaponRig');manuals=objects('KFVRManualReload');guns=objects('KFVRTrackedSingle')
 if not manuals:raise ValueError('Physical mode was never observed')
 start,end=manuals[0],manuals[-1];shots=rigs[-1]['TraceCount']-rigs[0]['TraceCount'];initial=start['OwnedRounds']+start['SpentRounds']+start['DiscardedRounds']
 def at(name,n,predicate,typ='KFVRManualReload'):
  data=phase(n,typ);checks[name]=bool(data) and all(predicate(x) for x in data)
 checks['every_sample_conserves_ammo']=all(m['bValid'] and m['OwnedRounds']+m['SpentRounds']+m['DiscardedRounds']==initial for m in manuals)
 at('catch_keeps_chamber',5,lambda m:m['HeldMagazine']==1 and m['InsertedMagazine']==0 and m['LoadedRounds']==1 and m['bChambered'])
 at('retained_chamber_fires_once',7,lambda m:m['SpentRounds']==1 and m['LoadedRounds']==0 and not m['bChambered'])
 at('empty_trigger_cannot_fire',8,lambda m:m['SpentRounds']==1)
 at('partial_magazine_stowed',10,lambda m:m['StowCount']==1 and m['HeldMagazine']==0)
 at('replacement_has_new_identity',12,lambda m:m['HeldMagazine']==2 and m['GrabCount']==1)
 at('insertion_does_not_feed',14,lambda m:m['InsertedMagazine']==2 and not m['bChambered'] and m['SpentRounds']==1)
 at('partial_slide_pull_does_not_feed',19,lambda m:m['RackCount']==0 and not m['bChambered'] and m['SpentRounds']==1)
 at('slide_open_blocks_fire',23,lambda m:m['SlideTravel']>.9 and not m['bChambered'] and m['SpentRounds']==1)
 at('full_return_feeds',24,lambda m:m['RackCount']==1 and m['bChambered'] and m['LoadedRounds']==15)
 at('stock_shot_consumes_chamber_and_feeds',26,lambda m:m['SpentRounds']==2 and m['LoadedRounds']==14 and m['bChambered'])
 at('tactical_reload_retains_plus_one',33,lambda m:m['LoadedRounds']==16 and m['bChambered'] and m['InsertedMagazine']==3)
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
 checks['input_released_at_stop']=not rigs[-1]['bTracking'] and not rigs[-1]['bFiring']
 report=dict(passed=all(checks.values()),checks=checks,shots=shots,initial_rounds=initial,remaining_rounds=end['OwnedRounds'],maximum_magazine_error_uu=max((m['MagazineError'] for m in held),default=-1),synthetic_controllers=True,live_game=True,headset_presented=False)
 (folder/'reload-validation.json').write_text(json.dumps(report,indent=2)+'\n')
 if not report['passed']:raise ValueError('Reload checks failed: '+', '.join(k for k,v in checks.items() if not v))
 return report
if __name__=='__main__':
 import sys
 print(json.dumps(validate(sys.argv[1]),indent=2))
