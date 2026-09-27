"""Bounded native KF VR session with runtime resolution and optional controls."""
import argparse,datetime,json,pathlib,subprocess,time
from trace_kf import trace
p=argparse.ArgumentParser();p.add_argument('--pid',type=int,required=True);p.add_argument('--game',type=pathlib.Path,required=True);p.add_argument('--scale',type=int,default=100);p.add_argument('--movement',action='store_true');p.add_argument('--weapons',action='store_true');p.add_argument('--seconds',type=int,default=120);p.add_argument('--snap-degrees',type=float,default=30);a=p.parse_args();a.movement=a.movement or a.weapons
if not 15<=a.seconds<=180:raise ValueError('Session duration must be 15..180 seconds')
if not 15<=a.snap_degrees<=90:raise ValueError('Snap angle must be 15..90 degrees')
if not 25<=a.scale<=100:raise ValueError('Bounded native trial scale must be 25..100 percent')
root=pathlib.Path(__file__).resolve().parents[1];folder=root/'reports'/('native-headset-'+datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%d-%H%M%S'));folder.mkdir()
with (folder/'host.json').open('w') as output,(folder/'host-errors.txt').open('w') as errors:
 host=subprocess.Popen([str(root/'build/x64/KFVRXrHost.exe'),'--ipc','--frame-budget','50','--render-scale',str(a.scale),'--wait-first-pair','--seconds',str(a.seconds+3)],stdout=output,stderr=errors,creationflags=subprocess.CREATE_NO_WINDOW)
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
  trace(a.pid,a.game,root,target_trial=True,camera_trial=True,channel_token=token,movement_trial=a.movement,turning_trial=a.movement,snap_degrees=a.snap_degrees,duration_ms=a.seconds*1000,weapon_trial=a.weapons)
  host.wait(timeout=40)
  if host.returncode:raise RuntimeError('XR host failed: '+str(folder))
 finally:
  if host.poll() is None:host.terminate();host.wait(timeout=5)
report=json.loads((folder/'host.json').read_text());print(json.dumps(report,indent=2));print('Headset evidence: '+str(folder))
if not report.get('submitted_pairs'):raise RuntimeError('XR submitted no native pairs')
