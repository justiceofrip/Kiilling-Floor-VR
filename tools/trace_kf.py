"""Opt-in bounded native trace; standalone KF only. No auto-start installation."""
import argparse,ctypes,datetime,hashlib,json,pathlib,struct,subprocess
from observe_kf import Observer
import pefile

def trace(pid,game,root):
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
  p=snapshot['properties'];config=struct.pack('<12I',632,1,12000,180,int(controller['address'],16),controller['index'],viewport,int(level_address,16),level['index'],p['Actor.Level']['offset'],p['PlayerController.Player']['offset'],p['LevelInfo.NetMode']['offset'])
  names=[('Engine.dll','?Draw@UGameEngine@@UAEXPAVUViewport@@HPAEPAH@Z'),('Engine.dll','?Tick@UGameEngine@@UAEXM@Z'),('Engine.dll','?Render@FPlayerSceneNode@@UAEXPAVFRenderInterface@@@Z'),('D3D9Drv.dll','?Present@UD3D9RenderDevice@@UAEXPAVUViewport@@@Z')]
  evidence=[]
  for file,name in names:
   path=game/'System'/file;pe=pefile.PE(str(path));symbol=next(s for s in pe.DIRECTORY_ENTRY_EXPORT.symbols if s.name and s.name.decode()==name);base=observer.modules[file.lower()][0];pe.relocate_image(base);expected=pe.get_data(symbol.address,16)
   if observer.read(base+symbol.address,16)!=expected:raise ValueError('Native target already modified: '+name)
   if file=='D3D9Drv.dll':
    body=pe.get_data(symbol.address,0x120)
    if bytes.fromhex('8b8e5c490000') not in body or bytes.fromhex('ff5244') not in body:raise ValueError('D3D9 device/Present relationship not verified')
   config+=expected;evidence.append(dict(file=file,sha256=hashlib.sha256(path.read_bytes()).hexdigest(),name=name,rva=symbol.address,expected=expected.hex(),address=base+symbol.address))
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
  (directory/'summary.json').write_text(json.dumps(summary,indent=2));print(json.dumps(summary,indent=2))
  if result.returncode or not restored:raise RuntimeError('Native trace or restoration failed; inspect saved reports')
 finally:observer.close()
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('--pid',type=int,required=True);p.add_argument('--game',type=pathlib.Path,required=True);a=p.parse_args();trace(a.pid,a.game,pathlib.Path(__file__).resolve().parents[1])

