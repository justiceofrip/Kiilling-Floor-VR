"""Create a separate KF configuration for VR cadence; never edit normal INIs."""
import argparse,hashlib,json,pathlib,re,shutil

def set_value(data,section,key,value):
 # Preserve the original byte encoding, repeated keys and unrelated settings.
 header=re.compile(rb'(?mi)^\['+re.escape(section.encode('ascii'))+rb'\][ \t]*\r?$',re.M)
 found=header.search(data)
 if not found:raise ValueError('Installed profile lacks section '+section)
 next_section=re.search(rb'(?m)^\[',data[found.end():])
 end=found.end()+next_section.start() if next_section else len(data)
 body=data[found.end():end]
 setting=re.compile(rb'(?mi)^'+re.escape(key.encode('ascii'))+rb'=[^\r\n]*')
 if not setting.search(body):raise ValueError('Installed profile lacks setting '+section+'.'+key)
 body,count=setting.subn(key.encode('ascii')+b'='+value.encode('ascii'),body)
 if count!=1:raise ValueError('Ambiguous profile setting '+key)
 return data[:found.end()]+body+data[end:]

def prepare(game,output,max_fps,renderer_test=False):
 game=game.resolve();output=output.resolve()
 if not 60<=max_fps<=240:raise ValueError('VR frame limit must be 60..240')
 if not (game/'System/KillingFloor.exe').is_file():raise ValueError('Selected KF installation is missing')
 if output==game or game in output.parents:raise ValueError('VR profile must be outside the game installation')
 output.mkdir(parents=True,exist_ok=True)
 originals={name:game/'System'/name for name in ['KillingFloor.ini','User.ini']}
 hashes={name:hashlib.sha256(path.read_bytes()).hexdigest() for name,path in originals.items()}
 backups=output/'originals';backups.mkdir(exist_ok=True)
 for name,path in originals.items():
  backup=backups/(hashes[name]+'-'+name)
  if not backup.exists():shutil.copyfile(path,backup)
  if hashlib.sha256(backup.read_bytes()).hexdigest()!=hashes[name]:raise RuntimeError('Original backup hash mismatch')
 engine=output/'KFVR.ini';user=output/'KFVRUser.ini'
 data=engine.read_bytes() if engine.exists() else originals['KillingFloor.ini'].read_bytes()
 data=set_value(data,'D3D9Drv.D3D9RenderDevice','UseVSync','False')
 data=set_value(data,'Engine.LevelInfo','MaxClientFrameRate',str(max_fps)+'.000000')
 engine.write_bytes(data)
 if not user.exists():shutil.copyfile(originals['User.ini'],user)
 if renderer_test:
  # The renderer benchmark alone disables desktop mouse look. Never overwrite
  # the ordinary VR user's settings or change class defaults in the live game.
  benchmark_user=output/'KFVRRendererUser.ini'
  benchmark_data=set_value(user.read_bytes(),'Engine.PlayerInput','MouseSensitivity','0.000000')
  benchmark_data=set_value(benchmark_data,'Engine.PlayerInput','MouseSmoothingMode','0')
  benchmark_user.write_bytes(benchmark_data);user=benchmark_user
 report=dict(engine_ini=str(engine),user_ini=str(user),max_fps=max_fps,desktop_vsync=False,renderer_test=renderer_test,originals={name:dict(path=str(path),sha256=hashes[name]) for name,path in originals.items()})
 (output/'source-manifest.json').write_text(json.dumps(report,indent=2)+'\n')
 if any(hashlib.sha256(path.read_bytes()).hexdigest()!=hashes[name] for name,path in originals.items()):raise RuntimeError('Original INI changed during profile creation')
 return report

if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('--game',type=pathlib.Path,required=True);p.add_argument('--output',type=pathlib.Path,required=True);p.add_argument('--max-fps',type=int,default=144);p.add_argument('--renderer-test',action='store_true');a=p.parse_args()
 print(json.dumps(prepare(a.game,a.output,a.max_fps,a.renderer_test)))
