"""Opt-in bounded native trace; standalone KF only. No auto-start installation."""
import argparse,ctypes,datetime,hashlib,json,pathlib,struct,subprocess
from observe_kf import Observer
import pefile
from native_profile import TARGETS, AUDITED_SHA256

def trace(pid,game,root,target_trial=False,camera_trial=False,stereo_trial=False):
 stamp=datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%d-%H%M%S-%f')
 directory=root/'reports'/('native-trace-'+stamp);directory.mkdir(parents=True)
 observer=Observer(pid,game)
 try:
  snapshot=observer.snapshot();(directory/'before.json').write_text(json.dumps(snapshot,indent=2))
  controllers=[r for r in snapshot['observations'] if r['class']=='KFPlayerController' and r['values'].get('PlayerController.Player')]
  if len(controllers)!=1:raise ValueError('Need exactly one local KF controller')
  controller=controllers[0];level_address=controller['values']['Actor.Level']['address'];viewport=int(controller['values']['PlayerController.Player']['address'],16)
  level=next(r for r in snapshot['observations'] if r['address']==level_address)
  if level['values']['LevelInfo.NetMode']!=0:raise ValueError('Experimental native trace requires NM_Standalone')
  p=snapshot['properties'];config=struct.pack('<15I',884,5,12000,180,int(controller['address'],16),controller['index'],viewport,int(level_address,16),level['index'],p['Actor.Level']['offset'],p['PlayerController.Player']['offset'],p['LevelInfo.NetMode']['offset'],int(target_trial),int(camera_trial),int(stereo_trial))
  names=TARGETS
  evidence=[]
  for file,name in names:
   path=game/'System'/file
   if hashlib.sha256(path.read_bytes()).hexdigest()!=AUDITED_SHA256[file]:raise ValueError('Unaudited native binary: '+file)
   pe=pefile.PE(str(path));symbol=next(s for s in pe.DIRECTORY_ENTRY_EXPORT.symbols if s.name and s.name.decode()==name);base=observer.modules[file.lower()][0];pe.relocate_image(base);expected=pe.get_data(symbol.address,16)
   if observer.read(base+symbol.address,16)!=expected:raise ValueError('Native target already modified: '+name)
   if file=='D3D9Drv.dll':
    body=pe.get_data(symbol.address,0x120)
    if bytes.fromhex('8b8e5c490000') not in body or bytes.fromhex('ff5244') not in body:raise ValueError('D3D9 device/Present relationship not verified')
   config+=expected;evidence.append(dict(file=file,sha256=hashlib.sha256(path.read_bytes()).hexdigest(),name=name,rva=symbol.address,expected=expected.hex(),address=base+symbol.address))
  target_evidence=[]
  extra=[('D3D9Drv.dll',rva) for rva in [0x18860,0x1b240,0x189d0,0x17150,0x19100]]+[('Engine.dll',0x1bbce0),('Engine.dll',0x1bbd50)]
  for file,rva in extra:
   if not target_trial:config+=bytes(16);continue
   path=game/'System'/file;pe=pefile.PE(str(path));base=observer.modules[file.lower()][0];pe.relocate_image(base);expected=pe.get_data(rva,16)
   if observer.read(base+rva,16)!=expected:raise ValueError('Native target interface already modified')
   config+=expected;target_evidence.append(dict(file=file,rva=rva,expected=expected.hex()))
  for rva in [0x1a5ab0,0x1a2f30,0x1a9010]:
   if not camera_trial:config+=bytes(16);continue
   pe=pefile.PE(str(game/'System/Engine.dll'));base=observer.modules['engine.dll'][0];pe.relocate_image(base);expected=pe.get_data(rva,16)
   if observer.read(base+rva,16)!=expected:raise ValueError('Camera method already modified')
   config+=expected;target_evidence.append(dict(file='Engine.dll',rva=rva,expected=expected.hex()))
  (directory/'target-evidence.json').write_text(json.dumps(target_evidence,indent=2))
  report_path=directory/'trace.json';encoded=(str(report_path)+'\0').encode('utf-16le')
  if len(encoded)>520:raise ValueError('Report path too long')
  config+=encoded.ljust(520,b'\0');request_path=directory/'request.bin';request_path.write_bytes(config)
  source=root/'build/x86/KFVRNativeProbe.dll';dll=directory/'KFVRNativeProbe.dll';dll.write_bytes(source.read_bytes())
  launcher=root/'build/x86/KFVRNativeTrace.exe'
  (directory/'payload.json').write_text(json.dumps(dict(dll_sha256=hashlib.sha256(dll.read_bytes()).hexdigest(),launcher_sha256=hashlib.sha256(launcher.read_bytes()).hexdigest(),targets=evidence),indent=2))
  result=subprocess.run([str(launcher),'--game',str(game),'--dll',str(dll),'--request',str(request_path)],capture_output=True,text=True,timeout=30)
  (directory/'launcher.txt').write_text(result.stdout+result.stderr)
  restored=all(observer.read(e['address'],16).hex()==e['expected'] for e in evidence)
  summary=dict(pid=pid,launcher_exit=result.returncode,entry_bytes_restored=restored,game_files_modified=False,report=str(report_path),module_resident_until_exit=True)
  if report_path.exists():
   report=json.loads(report_path.read_text());summary.update(frames=len(report['frames']),hooks_disabled=report['hooks_disabled'],render_counts=sorted(set(f['renders'] for f in report['frames'])),simulation_advanced_inside_draw=any(f['ticks_before']!=f['ticks_after'] for f in report['frames']))
  if report_path.exists():
   totals={};phases={0:'draw_outside_player',1:'player_outside_level',2:'level',3:'private_eye'}
   for frame in report['frames']:
    for call in frame.get('scripts',[]):
     key=observer.name(call['owner_name_id'])+'.'+observer.name(call['name_id'])+'@'+phases[call['phase']]
     totals[key]=totals.get(key,0)+call['calls']
   (directory/'render-activity.json').write_text(json.dumps(dict(script_calls=totals,overflow=sum(f.get('script_overflow',0) for f in report['frames']),actor_animation_calls_inside_draw=sum(f.get('actor_animation_calls_inside_draw',0) for f in report['frames']),skeletal_animation_calls_inside_draw=sum(f.get('skeletal_animation_calls_inside_draw',0) for f in report['frames']),skeletal_getframe_calls=sum(f.get('skeletal_getframe_calls',0) for f in report['frames']),level_renders=sum(f.get('level_renders',0) for f in report['frames'])),indent=2))
  target_passed=True
  if target_trial:
   target_path=directory/'native-target.json';trial=json.loads(target_path.read_text()) if target_path.exists() else {}
   flags=['attempted','ready','distinct_surface','color_target_restored','depth_target_restored','viewport_restored','state_stack_restored','cached_state_restored','constructor_canary_intact']
   target_passed=all(trial.get(k) is True for k in flags) and trial.get('hresult')==0 and trial.get('verified_pixels')==512*384
   summary['target_trial_passed']=target_passed
  camera_passed=True
  if camera_trial:
   camera_path=directory/'native-camera-trial.json';camera=json.loads(camera_path.read_text()) if camera_path.exists() else {}
   camera_passed=camera.get('ready') is True and camera.get('prerequisites') is True and camera.get('original_scene_unchanged') is True and len(camera.get('cases',[]))==5
   for row in camera.get('cases',[]):
    camera_passed=camera_passed and all(row.get(k) is True for k in ['fields_match','canaries_intact','finite']) and all(row.get(k,1e10)<0.02 for k in ['camera_max_error','view_max_error','projection_inverse_max_error','view_projection_max_error'])
   summary['camera_trial_passed']=camera_passed
  stereo_passed=True
  if stereo_trial:
   stereo_path=directory/'native-stereo.json';stereo=json.loads(stereo_path.read_text()) if stereo_path.exists() else {}
   stereo_passed=all(stereo.get(k) is True for k in ['ready','state_restored','canaries_intact','cameras_preserved','distinct_surfaces']) and stereo.get('hresult')==0 and stereo.get('renders')==2
   if stereo_passed:
    left=(directory/'native-left.bmp').read_bytes()[54:];right=(directory/'native-right.bmp').read_bytes()[54:]
    unique=[len(set(data[i:i+3] for i in range(0,len(data),4))) for data in [left,right]]
    differing=sum(left[i:i+3]!=right[i:i+3] for i in range(0,len(left),4))
    stereo_passed=len(left)==len(right)==512*384*4 and min(unique)>64 and differing>1024
    summary.update(eye_unique_colors=unique,differing_eye_pixels=differing)
   summary['stereo_trial_passed']=stereo_passed
  (directory/'summary.json').write_text(json.dumps(summary,indent=2));print(json.dumps(summary,indent=2))
  if result.returncode or not restored or not target_passed or not camera_passed or not stereo_passed:raise RuntimeError('Native trace or restoration failed; inspect saved reports')
 finally:observer.close()
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('--pid',type=int,required=True);p.add_argument('--game',type=pathlib.Path,required=True);p.add_argument('--target-trial',action='store_true');p.add_argument('--camera-trial',action='store_true');p.add_argument('--stereo-trial',action='store_true');a=p.parse_args();trace(a.pid,a.game,pathlib.Path(__file__).resolve().parents[1],a.target_trial or a.camera_trial or a.stereo_trial,a.camera_trial or a.stereo_trial,a.stereo_trial)

