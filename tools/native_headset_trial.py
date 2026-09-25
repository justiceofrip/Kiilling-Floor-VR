"""Brief real KF stereo/head-tracking test using the accepted native bridge."""
import argparse,datetime,json,pathlib,subprocess,time
from trace_kf import trace
p=argparse.ArgumentParser();p.add_argument('--pid',type=int,required=True);p.add_argument('--game',type=pathlib.Path,required=True);p.add_argument('--scale',type=int,default=40);a=p.parse_args()
if not 25<=a.scale<=75:raise ValueError('Bounded native trial scale must be 25..75 percent')
root=pathlib.Path(__file__).resolve().parents[1];folder=root/'reports'/('native-headset-'+datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%d-%H%M%S'));folder.mkdir()
with (folder/'host.json').open('w') as output,(folder/'host-errors.txt').open('w') as errors:
 host=subprocess.Popen([str(root/'build/x64/KFVRXrHost.exe'),'--ipc','--frame-budget','50','--render-scale',str(a.scale),'--wait-first-pair','--seconds','18'],stdout=output,stderr=errors,creationflags=subprocess.CREATE_NO_WINDOW)
 try:
  deadline=time.monotonic()+10;token=None
  while time.monotonic()<deadline and host.poll() is None:
   for line in (folder/'host-errors.txt').read_text(errors='replace').splitlines():
    if line.startswith('frame_channel='):token=line.split('=',1)[1].strip()
   if token:break
   time.sleep(.05)
  if not token:raise RuntimeError('XR host did not create a channel: '+str(folder))
  focus_deadline=time.monotonic()+18
  while time.monotonic()<focus_deadline and host.poll() is None:
   if 'xr_session_state=5' in (folder/'host-errors.txt').read_text(errors='replace'):break
   time.sleep(.05)
  else:raise RuntimeError('XR did not gain headset focus; native injection skipped: '+str(folder))
  trace(a.pid,a.game,root,target_trial=True,camera_trial=True,channel_token=token)
  host.wait(timeout=40)
  if host.returncode:raise RuntimeError('XR host failed: '+str(folder))
 finally:
  if host.poll() is None:host.terminate();host.wait(timeout=5)
report=json.loads((folder/'host.json').read_text());print(json.dumps(report,indent=2));print('Headset evidence: '+str(folder))
if not report.get('submitted_pairs'):raise RuntimeError('XR submitted no native pairs')
