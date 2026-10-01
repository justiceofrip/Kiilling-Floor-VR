# Historical analyzer for removed prototype checkpoint 6520481.
# Current Bullpup tests use validate_reload_ux.py and the magazine-only profile.
"""Independent desktop evidence for the opt-in Bullpup authored-action trial.

Uses the ordinary reload phases (including its two-second full-pull dwell).
Automatic bursts are counted from stock fire/trace/ledger counters; this tool
makes no five-shot assumption and does not claim sub-frame cadence timing,
mesh surface contact, headset comfort, or a recovered native charging handle.
"""
import json
import math
from pathlib import Path

from validate_grip_pose import rotate
from validate_reload_trial import capacity_evidence

GUN = 'KFVRTrackedBullpup'
MIN_SAMPLES = 3


def finite(value):
    return type(value) in (int, float) and math.isfinite(value)


def vector(value):
    return isinstance(value, (list, tuple)) and len(value) == 3 and all(finite(x) for x in value)


def pointer(values, name):
    return next((value for key, value in values.items() if key.casefold() == name.casefold()), None)


def world(visual, local):
    if not vector(local) or not vector(visual.get('Location')) or not finite(visual.get('DrawScale')):
        raise ValueError('Missing finite source transform')
    rotation = visual.get('Rotation')
    if not isinstance(rotation, (list, tuple)) or len(rotation) != 3 or any(type(x) is not int for x in rotation):
        raise ValueError('Missing actual source rotation')
    return [x+y for x, y in zip(visual['Location'], rotate([x*visual['DrawScale'] for x in local], rotation))]


def distance(a, b):
    return math.dist(a, b) if vector(a) and vector(b) else math.inf


