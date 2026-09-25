"""Bounded actual native game-to-x64 receiver test; no headset required."""
import argparse,datetime,json,pathlib,subprocess
from trace_kf import trace
p=argparse.ArgumentParser();p.add_argument('--pid',type=int,required=True);p.add_argument('--game',type=pathlib.Path,required=True);a=p.parse_args()
root=pathlib.Path(__file__).resolve().parents[1];folder=root/'reports'/('native-bridge-'+datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%d-%H%M%S'));folder.mkdir()
with (folder/'receiver-errors.txt').open('w') as errors:
 host=subprocess.Popen([str(root/'build/x64/KFVRNativeBridgeProbe.exe'),str(folder)],stdout=subprocess.PIPE,stderr=errors,text=True,creationflags=subprocess.CREATE_NO_WINDOW)
 try:
  token=host.stdout.readline().strip()
  if len(token)!=32:raise RuntimeError('Receiver did not create a channel')
  trace(a.pid,a.game,root,target_trial=True,camera_trial=True,channel_token=token)
  output=host.communicate(timeout=30)[0];(folder/'receiver-output.txt').write_text(output)
  if host.returncode:raise RuntimeError('Receiver failed; inspect '+str(folder))
  print((folder/'receiver.json').read_text());print('Receiver evidence: '+str(folder))
 finally:
  if host.poll() is None:host.terminate();host.wait(timeout=5)
