"""Read-only Bullpup reload calibration; runtime asset values stay in reports.

Protocol v1: sixteen whitespace-separated floats, all in weapon model axes:
magazine root relative to weapon palm (metres, 3), held magazine grasp palm
relative to magazine root (metres, 3), insertion axis (3), action knob grasp
relative to weapon palm (metres, 3), action pull axis (3), action travel (metres).
ToXR is (model Y, model Z, -model X); rotate each offset by the applicable
controller pose. Magazine grasp uses the offhand rotation. Action grasp and
pull use the dominant controller rotation. Select the rifle at shoulder slot2.

This describes an explicitly authored prototype handle, not an installed bolt.
"""
import datetime
import json
import math
from pathlib import Path
import time

from insertion_fixture import _NotReady, _number, _one, _vector
from observe_kf import Observer, unpack
from observe_weapon_rig import sample

PROTOCOL = 'kfvr-rifle-reload-v1'
FIELDS = ('magazine_from_weapon_palm_metres', 'magazine_grasp_palm_metres',
          'insertion_axis', 'action_grasp_from_weapon_palm_metres', 'action_pull_axis')


def validate_values(values):
    if not isinstance(values, (list, tuple)) or len(values) != 16:
        raise ValueError('Rifle fixture requires exactly sixteen numbers')
    result = [_number(value, 'fixture') for value in values]
    if any(abs(value) > 1 for value in result):
        raise ValueError('Rifle fixture contains an unreasonable metre offset')
    for start in (6, 12):
        if abs(math.dist(result[start:start + 3], (0, 0, 0)) - 1) > .001:
            raise ValueError('Rifle fixture direction is not normalized')
    if not 0 < result[15] <= .30:
        raise ValueError('Invalid rifle action travel')
    return result


