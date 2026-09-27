"""Opt-in bounded native trace; standalone KF only. No auto-start installation."""
import argparse,ctypes,datetime,hashlib,json,math,pathlib,struct,subprocess
from observe_kf import Observer
import pefile
from native_profile import TARGETS, AUDITED_SHA256

def validate_eye_packet(camera,eye_data,pose_trial,asymmetric_trial):
 summary={}
 native_body=camera['cases'][0]['matrices'][16:32]
 matrices_valid=len(eye_data)==2 and all(len(e)==96 and all(isinstance(v,(float,int)) and math.isfinite(v) for v in e) for e in eye_data)
 if matrices_valid:
  centers=[e[28:31] for e in eye_data];separation=math.dist(*centers)
  midpoint=[(a+b)/2 for a,b in zip(*centers)];shift=[midpoint[i]-native_body[12+i] for i in range(3)]
  expected_shift=[sum(native_body[row*4+i]*amount for row,amount in enumerate([6,4,5])) for i in range(3)] if pose_trial else [0,0,0]
  relative_trace=sum(eye_data[0][16+i*4+j]*native_body[i*4+j] for i in range(3) for j in range(3))
  angle=math.acos(max(-1,min(1,(relative_trace-1)/2)))
  matrices_valid=abs(separation-3.2)<.002 and max(abs(a-b) for a,b in zip(shift,expected_shift))<.002 and abs(angle-(.35 if pose_trial else 0))<.002
  matrices_valid=matrices_valid and max(abs(eye_data[0][16+i*4+j]-eye_data[1][16+i*4+j]) for i in range(3) for j in range(3))<.00001
  matrices_valid=matrices_valid and max(abs((centers[1][i]-centers[0][i])-3.2*eye_data[0][16+i]) for i in range(3))<.002
  expected_offsets=[.05,-.05] if asymmetric_trial or pose_trial else [0,0]
  matrices_valid=matrices_valid and all(abs(e[40]-x)<.00001 for e,x in zip(eye_data,expected_offsets))
  summary.update(eye_separation_world_units=separation,head_shift_world_units=shift,head_angle_radians=angle)
 summary['camera_packet_valid']=matrices_valid
 return summary

