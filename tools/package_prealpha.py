"""Build a curated runtime ZIP from a clean, committed pre-alpha checkout."""
import argparse,hashlib,json,pathlib,re,subprocess,zipfile
ROOT=pathlib.Path(__file__).resolve().parents[1]
PEFILE_SHA256='76f8b485dcd3b1bb8166f1128d395fa3d87af26360c2358fb75b80019b957c6f'
FILES=[
 'Check-KFVR.ps1','Start-KFVRDev.ps1','Play-KFVR.ps1','LICENSE',
 'docs/PRE_ALPHA_TEST.md','docs/PRE_ALPHA_RELEASE.md',
 'tools/preflight.py','tools/native_headset_trial.py','tools/runtime_resolution.py','tools/trace_kf.py',
 'tools/observe_kf.py','tools/native_profile.py','tools/stage_dev_package.py','tools/prepare_vr_profile.py',
 'build/x86/KFVRNativeTrace.exe','build/x86/KFVRNativeProbe.dll',
 'build/x64/KFVRXrHost.exe','build/x64/runtime/openxr/win64/openxr_loader.dll',
 'build/unrealscript/KFVRGameplay.u',
 'licenses/BFVR-MIT.txt','licenses/MinHook-BSD.txt','licenses/OpenXR-Apache-2.0.txt'
]

def package(version,wheel):
 if not re.fullmatch(r'[0-9]+[.][0-9]+[.][0-9]+-prealpha[.][0-9]+(?:-rc[0-9]+)?',version):raise ValueError('Expected a pre-alpha version')
 status=subprocess.check_output(['git','status','--porcelain'],cwd=ROOT,text=True)
 if status.strip():raise ValueError('Commit the exact candidate before packaging; checkout is not clean')
 revision=subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip()
 for arch in ['x86','x64']:
  report=json.loads((ROOT/'reports'/f'{arch}-build.json').read_text())
  if not report.get('built') or not report.get('tests_passed'):raise ValueError('Missing passing build: '+arch)
 payload={name:(ROOT/name).read_bytes() for name in FILES}
 wheel_bytes=wheel.read_bytes()
 if hashlib.sha256(wheel_bytes).hexdigest()!=PEFILE_SHA256:raise ValueError('Pinned pefile wheel checksum differs')
 with zipfile.ZipFile(wheel) as dependency:
  for member in dependency.infolist():
   path=pathlib.PurePosixPath(member.filename)
   if path.is_absolute() or '..' in path.parts or member.is_dir():raise ValueError('Unexpected dependency path')
   payload['local/pydeps/'+member.filename]=dependency.read(member)
 if 'local/pydeps/pefile-2024.8.26.dist-info/LICENSE' not in payload:raise ValueError('Dependency license missing')
 payload['README-FIRST.txt']=b'Kiilling Floor VR pre-alpha. Open docs/PRE_ALPHA_TEST.md for requirements, launch commands, controls and known limits. This build requires your own original Killing Floor installation.\r\n'
 manifest=dict(project='Kiilling Floor VR',version=version,source_commit=revision,kind='pre-alpha release candidate',headset_acceptance='failed previous pass; corrective candidate needs retest',files={name:hashlib.sha256(data).hexdigest() for name,data in sorted(payload.items())})
 payload['release-manifest.json']=(json.dumps(manifest,indent=2)+'\n').encode()
 output=ROOT/'dist';output.mkdir(exist_ok=True)
 archive=output/f'Kiilling-Floor-VR-{version}.zip'
 if archive.exists():raise FileExistsError('Version already packaged: '+str(archive))
 with zipfile.ZipFile(archive,'w',compression=zipfile.ZIP_DEFLATED,compresslevel=9) as bundle:
  for name,data in sorted(payload.items()):
   info=zipfile.ZipInfo(name,date_time=(2026,9,27,0,0,0));info.compress_type=zipfile.ZIP_DEFLATED;bundle.writestr(info,data)
 with zipfile.ZipFile(archive) as bundle:
  if bundle.testzip():raise RuntimeError('Archive CRC verification failed')
  if set(bundle.namelist())!=set(payload):raise RuntimeError('Archive manifest differs')
  for name,expected in manifest['files'].items():
   if hashlib.sha256(bundle.read(name)).hexdigest()!=expected:raise RuntimeError('Archive hash verification failed: '+name)
 checksum=hashlib.sha256(archive.read_bytes()).hexdigest()
 archive.with_suffix('.zip.sha256').write_text(checksum+'  '+archive.name+'\n')
 print(json.dumps(dict(archive=str(archive),sha256=checksum,bytes=archive.stat().st_size,files=len(payload),source_commit=revision,headset_acceptance=manifest['headset_acceptance']),indent=2))

if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('--version',required=True);p.add_argument('--pefile-wheel',type=pathlib.Path,default=ROOT/'local/release-deps/pefile-2024.8.26-py3-none-any.whl');a=p.parse_args();package(a.version,a.pefile_wheel)
