"""Validate the 62-second live reload UX trial from recorded observations.

No game reads, writes, inputs or injection occur here. Geometry uses completed
rig snapshots and actual bone/palm observations; reported script error fields
are not evidence. Bullpup and SCAR have a magazine-only policy and no authored handle.
"""
import json
import math
from pathlib import Path

from validate_grip_pose import rotate

GUNS = dict(single='KFVRTrackedSingle', deagle='KFVRTrackedDeagle',
            mk23='KFVRTrackedMK23', bullpup='KFVRTrackedBullpup', scar='KFVRTrackedSCAR')
AXES = ((1, 0, 0), (0, 1, 0), (0, 0, 1))
MIN_SAMPLES = 3
# These controller phases deliberately add 2cm model-Y mismatch. Phase24's
# stationary recoil-overlap grab deliberately has no such input offset.
OFFSET_PHASES = (15, 16, 20, 21, 22, 26, 27, 31, 32, 33)


def finite(value):
    return type(value) in (int, float) and math.isfinite(value)


def vector(value):
    return isinstance(value, (list, tuple)) and len(value) == 3 and all(map(finite, value))


def distance(a, b):
    return math.dist(a, b) if vector(a) and vector(b) else math.inf


def pointer(values, name):
    return next((v for k, v in values.items() if k.casefold() == name.casefold()), None)


def world(visual, local):
    if not vector(local) or not vector(visual.get('Location')) or not finite(visual.get('DrawScale')):
        raise ValueError('Missing finite visual transform')
    r = visual.get('Rotation')
    if not isinstance(r, (list, tuple)) or len(r) != 3 or any(type(x) is not int for x in r):
        raise ValueError('Missing rotator')
    return [a+b for a, b in zip(visual['Location'], rotate([x*visual['DrawScale'] for x in local], r))]


def collect(rows, gun_type):
    records, rejected = [], []
    for row in rows:
        if row.get('rig_sample_consistent') is not True:
            continue
        objs = {o['address']: o for o in row.get('objects', []) if not o['values'].get('bDeleteMe')}
        rigs = [o for o in objs.values() if o['type'] == 'KFVRWeaponRig']
        if len(rigs) != 1:
            rejected.append(dict(uptime_ms=row.get('uptime_ms'), reason='rig ownership'))
            continue
        rig = rigs[0]; r = rig['values']; gun = objs.get(r.get('Gun'), {})
        if gun.get('type') != gun_type:
            continue
        visual = objs.get(r.get('Visual'), {}); manual = objs.get(r.get('ManualReload'), {})
        g = gun['values']; v = visual.get('values', {}); m = manual.get('values', {})
        if not (g.get('VRRig') == rig['address'] and visual.get('type') == 'KFVRWeaponVisual'
                and v.get('SourceWeapon') == gun['address'] and manual.get('type') == 'KFVRManualReload'
                and m.get('Gun') == gun['address'] and m.get('Rig') == rig['address']
                and g.get('Manual') == manual['address']):
            rejected.append(dict(uptime_ms=row.get('uptime_ms'), reason='gun/visual/manual ownership'))
            continue
        records.append(dict(row=row, objects=objs, rig=rig['address'], gun=gun['address'],
                            visual=visual['address'], manual=manual['address'], r=r, g=g, v=v, m=m))
    return records, rejected


