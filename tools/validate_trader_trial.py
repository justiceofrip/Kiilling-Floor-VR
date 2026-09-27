"""Compare controller phases with real stock trader/inventory effects."""
import json,pathlib

def validate(folder):
 folder=pathlib.Path(folder);rows=[json.loads(x) for x in (folder/'weapon-observations.jsonl').read_text().splitlines()];controls=[json.loads(x) for x in (folder/'controls.jsonl').read_text().splitlines()]
 def objects(typ,selected=rows):return [o['values'] for r in selected for o in r['objects'] if o['type']==typ]
 def phase(n,typ='KFVRTraderPanel'):
  times=[c['uptime_ms'] for c in controls if c['phase']==n and c['sent']]
  return objects(typ,[r for r in rows if times and min(times)+700<=r['uptime_ms']<=max(times)])
 def last(n,typ='KFVRTraderPanel'):
  data=phase(n,typ);return data[-1] if data else {}
 panels=objects('KFVRTraderPanel');rigs=objects('KFVRWeaponRig');fixtures=objects('KFVRTraderTest');checks={}
 def state(n,**values):
  data=phase(n,'KFVRTraderTest');return bool(data) and all(all(o.get(k)==v for k,v in values.items()) for o in data)
 checks['actual_map_shop']=bool(fixtures) and all(f['bReady'] for f in fixtures)
 checks['opened_controller_panel']=bool(phase(2)) and all(p['bOpen'] and p['RenderCalls']>0 for p in phase(2))
 checks['buy_deagle_once_while_held']=state(4,Deagles=1) and last(4).get('BuyCount')==1 and last(4).get('SellCount')==0
 checks['converted_purchase_keeps_resale']=any(w['SellValue']>0 for w in phase(4,'KFVRTrackedDeagle'))
 checks['sale_credits_recorded_price']=state(6,Deagles=0) and last(6).get('SellCount')==1 and last(6).get('LastSellValue',0)>0 and abs(last(6).get('CashAfter',0)-last(6).get('CashBefore',0)-last(6).get('LastSellValue',-999))<0.01
 checks['buy_bullpup']=state(8,Bullpups=1)
 checks['stock_ammo_added_and_paid']=last(10).get('AmmoAfter',0)>last(10).get('AmmoBefore',0) and last(10).get('CashAfter',0)<last(10).get('CashBefore',0)
 checks['purchases_charge_listed_price']=all(last(n).get('LastPrice',0)>0 and abs(last(n).get('CashBefore',0)-last(n).get('CashAfter',0)-last(n).get('LastPrice',-1))<0.01 for n in [4,8,10,12,14,21,28])
 checks['stock_armor']=bool(phase(12,'KFHumanPawn')) and all(p['ShieldStrength']==100 for p in phase(12,'KFHumanPawn'))
 checks['stock_healing']=bool(phase(14,'KFHumanPawn')) and all(p['Health']>40 and p['Health']+p['healthToGive']==100 for p in phase(14,'KFHumanPawn')) and bool(phase(17,'KFHumanPawn')) and all(p['Health']==100 for p in phase(17,'KFHumanPawn'))
 checks['focus_closes_menu']=bool(phase(15)) and all(not p['bOpen'] and p['bHidden'] for p in phase(15))
 checks['buy_duals_once_while_held']=state(21,Duals=1) and last(21).get('BuyCount')==3
 checks['full_dual_ammo_rejects_charge']=last(23).get('ActionCount')==last(21).get('ActionCount') and last(23).get('RejectedCount')==last(21).get('RejectedCount',-1)+1 and last(23,'KFVRTraderTest').get('Cash')==last(21,'KFVRTraderTest').get('Cash') and all(w['VRTotalAmmo']==240 for w in phase(23,'KFVRTrackedDualies'))
 checks['selling_duals_restores_single']=state(26,Duals=0) and bool(phase(26,'KFVRTrackedSingle'))
 checks['repurchase_deagle']=state(28,Deagles=1)
 checks['close_button']=bool(phase(30)) and all(not p['bOpen'] for p in phase(30))
 checks['weapon_input_resumes']=bool(phase(32,'KFVRWeaponRig')) and max(r['TraceCount'] for r in phase(32,'KFVRWeaponRig'))>rigs[0]['TraceCount']
 checks['lost_hand_blocks_purchase']=last(35).get('ActionCount')==last(34).get('ActionCount') and last(35).get('HoverRow')==-1
 checks['held_trigger_recovery_blocks_purchase']=last(36).get('ActionCount')==last(34).get('ActionCount')
 checks['fresh_trigger_sells_after_recovery']=state(39,Deagles=0) and last(39).get('SellCount')==3
 checks['no_duplicate_inventory']=bool(fixtures) and all(max(f['Deagles'],f['Bullpups'],f['Duals'])<=1 for f in fixtures)
 checks['modal_input_does_not_fire']=all(bool(phase(n,'KFVRWeaponRig')) and all(r['TraceCount']==rigs[0]['TraceCount'] for r in phase(n,'KFVRWeaponRig')) for n in range(1,30))
 checks['cash_and_weight_remain_valid']=bool(fixtures) and all(f['Cash']>=0 and 0<=f['CarryWeight']<=15 for f in fixtures)
 checks['insufficient_cash_rejected']=state(45,Deagles=0,Cash=0,TestStage=1) and last(45).get('BuyCount')==last(42).get('BuyCount') and last(45).get('RejectedCount')==last(42).get('RejectedCount',-1)+1
 checks['overweight_purchase_rejected']=state(48,Deagles=0,Cash=2000,TestStage=2) and last(48).get('BuyCount')==last(42).get('BuyCount') and last(48).get('RejectedCount')==last(45).get('RejectedCount',-1)+1
 checks['actual_shop_closure_closes_panel']=last(48).get('bOpen')==True and last(51).get('CloseCount')==last(48).get('CloseCount',-1)+1 and state(51,bShopOpen=False,TestStage=3) and bool(phase(51)) and all(not p['bOpen'] and p['bHidden'] for p in phase(51))
 checks['stop_releases_menu_and_input']=bool(panels) and not panels[-1]['bOpen'] and panels[-1]['bHidden'] and not rigs[-1]['bTracking']
 report=dict(passed=all(checks.values()),checks=checks,last_panel=panels[-1] if panels else {},live_game=True,synthetic_controllers=True,headset_presented=False)
 (folder/'trader-validation.json').write_text(json.dumps(report,indent=2)+'\n')
 if not report['passed']:raise ValueError('Trader checks failed: '+', '.join(k for k,v in checks.items() if not v))
 return report
if __name__=='__main__':
 import sys
 print(json.dumps(validate(sys.argv[1]),indent=2))
