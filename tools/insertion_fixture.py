"""Read-only live calibration for synthetic magazine insertion trials.

Coordinates come from the selected installation at run time. Generated fixture
files belong in the trial's ignored reports directory, never in shipped assets.
"""
import datetime
import json
import math
from pathlib import Path
import time

from observe_kf import Observer, unpack
from observe_weapon_rig import sample


class _NotReady(Exception):
    pass


def _one(rows, description):
    if not rows:
        raise _NotReady('Missing ' + description)
    if len(rows) != 1:
        raise ValueError('Expected exactly one ' + description)
    return rows[0]


def _number(value, name):
    if isinstance(value, bool) or not isinstance(value, (int, float)) or not math.isfinite(value):
        raise ValueError('Invalid finite calibration value: ' + name)
    return float(value)


def _vector(values, name):
    value = values.get(name)
    if not isinstance(value, (tuple, list)) or len(value) != 3:
        raise ValueError('Missing three-component calibration: ' + name)
    return [_number(v, name) for v in value]


def _capture(observer, snapshot, weapon):
    objects = [o for o in sample(observer) if not o['values'].get('bDeleteMe', False)]
    if not observer.rig_sample_consistent:
        raise _NotReady('Rig changed during calibration snapshot')
    rig = _one([o for o in objects if o['type'] == 'KFVRWeaponRig'], 'live weapon rig')
    rv = rig['values']
    pawn = _one([o for o in snapshot['observations'] if o['class'] == 'KFHumanPawn'
                 and o['address'] == rv.get('BoundPawn') and o['values'].get('Pawn.Health', 0) > 0],
                'living bound pawn')
    pc = _one([o for o in snapshot['observations'] if o['class'] == 'KFPlayerController'
               and o['address'] == rv.get('PC')], 'bound player controller')
    if (pc['values'].get('Controller.Pawn') or {}).get('address') != pawn['address']:
        raise _NotReady('Controller no longer owns the bound pawn')
    owner = snapshot['properties'].get('Actor.Owner')
    if owner is None or owner['type'] != 'ObjectProperty' or owner['size'] != 4:
        raise ValueError('Actor.Owner reflection is unavailable')

    def owned_by(obj, address):
        return unpack(observer.read(int(obj['address'], 16) + owner['offset'], 4)) == int(address, 16)

    if not owned_by(rig, pc['address']):
        raise ValueError('Weapon rig is not owned by its bound controller')
    target_class = 'KFVRTrackedSingle' if weapon == 'single' else 'KFVRTrackedDeagle'
    gun = _one([o for o in objects if o['type'] == target_class
                and o['values'].get('VRRig') == rig['address'] and owned_by(o, pawn['address'])],
               'owned ' + target_class)
    visual = _one([o for o in objects if o['type'] == 'KFVRWeaponVisual'
                   and o['values'].get('SourceWeapon') == gun['address'] and owned_by(o, rig['address'])],
                  'owned magazine visual')
    v = visual['values']
    if 'bInsertionCalibrated' not in v or 'bInsertionFailed' not in v:
        raise ValueError('Selected visual lacks insertion calibration; build and stage the current scripts')
    if v['bInsertionFailed']:
        raise ValueError('Installed reload insertion calibration failed for ' + target_class)
    if not v['bInsertionCalibrated']:
        raise _NotReady('Insertion calibration is still pending for ' + target_class)
    if not v.get('bCalibrated') or not v.get('bPhysicalProfile'):
        raise ValueError('Selected visual has no valid physical weapon profile')
    units = _number(v.get('ModelUnitsPerMetre'), 'ModelUnitsPerMetre')
    if not 10 <= units <= 10000:
        raise ValueError('Unreasonable model units per metre')
    axis = _vector(v, 'InsertionAxis')
    basis_x = _vector(v, 'InsertionX')
    basis_y = _vector(v, 'InsertionY')
    for name, direction in [('axis', axis), ('X', basis_x), ('Y', basis_y)]:
        if abs(math.sqrt(sum(x*x for x in direction)) - 1) > 0.001:
            raise ValueError('Insertion ' + name + ' is not a unit direction')
    if any(abs(sum(x*y for x, y in zip(a, b))) > 0.001
           for a, b in [(axis, basis_x), (axis, basis_y), (basis_x, basis_y)]):
        raise ValueError('Insertion frame is not orthogonal')
    magazine = _vector(v, 'MagazineLocal')
    palm = _vector(v, 'PalmLocal')
    gun_offset = [(x-y)/units for x, y in zip(magazine, palm)]
    native = rv.get('bNativeHandTest')
    if not isinstance(native, bool):
        raise ValueError('Missing native-hand selection')
    grip_offset = [0.0, 0.0, 0.0]
    grip_address = None
    if native:
        grip = _one([o for o in objects if o['type'] == 'KFVRMagazineGrip'
                     and o['address'] == v.get('MagazineGrip')
                     and o['values'].get('SourceWeapon') == gun['address']
                     and owned_by(o, visual['address'])], 'native magazine grasp profile')
        if not grip['values'].get('bCalibrated'):
            raise _NotReady('Native magazine grasp is not calibrated')
        grip_offset = [x/units for x in _vector(grip['values'], 'PalmLocal')]
        grip_address = grip['address']
    if max(math.sqrt(sum(x*x for x in offset)) for offset in [gun_offset, grip_offset]) > 1:
        raise ValueError('Unreasonable magazine/palm calibration offset')
    metrics = {name: _number(v.get(name), name) for name in
               ['InsertionSampleTravel', 'InsertionEndDistance', 'InsertionRestError']}
    if any(value < 0 for value in metrics.values()):
        raise ValueError('Negative insertion calibration distance')
    values = gun_offset + grip_offset + axis
    if len(values) != 9 or not all(math.isfinite(x) for x in values):
        raise ValueError('Fixture must contain exactly nine finite values')
    return dict(weapon=weapon, source_pid=observer.pid, read_only=True, generated_from_live_assets=True,
                rig_address=rig['address'], pawn_address=pawn['address'], gun_address=gun['address'],
                visual_address=visual['address'], magazine_grip_address=grip_address,
                native_hands=native, model_units_per_metre=units,
                insertion_calibrated=True, insertion_failed=False, calibration_distance_units='metres',
                calibration=metrics, insertion_x=basis_x, insertion_y=basis_y,
                magazine_from_weapon_palm_metres=gun_offset, magazine_grasp_palm_metres=grip_offset,
                insertion_axis=axis, values=values, core_sha256=snapshot['core_sha256'])