def geometry(records, rifle, native):
    errors = {k: [] for k in ('slide', 'action_palm', 'action_target', 'action_axis',
                              'raw_palm', 'muzzle', 'right_palm', 'held_magazine',
                              'held_palm', 'magazine_axes', 'rest_magazine', 'ejected_magazine')}
    counts = dict(active=0, attached=0, held=0, ejected=0, resting=0)
    issues, offsets, action_frames, capture_evidence = [], [], [], []
    offset_evidence = {n: [] for n in OFFSET_PHASES}
    okay = dict(ownership=True, hand=True, profile=True, orientation=True)
    previous_held = False; captured_axes = None
    for rec in records:
        r, v, m, objs = (rec[k] for k in ('r', 'v', 'm', 'objects'))
        if not (r.get('bTracking') is True and r.get('bHolstered') is False and r.get('Tracking.Flags', 0) & 2):
            previous_held = False
            continue
        counts['active'] += 1
        try:
            units, scale = v['ModelUnitsPerMetre'], v['DrawScale']
            if not finite(units) or not 10 <= units <= 10000 or not finite(scale) or scale <= 0:
                raise ValueError('Invalid model scale')
            if not finite(r['Tracking.UnitsPerMetre']) or r['Tracking.UnitsPerMetre'] <= 0:
                raise ValueError('Invalid world scale')
            okay['profile'] &= (v.get('bHidden') is False and v.get('bMagazineProfile') is True
                and v.get('bInsertionCalibrated') is True and v.get('bInsertionFailed') is False
                and v.get('bFeedOnSeat') is rifle and v.get('bActionProfile') is (not rifle))
            errors['muzzle'].append(distance(r.get('MuzzleLocation'), world(v, v['TipLocal'])))
            right = objs.get(pointer(r, 'RightHand'), {}); rh = right.get('values', {})
            okay['hand'] &= (right.get('type') == 'KFVRHandVisual' and rh.get('bHidden') is False
                              and rh.get('bNativeHandMesh') is native)
            if native:
                okay['hand'] &= rh.get('bGripPose') is True and rh.get('GripVisual') == rec['visual']
                errors['right_palm'].append(distance(rh.get('PalmWorld'), world(v, v['GripPose.16.Origin'])))
            else:
                errors['right_palm'].append(distance(rh.get('PalmWorld'), r.get('Tracking.RightPosition')))
            left = objs.get(pointer(r, 'LeftHand'), {}); h = left.get('values', {})
            held = m.get('bSlideHeld') is True
            okay['ownership'] &= h.get('bActionAttached') is held
            if held:
                okay['ownership'] &= (not rifle and r.get('bSupportHeld') is False and m.get('HeldMagazine') == 0
                    and (r.get('Tracking.Flags', 0) & 3) == 3 and h.get('bHidden') is False
                    and h.get('bGripPose') is False and h.get('bNativeHandMesh') is native)
            if not rifle:
                axis, amount = v['ActionPullAxis'], v['ActionTravelMetres']
                if abs(distance(axis, (0, 0, 0))-1) > .001 or not finite(amount) or not .01 <= amount <= .25:
                    raise ValueError('Invalid native action profile')
                travel = m['SlideTravel']
                if not finite(travel) or not 0 <= travel <= 1:
                    raise ValueError('Invalid action travel')
                slide = [x+y*amount*units*travel for x, y in zip(v['SlideLocal'], axis)]
                errors['slide'].append(distance(v.get('MeasuredActionPosition'), world(v, slide)))
                if held:
                    counts['attached'] += 1
                    point = world(v, [x+y*units for x, y in zip(slide, v['ActionPalmOffset'])])
                    errors['action_palm'].append(distance(h.get('PalmWorld'), point))
                    errors['action_target'].extend((distance(h.get('ActionPalmTarget'), point),
                                                    distance(m.get('ActionPalmWorld'), point)))
                    errors['raw_palm'].append(distance(h.get('TrackedPalmPosition'), r.get('Tracking.LeftPosition')))
                    offsets.append(distance(h.get('PalmWorld'), h.get('TrackedPalmPosition'))/r['Tracking.UnitsPerMetre'])
                    diagnostic_phase = rec.get('diagnostic_phase')
                    if diagnostic_phase in offset_evidence:
                        offset_evidence[diagnostic_phase].append(offsets[-1])
                    local_axes = [m['ActionPalm'+suffix] for suffix in 'XYZ']
                    if not all(vector(a) for a in local_axes):
                        raise ValueError('Missing finite captured action frame')
                    okay['orientation'] &= all(abs(sum(x*y for x, y in zip(a, b))-(1 if i == j else 0)) < .002
                        for i, a in enumerate(local_axes) for j, b in enumerate(local_axes))
                    if not previous_held:
                        captured_axes = [list(a) for a in local_axes]
                        phase_id = rec.get('input_phase')
                        # Phase24 grabs during recoil, which may finish before the
                        # observer's first completed snapshot. Its current weapon
                        # frame cannot reconstruct the earlier acquisition frame.
                        # Still require a fixed captured frame and independently
                        # matching actual hand axes throughout that entire grab.
                        compare_capture = phase_id != 24
                        if compare_capture:
                            gun_axes = [rotate(a, v['Rotation']) for a in AXES]
                            expected_capture = [[sum(x*y for x, y in zip(rotate(a, r['Tracking.LeftRotation']), b))
                                                 for b in gun_axes] for a in AXES]
                            okay['orientation'] &= all(distance(a, b) < .002 for a, b in zip(local_axes, expected_capture))
                        capture_evidence.append(dict(uptime_ms=rec['row']['uptime_ms'], phase=phase_id,
                            controller_relation_checked=compare_capture,
                            acquisition_frame_unobserved_during_recoil=not compare_capture))
                    okay['orientation'] &= all(distance(a, b) < .002 for a, b in zip(local_axes, captured_axes))
                    for index, a in enumerate(AXES):
                        expected = rotate(local_axes[index], v['Rotation'])
                        for rot in (h['Rotation'], h['ActionPalmRotation'], m['ActionPalmRotation']):
                            errors['action_axis'].append(distance(rotate(a, rot), expected))
                    action_frames.append(v['Rotation'])
            else:
                okay['profile'] &= v.get('ActionTravelMetres') == 0 and m.get('SlideTravel') == 0 and m.get('RackCount') == 0
            previous_held = held
            if m.get('HeldMagazine', 0) > 0 and (r.get('Tracking.Flags', 0) & 3) == 3:
                counts['held'] += 1
                okay['ownership'] &= r.get('bSupportHeld') is False and h.get('bActionAttached') is False
                expected = r['Tracking.LeftPosition']
                if native:
                    grip_obj = objs.get(v.get('MagazineGrip'), {}); grip = grip_obj.get('values', {})
                    okay['hand'] &= (grip_obj.get('type') == 'KFVRMagazineGrip' and grip.get('bCalibrated') is True
                        and grip.get('SourceWeapon') == rec['gun'] and h.get('bGripPose') is True
                        and h.get('GripVisual') == grip_obj.get('address') and h.get('bGripSourceLeft') is True)
                    offset = rotate([x*scale for x in grip['PalmLocal']], r['Tracking.LeftRotation'])
                    expected = [x-y for x, y in zip(expected, offset)]
                errors['held_magazine'].append(distance(v.get('MeasuredMagazinePosition'), expected))
                errors['held_palm'].append(distance(h.get('PalmWorld'), r.get('Tracking.LeftPosition')))
                for suffix in 'XYZ':
                    errors['magazine_axes'].append(distance(v.get('MeasuredMagazine'+suffix), rotate(v['Magazine'+suffix], r['Tracking.LeftRotation'])))
            elif m.get('bEjectedVisible') is True:
                counts['ejected'] += 1
                okay['ownership'] &= m.get('HeldMagazine') == m.get('InsertedMagazine') == 0
                errors['ejected_magazine'].append(distance(v.get('MeasuredMagazinePosition'), m.get('EjectedPosition')))
                for suffix in 'XYZ':
                    errors['magazine_axes'].append(distance(v.get('MeasuredMagazine'+suffix), rotate(v['Magazine'+suffix], m['EjectedRotation'])))
            elif m.get('InsertedMagazine', 0) > 0:
                counts['resting'] += 1
                errors['rest_magazine'].append(distance(v.get('MeasuredMagazinePosition'), world(v, v['MagazineLocal'])))
                for suffix in 'XYZ':
                    errors['magazine_axes'].append(distance(v.get('MeasuredMagazine'+suffix), rotate(v['Magazine'+suffix], v['Rotation'])))
        except (KeyError, TypeError, ValueError, IndexError, ZeroDivisionError, OverflowError) as error:
            if len(issues) < 12:
                issues.append(dict(uptime_ms=rec['row']['uptime_ms'], error=str(error)))
    def bounded(name, limit, count=MIN_SAMPLES):
        values = errors[name]
        return len(values) >= count and all(finite(x) and x < limit for x in values)
    checks = dict(geometry_has_required_observations=counts['active'] >= 20 and not issues,
        geometry_has_correct_weapon_capabilities=counts['active'] >= 20 and okay['profile'],
        manipulation_owns_only_offhand=okay['ownership'] and counts['held'] >= 3,
        actual_dominant_grip_preserved=okay['hand'] and bounded('right_palm', .005 if native else 3, 10),
        actual_muzzle_stays_on_weapon=bounded('muzzle', .05, 10),
        actual_magazine_follows_offhand=bounded('held_magazine', .05, 5) and bounded('held_palm', .005 if native else .05, 5),
        actual_inserted_magazine_stays_on_weapon=bounded('rest_magazine', .05, 10),
        actual_magazine_axes_follow_owner=bounded('magazine_axes', .002, 15),
        actual_discarded_magazine_follows_fall=bounded('ejected_magazine', .05, 3))
    if not rifle:
        checks.update(actual_native_slide_follows_measured_rail=bounded('slide', .05, 20),
            actual_action_hand_locks_to_slide=bounded('action_palm', .005, 10) and bounded('action_target', .005, 10),
            action_hand_keeps_captured_axes=okay['orientation'] and bounded('action_axis', .002, 30),
            action_hand_retains_raw_tracking_telemetry=bounded('raw_palm', .005, 10),
            action_hand_attachment_differs_from_free_tracking=all(len(values) >= MIN_SAMPLES and all(.015 < x < .025 for x in values) for values in offset_evidence.values()),
            action_hand_lock_exercised_on_tilted_weapon=any(abs(r[0]) > 1000 and abs(r[2]) > 1000 for r in action_frames))
    return checks, dict(counts=counts, errors={k:dict(samples=len(v), maximum=max(v) if v and all(map(finite, v)) else None)
                                             for k, v in errors.items()}, missing=issues,
                        action_palm_tracking_offset_metres=dict(samples=len(offsets), minimum=min(offsets, default=None), maximum=max(offsets, default=None)),
                        deliberate_offset_phases={str(n):dict(samples=len(values), required=MIN_SAMPLES, minimum=min(values, default=None), maximum=max(values, default=None)) for n, values in offset_evidence.items()},
                        action_capture_evidence=capture_evidence)


