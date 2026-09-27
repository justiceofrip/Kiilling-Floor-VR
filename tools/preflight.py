"""Read-only installation/bundle checks before an explicitly launched VR session."""
import argparse,hashlib,json,pathlib,platform,struct,sys
ROOT=pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'local/pydeps'))
from native_profile import AUDITED_SHA256
RUNTIME_FILES=['build/x86/KFVRNativeProbe.dll','build/x86/KFVRNativeTrace.exe','build/x64/KFVRXrHost.exe','build/x64/runtime/openxr/win64/openxr_loader.dll','build/unrealscript/KFVRGameplay.u']

def check(game):
 checks={};details={}
 checks['windows_64_bit_python']=platform.system()=='Windows' and struct.calcsize('P')==8
 checks['python_3_12']=sys.version_info[:2]==(3,12)
 checks['killing_floor_executable']=(game/'System/KillingFloor.exe').is_file()
 for name in RUNTIME_FILES:checks['bundle:'+name]=(ROOT/name).is_file()
 for name,expected in AUDITED_SHA256.items():
  path=game/'System'/name
  checks['supported_game:'+name]=path.is_file() and hashlib.sha256(path.read_bytes()).hexdigest()==expected
 try:
  import pefile
  checks['pefile_2024_8_26']=pefile.__version__=='2024.8.26'
 except ImportError:checks['pefile_2024_8_26']=False
 try:
  import winreg
  with winreg.OpenKey(winreg.HKEY_LOCAL_MACHINE,r'SOFTWARE\Khronos\OpenXR\1',0,winreg.KEY_READ|winreg.KEY_WOW64_64KEY) as key:
   runtime=winreg.QueryValueEx(key,'ActiveRuntime')[0]
  checks['openxr_runtime_registered']=pathlib.Path(runtime).is_file()
  details['active_runtime']=runtime
 except (ImportError,OSError):checks['openxr_runtime_registered']=False
 manifest=ROOT/'release-manifest.json'
 if manifest.exists():
  data=json.loads(manifest.read_text());details['release_version']=data['version'];details['source_commit']=data['source_commit']
  for name,expected in data['files'].items():
   path=(ROOT/name).resolve()
   checks['integrity:'+name]=ROOT in path.parents and path.is_file() and hashlib.sha256(path.read_bytes()).hexdigest()==expected
 return dict(passed=all(checks.values()),checks=checks,details=details,headset_tested=False,game_memory_written=False)

if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('--game',required=True,type=pathlib.Path);a=p.parse_args()
 result=check(a.game.resolve());print(json.dumps(result,indent=2));sys.exit(0 if result['passed'] else 1)