def _capture(observer, snapshot):
    objects = [o for o in sample(observer) if not o['values'].get('bDeleteMe')]
    if not observer.rig_sample_consistent:
        raise _NotReady('Rig changed during calibration snapshot')
    rig = _one([o for o in objects if o['type'] == 'KFVRWeaponRig'], 'live weapon rig')
    r = rig['values']
    pawn = _one([o for o in snapshot['observations'] if o['class'] == 'KFHumanPawn'
                 and o['address'] == r.get('BoundPawn') and o['values'].get('Pawn.Health', 0) > 0],
                'living bound pawn')
    pc = _one([o for o in snapshot['observations'] if o['class'] == 'KFPlayerController'
               and o['address'] == r.get('PC')], 'bound player controller')
    if (pc['values'].get('Controller.Pawn') or {}).get('address') != pawn['address']:
        raise _NotReady('Controller no longer owns the bound pawn')
    owner = snapshot['properties'].get('Actor.Owner')
    if owner is None or owner['type'] != 'ObjectProperty' or owner['size'] != 4:
        raise ValueError('Actor.Owner reflection is unavailable')

    def owned_by(obj, address):
        return unpack(observer.read(int(obj['address'], 16) + owner['offset'], 4)) == int(address, 16)

    if not owned_by(rig, pc['address']):
        raise ValueError('Rig is not owned by its bound controller')
    gun = _one([o for o in objects if o['type'] == 'KFVRTrackedBullpup'
                and o['values'].get('VRRig') == rig['address'] and owned_by(o, pawn['address'])],
               'owned tracked Bullpup')
    visual = _one([o for o in objects if o['type'] == 'KFVRWeaponVisual'
                   and o['values'].get('SourceWeapon') == gun['address'] and owned_by(o, rig['address'])],
                  'owned Bullpup visual')
    v = visual['values']
    required = ('bInsertionCalibrated', 'bInsertionFailed', 'bAuthoredActionProfile',
                'bHiddenTranslationCalibrated', 'bActionProfile', 'bPhysicalProfile')
    if any(key not in v for key in required):
        raise ValueError('Rifle fixture requires the current authored-action scripts and observer')
    if v['bInsertionFailed']:
        raise ValueError('Installed Bullpup insertion measurement failed')
    if not all(v.get(key) for key in ('bCalibrated', 'bMagazineProfile', 'bInsertionCalibrated',
                                     'bAuthoredActionProfile', 'bHiddenTranslationCalibrated')):
        raise _NotReady('Authored rifle action or hidden magazine calibration is pending')
    if v['bActionProfile'] or v['bPhysicalProfile'] or v.get('bTranslationProbe'):
        raise ValueError('Authored rifle must retain absent native action and no visible probe')
    action = _one([o for o in objects if o['type'] == 'KFVRActionVisual'
                   and o['address'] == v.get('AuthoredAction')
                   and o['values'].get('SourceVisual') == visual['address']
                   and owned_by(o, visual['address'])], 'owned authored charging handle')
    a = action['values']
    if a.get('bInitialized') is not True:
        raise _NotReady('Authored charging handle is not initialized')
    units = _number(v.get('ModelUnitsPerMetre'), 'ModelUnitsPerMetre')
    if abs(units - 300) > .001:
        raise ValueError('Prototype handle requires its authored 300 model units/metre')
    magazine, palm = _vector(v, 'MagazineLocal'), _vector(v, 'PalmLocal')
    axis = _vector(v, 'InsertionAxis')
    basis = [_vector(v, key) for key in ('InsertionX', 'InsertionY')] + [axis]
    if any(abs(math.dist(direction, (0, 0, 0)) - 1) > .001 for direction in basis) or any(
            abs(sum(x*y for x, y in zip(basis[i], basis[j]))) > .001 for i, j in ((0, 1), (0, 2), (1, 2))):
        raise ValueError('Installed insertion frame is not orthonormal')
    anchor, pull = _vector(a, 'AnchorLocal'), _vector(a, 'PullAxisLocal')
    travel = _number(a.get('TravelMetres'), 'TravelMetres')
    expected_anchor = [x + y*units for x, y in zip(magazine, (.05, .02, .055))]
    if math.dist(anchor, expected_anchor) > .001 or math.dist(pull, (-1, 0, 0)) > .001 or abs(travel - .06) > .000001:
        raise ValueError('Unexpected authored prototype action profile')
    native = r.get('bNativeHandTest')
    if type(native) is not bool:
        raise ValueError('Missing requested hand mode')
    grasp = [0.0, 0.0, 0.0]
    grip_address = None
    if native:
        grip = _one([o for o in objects if o['type'] == 'KFVRMagazineGrip'
                     and o['address'] == v.get('MagazineGrip')
                     and o['values'].get('SourceWeapon') == gun['address']
                     and owned_by(o, visual['address'])], 'owned native magazine grasp')
        if not grip['values'].get('bCalibrated') or not grip['values'].get('bLeftGripPose'):
            raise _NotReady('Native rifle magazine grasp is not calibrated')
        grasp = [x/units for x in _vector(grip['values'], 'PalmLocal')]
        grip_address = grip['address']
    root = [(x-y)/units for x, y in zip(magazine, palm)]
    action_grasp = [(x-y)/units + z for x, y, z in zip(anchor, palm, (0, .018, 0))]
    metrics = {key: _number(v.get(key), key) for key in
               ('InsertionSampleTravel', 'InsertionEndDistance', 'InsertionRestError')}
    if not (.025 <= metrics['InsertionSampleTravel'] <= .20 and 0 <= metrics['InsertionEndDistance'] <= .04
            and 0 <= metrics['InsertionRestError'] <= .001):
        raise ValueError('Installed insertion distances failed existing measurement bounds')
    values = validate_values(root + grasp + axis + action_grasp + pull + [travel])
    return dict(protocol=PROTOCOL, weapon='bullpup', selection_slot=2, source_pid=observer.pid,
                read_only=True, generated_from_live_assets=True, authored_action_prototype=True,
                rig_address=rig['address'], pawn_address=pawn['address'], gun_address=gun['address'],
                visual_address=visual['address'], action_address=action['address'],
                magazine_grip_address=grip_address, native_hands=native, model_units_per_metre=units,
                magazine_from_weapon_palm_metres=root, magazine_grasp_palm_metres=grasp,
                insertion_axis=axis, insertion_x=basis[0], insertion_y=basis[1], calibration=metrics,
                action_anchor_local=anchor, action_knob_offset_metres=[0, .018, 0],
                action_grasp_from_weapon_palm_metres=action_grasp, action_pull_axis=pull,
                action_travel_metres=travel, values=values, core_sha256=snapshot['core_sha256'])


def make_fixture(pid, game, folder):
    """Wait up to ten seconds for calibration; never select or mutate the game."""
    game, folder = Path(game), Path(folder)
    deadline = time.monotonic() + 10
    while True:
        observer = None
        try:
            observer = Observer(pid, game)
            metadata = _capture(observer, observer.snapshot())
            break
        except _NotReady as error:
            reason = str(error)
        except ValueError as error:
            if not any(text in str(error) for text in ('Array changed during snapshot', 'FName index mismatch')):
                raise
            reason = str(error)
        finally:
            if observer is not None:
                observer.close()
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            raise RuntimeError('Live rifle fixture not ready within 10 seconds: ' + reason)
        time.sleep(min(.1, remaining))
    metadata['captured_utc'] = datetime.datetime.now(datetime.timezone.utc).isoformat()
    folder.mkdir(parents=True, exist_ok=True)
    (folder/'rifle-reload-fixture.json').write_text(json.dumps(metadata, indent=2)+'\n', encoding='utf-8')
    path = folder/'rifle-reload-fixture.txt'
    path.write_text(' '.join(format(x, '.9g') for x in metadata['values'])+'\n', encoding='ascii')
    return path


if __name__ == '__main__':
    import argparse
    parser = argparse.ArgumentParser()
    parser.add_argument('--pid', type=int, required=True)
    parser.add_argument('--game', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    print(make_fixture(args.pid, args.game, args.output))
