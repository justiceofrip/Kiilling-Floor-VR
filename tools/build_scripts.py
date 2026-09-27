"""Build only KFVR scripts in an isolated copy of the installed SDK runtime."""
import argparse,hashlib,json,pathlib,re,shutil,subprocess
ROOT=pathlib.Path(__file__).resolve().parents[1]
def build(game):
 source=game/'System';work=ROOT/'local/compiler';system=work/'System';system.mkdir(parents=True,exist_ok=True)
 if not (source/'ucc.exe').exists():raise FileNotFoundError('Install official Killing Floor SDK (Steam app 1260) to obtain ucc.exe')
 manifest=[]
 for p in source.iterdir():
  if not p.is_file() or (p.suffix.lower() not in ['.dll','.u','.int'] and p.name.lower()!='ucc.exe'):continue
  dest=system/p.name
  if not dest.exists() or dest.stat().st_size!=p.stat().st_size or dest.stat().st_mtime_ns!=p.stat().st_mtime_ns:shutil.copy2(p,dest)
  manifest.append(dict(file=p.name,sha256=hashlib.sha256(p.read_bytes()).hexdigest()))
 configuration=(source/'Default.ini').read_text(encoding='utf-8-sig')
 lines=[]
 for line in configuration.splitlines():
  if line.lower().startswith('editpackages='):
   name=line.split('=',1)[1].strip()
   if not (system/(name+'.u')).exists():continue
  if line.lower().startswith('paths='):
   value=line.split('=',1)[1].strip().replace('\\','/')
   if value.lower().startswith('../system/'):line='Paths=../System/*.u'
   elif value.startswith('../'):line='Paths='+str(game/value[3:])
  lines.append(line)
 configuration='\n'.join(lines)+'\n'
 pos=configuration.index('[Editor.EditorEngine]')+len('[Editor.EditorEngine]')
 configuration=configuration[:pos]+'\nEditPackages=KFVRGameplay'+configuration[pos:]
 # Put our package after all native dependencies, preserving its own section.
 configuration=configuration.replace('\nEditPackages=KFVRGameplay','')
 lines=configuration.splitlines();last=max(i for i,line in enumerate(lines) if line.lower().startswith('editpackages='));lines.insert(last+1,'EditPackages=KFVRGameplay');configuration='\n'.join(lines)+'\n'
 for name in ['KFVRMake.ini','Default.ini','KillingFloor.ini']:(system/name).write_text(configuration)
 shutil.copy2(source/'defuser.ini',system/'User.ini')
 for p in (ROOT/'unrealscript').rglob('*.uc'):
  dest=work/p.relative_to(ROOT/'unrealscript');dest.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(p,dest)
 package=system/'KFVRGameplay.u'
 # A single generated file in the checked isolated directory; no game deletion.
 if package.parent.resolve()!=system.resolve():raise ValueError('Invalid compiler target')
 package.unlink(missing_ok=True)
 reports=ROOT/'reports';reports.mkdir(exist_ok=True)
 for label,args in [('make',['make','ini=KFVRMake.ini']),('ammo-tests',['KFVRGameplay.KFVRAmmoTests','ini=KFVRMake.ini'])]:
  result=subprocess.run([str(system/'ucc.exe'),*args],cwd=system,capture_output=True,timeout=90)
  output=(result.stdout+result.stderr).decode('utf-8',errors='replace');(reports/('unrealscript-'+label+'.log')).write_text(output);print('\n'.join(line for line in output[-6500:].splitlines() if 'SteamID' not in line and line.strip()))
  if result.returncode or (label=='make' and not package.exists()) or (label=='ammo-tests' and not re.search(r'KFVR_TEST_RESULT checks=\d+ failures=0',output)):raise RuntimeError(label+' failed; see saved compiler log')
 parity=ROOT/'build/x64/KFVRAmmoParity.exe'
 if not parity.exists():raise FileNotFoundError('Build the x64 native project before running script parity checks')
 reference=subprocess.run([str(parity)],capture_output=True,text=True,check=True).stdout
 pattern=r'KFVR_AMMO_PARITY case=(\d+) step=(\d+) checksum=(\d+)'
 actual=re.findall(pattern,output);expected=re.findall(pattern,reference)
 if len(actual)!=20 or actual!=expected:raise RuntimeError('UnrealScript/C++ ammo behavior diverged: '+str(dict(actual=actual,expected=expected)))
 print('UnrealScript/C++ parity: 20 matched checkpoints across 20000 actions, two feed/cycle families and ownership changes')
 pose_result=subprocess.run([str(system/'ucc.exe'),'KFVRGameplay.KFVRReloadPoseTests','ini=KFVRMake.ini'],cwd=system,capture_output=True,timeout=90)
 pose_output=(pose_result.stdout+pose_result.stderr).decode('utf-8',errors='replace');(reports/'unrealscript-pose-tests.log').write_text(pose_output)
 pose_match=re.search(r'KFVR_POSE_TEST_RESULT checks=(\d+) failures=0',pose_output)
 if pose_result.returncode or not pose_match:raise RuntimeError('Reload pose tests failed; see saved compiler log')
 print('UnrealScript physical reload pose mapping: '+pose_match.group(1)+' checks passed')
 out=ROOT/'build/unrealscript';out.mkdir(parents=True,exist_ok=True);shutil.copy2(package,out/package.name)
 (reports/'unrealscript-build.json').write_text(json.dumps(dict(game=str(game),compiler=str(system/'ucc.exe'),package=str(out/package.name),package_sha256=hashlib.sha256(package.read_bytes()).hexdigest(),game_files_written=False,parity_checkpoints=len(actual),reload_pose_checks=int(pose_match.group(1)),inputs=manifest),indent=2))
if __name__=='__main__':
 parser=argparse.ArgumentParser();parser.add_argument('--game',type=pathlib.Path,required=True);build(parser.parse_args().game.resolve())
