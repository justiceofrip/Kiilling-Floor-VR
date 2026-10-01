"""Stage generated development packages after validating every selected target."""
import argparse
import hashlib
import json
import pathlib
import shutil

ROOT = pathlib.Path(__file__).resolve().parents[1]


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def stage(game, native_hands=False, root=ROOT):
    game = game.resolve()
    if not (game / 'System/KillingFloor.exe').is_file():
        raise ValueError('Invalid Killing Floor installation')
    packages = [(root / 'build/unrealscript/KFVRGameplay.u',
                 game / 'System/KFVRGameplay.u', root / 'reports/dev-install.json', True)]
    if native_hands:
        source = root / 'local/native-hands/KFVRLocalHands.u'
        prepared = json.loads((root / 'local/native-hands/manifest.json').read_text())
        if (prepared['package_sha256'] != sha(source) or
                prepared['source_sha256'] != sha(game / 'Animations/KF_Weapons_Trip.ukx')):
            raise ValueError('Local native-hand package/source mismatch; rebuild it for this installation')
        packages.append((source, game / 'System/KFVRLocalHands.u',
                         root / 'reports/native-hand-install.json', False))
    # Finish preflight before overwriting even an owned development package.
    for source, target, manifest, redistributable in packages:
        if not source.is_file():
            raise ValueError('Missing development build: ' + str(source))
        if target.exists():
            previous = json.loads(manifest.read_text()) if manifest.exists() else {}
            if previous.get('target') != str(target) or previous.get('sha256') != sha(target):
                raise ValueError('Existing package is not the recorded KFVR install: ' + str(target))
    for source, target, manifest, redistributable in packages:
        manifest.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, target)
        manifest.write_text(json.dumps(dict(target=str(target), sha256=sha(target),
                                           added_file=True, autoload=False,
                                           redistributable=redistributable), indent=2) + '\n')
        print('Staged ' + ('generated package: ' if redistributable else 'local-only native hands: ') + str(target))


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--game', type=pathlib.Path, required=True)
    parser.add_argument('--native-hands', action='store_true')
    args = parser.parse_args()
    stage(args.game, args.native_hands)
