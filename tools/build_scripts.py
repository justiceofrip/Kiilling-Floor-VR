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
 for p in (ROOT/'unrealscript').rglob('*'):
  if not p.is_file() or p.suffix.lower() not in ['.uc','.lwo','.psk','.psa']:continue
  dest=work/p.relative_to(ROOT/'unrealscript');dest.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(p,dest)
 package=system/'KFVRGameplay.u'
 # A single generated file in the checked isolated directory; no game deletion.
 if package.parent.resolve()!=system.resolve():raise ValueError('Invalid compiler target')
 package.unlink(missing_ok=True)
 reports=ROOT/'reports';reports.mkdir(exist_ok=True)
 for label,args in [('make',['make','ini=KFVRMake.ini']),('ammo-tests',['KFVRGameplay.KFVRAmmoTests','ini=KFVRMake.ini'])]:
  try:result=subprocess.run([str(system/'ucc.exe'),*args,'-unattended'],cwd=system,capture_output=True,timeout=90)
  except subprocess.TimeoutExpired as error:
   partial=(error.stdout or b'')+(error.stderr or b'')
   (reports/('unrealscript-'+label+'-timeout.log')).write_text(partial.decode('utf-8',errors='replace'))
   raise
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
 mag_result=subprocess.run([str(system/'ucc.exe'),'KFVRGameplay.KFVRMagazineTests','ini=KFVRMake.ini'],cwd=system,capture_output=True,timeout=90)
 mag_output=(mag_result.stdout+mag_result.stderr).decode('utf-8',errors='replace');(reports/'unrealscript-magazine-tests.log').write_text(mag_output)
 mag_match=re.search(r'KFVR_MAG_TEST_RESULT checks=(\d+) failures=0',mag_output)
 if mag_result.returncode or not mag_match:raise RuntimeError('Live magazine ledger tests failed; see saved compiler log')
 print('UnrealScript live magazine ledger: '+mag_match.group(1)+' checks passed')
 support_result=subprocess.run([str(system/'ucc.exe'),'KFVRGameplay.KFVRPoseMathTests','ini=KFVRMake.ini'],cwd=system,capture_output=True,timeout=90)
 support_output=(support_result.stdout+support_result.stderr).decode('utf-8',errors='replace');(reports/'unrealscript-support-tests.log').write_text(support_output)
 support_match=re.search(r'KFVR_SUPPORT_TEST_RESULT checks=(\d+) failures=0',support_output)
 if support_result.returncode or not support_match:raise RuntimeError('Two-hand pose tests failed; see saved compiler log')
 print('UnrealScript two-hand pose alignment: '+support_match.group(1)+' checks passed')
 melee_result=subprocess.run([str(system/'ucc.exe'),'KFVRGameplay.KFVRMeleeTests','ini=KFVRMake.ini'],cwd=system,capture_output=True,timeout=90)
 melee_output=(melee_result.stdout+melee_result.stderr).decode('utf-8',errors='replace');(reports/'unrealscript-melee-tests.log').write_text(melee_output)
 melee_match=re.search(r'KFVR_MELEE_TEST_RESULT checks=(\d+) failures=0',melee_output)
 if melee_result.returncode or not melee_match:raise RuntimeError('Physical melee tests failed; see saved compiler log')
 print('UnrealScript physical melee: '+melee_match.group(1)+' checks passed')
 dual_result=subprocess.run([str(system/'ucc.exe'),'KFVRGameplay.KFVRDualFeedTests','ini=KFVRMake.ini'],cwd=system,capture_output=True,timeout=90)
 dual_output=(dual_result.stdout+dual_result.stderr).decode('utf-8',errors='replace');(reports/'unrealscript-dual-tests.log').write_text(dual_output)
 dual_match=re.search(r'KFVR_DUAL_TEST_RESULT checks=(\d+) failures=0',dual_output)
 if dual_result.returncode or not dual_match:raise RuntimeError('Independent dual feed tests failed; see saved compiler log')
 print('UnrealScript independent dual feeds: '+dual_match.group(1)+' checks passed')
 dual_mag_result=subprocess.run([str(system/'ucc.exe'),'KFVRGameplay.KFVRDualMagazineTests','ini=KFVRMake.ini'],cwd=system,capture_output=True,timeout=90)
 dual_mag_output=(dual_mag_result.stdout+dual_mag_result.stderr).decode('utf-8',errors='replace');(reports/'unrealscript-dual-magazine-tests.log').write_text(dual_mag_output)
 dual_mag_match=re.search(r'KFVR_DUAL_MAG_TEST_RESULT checks=(\d+) failures=0',dual_mag_output)
 if dual_mag_result.returncode or not dual_mag_match:raise RuntimeError('Dual physical magazine tests failed; see saved compiler log')
 print('UnrealScript dual physical magazines: '+dual_mag_match.group(1)+' checks passed')
 arm_result=subprocess.run([str(system/'ucc.exe'),'KFVRGameplay.KFVRArmMathTests','ini=KFVRMake.ini'],cwd=system,capture_output=True,timeout=90)
 arm_output=(arm_result.stdout+arm_result.stderr).decode('utf-8',errors='replace');(reports/'unrealscript-arm-tests.log').write_text(arm_output)
 arm_match=re.search(r'KFVR_ARM_TEST_RESULT checks=(\d+) failures=0',arm_output)
 if arm_result.returncode or not arm_match:raise RuntimeError('Arm geometry tests failed; see saved compiler log')
 print('UnrealScript arm geometry: '+arm_match.group(1)+' checks passed')
 throw_result=subprocess.run([str(system/'ucc.exe'),'KFVRGameplay.KFVRThrowMotionTests','ini=KFVRMake.ini'],cwd=system,capture_output=True,timeout=90)
 throw_output=(throw_result.stdout+throw_result.stderr).decode('utf-8',errors='replace');(reports/'unrealscript-throw-tests.log').write_text(throw_output)
 throw_match=re.search(r'KFVR_THROW_RESULT checks=(\d+) failures=0',throw_output)
 if throw_result.returncode or not throw_match:raise RuntimeError('Throw velocity tests failed; see saved compiler log')
 print('UnrealScript throw velocity: '+throw_match.group(1)+' checks passed')
 out=ROOT/'build/unrealscript';out.mkdir(parents=True,exist_ok=True);shutil.copy2(package,out/package.name)
 (reports/'unrealscript-build.json').write_text(json.dumps(dict(game=str(game),compiler=str(system/'ucc.exe'),package=str(out/package.name),package_sha256=hashlib.sha256(package.read_bytes()).hexdigest(),game_files_written=False,parity_checkpoints=len(actual),reload_pose_checks=int(pose_match.group(1)),magazine_checks=int(mag_match.group(1)),support_pose_checks=int(support_match.group(1)),arm_pose_checks=int(arm_match.group(1)),throw_motion_checks=int(throw_match.group(1)),melee_checks=int(melee_match.group(1)),dual_feed_checks=int(dual_match.group(1)),dual_magazine_checks=int(dual_mag_match.group(1)),inputs=manifest),indent=2))
if __name__=='__main__':
 parser=argparse.ArgumentParser();parser.add_argument('--game',type=pathlib.Path,required=True);build(parser.parse_args().game.resolve())