def make_fixture(pid, game, folder, weapon='single') -> Path:
    """Wait at most ten seconds for an owned calibrated pistol, then save it.

    This does not select a weapon or modify the game. The native trial runner
    still performs its normal private-world/viewport preflight before injection.
    """
    if weapon not in ('single', 'deagle'):
        raise ValueError('Insertion fixture weapon must be single or deagle')
    game, folder = Path(game), Path(folder)
    deadline = time.monotonic() + 10
    reason = 'No calibration snapshot'
    while True:
        observer = None
        try:
            observer = Observer(pid, game)
            snapshot = observer.snapshot()
            metadata = _capture(observer, snapshot, weapon)
            break
        except _NotReady as error:
            reason = str(error)
        except ValueError as error:
            # These are the known object/name-array creation races. Do not hide
            # wrong processes, invalid modules, layouts or calibration failures.
            if not any(message in str(error) for message in
                       ('Array changed during snapshot', 'FName index mismatch')):
                raise
            reason = str(error)
        finally:
            if observer is not None:
                observer.close()
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            raise RuntimeError('Live insertion fixture not ready within 10 seconds: ' + reason)
        time.sleep(min(0.1, remaining))
    metadata['captured_utc'] = datetime.datetime.now(datetime.timezone.utc).isoformat()
    folder.mkdir(parents=True, exist_ok=True)
    (folder/'reload-fixture.json').write_text(json.dumps(metadata, indent=2) + '\n', encoding='utf-8')
    path = folder/'reload-fixture.txt'
    path.write_text(' '.join(format(value, '.9g') for value in metadata['values']) + '\n', encoding='ascii')
    return path
