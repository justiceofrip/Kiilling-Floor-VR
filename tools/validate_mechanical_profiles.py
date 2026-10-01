"""Read-only inventory evidence for separate magazine and action capabilities."""
import json
import math
from pathlib import Path

from validate_grip_pose import rotate

REQUIRED = ('KFVRTrackedSingle', 'KFVRTrackedDeagle', 'KFVRTrackedBullpup')
MIN_SAMPLES = 10


def finite(value):
    return type(value) in (int, float) and math.isfinite(value)


def vector(value):
    return isinstance(value, (list, tuple)) and len(value) == 3 and all(finite(x) for x in value)


def basis(values):
    return all(vector(v) and abs(math.dist(v, (0, 0, 0)) - 1) < .001 for v in values) and all(
        abs(sum(x * y for x, y in zip(values[a], values[b]))) < .001
        for a, b in ((0, 1), (0, 2), (1, 2)))


def analyze(rows):
    metrics = {typ: dict(samples=0, translation_samples=0, socket_samples=0,
                        native_samples=0, native_grasp_samples=0, active_loaded_samples=0,
                        source_weapons=set(), visuals=set()) for typ in REQUIRED}
    results = {typ: dict(association=True, visual=True, capability=True, translation=True,
                        socket=True, native_grasp=True, timed=True) for typ in REQUIRED}
    missing = {}; failures = []; coherent_rows = 0; ownership_samples = 0
    ownership_ok = True; ready_before = set(); position_errors = []; model_errors = []; axis_errors = []
    pose_missing = 0; pose_settling = 0

    def fields(values, keys, label):
        for key in keys:
            if key not in values:
                name = label + '.' + key
                missing[name] = missing.get(name, 0) + 1

    def reject(line, typ, check):
        if len(failures) < 12:
            failures.append(dict(line=line, weapon=typ, check=check))

    for line, row in enumerate(rows, 1):
        if row.get('rig_sample_consistent') is not True:
            continue
        coherent_rows += 1
        objs = {o['address']: o for o in row['objects'] if not o['values'].get('bDeleteMe')}
        now_ready = set()
        for visual in objs.values():
            if visual['type'] != 'KFVRWeaponVisual':
                continue
            v = visual['values']; gun = objs.get(v.get('SourceWeapon'), {})
            typ = gun.get('type')
            if typ not in REQUIRED:
                continue
            g = gun['values']; rig = objs.get(g.get('VRRig'), {}); r = rig.get('values', {})
            m = metrics[typ]; state = results[typ]; m['samples'] += 1
            m['source_weapons'].add(gun['address']); m['visuals'].add(visual['address'])
            fields(g, ('VRRig', 'Manual', 'MagAmmoRemaining'), typ)
            fields(r, ('bNativeHandTest', 'Gun', 'Visual', 'ManualReload', 'bTracking',
                       'bHolstered', 'Tracking.Flags'), 'KFVRWeaponRig')
            fields(v, ('bMagazineProfile', 'bActionProfile', 'bPhysicalProfile', 'bCalibrated',
                       'bLeftGripPose', 'bRightGripPose', 'bMagazineTranslationFrame',
                       'bActionTranslationFrame', 'ActionTravelMetres', 'bTranslationCalibrated', 'bTranslationProbe',
                       'BoneTranslationScale', 'bInsertionCalibrated', 'bInsertionFailed',
                       'MagazineGrip', 'SlideLocal', 'MeasuredActionPosition'), typ + '.visual')
            state['association'] &= rig.get('type') == 'KFVRWeaponRig' and type(r.get('bNativeHandTest')) is bool
            state['visual'] &= all(v.get(k) is True for k in ('bCalibrated', 'bLeftGripPose', 'bRightGripPose'))
            travel = v.get('ActionTravelMetres')
            if typ == 'KFVRTrackedBullpup':
                state['capability'] &= (v.get('bMagazineProfile') is True and v.get('bActionProfile') is False
                    and v.get('bPhysicalProfile') is False and v.get('bActionTranslationFrame') is False
                    and v.get('bMagazineTranslationFrame') is True and finite(travel) and travel == 0
                    and vector(v.get('SlideLocal')) and math.dist(v['SlideLocal'], (0, 0, 0)) == 0
                    and vector(v.get('MeasuredActionPosition')) and math.dist(v['MeasuredActionPosition'], (0, 0, 0)) == 0)
                state['timed'] &= g.get('Manual') == '0x0'
                state['translation'] &= v.get('bTranslationCalibrated') is False and v.get('bTranslationProbe') is False
            else:
                state['capability'] &= (all(v.get(k) is True for k in ('bMagazineProfile', 'bActionProfile',
                    'bPhysicalProfile', 'bMagazineTranslationFrame', 'bActionTranslationFrame'))
                    and finite(travel) and abs(travel - .05) < .000001)
            if v.get('bTranslationCalibrated') is True:
                m['translation_samples'] += 1
                scale = v.get('BoneTranslationScale')
                state['translation'] &= finite(scale) and .01 < scale < 100
            state['socket'] &= v.get('bInsertionFailed') is False
            if v.get('bInsertionCalibrated') is True:
                m['socket_samples'] += 1
                fields(v, ('InsertionSampleTravel', 'InsertionEndDistance', 'InsertionRestError',
                           'InsertionX', 'InsertionY', 'InsertionAxis', 'MagazineX', 'MagazineY', 'MagazineZ'), typ + '.visual')
                distances = [v.get(k) for k in ('InsertionSampleTravel', 'InsertionEndDistance', 'InsertionRestError')]
                state['socket'] &= (all(finite(x) for x in distances) and .025 <= distances[0] <= .20
                    and 0 <= distances[1] <= .04 and 0 <= distances[2] <= .001
                    and basis([v.get(k) for k in ('InsertionX', 'InsertionY', 'InsertionAxis')])
                    and basis([v.get(k) for k in ('MagazineX', 'MagazineY', 'MagazineZ')]))
            if r.get('bNativeHandTest') is True:
                m['native_samples'] += 1
                grip = objs.get(v.get('MagazineGrip'), {}); h = grip.get('values', {})
                fields(h, ('SourceWeapon', 'bCalibrated', 'bLeftGripPose', 'bHidden'), typ + '.magazine_grasp')
                calibrated = (grip.get('type') == 'KFVRMagazineGrip' and h.get('SourceWeapon') == gun['address']
                              and h.get('bCalibrated') is True and h.get('bLeftGripPose') is True)
                state['native_grasp'] &= calibrated
                if calibrated:
                    m['native_grasp_samples'] += 1
                manual = objs.get(g.get('Manual'), {}); a = manual.get('values', {})
                owned = (manual.get('type') == 'KFVRManualReload' and a.get('Gun') == gun['address']
                    and type(a.get('HeldMagazine')) is int and a['HeldMagazine'] > 0
                    and r.get('ManualReload') == manual.get('address') and r.get('Gun') == gun['address']
                    and r.get('Visual') == visual['address'] and r.get('bTracking') is True
                    and r.get('bHolstered') is False and type(r.get('Tracking.Flags')) is int
                    and r['Tracking.Flags'] & 3 == 3)
                ownership_samples += 1
                if not owned:
                    ownership_ok &= h.get('bHidden') is True
                    for hand in objs.values():
                        if hand['type'] != 'KFVRHandVisual':
                            continue
                        hv = hand['values']
                        if hv.get('GripVisual') == grip.get('address') and hv.get('bHidden') is False:
                            fields(hv, ('bGripPose',), 'KFVRHandVisual')
                            ownership_ok &= hv.get('bGripPose') is False
            active = (rig.get('type') == 'KFVRWeaponRig' and r.get('Gun') == gun['address']
                      and r.get('Visual') == visual['address'] and r.get('bTracking') is True
                      and r.get('bHolstered') is False and v.get('bHidden') is False)
            if typ == 'KFVRTrackedBullpup' and active:
                state['timed'] &= r.get('ManualReload') == '0x0'
                if type(g.get('MagAmmoRemaining')) is int and g['MagAmmoRemaining'] > 0:
                    m['active_loaded_samples'] += 1
                # Magazine-only calibration never runs the visible translation
                # probe. Require an earlier coherent active/socket-ready sample
                # so hidden preload state cannot stand in for a completed pose.
                if v.get('bInsertionCalibrated') is True:
                    now_ready.add(visual['address'])
                if visual['address'] not in ready_before or v.get('bInsertionCalibrated') is not True:
                    pose_settling += 1
                else:
                    keys = ('Location', 'Rotation', 'DrawScale', 'MagazineLocal', 'MeasuredMagazinePosition',
                            'MagazineX', 'MagazineY', 'MagazineZ', 'MeasuredMagazineX', 'MeasuredMagazineY', 'MeasuredMagazineZ')
                    fields(v, keys, typ + '.visual')
                    valid = (all(vector(v.get(k)) for k in keys if k not in ('Rotation', 'DrawScale'))
                             and isinstance(v.get('Rotation'), (list, tuple)) and len(v['Rotation']) == 3
                             and all(type(x) is int for x in v['Rotation']) and finite(v.get('DrawScale')) and v['DrawScale'] > 0)
                    if not valid:
                        pose_missing += 1
                    else:
                        frame = [rotate(axis, v['Rotation']) for axis in ((1, 0, 0), (0, 1, 0), (0, 0, 1))]
                        delta = [a - b for a, b in zip(v['MeasuredMagazinePosition'], v['Location'])]
                        local = [sum(a * b for a, b in zip(delta, axis)) / v['DrawScale'] for axis in frame]
                        error = math.dist(local, v['MagazineLocal']); model_errors.append(error)
                        position_errors.append(error * v['DrawScale'])
                        for axis in ('X', 'Y', 'Z'):
                            actual = [sum(a * b for a, b in zip(v['MeasuredMagazine' + axis], direction)) for direction in frame]
                            axis_errors.append(math.dist(actual, v['Magazine' + axis]))
            for check, good in state.items():
                if not good:
                    reject(line, typ, check)
        ready_before = now_ready
    checks = dict(required_fields_present=not missing)
    for typ, m in metrics.items():
        state = results[typ]; covered = m['samples'] >= MIN_SAMPLES
        checks[typ + '_covered'] = covered
        checks[typ + '_associated_calibrated_visual'] = covered and state['association'] and state['visual']
        checks[typ + '_capabilities_preserved'] = covered and state['capability']
        if typ == 'KFVRTrackedBullpup':
            checks['bullpup_translation_probe_remains_disabled'] = covered and state['translation']
        else:
            checks[typ + '_translation_calibrated'] = m['translation_samples'] >= MIN_SAMPLES and state['translation']
        checks[typ + '_magazine_socket_calibrated'] = m['socket_samples'] >= MIN_SAMPLES and state['socket']
        checks[typ + '_native_magazine_grasp_calibrated'] = covered and (m['native_samples'] == 0 or
            m['native_grasp_samples'] >= MIN_SAMPLES and state['native_grasp'])
        m['source_weapons'] = sorted(m['source_weapons']); m['visuals'] = sorted(m['visuals'])
    rifle = metrics['KFVRTrackedBullpup']
    checks['loaded_bullpup_remains_on_timed_reload'] = rifle['active_loaded_samples'] >= MIN_SAMPLES and results['KFVRTrackedBullpup']['timed']
    checks['no_magazine_grasp_without_manual_ownership'] = ownership_ok and (not any(m['native_samples'] for m in metrics.values()) or ownership_samples >= MIN_SAMPLES)
    checks['bullpup_magazine_remains_at_installed_rest'] = len(position_errors) >= MIN_SAMPLES and pose_missing == 0 and max(position_errors, default=math.inf) < .05
    checks['bullpup_magazine_retains_installed_axes'] = len(position_errors) >= MIN_SAMPLES and pose_missing == 0 and max(axis_errors, default=math.inf) < .002
    return dict(passed=all(checks.values()), checks=checks, metrics=metrics, coherent_rows=coherent_rows,
                required_samples=MIN_SAMPLES, missing_fields=missing, failures=failures,
                magazine_grasp_ownership_samples=ownership_samples, bullpup_pose_samples=len(position_errors),
                bullpup_pose_missing=pose_missing, bullpup_pose_settling_samples=pose_settling,
                max_bullpup_magazine_position_error_uu=max(position_errors, default=None),
                max_bullpup_magazine_position_error_model_uu=max(model_errors, default=None),
                max_bullpup_magazine_axis_error=max(axis_errors, default=None),
                action_motion_tested=False, grip_and_muzzle_validation_separate=True,
                live_game=True, synthetic_controllers=True, headset_presented=False)


def validate(folder):
    folder = Path(folder)
    rows = [json.loads(line) for line in (folder / 'weapon-observations.jsonl').read_text().splitlines()]
    report = analyze(rows)
    (folder / 'mechanical-profile-validation.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    if not report['passed']:
        raise ValueError('Mechanical profile checks failed: ' + ', '.join(k for k, v in report['checks'].items() if not v))
    return report


if __name__ == '__main__':
    import sys
    print(json.dumps(validate(sys.argv[1]), indent=2))