def geometry_evidence(records, native):
    """Actual actor/bone/palm reads, not script-reported error estimates."""
    errors = {key: [] for key in ('action', 'action_rotation', 'muzzle', 'right_palm',
                                  'held_magazine', 'left_palm', 'socket', 'rest_magazine', 'rest_axis')}
    counts = dict(active=0, held=0, resting=0, action_held=0)
    okay = dict(action=True, capabilities=True, transforms=True, right_hand=True,
                left_hand=True, ownership=True, socket=True, rest=True)
    failures = []
    for rec in records:
        r, v, a, m = (rec[key] for key in ('r', 'v', 'a', 'm'))
        if not (r.get('Gun') == rec['gun'] and r.get('bTracking') is True and
                r.get('bHolstered') is False and (r.get('Tracking.Flags', 0) & 3) == 3):
            continue
        counts['active'] += 1
        try:
            units = v['ModelUnitsPerMetre']
            travel = m['SlideTravel'] if m else 0.0
            scale = v['DrawScale']
            okay['capabilities'] &= (all(v.get(key) is True for key in ('bCalibrated', 'bMagazineProfile',
                'bInsertionCalibrated', 'bAuthoredActionProfile', 'bHiddenTranslationCalibrated', 'bTranslationCalibrated'))
                and all(v.get(key) is False for key in ('bInsertionFailed', 'bActionProfile', 'bPhysicalProfile', 'bTranslationProbe'))
                and finite(units) and abs(units-300) < .001)
            # Reconstruct the prototype anchor from the measured magazine root;
            # actor.ExpectedPosition, PositionError and visual errors are not used.
            anchor = [x+y*units for x, y in zip(v['MagazineLocal'], (.05, .02, .055))]
            expected = world(v, [x + (-.06*300*max(0.0, min(1.0, travel)) if i == 0 else 0)
                                  for i, x in enumerate(anchor)])
            errors['action'].append(distance(a.get('Location'), expected))
            axis_error = max(distance(rotate(axis, a['Rotation']), rotate(axis, v['Rotation']))
                             for axis in ((1, 0, 0), (0, 1, 0), (0, 0, 1)))
            errors['action_rotation'].append(axis_error)
            okay['action'] &= (a.get('SourceVisual') == rec['visual'] and a.get('bInitialized') is True
                and a.get('bHidden') is False and v.get('bHidden') is False
                and finite(a.get('DrawScale')) and abs(a['DrawScale']-scale) < .000001
                and distance(a.get('AnchorLocal'), anchor) < .001
                and distance(a.get('PullAxisLocal'), (-1, 0, 0)) < .001
                and finite(a.get('TravelMetres')) and abs(a['TravelMetres']-.06) < .000001
                and finite(a.get('AppliedTravel')) and abs(a['AppliedTravel']-travel) < .001
                and type(a.get('PoseCalls')) is int and a['PoseCalls'] > 0)
            errors['muzzle'].append(distance(r.get('MuzzleLocation'), world(v, v['TipLocal'])))
            hand = rec['objects'].get(pointer(r, 'RightHand'), {})
            h = hand.get('values', {})
            okay['right_hand'] &= (hand.get('type') == 'KFVRHandVisual' and h.get('bLeftHand') is False
                and h.get('bHidden') is False and h.get('bNativeHandMesh') is native)
            target = world(v, v['GripPose.16.Origin']) if native else r.get('Tracking.RightPosition')
            if native:
                okay['right_hand'] &= h.get('bGripPose') is True and h.get('bGripSourceLeft') is False and h.get('GripVisual') == rec['visual']
            errors['right_palm'].append(distance(h.get('PalmWorld'), target))
            if m and (m.get('HeldMagazine', 0) > 0 or m.get('bSlideHeld')):
                okay['ownership'] &= r.get('bSupportHeld') is False
            if m and m.get('bSlideHeld'):
                counts['action_held'] += 1
            if m and m.get('HeldMagazine', 0) > 0:
                counts['held'] += 1
                hand = rec['objects'].get(pointer(r, 'LeftHand'), {})
                h = hand.get('values', {})
                grip = rec['objects'].get(h.get('GripVisual'), {})
                g = grip.get('values', {})
                okay['left_hand'] &= (hand.get('type') == 'KFVRHandVisual' and h.get('bLeftHand') is True
                    and h.get('bHidden') is False and h.get('bNativeHandMesh') is native)
                target = r.get('Tracking.LeftPosition')
                if native:
                    okay['left_hand'] &= (grip.get('type') == 'KFVRMagazineGrip' and g.get('SourceWeapon') == rec['gun']
                        and g.get('bCalibrated') is True and h.get('bGripPose') is True and h.get('bGripSourceLeft') is True)
                    grasp = rotate([x*g['DrawScale'] for x in g['GripPose.0.Origin']], g['Rotation'])
                    expected_palm = [x+y for x, y in zip(v['MeasuredMagazinePosition'], grasp)]
                    errors['held_magazine'].append(distance(h.get('PalmWorld'), expected_palm))
                else:
                    errors['held_magazine'].append(distance(v.get('MeasuredMagazinePosition'), target))
                errors['left_palm'].append(distance(h.get('PalmWorld'), target))
                rest = world(v, v['MagazineLocal'])
                unit_scale = r['Tracking.UnitsPerMetre']
                delta = [(x-y)/unit_scale for x, y in zip(v['MeasuredMagazinePosition'], rest)]
                axes = [rotate(v[key], v['Rotation']) for key in ('InsertionX', 'InsertionY', 'InsertionAxis')]
                local = [sum(x*y for x, y in zip(delta, axis)) for axis in axes]
                errors['socket'].append(distance(local, m.get('InsertionPosition')))
            elif not m or m.get('InsertedMagazine', 0) > 0:
                counts['resting'] += 1
                errors['rest_magazine'].append(distance(v.get('MeasuredMagazinePosition'), world(v, v['MagazineLocal'])))
                for axis in 'XYZ':
                    errors['rest_axis'].append(distance(v.get('MeasuredMagazine'+axis), rotate(v['Magazine'+axis], v['Rotation'])))
        except (KeyError, TypeError, ValueError, IndexError, ZeroDivisionError, OverflowError) as error:
            okay['transforms'] = False
            if len(failures) < 8:
                failures.append(dict(uptime_ms=rec['row']['uptime_ms'], problem=str(error)))
    def bounded(key, limit, count=MIN_SAMPLES):
        return len(errors[key]) >= count and all(finite(x) and x < limit for x in errors[key])
    checks = dict(
        authored_action_retains_native_capability_boundary=counts['active'] >= 10 and okay['capabilities'],
        actual_action_actor_matches_independent_transform=okay['action'] and okay['transforms'] and bounded('action', .05, 10) and bounded('action_rotation', .002, 10),
        native_muzzle_stays_on_installed_tip=okay['transforms'] and bounded('muzzle', .05, 10),
        actual_dominant_palm_keeps_accepted_grip=okay['right_hand'] and bounded('right_palm', .005 if native else 3, 10),
        actual_magazine_keeps_offhand_grasp=okay['left_hand'] and bounded('held_magazine', .05, 5) and bounded('left_palm', .005 if native else .05, 5),
        actual_magazine_matches_socket_measurement=bounded('socket', .001, 5),
        inserted_magazine_keeps_installed_frame=bounded('rest_magazine', .05, 10) and bounded('rest_axis', .002, 30),
        manipulation_reserves_support_ownership=counts['held'] >= 5 and counts['action_held'] >= 3 and okay['ownership'])
    return checks, dict(counts=counts, missing_transform_samples=failures,
        errors={key: dict(samples=len(values), maximum=max(values) if values and all(finite(x) for x in values) else None)
                for key, values in errors.items()})