def chest_anchor(r):
    """Independent body-space reconstruction, not Manual.SupplyLocation."""
    units, head, body = r['Tracking.UnitsPerMetre'], r['Tracking.HeadPosition'], r['Tracking.BodyRotation']
    if not finite(units) or not 1 <= units <= 200 or not vector(head):
        raise ValueError('Invalid chest head/scale')
    if not isinstance(body, (list, tuple)) or len(body) != 3 or any(type(x) is not int for x in body):
        raise ValueError('Missing body rotator')
    return [x+y for x, y in zip(head, rotate([x*units for x in (.22, -.10, -.36)], body))]


def supply_geometry(records):
    errors = {k: [] for k in ('manual_anchor', 'actual_center', 'measured_center', 'orientation', 'radius_metres')}
    issues = []; discovery_gaps = []; counts = dict(active=0, visible=0, hidden=0, discovery_gaps=0)
    okay = dict(ownership=True, visibility=True, native_asset=True)
    for rec in records:
        r, m, objs = (rec[k] for k in ('r', 'm', 'objects'))
        try:
            # A suspended manual does not republish its stored anchor. Compare
            # only after both tracked hands permit the ordinary update.
            active = r['bTracking'] is True and r['bHolstered'] is False and (r['Tracking.Flags'] & 3) == 3
            actor = objs.get(r['AmmoSupply'], {})
            supplies = [o for o in objs.values() if o['type'] == 'KFVRAmmoSupplyVisual']
            if active:
                counts['active'] += 1
                anchor = chest_anchor(r)
                errors['manual_anchor'].append(distance(m['SupplyLocation'], anchor))
            # At an empty well all owned non-chamber rounds are stored/loose
            # supply. This reads the conserved ledger, not a visual flag.
            available = (active and not m['bSlideHeld'] and m['InsertedMagazine'] == m['HeldMagazine'] == 0
                         and m['OwnedRounds'] > m['LoadedRounds'])
            if not actor:
                okay['ownership'] &= not supplies
                if r['AmmoSupply'] not in (None, '0x0', 0):
                    # The observer discovers newly spawned actors separately
                    # from coherent value reads. A non-null pointer whose
                    # object is absent is unknown, not hidden/visible evidence.
                    # Known visible and hidden rows still need full coverage.
                    counts['discovery_gaps'] += 1
                    if len(discovery_gaps) < 12:
                        discovery_gaps.append(dict(uptime_ms=rec['row']['uptime_ms'], address=r['AmmoSupply'], available=available))
                else:
                    okay['visibility'] &= not available
                continue
            a = actor['values']
            okay['ownership'] &= (actor['type'] == 'KFVRAmmoSupplyVisual' and len(supplies) == 1
                                  and a['Owner'] == rec['rig'] and r['bAmmoSupplyFailed'] is False)
            okay['native_asset'] &= (a['bInitialized'] is True and a['NativeAssetPath'] == 'KillingFloorStatics.L85Ammo'
                                    and a['StaticMesh'] not in (None, '0x0', 0))
            okay['visibility'] &= type(a['bHidden']) is bool and a['bHidden'] is (not available)
            if a['bHidden'] is True:
                counts['hidden'] += 1
                continue
            counts['visible'] += 1
            anchor = chest_anchor(r)
            units, radius, diameter, scale = r['Tracking.UnitsPerMetre'], a['LocalRadius'], a['DisplayDiameterMetres'], a['DrawScale']
            if not all(map(finite, (radius, diameter, scale))) or not .01 < radius < 10000 or not .08 <= diameter <= .32 or scale <= 0:
                raise ValueError('Invalid native supply bounds/scale')
            errors['actual_center'].append(distance(world(a, a['LocalCenter']), anchor))
            errors['measured_center'].append(distance(a['MeasuredCenter'], anchor))
            errors['radius_metres'].append(abs(radius*scale/units-diameter*.5))
            local = a['LocalDisplayRotation']
            if not isinstance(local, (list, tuple)) or len(local) != 3 or any(type(x) is not int for x in local):
                raise ValueError('Missing local supply rotator')
            for axis in AXES:
                errors['orientation'].append(distance(rotate(axis, a['Rotation']), rotate(rotate(axis, local), r['Tracking.BodyRotation'])))
        except (KeyError, TypeError, ValueError, IndexError, ZeroDivisionError, OverflowError) as error:
            if len(issues) < 12:
                issues.append(dict(uptime_ms=rec['row']['uptime_ms'], error=str(error)))
    def bounded(name, limit, count=10):
        values = errors[name]
        return len(values) >= count and all(finite(x) and x < limit for x in values)
    checks = dict(chest_supply_has_required_observations=counts['active'] >= 20 and counts['visible'] >= 10 and counts['hidden'] >= 10 and not issues,
        chest_supply_owned_by_current_rig=counts['visible'] >= 10 and okay['ownership'],
        chest_supply_uses_audited_native_asset=counts['visible'] >= 10 and okay['native_asset'],
        chest_supply_visibility_matches_available_magazines=counts['visible'] >= 10 and counts['hidden'] >= 10 and okay['visibility'],
        chest_interaction_anchor_matches_independent_body_geometry=bounded('manual_anchor', .005, 20),
        actual_native_supply_center_matches_chest=bounded('actual_center', .005) and bounded('measured_center', .005),
        actual_supply_rotation_matches_body_and_local_tilt=bounded('orientation', .002, 30),
        actual_supply_scale_matches_configured_metres=bounded('radius_metres', .00001))
    return checks, dict(counts=counts, missing=issues, discovery_gaps=discovery_gaps,
        unknown_asset_samples=counts['discovery_gaps'],
        errors={k:dict(samples=len(v), maximum=max(v) if v and all(map(finite, v)) else None) for k, v in errors.items()},
        authored_anchor_metres=[.22, -.10, -.36], interaction_radius_metres=.16,
        native_mesh='KillingFloorStatics.L85Ammo', comfort_tested=False)


