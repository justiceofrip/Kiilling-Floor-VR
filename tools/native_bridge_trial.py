"""Bounded actual native game-to-x64 receiver test; no headset required."""
import argparse,datetime,json,pathlib,subprocess,sys
from trace_kf import trace
p=argparse.ArgumentParser();p.add_argument('--pid',type=int,required=True);p.add_argument('--game',type=pathlib.Path,required=True);p.add_argument('--walking-aim',action='store_true');p.add_argument('--movement',action='store_true');p.add_argument('--cadence',action='store_true');p.add_argument('--weapon',action='store_true');p.add_argument('--inventory',action='store_true');p.add_argument('--grenade',action='store_true');p.add_argument('--trader',action='store_true');p.add_argument('--hud',action='store_true');p.add_argument('--hands',action='store_true');p.add_argument('--arms',action='store_true');p.add_argument('--weld',action='store_true');p.add_argument('--heal',action='store_true');p.add_argument('--dual-belt',action='store_true');p.add_argument('--dual',action='store_true');p.add_argument('--melee',action='store_true');p.add_argument('--reload',action='store_true');p.add_argument('--reload-interrupt',action='store_true');p.add_argument('--reload-alignment',action='store_true');p.add_argument('--reload-insertion',action='store_true');p.add_argument('--deagle-reload',action='store_true');p.add_argument('--turning',action='store_true');p.add_argument('--width',type=int,default=640);p.add_argument('--height',type=int,default=480);a=p.parse_args();a.turning=a.turning or a.hands or a.melee or a.grenade;a.reload=a.reload or a.deagle_reload or a.reload_interrupt or a.reload_alignment or a.reload_insertion;a.weapon=a.weapon or a.walking_aim or a.hud or a.hands or a.inventory or a.reload or a.melee or a.dual or a.dual_belt or a.heal or a.weld or a.arms or a.trader or a.grenade;a.movement=a.movement or a.turning or a.weapon or a.cadence
if sum((a.reload_interrupt,a.reload_alignment,a.reload_insertion))>1:p.error('Choose one reload diagnostic variant per trial')
root=pathlib.Path(__file__).resolve().parents[1];folder=root/'reports'/('native-bridge-'+datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%d-%H%M%S'));folder.mkdir()
fixture_args=[]
if a.reload:
 from insertion_fixture import make_fixture
 fixture_args=['--reload-fixture',str(make_fixture(a.pid,a.game,folder,weapon='deagle' if a.deagle_reload else 'single'))]
with (folder/'receiver-errors.txt').open('w') as errors:
 host=subprocess.Popen([str(root/'build/x64/KFVRNativeBridgeProbe.exe'),str(folder),'--width',str(a.width),'--height',str(a.height)]+(['--walking-aim'] if a.walking_aim else [])+(['--hud'] if a.hud else [])+(['--hands'] if a.hands else [])+(['--cadence'] if a.cadence else [])+(['--movement'] if a.movement else [])+(['--turning'] if a.turning else [])+(['--weapon'] if a.weapon else [])+(['--inventory'] if a.inventory else [])+(['--melee'] if a.melee else [])+(['--grenade'] if a.grenade else ['--trader'] if a.trader else ['--arms'] if a.arms else ['--weld'] if a.weld else ['--heal'] if a.heal else ['--dual-belt'] if a.dual_belt else ['--dual'] if a.dual else [])+(['--reload-interrupt'] if a.reload_interrupt else [])+(['--reload-alignment'] if a.reload_alignment else [])+(['--reload-insertion'] if a.reload_insertion else [])+fixture_args+(['--deagle-reload'] if a.deagle_reload else ['--reload'] if a.reload else []),stdout=subprocess.PIPE,stderr=errors,text=True,creationflags=subprocess.CREATE_NO_WINDOW)
 monitor=None;monitor_log=(folder/"observer-log.txt").open("w")
 try:
  if a.weapon:monitor=subprocess.Popen([sys.executable,str(root/'tools/observe_weapon_rig.py'),'--pid',str(a.pid),'--game',str(a.game),'--output',str(folder/'weapon-observations.jsonl'),'--seconds','45' if a.walking_aim else '115' if a.grenade else '160' if a.trader else '60' if a.hud else '65' if a.hands else '55' if a.arms else '70' if a.weld else '60' if a.heal else '100' if a.dual_belt else '85' if a.reload or a.dual else '45' if a.inventory or a.melee else '22'],stdout=monitor_log,stderr=subprocess.STDOUT,creationflags=subprocess.CREATE_NO_WINDOW)
  token=host.stdout.readline().strip()
  if len(token)!=32:raise RuntimeError('Receiver did not create a channel')
  summary=trace(a.pid,a.game,root,target_trial=True,camera_trial=True,channel_token=token,movement_trial=a.movement,turning_trial=a.turning,weapon_trial=a.weapon,duration_ms=35000 if a.walking_aim else 40000 if a.hud else 50000 if a.hands else 30000 if a.cadence else 100000 if a.grenade else 145000 if a.trader else 40000 if a.arms else 50000 if a.weld else 40000 if a.heal else 80000 if a.dual_belt else 70000 if a.reload else 65000 if a.dual else 35000 if a.inventory or a.melee else 15000)
  (folder/'trace-summary.json').write_text(json.dumps(summary,indent=2)+'\n')
  output=host.communicate(timeout=30)[0];(folder/'receiver-output.txt').write_text(output)
  if host.returncode:raise RuntimeError('Receiver failed; inspect '+str(folder))
  if a.movement and not a.weapon and not a.cadence:
   from validate_movement_trial import validate
   validate(pathlib.Path(summary['report']).parent/'native-input.json',folder/'controls.jsonl',folder/'movement-validation.json',turning=a.turning)
  if a.weapon:
   (folder/'weapon-observations.stop').touch();monitor.wait(timeout=5)
   if monitor.returncode:raise RuntimeError('Live weapon observer failed')
   if a.walking_aim:
    from validate_walking_aim import validate as validate_walk
    print(json.dumps(validate_walk(folder),indent=2))
   elif a.hud:
    from validate_wrist_display import validate as validate_hud
    print(json.dumps(validate_hud(folder),indent=2))
   elif a.hands:
    from validate_hand_trial import validate as validate_hands
    print(json.dumps(validate_hands(folder),indent=2))
   elif a.grenade:
    from validate_grenade_trial import validate as validate_grenade
    print(json.dumps(validate_grenade(folder),indent=2))
   elif a.trader:
    from validate_trader_trial import validate as validate_trader
    print(json.dumps(validate_trader(folder),indent=2))
   elif a.arms:
    from validate_arm_trial import validate as validate_arm
    print(json.dumps(validate_arm(folder),indent=2))
   elif a.weld:
    from validate_weld_trial import validate as validate_weld
    print(json.dumps(validate_weld(folder),indent=2))
   elif a.heal:
    from validate_heal_trial import validate as validate_heal
    print(json.dumps(validate_heal(folder),indent=2))
   elif a.dual_belt:
    from validate_dual_belt_trial import validate as validate_dual_belt
    print(json.dumps(validate_dual_belt(folder),indent=2))
   elif a.dual:
    from validate_dual_trial import validate as validate_dual
    print(json.dumps(validate_dual(folder),indent=2))
   elif a.melee:
    from validate_melee_trial import validate as validate_melee
    print(json.dumps(validate_melee(folder),indent=2))
   elif a.reload_alignment:
    from validate_reload_alignment import validate as validate_alignment
    print(json.dumps(validate_alignment(folder,weapon="deagle" if a.deagle_reload else "single"),indent=2))
   elif a.reload:
    from validate_reload_trial import validate as validate_reload
    print(json.dumps(validate_reload(folder,weapon="deagle" if a.deagle_reload else "single",interrupted=a.reload_interrupt,insertion=a.reload_insertion),indent=2))
   elif a.inventory:
    from validate_inventory_trial import validate as validate_inventory
    print(json.dumps(validate_inventory(folder),indent=2))
   else:
    from validate_weapon_trial import validate as validate_weapon
    print(json.dumps(validate_weapon(folder),indent=2))
  if a.inventory or a.heal or a.weld or a.dual or a.reload:
   from validate_grip_pose import validate as validate_grip
   required=('KFVRTrackedSyringe:R',) if a.heal else ('KFVRTrackedWelder:R',) if a.weld else ('KFVRTrackedDualies:R','KFVRTrackedDualies:L') if a.dual else ('KFVRTrackedSingle:R','KFVRTrackedDeagle:R','KFVRTrackedBullpup:R','KFVRTrackedBullpup:L')
   if a.reload:required=(('KFVRTrackedDeagle' if a.deagle_reload else 'KFVRTrackedSingle')+':L',)
   print(json.dumps(validate_grip(folder,required),indent=2))
  if a.weapon and not a.walking_aim and not a.hud and not a.hands and not a.arms and not a.trader and not a.grenade:
   from validate_status_display import validate as validate_status
   print(json.dumps(validate_status(folder,inventory=a.inventory,dual=a.dual,belt=a.dual_belt),indent=2))
  print((folder/'receiver.json').read_text());print('Receiver evidence: '+str(folder))
 finally:
  if monitor is not None and monitor.poll() is None:monitor.terminate();monitor.wait(timeout=5)
  if host.poll() is None:host.terminate();host.wait(timeout=5)
  monitor_log.close()
