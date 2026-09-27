"""Stage only our generated standalone development package, preserving existing files."""
import argparse,hashlib,json,pathlib,shutil
ROOT=pathlib.Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--game',type=pathlib.Path,required=True);a=p.parse_args()
source=ROOT/'build/unrealscript/KFVRGameplay.u';target=a.game.resolve()/'System/KFVRGameplay.u';manifest=ROOT/'reports/dev-install.json'
if not source.is_file() or not (target.parent/'KillingFloor.exe').is_file():raise ValueError('Invalid build or installation')
sha=lambda path:hashlib.sha256(path.read_bytes()).hexdigest()
if target.exists():
 previous=json.loads(manifest.read_text()) if manifest.exists() else {}
 if previous.get('target')!=str(target) or previous.get('sha256')!=sha(target):raise ValueError('Existing package is not the recorded KFVR development install')
manifest.parent.mkdir(parents=True,exist_ok=True)
shutil.copy2(source,target)
manifest.write_text(json.dumps(dict(target=str(target),sha256=sha(target),added_file=True,autoload=False),indent=2))
print('Staged generated package: '+str(target))
