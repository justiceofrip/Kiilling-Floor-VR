"""Bounded actual native game-to-x64 receiver test; no headset required."""
import argparse,datetime,json,pathlib,subprocess
from trace_kf import trace
p=argparse.ArgumentParser();p.add_argument('--pid',type=int,required=True);p.add_argument('--game',type=pathlib.Path,required=True);p.add_argument('--movement',action='store_true');a=p.parse_args()
root=pathlib.Path(__file__).resolve().parents[1];folder=root/'reports'/('native-bridge-'+datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%d-%H%M%S'));folder.mkdir()
with (folder/'receiver-errors.txt').open('w') as errors:
 host=subprocess.Popen([str(root/'build/x64/KFVRNativeBridgeProbe.exe'),str(folder)]+(['--movement'] if a.movement else []),stdout=subprocess.PIPE,stderr=errors,text=True,creationflags=subprocess.CREATE_NO_WINDOW)
 try:
  token=host.stdout.readline().strip()
  if len(token)!=32:raise RuntimeError('Receiver did not create a channel')
  summary=trace(a.pid,a.game,root,target_trial=True,camera_trial=True,channel_token=token,movement_trial=a.movement)
  output=host.communicate(timeout=30)[0];(folder/'receiver-output.txt').write_text(output)
  if host.returncode:raise RuntimeError('Receiver failed; inspect '+str(folder))
  if a.movement:
   from validate_movement_trial import validate
   validate(pathlib.Path(summary['report']).parent/'native-input.json',folder/'controls.jsonl',folder/'movement-validation.json')
  print((folder/'receiver.json').read_text());print('Receiver evidence: '+str(folder))
 finally:
  if host.poll() is None:host.terminate();host.wait(timeout=5)