def analyze(controls, rows, fixture=None, require_extended=True):
    checks = {}; metrics = {}; records = []; unresolved = 0
    for row in rows:
        if row.get('rig_sample_consistent') is not True:
            continue
        objs = {o['address']: o for o in row.get('objects', []) if not o['values'].get('bDeleteMe')}
        rigs = [o for o in objs.values() if o['type'] == 'KFVRWeaponRig']
        guns = [o for o in objs.values() if o['type'] == GUN]
        if len(rigs) != 1 or len(guns) != 1:
            unresolved += 1
            continue
        rig, gun = rigs[0], guns[0]
        visuals = [o for o in objs.values() if o['type'] == 'KFVRWeaponVisual' and o['values'].get('SourceWeapon') == gun['address']]
        if len(visuals) != 1 or gun['values'].get('VRRig') != rig['address']:
            unresolved += 1
            continue
        visual = visuals[0]; v = visual['values']; action = objs.get(v.get('AuthoredAction'), {})
        manual = objs.get(gun['values'].get('Manual'), {})
        m = manual.get('values', {}) if manual.get('type') == 'KFVRManualReload' and manual['values'].get('Gun') == gun['address'] else {}
        records.append(dict(row=row, objects=objs, r=rig['values'], g=gun['values'], v=v,
            a=action.get('values', {}) if action.get('type') == 'KFVRActionVisual' else {}, m=m,
            gun=gun['address'], visual=visual['address']))
    sent = [c for c in controls if c.get('sent') is True]
    modes = [rec['r'].get('bNativeHandTest') for rec in records]
    native = bool(modes) and modes[0] is True
    checks['requested_hand_mode_is_consistent'] = bool(modes) and all(type(mode) is bool and mode == native for mode in modes)
    checks['one_owned_rifle_is_observed'] = len(records) >= 30 and len({rec['gun'] for rec in records}) == 1

    def phase(n, settle=150):
        times = [c['uptime_ms'] for c in sent if c.get('phase') == n]
        return [rec for rec in records if times and min(times)+settle <= rec['row']['uptime_ms'] <= max(times)]

    def at(name, n, predicate):
        data = phase(n)
        try:
            good = len(data) >= MIN_SAMPLES and len({rec['r'].get('Tracking.Serial') for rec in data}) >= MIN_SAMPLES and all(predicate(rec) for rec in data)
        except (KeyError, TypeError, ValueError, IndexError):
            good = False
        checks[name] = good
        metrics[str(n)] = dict(coherent_samples=len(data), required_samples=MIN_SAMPLES)

    def ma(name, n, predicate):
        at(name, n, lambda rec: bool(rec['m']) and predicate(rec['m']))

    def last(n):
        data = phase(n)
        return data[-1] if data else dict(m={}, r={}, g={})

    def shoulder_selected(p):
        r = p['r']
        offset = rotate([x*r['Tracking.UnitsPerMetre'] for x in (-.18, .22, -.15)], r['Tracking.BodyRotation'])
        expected = [x+y for x, y in zip(r['Tracking.HeadPosition'], offset)]
        return r.get('Gun') == p['gun'] and not r.get('bHolstered', True) and distance(r.get('HolsterLocation'), expected) < .05
    at('shoulder_draw_selects_rifle', 53, shoulder_selected)
    ma('catch_retains_chamber', 5, lambda m: m['HeldMagazine'] == 1 and m['InsertedMagazine'] == 0 and m['LoadedRounds'] == 1 and m['bChambered'])
    ma('retained_chamber_fires_once', 7, lambda m: m['SpentRounds'] == 1 and m['LoadedRounds'] == 0 and not m['bChambered'])
    ma('empty_trigger_cannot_fire', 8, lambda m: m['SpentRounds'] == 1 and not m['bChambered'])
    ma('partial_magazine_is_stowed', 10, lambda m: m['StowCount'] == 1 and m['HeldMagazine'] == 0)
    ma('replacement_has_new_identity', 11, lambda m: m['HeldMagazine'] == 2 and m['GrabCount'] == 1)
    ma('insertion_does_not_feed_chamber', 14, lambda m: m['InsertedMagazine'] == 2 and m['HeldMagazine'] == 0 and m['SeatCount'] == 1 and not m['bChambered'] and m['SpentRounds'] == 1)
    ma('partial_handle_pull_is_measured', 17, lambda m: .2 < m['SlideTravel'] < .6 and m['bSlideHeld'] and not m['bChambered'])
    ma('partial_handle_return_does_not_feed', 19, lambda m: m['RackCount'] == 0 and m['SlideTravel'] <= .1 and not m['bChambered'] and m['SpentRounds'] == 1)
    ma('full_handle_pull_blocks_fire', 23, lambda m: m['SlideTravel'] > .9 and m['bSlideHeld'] and not m['bChambered'] and m['SpentRounds'] == 1)
    ma('full_handle_return_feeds', 24, lambda m: m['RackCount'] == 1 and m['bChambered'] and m['LoadedRounds'] == m['CurrentCapacity'])
    ma('first_stock_burst_consumes_and_feeds', 26, lambda m: m['SpentRounds'] >= 3 and m['LoadedRounds'] == m['CurrentCapacity']-(m['SpentRounds']-1) and m['bChambered'])
    spent26 = last(26)['m'].get('SpentRounds')
    ma('tactical_reload_preserves_plus_one', 33, lambda m: m['InsertedMagazine'] == 3 and m['HeldMagazine'] == 0 and m['LoadedRounds'] == m['CurrentCapacity']+1 and m['bChambered'] and m['SpentRounds'] == spent26)
    at('focus_loss_releases_input', 35, lambda p: p['r'].get('bTracking') is False and p['r'].get('bFiring') is False)
    spent35 = last(35)['m'].get('SpentRounds')
    ma('held_trigger_needs_neutral_after_focus_loss', 36, lambda m: type(spent35) is int and m['SpentRounds'] == spent35)
    at('held_trigger_does_not_restart_auto_fire', 36, lambda p: p['r'].get('bFiring') is False and p['r'].get('bTriggerArmed') is False)
    ma('switch_retains_partial_magazine', 44, lambda m: m['HeldMagazine'] == 0 and m['InsertedMagazine'] == 0 and m['StowCount'] == 3 and m['bChambered'])
    ma('shoulder_redraw_retains_chamber', 48, lambda m: m['LoadedRounds'] == 1 and m['InsertedMagazine'] == 0 and m['bChambered'])
    spent48 = last(48)['m'].get('SpentRounds')
    ma('final_chamber_fires_once', 50, lambda m: type(spent48) is int and m['SpentRounds'] == spent48+1 and m['LoadedRounds'] == 0 and not m['bChambered'])
    main = [rec for rec in records if rec['m'] and rec['row']['uptime_ms'] >= min((c['uptime_ms'] for c in sent if c.get('phase') == 2), default=math.inf)]
    budgets = []
    try:
        budgets = [sum(p['m'][key] for key in ('OwnedRounds', 'SpentRounds', 'DiscardedRounds')) for p in main]
        checks['every_coherent_ledger_conserves_ammo'] = len(main) >= 20 and len(set(budgets)) == 1 and all(p['m'].get('bValid') is True for p in main)
        checks['actual_stock_loaded_matches_physical_ledger'] = len(main) >= 20 and all(p['g']['MagAmmoRemaining'] == p['m']['LoadedRounds'] for p in main)
    except (KeyError, TypeError):
        checks['every_coherent_ledger_conserves_ammo'] = checks['actual_stock_loaded_matches_physical_ledger'] = False
    # VRTotalAmmo is a WeaponTick cache, so it may precede a legitimate shot in
    # the same completed rig presentation. Check settled nonfiring phases only.
    cache_records = [p for n in (7, 10, 14, 19, 24, 26, 33, 35, 36, 39, 44, 46, 48, 50) for p in phase(n) if p['m']]
    checks['settled_stock_ammo_cache_matches_ledger'] = len(cache_records) >= 30 and all(
        p['g'].get('VRTotalAmmo') == p['m'].get('OwnedRounds') for p in cache_records)
    # Newly spawned manual actors enter the asynchronous observer inventory
    # after discovery completes. Phase5 is a sustained, explicitly checked
    # pre-shot baseline; phase2 may still lack the actor despite a valid rig.
    at('pre_shot_baseline_observed', 5, lambda p: bool(p['m']) and p['m']['SpentRounds'] == 0
       and p['m']['DiscardedRounds'] == 0 and p['r']['TraceCount'] == 0 and p['g']['VRFireAttempts'] == 0)
    first, end = last(5), last(50)
    try:
        shots = end['m']['SpentRounds'] - first['m']['SpentRounds']
        checks['actual_stock_shots_match_trace_and_ledger'] = shots >= 8 and shots == end['r']['TraceCount']-first['r']['TraceCount'] and end['g']['VRFireAttempts']-first['g']['VRFireAttempts'] >= shots
        checks['two_exchanges_plus_retained_switch'] = end['m']['EjectCount'] == end['m']['StowCount'] == 3 and end['m']['GrabCount'] == end['m']['SeatCount'] == 2 and end['m']['DropCount'] == end['m']['DiscardedRounds'] == 0
    except (KeyError, TypeError):
        shots = None
        checks['actual_stock_shots_match_trace_and_ledger'] = checks['two_exchanges_plus_retained_switch'] = False
    bursts = {}
    for n, before, after in ((25, 24, 26), (34, 33, 35), (38, 37, 39)):
        commands = [c for c in sent if c.get('phase') == n]
        checks['continuous_trigger_sent_phase_'+str(n)] = len(commands) >= 3 and all(finite(c.get('right_trigger')) and c['right_trigger'] > .7 and c.get('focused') is True for c in commands)
        try:
            a, b = last(before), last(after)
            count = b['m']['SpentRounds']-a['m']['SpentRounds']
            burst_ok = count >= 2 and count == b['g']['VRFireAttempts']-a['g']['VRFireAttempts'] == b['r']['TraceCount']-a['r']['TraceCount']
        except (KeyError, TypeError):
            count, burst_ok = None, False
        bursts[str(n)] = dict(actual_shots=count, sent_samples=len(commands))
        checks['stock_auto_burst_phase_'+str(n)] = burst_ok
    checks['no_stock_timed_reload_in_physical_mode'] = bool(main) and all(p['g'].get('bIsReloading') is False for p in main)
    for n in (53, 55, 57, 58, 59):
        at('mode_toggle_rest_frame_phase_'+str(n), n, lambda p: p['v'].get('bTranslationProbe') is False
           and distance(p['v'].get('MeasuredMagazinePosition'), world(p['v'], p['v']['MagazineLocal'])) < .05
           and all(distance(p['v'].get('MeasuredMagazine'+axis), rotate(p['v']['Magazine'+axis], p['v']['Rotation'])) < .002 for axis in 'XYZ'))
    for n in (17, 23, 24):
        at('actual_handle_pose_coverage_phase_'+str(n), n, lambda p: p['a'].get('bHidden') is False and p['a'].get('PoseCalls', 0) > 0)
    # The action helper must also stop showing when ownership/focus disappears.
    at('authored_action_hidden_on_focus_loss', 35, lambda p: p['v'].get('bHidden') is True and p['a'].get('bHidden') is True)
    def holstered_action_matches(p):
        a, visual = p['a'], p['v']
        anchor = [x+y*visual['ModelUnitsPerMetre'] for x, y in zip(visual['MagazineLocal'], (.05, .02, .055))]
        return (p['r'].get('Gun') != p['gun'] and a.get('bHidden') is visual.get('bHidden')
            and (a.get('bHidden') is True or (distance(a.get('Location'), world(visual, anchor)) < .05
                and a.get('Rotation') == visual.get('Rotation') and a.get('DrawScale') == visual.get('DrawScale'))))
    for n in (44, 46):
        at('holstered_action_follows_visible_parent_phase_'+str(n), n, holstered_action_matches)
    extended = any(c.get('phase', -1) >= 61 for c in sent)
    if require_extended: checks['extended_protocol_required'] = extended
    extension_bursts = {}
    offaxis_distances = []
    original_shots = shots
    if extended:
        # Continue from the original empty-chamber endpoint. Interrupted pulls
        # may reset visual travel, but must never manufacture a feed event.
        held61 = last(61)['m'].get('HeldMagazine')
        ma('extension_supply_grab', 61, lambda m: type(held61) is int and held61 > 0 and m['HeldMagazine'] == held61 and m['InsertedMagazine'] == 0 and m['GrabCount'] == 3)
        ma('extension_seat_still_needs_chamber', 63, lambda m: m['InsertedMagazine'] == held61 and m['HeldMagazine'] == 0 and m['SeatCount'] == 3 and not m['bChambered'])
        ma('interrupted_handle_reaches_full_pull', 65, lambda m: m['SlideTravel'] > .9 and m['bSlideHeld'] and m['RackCount'] == 1 and not m['bChambered'])
        at('action_offhand_tracking_loss_observed', 66, lambda p: (p['r'].get('Tracking.Flags', 0) & 3) == 2)
        for n in (66, 67):
            commands = [c for c in sent if c.get('phase') == n]
            checks['action_interruption_input_phase_'+str(n)] = len(commands) >= 3 and all(
                c.get('left_grip_valid') is (n == 67) and c.get('right_grip_valid') is True
                and finite(c.get('left_squeeze')) and c['left_squeeze'] > .7 for c in commands)
            ma('interrupted_handle_cannot_feed_phase_'+str(n), n, lambda m: m['SlideTravel'] <= .1 and not m['bSlideHeld'] and not m['bChambered'] and m['RackCount'] == 1 and m['DiscardedRounds'] == 0)
        ma('held_tracking_return_needs_fresh_edge', 67, lambda m: m['bLeftArmed'] is False)
        ma('action_release_rearms_after_loss', 68, lambda m: m['bLeftArmed'] is True and not m['bSlideHeld'] and not m['bChambered'])
        ma('fresh_action_grab_after_loss', 69, lambda m: m['bSlideHeld'] and not m['bChambered'])
        ma('fresh_action_full_pull', 70, lambda m: m['SlideTravel'] > .9 and m['bSlideHeld'] and not m['bChambered'])
        ma('held_return_feeds_exactly_once', 71, lambda m: m['SlideTravel'] <= .1 and m['bSlideHeld'] and m['bChambered'] and m['RackCount'] == 2 and m['DiscardedRounds'] == 0)
        at('feed_precedes_action_grip_release', 71, lambda p: p['r']['Tracking.LeftGrip'] > .7)
        ma('loaded_chamber_extraction_discards_one', 74, lambda m: m['SlideTravel'] > .9 and not m['bChambered'] and m['DiscardedRounds'] == 1 and m['RackCount'] == 2)
        for n in (75, 76):
            ma('offaxis_cancel_never_feeds_phase_'+str(n), n, lambda m: m['SlideTravel'] <= .1 and not m['bSlideHeld'] and not m['bChambered'] and m['RackCount'] == 2 and m['DiscardedRounds'] == 1)
        def relative_hand(rec):
            r = rec['r']
            delta = [(x-y)/r['Tracking.UnitsPerMetre'] for x, y in zip(r['Tracking.LeftPosition'], r['Tracking.RightPosition'])]
            return [sum(x*y for x, y in zip(delta, rotate(axis, r['Tracking.RightRotation'])))
                    for axis in ((1, 0, 0), (0, 1, 0), (0, 0, 1))]
        try:
            starts = [relative_hand(p) for p in phase(73)]
            offaxis_distances = [max(abs(point[1]-starts[-1][1]), abs(point[2]-starts[-1][2]))
                for point in (relative_hand(p) for p in phase(75))] if len(starts) >= MIN_SAMPLES else []
        except (KeyError, TypeError, ValueError, IndexError, ZeroDivisionError):
            offaxis_distances = []
        checks['actual_release_exceeds_lateral_rail_envelope'] = len(offaxis_distances) >= MIN_SAMPLES and all(
            finite(distance) and distance > .12 for distance in offaxis_distances)
        ma('fresh_pull_after_offaxis_cancel', 78, lambda m: m['SlideTravel'] > .9 and m['bSlideHeld'] and not m['bChambered'] and m['RackCount'] == 2 and m['DiscardedRounds'] == 1)
        ma('spring_release_feeds_once_after_cancel', 79, lambda m: m['SlideTravel'] <= .1 and not m['bSlideHeld'] and m['bChambered'] and m['RackCount'] == 3 and m['DiscardedRounds'] == 1)
        for n, before, after in ((72, 71, 73), (80, 79, 81)):
            commands = [c for c in sent if c.get('phase') == n]
            checks['continuous_trigger_sent_phase_'+str(n)] = len(commands) >= 3 and all(
                finite(c.get('right_trigger')) and c['right_trigger'] > .7 and c.get('focused') is True for c in commands)
            try:
                a, b = last(before), last(after)
                count = b['m']['SpentRounds']-a['m']['SpentRounds']
                good = (count >= 2 and count == b['r']['TraceCount']-a['r']['TraceCount']
                    and count == b['g']['VRFireAttempts']-a['g']['VRFireAttempts']
                    and b['m']['LoadedRounds'] == a['m']['LoadedRounds']-count and b['m']['bChambered'])
            except (KeyError, TypeError):
                count, good = None, False
            extension_bursts[str(n)] = dict(actual_shots=count, sent_samples=len(commands))
            checks['stock_auto_burst_phase_'+str(n)] = good
        final = last(81)
        try:
            shots = final['m']['SpentRounds']-first['m']['SpentRounds']
            checks['extended_actual_shots_match_trace_and_ledger'] = shots > original_shots and shots == final['r']['TraceCount']-first['r']['TraceCount'] and final['g']['VRFireAttempts']-first['g']['VRFireAttempts'] >= shots
        except (KeyError, TypeError):
            shots = None
            checks['extended_actual_shots_match_trace_and_ledger'] = False
        ma('extended_final_chamber_and_rounds_conserved', 81, lambda m: m['bValid'] and m['bChambered'] and m['RackCount'] == 3 and m['DiscardedRounds'] == 1 and m['SeatCount'] == 3 and m['GrabCount'] == 3 and m['EjectCount'] == m['StowCount'] == 3 and m['DropCount'] == 0)
        # Existing geometry checks include every coherent extension sample.
        for n in (65, 67, 70, 71, 74, 75, 78, 79):
            at('extended_actual_handle_pose_coverage_phase_'+str(n), n, lambda p: p['a'].get('bHidden') is False and p['a'].get('PoseCalls', 0) > 0)
    final_rigs = [o['values'] for o in rows[-1].get('objects', []) if o['type'] == 'KFVRWeaponRig'] if rows else []
    final_actions = [o['values'] for o in rows[-1].get('objects', []) if o['type'] == 'KFVRActionVisual' and not o['values'].get('bDeleteMe')] if rows else []
    checks['final_input_and_authored_action_released'] = len(final_rigs) == 1 and final_rigs[0].get('bTracking') is False and final_rigs[0].get('bFiring') is False and bool(final_actions) and all(a.get('bHidden') is True for a in final_actions)
    geometry_checks, geometry = geometry_evidence(records, native)
    checks.update(geometry_checks)
    capacity_checks, capacity = capacity_evidence(rows, GUN)
    checks.update(capacity_checks)
    other_states = {}
    rifle_addresses = {p['gun'] for p in records}
    for row in rows:
        if row.get('rig_sample_consistent') is not True:
            continue
        for obj in row.get('objects', []):
            m = obj['values']
            if obj['type'] == 'KFVRManualReload' and not m.get('bDeleteMe') and m.get('Gun') not in rifle_addresses:
                other_states.setdefault(obj['address'], set()).add(tuple(m.get(key) for key in ('OwnedRounds', 'LoadedRounds', 'SpentRounds', 'DiscardedRounds', 'HeldMagazine', 'InsertedMagazine', 'Revision')))
    checks['other_physical_ledgers_unchanged'] = all(len(states) == 1 for states in other_states.values())
    if fixture is not None:
        checks['fixture_matches_actual_live_rifle'] = (fixture.get('protocol') == 'kfvr-rifle-reload-v1'
            and fixture.get('weapon') == 'bullpup' and fixture.get('selection_slot') == 2
            and fixture.get('native_hands') is native and fixture.get('gun_address') in rifle_addresses
            and fixture.get('authored_action_prototype') is True)
    return dict(passed=all(checks.values()), checks=checks, phase_coverage=metrics, geometry=geometry,
        capacity_evidence=capacity, automatic_bursts=bursts, stock_shots=shots, original_workflow_shots=original_shots,
        extended_action_trial=extended, extended_automatic_bursts=extension_bursts,
        offaxis_release_metres=offaxis_distances,
        initial_ammo_budget=budgets[0] if budgets else None, coherent_rifle_samples=len(records),
        unresolved_rifle_rows=unresolved, native_hands=native, authored_action_prototype=True,
        automatic_fire_observed=all(checks.get('stock_auto_burst_phase_'+str(n)) for n in (25, 34, 38)),
        shot_interval_timing_measured=False, stock_total_directly_observed=False, headset_presented=False, live_game=True,
        synthetic_controllers=True, surface_contact_and_comfort_tested=False)


def validate(folder, require_extended=True):
    folder = Path(folder)
    controls = [json.loads(line) for line in (folder/'controls.jsonl').read_text().splitlines()]
    rows = [json.loads(line) for line in (folder/'weapon-observations.jsonl').read_text().splitlines()]
    path = folder/'rifle-reload-fixture.json'
    fixture = json.loads(path.read_text(encoding='utf-8')) if path.exists() else None
    report = analyze(controls, rows, fixture, require_extended=require_extended)
    report['checks']['live_fixture_provenance_saved'] = fixture is not None
    report['passed'] = all(report['checks'].values())
    (folder/'rifle-reload-validation.json').write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')
    if not report['passed']:
        raise ValueError('Rifle reload checks failed: '+', '.join(key for key, value in report['checks'].items() if not value))
    return report


if __name__ == '__main__':
    import sys
    print(json.dumps(validate(sys.argv[1]), indent=2))
