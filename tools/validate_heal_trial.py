"""Actual stock healing, charge, contact and cancellation through tracked input."""
import json,pathlib

def validate(folder):
 folder=pathlib.Path(folder);rows=[json.loads(x) for x in (folder/'weapon-observations.jsonl').read_text().splitlines()];controls=[json.loads(x) for x in (folder/'controls.jsonl').read_text().splitlines()]
 def objects(typ,selected=rows):return [o['values'] for r in selected for o in r['objects'] if o['type']==typ]
 def phase(n,typ='KFVRTrackedSyringe'):
  times=[c['uptime_ms'] for c in controls if c['phase']==n and c['sent']]
  return objects(typ,[r for r in rows if times and min(times)+250<=r['uptime_ms']<=max(times)])
 guns=objects('KFVRTrackedSyringe');rigs=objects('KFVRWeaponRig');pawns=objects('KFHumanPawn');checks={}
 if not guns or not pawns:raise ValueError('Missing syringe or living test pawn')
 start,end=guns[0],guns[-1]
 def at(name,n,predicate,typ='KFVRTrackedSyringe'):
  data=phase(n,typ);checks[name]=bool(data) and all(predicate(x) for x in data)
 at('empty_space_does_not_heal',3,lambda s:s['HealBegins']==start['HealBegins'] and s['ChargeSpent']==start['ChargeSpent'])
 at('measured_needle_reaches_body',4,lambda s:s['bSelfContact'] and 0.005<s['ContactDistance']<0.12)
 at('focus_loss_cancels_before_charge',6,lambda s:s['HealBegins']==start['HealBegins']+1 and s['CancelledHeals']==start['CancelledHeals']+1 and s['ChargeSpent']==start['ChargeSpent'])
 at('held_focus_return_needs_neutral',6,lambda s:not s['bHealApproved'] and not s['bPendingHeal'])
 at('neutral_then_contact_heals_once',11,lambda s:s['SelfHeals']==start['SelfHeals']+1 and s['ChargeSpent']==start['ChargeSpent']+500 and not s['bPendingHeal'])
 at('insufficient_charge_cannot_heal_again',14,lambda s:s['SelfHeals']==start['SelfHeals']+1)
 at('right_tracking_loss_releases',15,lambda s:not s['bHealApproved'] and not s['bPendingHeal'])
 at('held_tracking_return_needs_neutral',16,lambda s:not s['bHealApproved'])
 at('holstered_trigger_cannot_heal',22,lambda s:s['SelfHeals']==start['SelfHeals']+1 and not s['bHealApproved'])
 checks['one_completed_stock_self_heal']=end['SelfHeals']-start['SelfHeals']==1 and end['OtherHeals']==start['OtherHeals'] and end['ChargeSpent']-start['ChargeSpent']==500
 checks['actual_pawn_health_increases']=pawns[0]['Health']==40 and 90<=pawns[-1]['Health']<=pawns[-1]['HealthMax']
 checks['normal_charge_recovery_runs']=0<end['VRTotalAmmo']<=500
 checks['no_gunshot_traces']=rigs[-1]['TraceCount']==rigs[0]['TraceCount']
 checks['draw_stow_redraw']=rigs[-1]['DrawCount']-rigs[0]['DrawCount']==2 and rigs[-1]['HolsterCount']-rigs[0]['HolsterCount']==1
 checks['stop_cancels_owned_input']=not end['bHealFiring'] and not end['bPendingHeal'] and not end['bVRControlled'] and not rigs[-1]['bTracking']
 panels=objects('KFVRStatusDisplay');checks['status_reports_charge']=any(p.get('AmmoLine','').startswith('CHARGE ') for p in panels)
 pulses=json.loads((folder/'receiver.json').read_text())['haptic_pulses'];checks['completion_and_holster_feedback']=pulses==4
 from validate_tool_attachment import attachment_checks
 tool_checks,tool_report=attachment_checks(rows,controls,'KFVRTrackedSyringe',[3, 4, 7, 9, 12, 17, 27],27)
 checks.update(tool_checks)
 report=dict(passed=all(checks.values()),checks=checks,tool_attachment=tool_report,self_heals=end['SelfHeals']-start['SelfHeals'],cancelled=end['CancelledHeals']-start['CancelledHeals'],charge_spent=end['ChargeSpent']-start['ChargeSpent'],final_charge=end['VRTotalAmmo'],initial_health=pawns[0]['Health'],final_health=pawns[-1]['Health'],haptic_pulses=pulses,live_game=True,synthetic_controllers=True,headset_presented=False,other_player_healing_tested=False)
 (folder/'heal-validation.json').write_text(json.dumps(report,indent=2)+'\n')
 if not report['passed']:raise ValueError('Healing checks failed: '+', '.join(k for k,v in checks.items() if not v))
 return report
if __name__=='__main__':
 import sys
 print(json.dumps(validate(sys.argv[1]),indent=2))
