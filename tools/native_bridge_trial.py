"""Bounded actual native game-to-x64 receiver test; no headset required."""
import argparse,datetime,json,pathlib,subprocess,sys
from trace_kf import trace
p=argparse.ArgumentParser();p.add_argument('--pid',type=int,required=True);p.add_argument('--game',type=pathlib.Path,required=True);p.add_argument('--movement',action='store_true');p.add_argument('--weapon',action='store_true');p.add_argument('--inventory',action='store_true');p.add_argument('--reload',action='store_true');p.add_argument('--deagle-reload',action='store_true');p.add_argument('--turning',action='store_true');p.add_argument('--width',type=int,default=640);p.add_argument('--height',type=int,default=480);a=p.parse_args();a.reload=a.reload or a.deagle_reload;a.weapon=a.weapon or a.inventory or a.reload;a.movement=a.movement or a.turning or a.weapon
root=pathlib.Path(__file__).resolve().parents[1];folder=root/'reports'/('native-bridge-'+datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%d-%H%M%S'));folder.mkdir()
with (folder/'receiver-errors.txt').open('w') as errors:
 host=subprocess.Popen([str(root/'build/x64/KFVRNativeBridgeProbe.exe'),str(folder),'--width',str(a.width),'--height',str(a.height)]+(['--movement'] if a.movement else [])+(['--turning'] if a.turning else [])+(['--weapon'] if a.weapon else [])+(['--inventory'] if a.inventory else [])+(['--deagle-reload'] if a.deagle_reload else ['--reload'] if a.reload else []),stdout=subprocess.PIPE,stderr=errors,text=True,creationflags=subprocess.CREATE_NO_WINDOW)
 monitor=None;monitor_log=(folder/"observer-log.txt").open("w")
 try:
  if a.weapon:monitor=subprocess.Popen([sys.executable,str(root/'tools/observe_weapon_rig.py'),'--pid',str(a.pid),'--game',str(a.game),'--output',str(folder/'weapon-observations.jsonl'),'--seconds','85' if a.reload else '45' if a.inventory else '22'],stdout=monitor_log,stderr=subprocess.STDOUT,creationflags=subprocess.CREATE_NO_WINDOW)
  token=host.stdout.readline().strip()
  if len(token)!=32:raise RuntimeError('Receiver did not create a channel')
  summary=trace(a.pid,a.game,root,target_trial=True,camera_trial=True,channel_token=token,movement_trial=a.movement,turning_trial=a.turning,weapon_trial=a.weapon,duration_ms=70000 if a.reload else 35000 if a.inventory else 15000)
  output=host.communicate(timeout=30)[0];(folder/'receiver-output.txt').write_text(output)
  if host.returncode:raise RuntimeError('Receiver failed; inspect '+str(folder))
  if a.movement and not a.weapon:
   from validate_movement_trial import validate
   validate(pathlib.Path(summary['report']).parent/'native-input.json',folder/'controls.jsonl',folder/'movement-validation.json',turning=a.turning)
  if a.weapon:
   (folder/'weapon-observations.stop').touch();monitor.wait(timeout=5)
   if monitor.returncode:raise RuntimeError('Live weapon observer failed')
   if a.reload:
    from validate_reload_trial import validate as validate_reload
    print(json.dumps(validate_reload(folder,weapon="deagle" if a.deagle_reload else "single"),indent=2))
   elif a.inventory:
    from validate_inventory_trial import validate as validate_inventory
    print(json.dumps(validate_inventory(folder),indent=2))
   else:
    from validate_weapon_trial import validate as validate_weapon
    print(json.dumps(validate_weapon(folder),indent=2))
  print((folder/'receiver.json').read_text());print('Receiver evidence: '+str(folder))
 finally:
  if monitor is not None and monitor.poll() is None:monitor.terminate();monitor.wait(timeout=5)
  if host.poll() is None:host.terminate();host.wait(timeout=5)
  monitor_log.close()
