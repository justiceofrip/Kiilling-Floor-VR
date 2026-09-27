"""Compare the native-world status display with independently read game state."""
import json,pathlib

def validate(folder,inventory=False,dual=False,belt=False):
 folder=pathlib.Path(folder);rows=[json.loads(x) for x in (folder/'weapon-observations.jsonl').read_text().splitlines()];controls=[json.loads(x) for x in (folder/'controls.jsonl').read_text().splitlines()]
 def panels(selected=rows):return [o['values'] for r in selected for o in r['objects'] if o['type']=='KFVRStatusDisplay']
 data=panels();checks={}
 if not data:raise ValueError('No VR status display observed')
 def phase(n):
  times=[c['uptime_ms'] for c in controls if c['phase']==n and c['sent']]
  return panels([r for r in rows if times and min(times)+200<=r['uptime_ms']<=max(times)])
 def at(name,n,predicate):
  selected=phase(n);checks[name]=bool(selected) and all(predicate(p) for p in selected)
 checks['one_persistent_display']=len({o['address'] for r in rows for o in r['objects'] if o['type']=='KFVRStatusDisplay'})==1
 posed=[p for p in data if p['TextureUpdates']>0]
 checks['native_text_rendered']=bool(posed) and data[-1]['RenderCalls']>0
 checks['texture_updates_only_on_changed_data']=data[-1]['RenderCalls']<=data[-1]['TextureUpdates']
 checks['follows_offhand_attachment']=bool(posed) and max(p['AttachmentError'] for p in posed)<.001
 checks['valid_loaded_and_spare_counts']=all(p['DisplayedLoaded']>=0 and p['DisplayedSpare']>=0 and p['DisplayedLeft']+p['DisplayedRight']==p['DisplayedLoaded'] for p in posed)
 comparisons=0;correct=True
 for r in rows:
  objects={o['address']:o for o in r['objects']}
  rigs=[o['values'] for o in r['objects'] if o['type']=='KFVRWeaponRig']
  for p in panels([r]):
   if p['TextureUpdates']<1 or not rigs:continue
   pawn=objects.get(rigs[0]['BoundPawn'])
   if pawn:
    comparisons+=1;v=pawn['values'];correct &= p['DisplayedHealth']==v['Health'] and p['DisplayedArmor']==int(v['ShieldStrength'])
 checks['vitals_match_real_pawn']=comparisons>0 and correct
 checks['hidden_after_stop']=data[-1]['bHidden']
 hands=[o['values'] for r in rows for o in r['objects'] if o['type']=='KFVRHandVisual' and o['values']['PoseFrames']>0]
 held=[h for h in hands if h.get('GripRole',0)>0 and h.get('InputGripCurl',1)<0.2]
 checks['held_objects_keep_a_resting_grasp']=bool(held) and all(h['GripCurl']>0.7 for h in held)
 checks['grasp_preserves_controller_palms']=bool(hands) and max(h['PalmError'] for h in hands)<0.05
 gunhands=[h for h in hands if h.get('GripRole')==1]
 checks['gun_index_keeps_capacitive_input']=bool(gunhands) and all(abs(h['IndexCurl']-h['InputIndexCurl'])<0.001 for h in gunhands)
 if inventory:
  free=[h for h in hands if h['bLeftHand'] and h.get('GripRole')==0 and h.get('InputGripCurl',1)<0.2]
  checks['free_offhand_can_open']=bool(free) and all(h['GripCurl']<0.2 for h in free)
  checks['rifle_support_has_object_grasp']=any(h['bLeftHand'] and h.get('GripRole')==3 for h in hands)

 if inventory:
  at('visible_with_free_raised_offhand',0,lambda p:not p['bHidden'])
  at('hidden_while_supporting_rifle',16,lambda p:p['bHidden'])
  at('hidden_on_focus_loss',23,lambda p:p['bHidden'])
  at('visible_again_after_focus_return',24,lambda p:not p['bHidden'])
  reloading=[p for p in phase(4) if p.get('ActionLine')=='RELOADING']
  # A held trigger can fire a different number of stock-cadence shots at
  # different frame rates. Compare the display with the independently read
  # weapon magazine during reload, rather than assuming one shot occurred.
  reloading_ids={id(p) for p in reloading};reload_matches=[]
  for row in rows:
   objects={o['address']:o for o in row['objects']}
   for panel in panels([row]):
    if id(panel) not in reloading_ids:continue
    gun=objects.get(panel['DisplayedGun']);v=gun['values'] if gun else {}
    reload_matches.append(bool(gun) and gun['type']=='KFVRTrackedSingle' and v.get('bIsReloading') and panel['DisplayedLoaded']==v.get('MagAmmoRemaining') and 0<=panel['DisplayedLoaded']<v.get('MagCapacity',0))
  checks['pistol_count_while_reloading']=bool(reload_matches) and all(reload_matches)
  ready=[p for p in phase(4) if p.get('ActionLine')=='READY']
  checks['pistol_count_after_reload']=bool(ready) and all(p['DisplayedLoaded']==15 for p in ready)
  ready=[p for p in phase(11) if p.get('ActionLine')=='READY']
  checks['deagle_count_after_reload']=bool(ready) and all(p['DisplayedLoaded']==8 for p in ready)
  ready=[p for p in phase(18) if p.get('ActionLine')=='READY']
  checks['rifle_count_after_reload']=bool(ready) and all(p['DisplayedLoaded']==40 for p in ready)
  types={o['address']:o['type'] for r in rows for o in r['objects']}
  checks['follows_three_weapon_switches']={types.get(p['DisplayedGun']) for p in posed}>={'KFVRTrackedSingle','KFVRTrackedDeagle','KFVRTrackedBullpup'}
  checks['shows_reload_state']=any(p.get('ActionLine')=='RELOADING' for p in data)
 if belt:
  at('shows_both_tactical_counts',12,lambda p:p['bDual'] and p['DisplayedLeft']==16 and p['DisplayedRight']==16)
  at('shows_belt_seating_prompt',12,lambda p:p.get('ActionLine')=='LIFT PISTOL FROM BELT')
  at('shows_presented_magazines',9,lambda p:p.get('ActionLine')=='MAGAZINES AT BELT')
 if dual:checks['independent_dual_counts_observed']=any(p['bDual'] and p['DisplayedLeft']!=p['DisplayedRight'] for p in posed)
 report=dict(passed=all(checks.values()),checks=checks,render_calls=data[-1]['RenderCalls'],texture_updates=data[-1]['TextureUpdates'],pawn_comparisons=comparisons,synthetic_controllers=True,live_game=True,headset_presented=False,pixel_inspection_separate=True)
 (folder/'status-validation.json').write_text(json.dumps(report,indent=2)+'\n')
 if not report['passed']:raise ValueError('Status checks failed: '+', '.join(k for k,v in checks.items() if not v))
 return report
if __name__=='__main__':
 import argparse
 p=argparse.ArgumentParser();p.add_argument('folder');p.add_argument('--inventory',action='store_true');p.add_argument('--dual',action='store_true');p.add_argument('--belt',action='store_true');a=p.parse_args();print(json.dumps(validate(a.folder,a.inventory,a.dual,a.belt),indent=2))
