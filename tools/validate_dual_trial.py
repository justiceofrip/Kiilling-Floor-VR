"""Independent controller aim/fire through actual KF dual-pistol modes."""
import json,pathlib,math

def validate(folder):
 folder=pathlib.Path(folder);rows=[json.loads(x) for x in (folder/'weapon-observations.jsonl').read_text().splitlines()];controls=[json.loads(x) for x in (folder/'controls.jsonl').read_text().splitlines()]
 def objects(typ,selected=rows):return [o['values'] for r in selected for o in r['objects'] if o['type']==typ]
 def phase(n,typ='KFVRTrackedDualies'):
  times=[c['uptime_ms'] for c in controls if c['phase']==n and c['sent']]
  return objects(typ,[r for r in rows if times and min(times)+250<=r['uptime_ms']<=max(times)])
 guns=objects('KFVRTrackedDualies');rigs=objects('KFVRWeaponRig');checks={}
 if not guns:raise ValueError('No live dual-pistol adapter observed')
 start,end=guns[0],guns[-1];r0,r1=start['RightShots'],start['RightShots']+5;l0,l1=start['LeftShots'],start['LeftShots']+5
 def at(name,n,predicate,typ='KFVRTrackedDualies'):
  data=phase(n,typ);checks[name]=bool(data) and all(predicate(x) for x in data)
 at('right_trigger_only',4,lambda g:g['RightShots']==r0+1 and g['LeftShots']==l0)
 at('left_trigger_only',6,lambda g:g['RightShots']==r0+1 and g['LeftShots']==l0+1)
 at('simultaneous_triggers',8,lambda g:g['RightShots']==r0+2 and g['LeftShots']==l0+2)
 at('focus_loss_releases_both',9,lambda g:not g['bLeftFiring'] and not g['bRightFiring'])
 at('held_focus_return_cannot_fire',10,lambda g:g['RightShots']==r0+2 and g['LeftShots']==l0+2 and not g['bLeftArmed'] and not g['bRightArmed'])
 at('neutral_rearms_both',12,lambda g:g['RightShots']==r0+3 and g['LeftShots']==l0+3)
 at('left_tracking_loss_releases',13,lambda g:not g['bLeftFiring'] and not g['bLeftArmed'])
 at('held_left_return_cannot_fire',14,lambda g:g['LeftShots']==l0+3 and not g['bLeftArmed'])
 at('timed_reload_fills_both',19,lambda g:g['LeftLoaded']==15 and g['RightLoaded']==15 and not g['bIsReloading'])
 at('holstered_triggers_cannot_fire',23,lambda g:g['RightShots']==r0+4 and g['LeftShots']==l0+4 and not g['bLeftFiring'] and not g['bRightFiring'])
 at('redraw_rearms_both',28,lambda g:g['RightShots']==r1 and g['LeftShots']==l1)
 at('right_loss_releases_pair',29,lambda g:not g['bLeftFiring'] and not g['bRightFiring'])
 at('held_right_return_cannot_fire',30,lambda g:g['RightShots']==r1 and g['LeftShots']==l1 and not g['bLeftArmed'] and not g['bRightArmed'])
 at('empty_left_cannot_borrow_right',38,lambda g:g['LeftLoaded']==0 and g['RightLoaded']==13 and g['LeftShots']==l0+18)
 at('right_fires_with_empty_left',40,lambda g:g['RightLoaded']==12 and g['LeftLoaded']==0 and g['RightShots']==r0+6 and g['LeftShots']==l0+18)
 at('both_refill_after_empty_left',45,lambda g:g['LeftLoaded']==15 and g['RightLoaded']==15 and not g['bIsReloading'])
 checks['twenty_six_stock_shots']=end['RightShots']==r0+7 and end['LeftShots']==l0+19 and rigs[-1]['TraceCount']-rigs[0]['TraceCount']==26
 checks['stock_ammo_consumed_once_per_shot']=start['VRTotalAmmo']-end['VRTotalAmmo']==26
 checks['independent_loaded_feeds_conserved']=all(g['LeftLoaded']+g['RightLoaded']==g['MagAmmoRemaining'] for g in guns)
 checks['final_loaded_count']=end['LeftLoaded']==end['RightLoaded']==14 and end['MagAmmoRemaining']==28
 aimed=phase(8);checks['independent_aim_directions']=bool(aimed) and all(sum(a*b for a,b in zip(g['LeftTraceDirection'],g['RightTraceDirection']))<.9 for g in aimed)
 checks['separated_muzzles']=bool(aimed) and all(math.dist(g['LeftTraceStart'],g['RightTraceStart'])>15 for g in aimed)
 hands=[g for g in objects('KFVRHandVisual') if g['PoseFrames']>0];checks['both_hands_follow_controllers']={h['bLeftHand'] for h in hands}=={True,False} and max(h['PalmError'] for h in hands)<.05
 visuals=[o for r in rows for o in r['objects'] if o['type']=='KFVRWeaponVisual' and o['values']['SourceWeapon'] in {x['address'] for row in rows for x in row['objects'] if x['type']=='KFVRTrackedDualies'}]
 checks['two_calibrated_pistol_models']={v['values']['bOffhandGun'] for v in visuals if v['values']['bCalibrated']}=={True,False}
 pulses=json.loads((folder/'receiver.json').read_text())['haptic_pulses'];checks['feedback_for_each_hand_and_holster']=pulses==26+rigs[-1]['DrawCount']-rigs[0]['DrawCount']+rigs[-1]['HolsterCount']-rigs[0]['HolsterCount']
 checks['stop_releases_both']=not end['bLeftFiring'] and not end['bRightFiring'] and not rigs[-1]['bTracking']
 checks['stop_hides_models']=all(o['values']['bHidden'] for o in rows[-1]['objects'] if o['type'] in ['KFVRWeaponVisual','KFVRHandVisual'])
 report=dict(passed=all(checks.values()),checks=checks,right_shots=end['RightShots']-r0,left_shots=end['LeftShots']-l0,ammo_used=start['VRTotalAmmo']-end['VRTotalAmmo'],haptic_pulses=pulses,live_game=True,synthetic_controllers=True,headset_presented=False,physical_belt_reload=False)
 (folder/'dual-validation.json').write_text(json.dumps(report,indent=2)+'\n')
 if not report['passed']:raise ValueError('Dual checks failed: '+', '.join(k for k,v in checks.items() if not v))
 return report
if __name__=='__main__':
 import sys
 print(json.dumps(validate(sys.argv[1]),indent=2))