def analyze(controls, rows, fixture, weapon='single'):
    if weapon not in GUNS:
        raise ValueError('Unsupported reload UX weapon: '+weapon)
    rifle = weapon in ('bullpup', 'scar'); checks = {}; metrics = {}; notes = []
    records, rejected = collect(rows, GUNS[weapon])
    sent = [c for c in controls if c.get('sent') is True]
    ranges = {n:[c['uptime_ms'] for c in sent if c.get('phase') == n] for n in range(61)}
    def phase(n, settle=150):
        times = ranges[n]
        return [p for p in records if times and min(times)+settle <= p['row']['uptime_ms'] <= max(times)]
    def coverage(data):
        return len(data) >= MIN_SAMPLES and len({p['r'].get('Tracking.InputSerial') for p in data}) >= MIN_SAMPLES
    def at(name, n, predicate, settle=150):
        data = phase(n, settle); failures = []
        for p in data:
            try:
                if not predicate(p): failures.append(p['row']['uptime_ms'])
            except (KeyError, TypeError, ValueError, IndexError, ZeroDivisionError, OverflowError) as e:
                failures.append(p['row']['uptime_ms'])
                if len(notes) < 12: notes.append(dict(check=name, problem=str(e)))
        checks[name] = coverage(data) and not failures
        metrics[str(n)] = dict(coherent_samples=len(data), distinct_inputs=len({p['r'].get('Tracking.InputSerial') for p in data}), required=MIN_SAMPLES, settle_ms=settle)
        if failures and len(notes) < 12: notes.append(dict(check=name, failed_sample_times=failures[:5]))
    def ma(name, n, predicate, settle=150):
        at(name, n, lambda p:predicate(p['m']), settle)
    def last(n):
        data = phase(n)
        return data[-1] if data else dict(m={}, r={}, g={}, v={})
    def field(n, key):
        return last(n)['m'].get(key)
    checks['all_controller_phases_sent'] = all(len(ranges[n]) >= 3 for n in range(61))
    checks['no_Y_mode_toggle_sent'] = bool(sent) and all(c.get('left_secondary') is False for c in sent)
    checks['left_trigger_unused'] = bool(sent) and all(finite(c.get('left_trigger')) and c['left_trigger'] == 0 for c in sent)
    checks['dominant_grip_unused_after_draw'] = bool(sent) and all(finite(c.get('right_squeeze')) and c['right_squeeze'] == 0 for c in sent if c.get('phase', -1) >= 2)
    for n in (4, 37, 39, 56):
        commands = [c for c in sent if c.get('phase') == n]
        checks['release_button_sent_phase_'+str(n)] = len(commands) >= 3 and all(c.get('right_secondary') is True for c in commands)
    for n in (15, 16, 20, 21, 22, 26, 27, 29, 31, 32, 33, 42, 43, 51, 52):
        commands = [c for c in sent if c.get('phase') == n]
        checks['grip_only_action_sent_phase_'+str(n)] = len(commands) >= 3 and all(finite(c.get('left_squeeze')) and c['left_squeeze'] > .7 and c.get('left_trigger') == c.get('right_squeeze') == 0 for c in commands)
    main = [p for p in records if ranges[2] and p['row']['uptime_ms'] >= min(ranges[2])+150
            and ranges[60] and p['row']['uptime_ms'] <= max(ranges[60])]
    for p in main:
        p['input_phase'] = max((n for n, times in ranges.items()
            if times and min(times) <= p['row']['uptime_ms']), default=None)
        p['diagnostic_phase'] = next((n for n, times in ranges.items()
            if times and min(times)+150 <= p['row']['uptime_ms'] <= max(times)), None)
    checks['one_owned_selected_weapon'] = len(main) >= 30 and len({p['gun'] for p in main}) == len({p['manual'] for p in main}) == 1
    checks['ownership_links_complete_during_trial'] = bool(main) and not any(main[0]['row']['uptime_ms'] <= e['uptime_ms'] <= main[-1]['row']['uptime_ms'] for e in rejected)
    native = fixture.get('native_hands')
    checks['requested_hand_mode_consistent'] = type(native) is bool and bool(main) and all(p['r'].get('bNativeHandTest') is native for p in main)
    checks['physical_reload_enabled_without_mode_toggle'] = bool(main) and all(p['r'].get('bPhysicalReloadDefault') is True for p in main)
    at('ready_weapon_has_live_manual_ledger', 2, lambda p:p['r'].get('bHolstered') is False and p['r'].get('bTracking') is True and p['m']['InsertedMagazine'] > 0 and p['m']['LoadedRounds'] > 1)
    initial_id = field(2, 'InsertedMagazine'); baseline_spent = field(2, 'SpentRounds')
    ma('catch_preserves_one_chamber', 5, lambda m:m['HeldMagazine'] == initial_id and m['InsertedMagazine'] == 0 and m['LoadedRounds'] == 1 and m['bChambered'] and m['EjectedVisualCount'] == field(2, 'EjectedVisualCount'))
    ma('chamber_shot_and_followup_dry', 7, lambda m:m['SpentRounds'] == baseline_spent+1 and m['LoadedRounds'] == 0 and not m['bChambered'])
    ma('partial_magazine_stowed', 9, lambda m:m['StowCount'] == field(2, 'StowCount')+1 and m['HeldMagazine'] == 0)
    ma('fresh_magazine_grabbed', 10, lambda m:m['HeldMagazine'] > 0 and m['HeldMagazine'] != initial_id and m['GrabCount'] == field(2, 'GrabCount')+1)
    fresh_id = field(10, 'HeldMagazine')
    ma('continuous_insertion_seats', 12, lambda m:m['InsertedMagazine'] == fresh_id and m['HeldMagazine'] == 0 and m['SeatCount'] == field(2, 'SeatCount')+1 and m['bChambered'] is rifle)
    checks['seating_precedes_grip_release'] = any(p['m'].get('InsertedMagazine') == fresh_id and p['m'].get('SeatCount') == 1 and p['r'].get('Tracking.LeftGrip', 0) > .7 for p in phase(11, 0))
    if rifle:
        checks['no_invented_rifle_action_actor'] = not any(o['type'] == 'KFVRActionVisual' and not o['values'].get('bDeleteMe') for row in rows for o in row.get('objects', []))
        checks['rifle_remains_magazine_only'] = bool(main) and all(p['v'].get('bFeedOnSeat') is True and p['v'].get('bActionProfile') is False and p['v'].get('ActionTravelMetres') == 0 and p['m'].get('bSlideHeld') is False and p['m'].get('RackCount') == 0 for p in main)
    else:
        ma('seating_does_not_feed_pistol', 14, lambda m:not m['bChambered'] and m['SpentRounds'] == baseline_spent+1)
        ma('partial_pull_measured_without_feed', 16, lambda m:m['bSlideHeld'] and abs(m['SlideTravel']-.02/last(16)['v']['ActionTravelMetres']) < .015 and not m['bChambered'])
        ma('partial_release_does_not_feed', 19, lambda m:not m['bSlideHeld'] and m['SlideTravel'] <= .1 and m['RackCount'] == 0 and not m['bChambered'] and m['SpentRounds'] == baseline_spent+1)
        ma('full_pull_blocks_trigger', 22, lambda m:m['bSlideHeld'] and m['SlideTravel'] > .9 and not m['bChambered'] and m['SpentRounds'] == baseline_spent+1)
        ma('release_after_full_pull_feeds', 23, lambda m:not m['bSlideHeld'] and m['RackCount'] == 1 and m['bChambered'] and m['SlideTravel'] <= .1)
        # The driver starts this grab 50ms after firing, with the real controller
        # fixed. Recoil may move presentation but must never cycle the ledger.
        commands = [c for c in sent if c.get('phase') == 24 and c.get('reload_phase_ms', -1) >= 50]
        checks['recoil_overlap_grip_sent'] = len(commands) >= 3 and all(c.get('left_squeeze', 0) > .7 and c.get('left_trigger') == c.get('right_squeeze') == 0 for c in commands)
        ma('stationary_grab_during_fire_cannot_cycle_action', 24, lambda m:m['bSlideHeld'] and m['SlideTravel'] < .02 and m['bChambered'] and m['RackCount'] == 1 and m['DiscardedRounds'] == field(23, 'DiscardedRounds'))
        ma('recoil_grab_release_cannot_feed', 25, lambda m:not m['bSlideHeld'] and m['SlideTravel'] <= .1 and m['bChambered'] and m['RackCount'] == 1 and m['DiscardedRounds'] == field(23, 'DiscardedRounds'))
        ma('full_pull_extracts_one_live_chamber', 27, lambda m:m['bSlideHeld'] and m['SlideTravel'] > .9 and not m['bChambered'] and m['DiscardedRounds'] == field(26, 'DiscardedRounds')+1 and m['RackCount'] == 1)
        at('tracking_loss_cancels_hand_and_feed', 28, lambda p:(p['r']['Tracking.Flags'] & 3) == 2 and not p['m']['bSlideHeld'] and not p['m']['bChambered'] and p['m']['RackCount'] == 1 and p['m']['SlideTravel'] <= .1 and p['objects'][pointer(p['r'], 'LeftHand')]['values'].get('bActionAttached') is False)
        for n in (29, 30):
            ma('recovery_needs_fresh_grip_phase_'+str(n), n, lambda m:not m['bSlideHeld'] and not m['bChambered'] and m['RackCount'] == 1 and m['SlideTravel'] <= .1)
        ma('fresh_grip_after_tracking_loss', 31, lambda m:m['bSlideHeld'] and not m['bChambered'] and m['RackCount'] == 1)
        ma('second_full_pull_awaits_return', 32, lambda m:m['bSlideHeld'] and m['SlideTravel'] > .9 and not m['bChambered'] and m['RackCount'] == 1)
        ma('guided_forward_return_feeds_while_gripped', 33, lambda m:m['bSlideHeld'] and m['SlideTravel'] <= .1 and m['bChambered'] and m['RackCount'] == 2)
        ma('later_release_cannot_feed_twice', 34, lambda m:not m['bSlideHeld'] and m['RackCount'] == 2 and m['LoadedRounds'] == field(33, 'LoadedRounds') and m['DiscardedRounds'] == field(27, 'DiscardedRounds'))
    for before, after, label in ((5, 7, 'retained_chamber'), (12, 14, 'after_seat'), (17, 19, 'after_partial'), (21, 23, 'while_open'), (23, 25, 'after_full_return'), (34, 36, 'after_guided_return')):
        a, b = last(before), last(after)
        try:
            spent = b['m']['SpentRounds']-a['m']['SpentRounds']
            trace = b['r']['TraceCount']-a['r']['TraceCount']
            attempts = b['g']['VRFireAttempts']-a['g']['VRFireAttempts']
            should_fire = rifle or label in ('retained_chamber', 'after_full_return', 'after_guided_return')
            checks['actual_stock_fire_'+label] = coverage(phase(before)) and coverage(phase(after)) and spent == trace and attempts >= trace and (spent > 0 if should_fire else spent == 0)
        except (KeyError, TypeError):
            checks['actual_stock_fire_'+label] = False
    budgets = []
    try:
        budgets = [sum(p['m'][k] for k in ('OwnedRounds', 'SpentRounds', 'DiscardedRounds')) for p in main]
        checks['every_coherent_ledger_conserves_ammo'] = len(main) >= 30 and len(set(budgets)) == 1 and all(p['m']['bValid'] is True and all(type(p['m'][k]) is int and p['m'][k] >= 0 for k in ('OwnedRounds', 'SpentRounds', 'DiscardedRounds')) for p in main)
        checks['actual_loaded_ammo_matches_ledger'] = bool(main) and all(p['g']['MagAmmoRemaining'] == p['m']['LoadedRounds'] for p in main)
        checks['stock_timed_reload_never_runs'] = bool(main) and all(p['g']['bIsReloading'] is False for p in main)
        a, b = last(2), last(41)
        checks['actual_stock_shots_match_conserved_spent'] = b['r']['TraceCount']-a['r']['TraceCount'] == b['m']['SpentRounds']-a['m']['SpentRounds'] > 0 and b['g']['VRFireAttempts']-a['g']['VRFireAttempts'] >= b['m']['SpentRounds']-a['m']['SpentRounds']
    except (KeyError, TypeError):
        for name in ('every_coherent_ledger_conserves_ammo', 'actual_loaded_ammo_matches_ledger', 'stock_timed_reload_never_runs', 'actual_stock_shots_match_conserved_spent'):
            checks[name] = False
    ma('uncaught_eject_discards_exact_magazine_rounds', 38, lambda m:m['InsertedMagazine'] == m['HeldMagazine'] == 0 and m['EjectCount'] == field(36, 'EjectCount')+1 and m['EjectedVisualCount'] == field(36, 'EjectedVisualCount')+1 and field(36, 'LoadedRounds') > int(field(36, 'bChambered')) and m['DiscardedRounds']-field(36, 'DiscardedRounds') == field(36, 'LoadedRounds')-int(field(36, 'bChambered')) and m['OwnedRounds'] == field(36, 'OwnedRounds')-(m['DiscardedRounds']-field(36, 'DiscardedRounds')) and m['bChambered'] == field(36, 'bChambered') and m['SpentRounds'] == field(36, 'SpentRounds'))
    for n in (39, 40, 41):
        # Neutral phase40 permits no ledger transition after duplicate B39;
        # its earliest completed snapshots are valid unchanged-state evidence.
        # Keep three distinct coherent inputs and the separate39/41 checks.
        ma('repeat_release_cannot_duplicate_eject_phase_'+str(n), n, lambda m:all(m[k] == field(38, k) for k in ('EjectCount', 'EjectedVisualCount', 'DropCount', 'OwnedRounds', 'SpentRounds', 'DiscardedRounds', 'HeldMagazine', 'InsertedMagazine', 'LoadedRounds')), settle=0 if n == 40 else 150)
    fall = [p for n in (37, 38) for p in phase(n, 0) if p['m'].get('bEjectedVisible') is True]
    positions = [p['v'].get('MeasuredMagazinePosition') for p in fall]
    travel = max((distance(a, b) for a in positions for b in positions), default=0)
    checks['discarded_magazine_has_visible_measured_motion'] = len(fall) >= 3 and all(vector(x) for x in positions) and finite(travel) and travel > .01*min((p['r'].get('Tracking.UnitsPerMetre', math.inf) for p in fall), default=math.inf)
    ma('discarded_visual_expires', 41, lambda m:m['bEjectedVisible'] is False)
    ma('exactly_one_uncaught_visual_and_two_releases', 41, lambda m:m['EjectedVisualCount'] == field(2, 'EjectedVisualCount')+1 and m['EjectCount'] == field(2, 'EjectCount')+2 and m['DropCount'] == field(2, 'DropCount'))
    # The original trial ends at41; its exact drop/shot assertions above keep
    # that baseline. These additional stages exercise the visible chest supply
    # after an uncaught ejection and manipulation with an empty magazine well.
    ma('partial_magazine_stowed_at_chest', 9, lambda m:m['ChestStowCount'] == field(2, 'ChestStowCount')+1)
    ma('first_fresh_magazine_grab_uses_chest', 10, lambda m:m['ChestGrabCount'] == field(2, 'ChestGrabCount')+1)
    for n in (8, 9, 10, 46, 47, 57, 59):
        at('controller_reaches_independent_chest_phase_'+str(n), n,
           lambda p:distance(p['r']['Tracking.LeftPosition'], chest_anchor(p['r'])) < .005)
    if rifle:
        for n in (42, 43, 44, 45, 46):
            ma('empty_well_rifle_gesture_cannot_change_ledger_phase_'+str(n), n,
               lambda m:all(m[k] == field(41, k) for k in ('HeldMagazine', 'InsertedMagazine', 'OwnedRounds', 'SpentRounds',
                    'DiscardedRounds', 'LoadedRounds', 'bChambered', 'RackCount', 'SeatCount', 'Revision')))
    else:
        ma('empty_well_action_can_be_grabbed', 42, lambda m:m['InsertedMagazine'] == m['HeldMagazine'] == 0
           and m['bSlideHeld'] and m['SlideTravel'] <= .1 and m['bChambered'] == field(41, 'bChambered'))
        ma('empty_well_full_pull_extracts_only_chamber', 43, lambda m:m['InsertedMagazine'] == m['HeldMagazine'] == 0
           and m['bSlideHeld'] and m['SlideTravel'] > .9 and m['LoadedRounds'] == 0 and not m['bChambered']
           and m['DiscardedRounds'] == field(41, 'DiscardedRounds')+int(field(41, 'bChambered'))
           and m['OwnedRounds'] == field(41, 'OwnedRounds')-int(field(41, 'bChambered'))
           and m['SpentRounds'] == field(41, 'SpentRounds') and m['RackCount'] == field(41, 'RackCount'))
        ma('empty_well_return_cannot_feed', 44, lambda m:m['InsertedMagazine'] == m['HeldMagazine'] == 0
           and not m['bSlideHeld'] and m['SlideTravel'] <= .1 and not m['bChambered'] and m['LoadedRounds'] == 0
           and m['RackCount'] == field(41, 'RackCount')+1
           and all(m[k] == field(43, k) for k in ('OwnedRounds', 'SpentRounds', 'DiscardedRounds', 'SeatCount')))
        at('empty_well_trigger_is_dry', 45, lambda p:p['m']['LoadedRounds'] == 0 and not p['m']['bChambered']
           and p['m']['SpentRounds'] == field(44, 'SpentRounds') and p['r']['TraceCount'] == last(44)['r']['TraceCount'])
    ma('empty_well_chest_is_ready', 46, lambda m:m['HeldMagazine'] == m['InsertedMagazine'] == 0
       and not m['bSlideHeld'] and m['OwnedRounds'] > m['LoadedRounds']
       and all(m[k] == field(44, k) for k in ('OwnedRounds', 'SpentRounds', 'DiscardedRounds', 'LoadedRounds', 'bChambered')))
    ma('uncaught_reload_gets_fresh_chest_magazine', 47, lambda m:m['InsertedMagazine'] == 0 and m['HeldMagazine'] > 0
       and m['GrabCount'] == field(46, 'GrabCount')+1 and m['ChestGrabCount'] == field(46, 'ChestGrabCount')+1
       and all(m[k] == field(46, k) for k in ('OwnedRounds', 'SpentRounds', 'DiscardedRounds', 'LoadedRounds', 'bChambered', 'EjectCount', 'DropCount', 'SeatCount')))
    chest_id = field(47, 'HeldMagazine')
    ma('chest_magazine_seats_with_expected_feed_policy', 49, lambda m:m['InsertedMagazine'] == chest_id and m['HeldMagazine'] == 0
       and m['SeatCount'] == field(47, 'SeatCount')+1 and m['bChambered'] is rifle
       and m['LoadedRounds'] > int(m['bChambered'])
       and all(m[k] == field(47, k) for k in ('OwnedRounds', 'SpentRounds', 'DiscardedRounds', 'DropCount', 'ChestGrabCount')))
    checks['chest_magazine_seats_before_grip_release'] = any(p['m'].get('InsertedMagazine') == chest_id
        and p['m'].get('SeatCount') == field(47, 'SeatCount')+1 and p['r'].get('Tracking.LeftGrip', 0) > .7 for p in phase(48, 0))
    if not rifle:
        ma('chest_seated_pistol_stays_dry_before_action', 51, lambda m:m['bSlideHeld'] and not m['bChambered']
           and m['SpentRounds'] == field(49, 'SpentRounds') and m['LoadedRounds'] == field(49, 'LoadedRounds'))
        ma('chest_reload_full_pull_awaits_return', 52, lambda m:m['bSlideHeld'] and m['SlideTravel'] > .9
           and not m['bChambered'] and m['RackCount'] == field(44, 'RackCount')
           and m['DiscardedRounds'] == field(49, 'DiscardedRounds'))
        ma('chest_reload_return_feeds_once', 53, lambda m:not m['bSlideHeld'] and m['SlideTravel'] <= .1 and m['bChambered']
           and m['RackCount'] == field(44, 'RackCount')+1 and m['LoadedRounds'] == field(49, 'LoadedRounds')
           and m['DiscardedRounds'] == field(49, 'DiscardedRounds'))
    for before, after, label, should_fire in ((49, 51, 'chest_seated', rifle), (53, 55, 'chest_completed', True)):
        a, b = last(before), last(after)
        try:
            spent = b['m']['SpentRounds']-a['m']['SpentRounds']
            trace = b['r']['TraceCount']-a['r']['TraceCount']
            attempts = b['g']['VRFireAttempts']-a['g']['VRFireAttempts']
            checks['actual_stock_fire_'+label] = coverage(phase(before)) and coverage(phase(after)) and spent == trace and attempts >= trace and (spent > 0 if should_fire else spent == 0)
        except (KeyError, TypeError):
            checks['actual_stock_fire_'+label] = False
    ma('second_uncaught_eject_discards_exact_magazine_rounds', 56, lambda m:m['InsertedMagazine'] == m['HeldMagazine'] == 0
       and m['EjectCount'] == field(55, 'EjectCount')+1 and m['EjectedVisualCount'] == field(55, 'EjectedVisualCount')+1
       and m['DiscardedRounds']-field(55, 'DiscardedRounds') == field(55, 'LoadedRounds')-int(field(55, 'bChambered'))
       and m['OwnedRounds'] == field(55, 'OwnedRounds')-(m['DiscardedRounds']-field(55, 'DiscardedRounds'))
       and m['bChambered'] == field(55, 'bChambered') and m['SpentRounds'] == field(55, 'SpentRounds'))
    ma('second_chest_grab_needs_no_catch', 57, lambda m:m['HeldMagazine'] > 0 and m['InsertedMagazine'] == 0
       and m['ChestGrabCount'] == field(56, 'ChestGrabCount')+1 and m['GrabCount'] == field(56, 'GrabCount')+1
       and all(m[k] == field(56, k) for k in ('OwnedRounds', 'SpentRounds', 'DiscardedRounds', 'SeatCount', 'DropCount', 'LoadedRounds', 'bChambered')))
    at('lost_offhand_keeps_chest_magazine_without_seating', 58, lambda p:(p['r']['Tracking.Flags'] & 3) == 2
       and p['m']['bRecoveryPending'] is True and p['m']['HeldInterruptions'] == field(57, 'HeldInterruptions')+1
       and all(p['m'][k] == field(57, k) for k in ('HeldMagazine', 'InsertedMagazine', 'OwnedRounds', 'SpentRounds', 'DiscardedRounds', 'SeatCount', 'DropCount', 'LoadedRounds', 'bChambered'))
       and p['objects'][pointer(p['r'], 'LeftHand')]['values']['bHidden'] is True)
    ma('recovered_open_hand_stows_without_drop_or_feed', 59, lambda m:m['HeldMagazine'] == m['InsertedMagazine'] == 0
       and m['bRecoveryPending'] is False and m['RecoveryStowCount'] == field(57, 'RecoveryStowCount')+1
       and m['StowCount'] == field(57, 'StowCount')+1
       and all(m[k] == field(57, k) for k in ('OwnedRounds', 'SpentRounds', 'DiscardedRounds', 'SeatCount', 'DropCount', 'ChestStowCount', 'ChestGrabCount', 'LoadedRounds', 'bChambered')))
    ma('recovery_stow_does_not_repeat', 60, lambda m:all(m[k] == field(59, k) for k in ('HeldMagazine', 'InsertedMagazine',
       'OwnedRounds', 'SpentRounds', 'DiscardedRounds', 'SeatCount', 'DropCount', 'StowCount', 'RecoveryStowCount', 'ChestStowCount', 'ChestGrabCount', 'LoadedRounds', 'bChambered', 'Revision')))
    try:
        a, b = last(2), last(60)
        checks['extended_stock_shots_match_conserved_spent'] = coverage(phase(60)) and b['r']['TraceCount']-a['r']['TraceCount'] == b['m']['SpentRounds']-a['m']['SpentRounds'] > 0 and b['g']['VRFireAttempts']-a['g']['VRFireAttempts'] >= b['m']['SpentRounds']-a['m']['SpentRounds']
    except (KeyError, TypeError):
        checks['extended_stock_shots_match_conserved_spent'] = False
    vals = fixture.get('values')
    checks['live_fixture_provenance'] = (fixture.get('protocol') == 'kfvr-reload-v2' and fixture.get('weapon') == weapon and fixture.get('read_only') is True and fixture.get('generated_from_live_assets') is True and type(fixture.get('source_pid')) is int and fixture['source_pid'] > 0 and isinstance(vals, list) and len(vals) == 16 and all(map(finite, vals)))
    try:
        p = last(2); v = p['v']; units = v['ModelUnitsPerMetre']
        root = [(x-y)/units for x, y in zip(v['MagazineLocal'], v['PalmLocal'])]
        action = [(x-y)/units+z for x, y, z in zip(v['SlideLocal'], v['PalmLocal'], v['ActionPalmOffset'])] if not rifle else vals[9:12]
        grasp = [0, 0, 0]
        if native:
            grip = p['objects'][v['MagazineGrip']]
            if grip['type'] != 'KFVRMagazineGrip' or grip['values'].get('bCalibrated') is not True or grip['values'].get('SourceWeapon') != p['gun']:
                raise ValueError('Missing owned native magazine fixture')
            grasp = [x/units for x in grip['values']['PalmLocal']]
        checks['fixture_matches_current_owned_weapon'] = (checks['live_fixture_provenance'] and fixture.get('gun_address') == p['gun'] and fixture.get('visual_address') == p['visual'] and fixture.get('rig_address') == p['rig'] and distance(vals[:3], root) < .00001 and distance(vals[3:6], grasp) < .00001 and distance(vals[6:9], v['InsertionAxis']) < .00001 and distance(vals[9:12], action) < .00001 and distance(vals[12:15], v['ActionPullAxis']) < .00001 and abs(vals[15]-v['ActionTravelMetres']) < .000001 and fixture.get('feed_on_seat') is rifle)
    except (KeyError, TypeError, ValueError, ZeroDivisionError):
        checks['fixture_matches_current_owned_weapon'] = False
    geo_checks, geo = geometry(main, rifle, native); checks.update(geo_checks)
    supply_checks, supply = supply_geometry(main); checks.update(supply_checks)
    from validate_reload_trial import capacity_evidence
    capacity_checks, capacity = capacity_evidence(rows, GUNS[weapon]); checks.update(capacity_checks)
    quiet = [p for n in (2, 3, 5, 7, 9, 10, 12, 14, 17, 19, 20, 21, 23, 25,
                         26, 27, 29, 30, 31, 32, 33, 34, 36, 38, 40, 41, 42, 43, 44, 46, 47, 49, 51, 52, 53, 55, 56, 57, 59, 60) for p in phase(n)]
    checks['settled_stock_total_matches_owned_rounds'] = len(quiet) >= 20 and all(p['g'].get('VRTotalAmmo') == p['m'].get('OwnedRounds') for p in quiet)
    other = {}
    for row in rows:
        if row.get('rig_sample_consistent') is not True:
            continue
        for obj in row.get('objects', []):
            if obj['type'] == 'KFVRManualReload' and not obj['values'].get('bDeleteMe'):
                m = obj['values']
                if m.get('Gun') not in {p['gun'] for p in main}:
                    other.setdefault(m.get('Gun'), []).append(tuple(m.get(k) for k in ('OwnedRounds', 'SpentRounds', 'DiscardedRounds', 'LoadedRounds', 'HeldMagazine', 'InsertedMagazine', 'Revision')))
    checks['other_weapon_ledgers_unchanged'] = all(len(set(states)) == 1 for states in other.values())
    final_rigs = [o['values'] for o in rows[-1].get('objects', []) if o['type'] == 'KFVRWeaponRig' and not o['values'].get('bDeleteMe')] if rows else []
    checks['final_input_released'] = len(final_rigs) == 1 and final_rigs[0].get('bTracking') is False and final_rigs[0].get('bFiring') is False
    return dict(passed=all(checks.values()), weapon=weapon, checks=checks, phase_coverage=metrics,
                geometry=geo, chest_supply=supply, capacity_evidence=capacity, coherent_owned_samples=len(main), rejected_ownership_samples=rejected[:12],
                failed_sample_details=notes, initial_ammo_budget=budgets[0] if budgets else None,
                discarded_magazine_travel_world_units=travel if finite(travel) else None,
                live_game=True, synthetic_controllers=True, headset_presented=False,
                surface_contact_and_comfort_tested=False)


def validate(folder, weapon='single'):
    folder = Path(folder)
    controls = [json.loads(s) for s in (folder/'controls.jsonl').read_text(encoding='utf-8').splitlines()]
    rows = [json.loads(s) for s in (folder/'weapon-observations.jsonl').read_text(encoding='utf-8').splitlines()]
    fixture = json.loads((folder/('rifle-reload-fixture.json' if weapon in ('bullpup', 'scar') else 'reload-fixture.json')).read_text(encoding='utf-8'))
    report = analyze(controls, rows, fixture, weapon)
    (folder/'reload-ux-validation.json').write_text(json.dumps(report, indent=2, allow_nan=False)+'\n', encoding='utf-8')
    if not report['passed']:
        raise ValueError('Reload UX checks failed: '+', '.join(k for k, v in report['checks'].items() if not v))
    return report


if __name__ == '__main__':
    import argparse
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('folder', type=Path)
    parser.add_argument('--weapon', choices=GUNS, default='single')
    args = parser.parse_args()
    print(json.dumps(validate(args.folder, args.weapon), indent=2))