def trace(pid,game,root,target_trial=False,camera_trial=False,stereo_trial=False,asymmetric_trial=False,pose_trial=False,channel_token=None,input_trial=False,movement_trial=False,turning_trial=False,snap_degrees=30,duration_ms=15000,weapon_trial=False):
 if not 100<=duration_ms<=(180000 if channel_token else 15000):raise ValueError("Invalid native session duration")
 movement_trial=movement_trial or turning_trial or weapon_trial
 if movement_trial and not channel_token:raise ValueError("Movement requires a bounded bridge session")
 input_trial=input_trial or movement_trial
 stamp=datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%d-%H%M%S-%f')
 directory=root/'reports'/('native-trace-'+stamp);directory.mkdir(parents=True)
 observer=Observer(pid,game)
 try:
  input_names=['aBaseX','aBaseY','aBaseZ','aMouseX','aMouseY','aForward','aTurn','aStrafe','aUp','aLookUp']
  snapshot=observer.snapshot({'PlayerController':set(input_names),'Pawn':{'Controller'}}) if input_trial else observer.snapshot();(directory/'before.json').write_text(json.dumps(snapshot,indent=2))
  controllers=[r for r in snapshot['observations'] if r['class']=='KFPlayerController' and r['values'].get('PlayerController.Player')]
  if channel_token and stereo_trial:raise ValueError('Bridge and one-shot stereo trials are separate modes')
  if len(controllers)!=1:raise ValueError('Need exactly one local KF controller')
  controller=controllers[0];level_address=controller['values']['Actor.Level']['address'];viewport=int(controller['values']['PlayerController.Player']['address'],16)
  level=next(r for r in snapshot['observations'] if r['address']==level_address)
  if level['values']['LevelInfo.NetMode']!=0:raise ValueError('Experimental native trace requires NM_Standalone')
  p=snapshot['properties'];config=struct.pack('<15I',1112,11,duration_ms if channel_token else min(duration_ms,12000),240 if channel_token else 180,int(controller['address'],16),controller['index'],viewport,int(level_address,16),level['index'],p['Actor.Level']['offset'],p['PlayerController.Player']['offset'],p['LevelInfo.NetMode']['offset'],int(target_trial),int(camera_trial),(3 if pose_trial else 2 if asymmetric_trial else int(stereo_trial)))
  images={}
  def installed_image(file):
   if file not in images:
    data=(game/'System'/file).read_bytes()
    if hashlib.sha256(data).hexdigest()!=AUDITED_SHA256[file]:raise ValueError('Unaudited native binary: '+file)
    pe=pefile.PE(data=data);base=observer.modules[file.lower()][0];pe.relocate_image(base);images[file]=(pe,base)
   return images[file]
  names=TARGETS
  evidence=[]
  for file,name in names:
   pe,base=installed_image(file);symbol=next(s for s in pe.DIRECTORY_ENTRY_EXPORT.symbols if s.name and s.name.decode()==name);expected=pe.get_data(symbol.address,16)
   if observer.read(base+symbol.address,16)!=expected:raise ValueError('Native target already modified: '+name)
   if file=='D3D9Drv.dll':
    body=pe.get_data(symbol.address,0x120)
    if bytes.fromhex('8b8e5c490000') not in body or bytes.fromhex('ff5244') not in body:raise ValueError('D3D9 device/Present relationship not verified')
   config+=expected;evidence.append(dict(file=file,sha256=AUDITED_SHA256[file],name=name,rva=symbol.address,expected=expected.hex(),address=base+symbol.address))
  target_evidence=[]
  extra=[('D3D9Drv.dll',rva) for rva in [0x18860,0x1b240,0x189d0,0x17150,0x19100]]+[('Engine.dll',0x1bbce0),('Engine.dll',0x1bbd50)]
  for file,rva in extra:
   if not target_trial:config+=bytes(16);continue
   pe,base=installed_image(file);expected=pe.get_data(rva,16)
   if observer.read(base+rva,16)!=expected:raise ValueError('Native target interface already modified')
   config+=expected;target_evidence.append(dict(file=file,rva=rva,expected=expected.hex()))
  for rva in [0x1a5ab0,0x1a2f30,0x1a9010]:
   if not camera_trial:config+=bytes(16);continue
   pe,base=installed_image('Engine.dll');expected=pe.get_data(rva,16)
   if observer.read(base+rva,16)!=expected:raise ValueError('Camera method already modified')
   config+=expected;target_evidence.append(dict(file='Engine.dll',rva=rva,expected=expected.hex()))
  (directory/'target-evidence.json').write_text(json.dumps(target_evidence,indent=2))
  report_path=directory/'trace.json';encoded=(str(report_path)+'\0').encode('utf-16le')
  if len(encoded)>520:raise ValueError('Report path too long')
  config+=encoded.ljust(520,b'\0')
  if channel_token and (len(channel_token)!=32 or any(c not in '0123456789abcdef' for c in channel_token)):raise ValueError('Invalid channel token')
  config+=struct.pack('<If',int(bool(channel_token)),50.0)+(str(channel_token or '')+'\0').encode('utf-16le').ljust(68,b'\0')
  input_offsets=[0]*10;tick_name=0;input_expected=bytes(16);input_evidence=[]
  if input_trial:
   for i,name in enumerate(input_names):
    prop=p['PlayerController.'+name]
    if prop['type']!='FloatProperty' or prop['size']!=4:raise ValueError('Invalid input property')
    input_offsets[i]=prop['offset']
   tick=next(row for row in observer.rows if row['class']=='Function' and row['name']=='PlayerTick' and row['outer']=='KFPlayerController')
   tick_name=struct.unpack('<I',observer.read(int(tick['address'],16)+0x24,4))[0]
   pe,base=installed_image('Engine.dll');rva=0xee110;input_expected=pe.get_data(rva,16)
   if observer.read(base+rva,16)!=input_expected:raise ValueError('ReadInput entry is already modified')
   input_evidence.append(dict(address=base+rva,expected=input_expected.hex()))
  config+=struct.pack('<12I',int(input_trial),*input_offsets,tick_name)+input_expected
  movement_offsets=[0]*5
  if movement_trial:
   for i,(name,typ,size) in enumerate([('Controller.Pawn','ObjectProperty',4),('Pawn.Health','IntProperty',4),('Pawn.Controller','ObjectProperty',4),('LevelInfo.Pauser','ObjectProperty',4),('Actor.Location','StructProperty',12)]):
    prop=p[name]
    if prop['type']!=typ or prop['size']!=size or not 44<=prop['offset']<=8192:raise ValueError('Invalid movement property: '+name)
    movement_offsets[i]=prop['offset']
   pawn_info=controller['values'].get('Controller.Pawn')
   if not pawn_info:raise ValueError('Join a solo match and spawn before starting VR controls')
   pawn=next((row for row in snapshot['observations'] if row['address']==pawn_info['address']),None)
   if not pawn or pawn['values'].get('Pawn.Health',0)<=0 or level['values'].get('LevelInfo.Pauser'):raise ValueError('VR controls require a living player in an unpaused solo match')
  config+=struct.pack('<6I',int(movement_trial),*movement_offsets)
  rotation_offset=rotation_name=0;call_expected=bytes(16)
  if turning_trial:
   prop=p['Actor.Rotation']
   if prop['type']!='StructProperty' or prop['size']!=12:raise ValueError('Invalid native rotation property')
   rotation_offset=prop['offset']
   rotate=next(row for row in observer.rows if row['class']=='Function' and row['name']=='UpdateRotation' and row['outer']=='KFPlayerController')
   rotation_name=struct.unpack('<I',observer.read(int(rotate['address'],16)+0x24,4))[0]
   tick_parameter=next(row for row in observer.rows if row['class']=='FloatProperty' and row['name']=='DeltaTime' and observer.path(int(row['address'],16)).endswith('KFPlayerController.PlayerTick.DeltaTime'))
   metadata=observer.read(int(tick_parameter['address'],16),80)
   if struct.unpack_from('<I',metadata,76)[0]!=0 or struct.unpack_from('<I',metadata,60)[0]!=4:raise ValueError('Unverified PlayerTick delta parameter')
   pe,base=installed_image('Core.dll');rva=0x326d0;call_expected=pe.get_data(rva,16)
   if observer.read(base+rva,16)!=call_expected:raise ValueError('CallFunction entry is already modified')
   input_evidence.append(dict(address=base+rva,expected=call_expected.hex()))
  if not 15<=snap_degrees<=90:raise ValueError('Snap angle must be 15..90 degrees')
  rig_data=[0]*7
  if weapon_trial:
   module=game/'System/KFVRGameplay.u'
   if not module.exists() or hashlib.sha256(module.read_bytes()).digest()!=hashlib.sha256((root/'build/unrealscript/KFVRGameplay.u').read_bytes()).digest():raise ValueError('Stage the current KFVR gameplay package and start a fresh KF process')
   rigs=[row for row in observer.rows if row['class']=='KFVRWeaponRig' and struct.unpack('<I',observer.read(int(row['address'],16)+p['Actor.Owner']['offset'],4))[0]==int(controller['address'],16)]
   if len(rigs)!=1:raise ValueError('Launch a standalone match with KFVRGameplay.KFVRDevMutator and spawn first')
   rig=rigs[0];address=int(rig['address'],16);cls=struct.unpack('<I',observer.read(address+0x28,4))[0]
   field=next(row for row in observer.rows if row['class']=='StructProperty' and row['name']=='Tracking' and row['outer']=='KFVRWeaponRig')
   metadata=observer.read(int(field['address'],16),80);offset=struct.unpack_from('<I',metadata,76)[0]
   if struct.unpack_from('<II',metadata,56)!=(1,124) or not 44<=offset<=8192:raise ValueError('Invalid tracked weapon packet')
   fields=[('LeftPosition',0,12),('LeftRotation',12,12),('RightPosition',24,12),('RightRotation',36,12),('LeftTrigger',48,4),('LeftGrip',52,4),('RightTrigger',56,4),('RightGrip',60,4),('LeftThumb',64,4),('LeftIndex',68,4),('LeftCurl',72,4),('RightThumb',76,4),('RightIndex',80,4),('RightCurl',84,4),('HeadPosition',88,12),('BodyRotation',100,12),('UnitsPerMetre',112,4),('Flags',116,4),('Serial',120,4)]
   for name,at,size in fields:
    row=next(row for row in observer.rows if row['name']==name and observer.path(int(row['address'],16)).endswith('KFVRWeaponRig.TrackingPacket.'+name))
    data=observer.read(int(row['address'],16),80)
    expected='StructProperty' if size==12 else 'IntProperty' if name in ['Flags','Serial'] else 'FloatProperty'
    if row['class']!=expected or struct.unpack_from('<II',data,56)!=(1,size) or struct.unpack_from('<I',data,76)[0]!=at:raise ValueError('Tracked weapon member mismatch: '+name)
   feedback=next(row for row in observer.rows if row['class']=='StructProperty' and row['name']=='Feedback' and row['outer']=='KFVRWeaponRig')
   data=observer.read(int(feedback['address'],16),80);feedback_offset=struct.unpack_from('<I',data,76)[0]
   if struct.unpack_from('<II',data,56)!=(1,20) or not 44<=feedback_offset<=8192:raise ValueError('Invalid weapon feedback packet')
   for name,at,typ in [('Serial',0,'IntProperty'),('LeftAmplitude',4,'FloatProperty'),('RightAmplitude',8,'FloatProperty'),('LeftMs',12,'IntProperty'),('RightMs',16,'IntProperty')]:
    row=next(row for row in observer.rows if row['name']==name and observer.path(int(row['address'],16)).endswith('KFVRWeaponRig.FeedbackPacket.'+name))
    data=observer.read(int(row['address'],16),80)
    if row['class']!=typ or struct.unpack_from('<II',data,56)!=(1,4) or struct.unpack_from('<I',data,76)[0]!=at:raise ValueError('Weapon feedback member mismatch')
   tick=next(row for row in observer.rows if row['class']=='Function' and row['name']=='Tick' and row['outer']=='KFVRWeaponRig')
   tick_name_rig=struct.unpack('<I',observer.read(int(tick['address'],16)+0x24,4))[0]
   rotation_offset=p['Actor.Rotation']['offset']
   if p['Actor.Rotation']['type']!='StructProperty' or p['Actor.Rotation']['size']!=12:raise ValueError('Invalid rotation field')
   rig_data=[address,rig['index'],cls,offset,tick_name_rig,p['Actor.Owner']['offset'],feedback_offset]
  config+=struct.pack('<3If',int(turning_trial),rotation_offset,rotation_name,snap_degrees)+call_expected
  config+=struct.pack('<8I',int(weapon_trial),*rig_data)
  assert len(config)==1112
  request_path=directory/'request.bin';request_path.write_bytes(config)
  source=root/'build/x86/KFVRNativeProbe.dll';dll=directory/'KFVRNativeProbe.dll';dll.write_bytes(source.read_bytes())
  launcher=root/'build/x86/KFVRNativeTrace.exe'
  (directory/'payload.json').write_text(json.dumps(dict(dll_sha256=hashlib.sha256(dll.read_bytes()).hexdigest(),launcher_sha256=hashlib.sha256(launcher.read_bytes()).hexdigest(),targets=evidence),indent=2))
  result=subprocess.run([str(launcher),'--game',str(game),'--dll',str(dll),'--request',str(request_path)],capture_output=True,text=True,timeout=duration_ms/1000+20)
  (directory/'launcher.txt').write_text(result.stdout+result.stderr)
  restored=all(observer.read(e['address'],16).hex()==e['expected'] for e in evidence+input_evidence)
  summary=dict(pid=pid,launcher_exit=result.returncode,entry_bytes_restored=restored,native_trace_modified_game_files=False,development_package_required=weapon_trial,report=str(report_path),module_resident_until_exit=True)
  if report_path.exists():
   report=json.loads(report_path.read_text());summary.update(frames=len(report['frames']),hooks_disabled=report['hooks_disabled'],render_counts=sorted(set(f['renders'] for f in report['frames'])),engine_ticks_observed_inside_draw=any(f['ticks_before']!=f['ticks_after'] for f in report['frames']))
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
    camera_passed=camera_passed and all(row.get(k) is True for k in ['fields_match','canaries_intact','finite']) and all(row.get(k,1e10)<0.02 for k in ['camera_max_error','view_max_error','projection_inverse_max_error','view_projection_max_error','derived_max_error'])
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
   if stereo_passed:
    eye_data=json.loads((directory/'native-eye-cameras.json').read_text())['eyes']
    metrics=validate_eye_packet(camera,eye_data,pose_trial,asymmetric_trial);summary.update(metrics);stereo_passed=metrics['camera_packet_valid']
   summary['stereo_trial_passed']=stereo_passed
  input_passed=True
  if input_trial:
   input_path=directory/'native-input.json';input_report=json.loads(input_path.read_text()) if input_path.exists() else {};samples=input_report.get('samples',[])
   input_passed=len(samples)>=10 and all(s['finite'] for s in samples) and sum(s['before_tick_observed'] and s['after_tick_observed'] for s in samples)>=10 and len(set(s['thread'] for s in samples))==1
   summary.update(input_trial_passed=input_passed,input_samples=len(samples),input_tick_observations=sum(s['before_tick_observed'] and s['after_tick_observed'] for s in samples))
  movement_passed=True
  if movement_trial:
   moves=input_report.get('movement',[])
   movement_passed=bool(moves) and input_report.get('movement_writes',0)>0 and input_report.get('restore_failures')==0 and input_report.get('movement_writes',0)==input_report.get('movement_restores',-1) and all(row['restored'] and row['finite'] for row in moves)
   summary.update(movement_trial_passed=movement_passed,movement_samples=len(moves),nonzero_movement_samples=sum(abs(row['forward'])+abs(row['strafe'])>.001 for row in moves))
  turning_passed=True
  if turning_trial:
   turning_passed=input_report.get('turn_failures')==0 and input_report.get('turn_writes',-1)==input_report.get('turn_restores',-2) and all(row.get('turn_restored') for row in moves)
   summary.update(turning_trial_passed=turning_passed,snap_turns=input_report.get('turn_writes',0))
  bridge_passed=True
  if channel_token:
   bridge_path=directory/'native-bridge.json';bridge=json.loads(bridge_path.read_text()) if bridge_path.exists() else {}
   bridge_passed=all(bridge.get(k) is True for k in ['closed','reset_hook_restored','resources_released']) and bridge.get('consumed_pairs',0)>0 and bridge.get('reason') in ['duration_finished','host_closed']
   summary.update(bridge_trial_passed=bridge_passed,bridge=bridge)
  weapon_passed=True
  if weapon_trial:
   weapon=json.loads((directory/'native-weapon.json').read_text());weapon_passed=weapon.get('valid',0)>10 and weapon.get('failed',1)==0
   summary.update(weapon_pose_delivery_passed=weapon_passed,weapon_valid_packets=weapon.get('valid',0))
  (directory/'summary.json').write_text(json.dumps(summary,indent=2));print(json.dumps(summary,indent=2))
  if result.returncode or not restored or not target_passed or not camera_passed or not stereo_passed or not bridge_passed or not input_passed or not movement_passed or not turning_passed or not weapon_passed:raise RuntimeError('Native trace or restoration failed; inspect saved reports')
  return summary
 finally:observer.close()
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('--pid',type=int,required=True);p.add_argument('--game',type=pathlib.Path,required=True);p.add_argument('--target-trial',action='store_true');p.add_argument('--camera-trial',action='store_true');p.add_argument('--stereo-trial',action='store_true');p.add_argument('--asymmetric-trial',action='store_true');p.add_argument('--pose-trial',action='store_true');p.add_argument('--channel-token');p.add_argument('--input-trial',action='store_true');p.add_argument('--movement-trial',action='store_true');p.add_argument('--turning-trial',action='store_true');a=p.parse_args();trace(a.pid,a.game,pathlib.Path(__file__).resolve().parents[1],bool(a.channel_token) or a.target_trial or a.camera_trial or a.stereo_trial or a.asymmetric_trial or a.pose_trial,bool(a.channel_token) or a.camera_trial or a.stereo_trial or a.asymmetric_trial or a.pose_trial,a.stereo_trial or a.asymmetric_trial or a.pose_trial,a.asymmetric_trial or a.pose_trial,a.pose_trial,a.channel_token,a.input_trial,a.movement_trial,a.turning_trial)

