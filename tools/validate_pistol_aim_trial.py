"""Read-only acceptance of immutable shot-time telemetry from the pistol trial.

No current/post-shot hand pose is used to judge a historical shot direction.
A failing pre-fix baseline is written in full before validate() raises.
"""
import argparse
import json
import math
from pathlib import Path

SECONDARY_WEAPONS = ('KFVRTrackedDeagle', 'KFVRTrackedMK23')
SHOT_FIELDS = ('LastShotSerial', 'LastShotInputSerial', 'LastShotPresentationSerial',
               'LastShotTime', 'LastShotWeapon', 'LastShotFireMode', 'LastShotStart',
               'LastShotAimDirection', 'LastShotDirection', 'LastShotStockSpread',
               'LastShotAppliedSpread')



class _PreflightNotReady(Exception):
    pass


def _initial_selection(snapshot, objects, secondary_weapon):
    """Resolve actual pawn selection, including the rig's first-Tick sync rule."""
    if secondary_weapon not in SECONDARY_WEAPONS:
        raise ValueError('Unsupported secondary pistol: '+str(secondary_weapon))
    live = [o for o in objects if not o['values'].get('bDeleteMe')]
    by_address = {o['address']: o for o in live}
    pcs = [o for o in snapshot['observations'] if o['class'] == 'KFPlayerController'
           and o['values'].get('PlayerController.Player')]
    if len(pcs) != 1:
        raise ValueError('Pistol aim requires exactly one local controller')
    pc = pcs[0]
    pawn_address = (pc['values'].get('Controller.Pawn') or {}).get('address')
    pawn = by_address.get(pawn_address)
    if not pawn or pawn['type'] != 'KFHumanPawn' or pawn['values'].get('Health', 0) <= 0:
        raise _PreflightNotReady('Local player has no living owned pawn')
    rigs = [o for o in live if o['type'] == 'KFVRWeaponRig' and o['values'].get('Owner') == pc['address']]
    if len(rigs) != 1:
        raise _PreflightNotReady('Expected one owned weapon rig')
    rig = rigs[0]; rv = rig['values']
    if rv.get('PC') != pc['address'] or rv.get('BoundPawn') != pawn_address:
        raise _PreflightNotReady('Weapon rig has not bound the local pawn')
    count = rv.get('InventoryCount')
    if type(count) is not int or count < 7:
        raise _PreflightNotReady('Waiting for all seven fixture weapons')
    if count != 7:
        raise ValueError('Pistol aim requires the seven-weapon fixture')
    owned = [o for o in live if o['type'].startswith('KFVRTracked') and o['values'].get('Owner') == pawn_address
             and o['values'].get('VRRig') == rig['address']]
    # The seventh registered fixture weapon is stock Frag (AttachRig slot 6).
    if len(owned) != 6:
        raise _PreflightNotReady('Six owned tracked fixture weapons are not yet observable')
    pistols = {}
    for name in ('KFVRTrackedSingle', secondary_weapon):
        found = [o for o in owned if o['type'] == name]
        if len(found) != 1:
            raise ValueError('Missing or ambiguous required pistol: '+name)
        ammo = found[0]['values'].get('MagAmmoRemaining')
        if type(ammo) is not int or ammo < 6:
            raise ValueError('Required pistol needs at least six loaded rounds: '+name)
        pistols[name] = dict(address=found[0]['address'], loaded_rounds=ammo)
    if any(o['type'] in SECONDARY_WEAPONS and o['type'] != secondary_weapon for o in owned):
        raise ValueError('Unexpected secondary pistol fixture')
    pawn_weapon = by_address.get(pawn['values'].get('Weapon'))
    if pawn_weapon not in owned:
        raise _PreflightNotReady('Actual Pawn.Weapon is absent or is not an owned tracked weapon')
    rig_address = rv.get('Gun')
    rig_weapon = by_address.get(rig_address) if rig_address not in (None, '0x0') else None
    if rig_address not in (None, '0x0') and rig_weapon not in owned:
        raise _PreflightNotReady('Rig.Gun is not an owned tracked weapon')
    if type(rv.get('bHolstered')) is not bool:
        raise ValueError('Missing reflected rig holster state')
    same = rig_address == pawn_weapon['address']
    # UpdateRig adopts Pawn.Weapon and clears bHolstered if Gun differs.
    effective_holstered = same and rv['bHolstered']
    return dict(read_only=True, controller_address=pc['address'], pawn_address=pawn_address,
                rig_address=rig['address'], inventory_count=count, pawn_weapon_address=pawn_weapon['address'],
                pawn_weapon_class=pawn_weapon['type'], rig_weapon_address=rig_address,
                rig_weapon_class=rig_weapon['type'] if rig_weapon else None, rig_holstered=rv['bHolstered'],
                effective_holstered=effective_holstered, expected_secondary_weapon=secondary_weapon,
                draw_first=pawn_weapon['type'] != 'KFVRTrackedSingle' or effective_holstered,
                pistols=pistols, selection_source='Pawn.Weapon; UpdateRig synchronizes a differing Rig.Gun')


def preflight_selection(pid, game, secondary_weapon='KFVRTrackedDeagle'):
    """Read only; require two stable owned selections before creating a receiver."""
    import time
    from observe_kf import Observer, unpack
    from observe_weapon_rig import sample
    deadline = time.monotonic()+10
    previous = None
    reason = 'No stable selection snapshot'
    while True:
        observer = None
        try:
            observer = Observer(pid, Path(game))
            snapshot = observer.snapshot()
            objects = sample(observer)
            if not observer.rig_sample_consistent:
                raise _PreflightNotReady('Rig changed during selection snapshot')
            metadata = _initial_selection(snapshot, objects, secondary_weapon)
            # These ownership/selection fields can change outside the rig Tick.
            # Re-read their reflected addresses after the multi-object sample.
            for prop, owner, expected in (
                ('Controller.Pawn', metadata['controller_address'], metadata['pawn_address']),
                ('Pawn.Weapon', metadata['pawn_address'], metadata['pawn_weapon_address'])):
                field = snapshot['properties'].get(prop, {})
                if field.get('type') != 'ObjectProperty' or field.get('size') != 4:
                    raise ValueError('Missing reflected selection pointer: '+prop)
                current = unpack(observer.read(int(owner, 16)+field['offset'], 4))
                if current != int(expected, 16):
                    raise _PreflightNotReady('Pawn ownership or selection changed during snapshot')
            if metadata == previous:
                metadata.update(source_pid=pid, stable_snapshots=2, core_sha256=snapshot['core_sha256'])
                return metadata
            previous = metadata
            reason = 'Waiting for a second stable selection snapshot'
        except _PreflightNotReady as error:
            previous = None; reason = str(error)
        except ValueError as error:
            if not any(message in str(error) for message in ('Array changed during snapshot', 'FName index mismatch')):
                raise
            previous = None; reason = str(error)
        finally:
            if observer is not None:
                observer.close()
        remaining = deadline-time.monotonic()
        if remaining <= 0:
            raise RuntimeError('Pistol aim preflight not ready within 10 seconds: '+reason)
        time.sleep(min(.05, remaining))


def finite(value):
    return type(value) in (int, float) and math.isfinite(value)


def vector(value):
    return isinstance(value, (list, tuple)) and len(value) == 3 and all(finite(x) for x in value)


def angle_degrees(a, b):
    if not vector(a) or not vector(b):
        return None
    length = math.sqrt(sum(x*x for x in a) * sum(x*x for x in b))
    if length < 1e-10:
        return None
    return math.degrees(math.acos(max(-1, min(1, sum(x*y for x, y in zip(a, b))/length))))


def weapon_name(value):
    return value.split('.')[-1] if isinstance(value, str) else ''


def analyze(controls, rows, secondary_weapon='KFVRTrackedDeagle', preflight=None):
    if secondary_weapon not in SECONDARY_WEAPONS:
        raise ValueError('Unsupported secondary pistol: '+str(secondary_weapon))
    weapons = ('KFVRTrackedSingle', secondary_weapon)
    checks = {}
    sent = [c for c in controls if c.get('sent')]
    phases = {}
    for c in sent:
        phases.setdefault(c['phase'], []).append(c)
    frames = []
    for row in rows:
        if not row.get('rig_sample_consistent'):
            continue
        objects = {o['address']: o for o in row.get('objects', []) if not o['values'].get('bDeleteMe')}
        rigs = [o['values'] for o in objects.values() if o['type'] == 'KFVRWeaponRig']
        if len(rigs) == 1:
            frames.append(dict(time=row['uptime_ms'], rig=rigs[0], objects=objects))
    checks['all_protocol_phases_observed'] = all(phases.get(n) for n in range(12))
    checks['pistol_aim_protocol_selected'] = bool(sent) and all(c.get('pistol_aim') is True for c in sent)
    if preflight is not None:
        valid_plan = (preflight.get('read_only') is True and preflight.get('stable_snapshots') == 2
                      and preflight.get('inventory_count') == 7
                      and preflight.get('expected_secondary_weapon') == secondary_weapon
                      and type(preflight.get('draw_first')) is bool)
        checks['startup_selection_preflight_recorded'] = valid_plan
        draw_first = preflight.get('draw_first')
        checks['startup_draw_matches_preflight'] = (valid_plan and bool(sent)
            and all(c.get('pistol_aim_draw_first') is draw_first for c in sent))
        startup = phases.get(1, [])+phases.get(2, [])
        checks['startup_gesture_matches_plan'] = (bool(startup) and all(
            c.get('right_grip_position') == ([.22, -.62, .05] if draw_first else [.2, -.25, -.5])
            and c.get('right_squeeze') == (1 if draw_first and c['phase'] == 2 else 0)
            and c.get('right_trigger') == 0 for c in startup))
    first_burst = min((c['uptime_ms'] for c in phases.get(4, [])), default=-1)
    before = [f for f in frames if f['time'] < first_burst]
    baseline = before[-1] if before else None
    final = frames[-1] if frames else None
    checks['coherent_baseline_before_firing'] = baseline is not None
    baseline_serial = baseline['rig'].get('LastShotSerial') if baseline else None
    end_serial = final['rig'].get('LastShotSerial') if final else None
    serial_bounds = type(baseline_serial) is int and type(end_serial) is int and 0 <= baseline_serial <= end_serial
    checks['committed_shot_serial_bounds'] = serial_bounds
    shot_map = {}
    mutations = []
    malformed = []
    for frame in frames:
        r = frame['rig']; serial = r.get('LastShotSerial')
        if not serial_bounds or type(serial) is not int or serial <= baseline_serial:
            continue
        sequence = r.get('LastShotSequence')
        if type(sequence) is not int or sequence <= 0 or sequence % 2:
            malformed.append(dict(time=frame['time'], serial=serial, reason='uncommitted sequence'))
            continue
        if any(k not in r for k in SHOT_FIELDS):
            malformed.append(dict(time=frame['time'], serial=serial, reason='missing fields'))
            continue
        data = {k: r[k] for k in SHOT_FIELDS}
        if serial in shot_map:
            if data != shot_map[serial]['telemetry']:
                mutations.append(dict(time=frame['time'], serial=serial))
        else:
            shot_map[serial] = dict(first_observed_uptime_ms=frame['time'], telemetry=data,
                                   weapon=weapon_name(data['LastShotWeapon']),
                                   angle_degrees=angle_degrees(data['LastShotAimDirection'], data['LastShotDirection']))
    shots = [shot_map[k] for k in sorted(shot_map)]
    checks['shot_records_committed_and_immutable'] = bool(shots) and not malformed and not mutations
    expected_serials = list(range(baseline_serial+1, end_serial+1)) if serial_bounds and end_serial-baseline_serial <= 1000 else []
    checks['every_new_shot_observed'] = bool(expected_serials) and sorted(shot_map) == expected_serials
    checks['exactly_two_expected_pistol_classes'] = set(s['weapon'] for s in shots) == set(weapons)
    checks['shot_pose_vectors_valid'] = bool(shots) and all(
        vector(s['telemetry']['LastShotStart']) and s['angle_degrees'] is not None
        and all(abs(math.sqrt(sum(x*x for x in s['telemetry'][key]))-1) < .001
                for key in ('LastShotAimDirection', 'LastShotDirection')) for s in shots)
    checks['shot_source_and_presentation_ids_present'] = bool(shots) and all(
        type(s['telemetry'][key]) is int and s['telemetry'][key] > 0
        for s in shots for key in ('LastShotInputSerial', 'LastShotPresentationSerial'))
    checks['primary_fire_only'] = bool(shots) and all(s['telemetry']['LastShotFireMode'] == 0 for s in shots)
    checks['same_shot_trace_matches_aim_within_point05_degrees'] = bool(shots) and all(
        s['angle_degrees'] is not None and s['angle_degrees'] <= .05 for s in shots)
    checks['physical_applied_spread_is_zero'] = bool(shots) and all(
        finite(s['telemetry']['LastShotAppliedSpread']) and s['telemetry']['LastShotAppliedSpread'] == 0 for s in shots)
    trace_delta = None
    if baseline and final and all(type(f['rig'].get('TraceCount')) is int for f in (baseline, final)):
        trace_delta = final['rig']['TraceCount'] - baseline['rig']['TraceCount']
    checks['actual_trace_count_matches_recorded_shots'] = bool(shots) and trace_delta == len(shots)
    per_weapon = {}
    for name, burst_phase, settle_phase in zip(weapons, (4, 9), (3, 8)):
        group = [s for s in shots if s['weapon'] == name]
        times = [s['telemetry']['LastShotTime'] for s in group]
        valid_times = all(finite(t) and t >= 0 for t in times)
        intervals = [b-a for a, b in zip(times, times[1:])] if valid_times else []
        spreads = [s['telemetry']['LastShotStockSpread'] for s in group]
        valid_spreads = bool(spreads) and all(finite(x) and x >= 0 for x in spreads)
        burst = phases.get(burst_phase, [])
        pulses = 0; down = False
        for c in burst:
            current = c.get('right_trigger', 0) > .9
            if current and not down:
                pulses += 1
            down = current
        poses = [(c.get('right_grip_position'), c.get('right_aim_orientation')) for c in burst]
        fixed_pose = bool(poses) and all(vector(pos) and isinstance(rot, list) and len(rot) == 4
                                      and all(finite(x) for x in rot) for pos, rot in poses)
        if fixed_pose:
            fixed_pose = all(pose == poses[0] for pose in poses)
        settle = phases.get(settle_phase, [])
        settle_frames = [f for f in frames if settle and min(c['uptime_ms'] for c in settle)+250 <= f['time'] <= max(c['uptime_ms'] for c in settle)]
        selected = len(settle_frames) >= 3 and all(
            f['objects'].get(f['rig'].get('Gun'), {}).get('type') == name and f['rig'].get('bTracking')
            and not f['rig'].get('bHolstered') for f in settle_frames)
        ammo = []
        for f in (baseline, final):
            values = [o['values'].get('MagAmmoRemaining') for o in f['objects'].values() if o['type'] == name] if f else []
            ammo.append(values[0] if len(values) == 1 and type(values[0]) is int else None)
        ammo_delta = ammo[0]-ammo[1] if all(type(x) is int for x in ammo) else None
        prefix = name + ':'
        checks[prefix+'expected_active_pistol_before_burst'] = selected
        checks[prefix+'six_discrete_trigger_pulses'] = pulses == 6 and all(c.get('focused') is True for c in burst)
        checks[prefix+'raw_controller_pose_fixed_during_burst'] = fixed_pose
        checks[prefix+'at_least_five_rapid_shots'] = len(group) >= 5
        checks[prefix+'one_shot_per_trigger_pulse'] = len(group) == pulses == 6
        checks[prefix+'rapid_game_cadence_preserved'] = len(intervals) >= 4 and all(.20 <= x <= .50 for x in intervals)
        checks[prefix+'stock_recent_shot_spread_grows'] = valid_spreads and max(spreads)-min(spreads) > 1e-6
        checks[prefix+'native_magazine_decrement_matches_shots'] = bool(group) and ammo_delta == len(group)
        per_weapon[name] = dict(shots=len(group), trigger_pulses=pulses, shot_game_intervals=intervals,
                                stock_spreads=spreads, applied_spreads=[s['telemetry']['LastShotAppliedSpread'] for s in group],
                                angles_degrees=[s['angle_degrees'] for s in group], ammo_before=ammo[0], ammo_after=ammo[1],
                                ammo_decrement=ammo_delta, settle_coherent_samples=len(settle_frames))
    end_phase = phases.get(11, [])
    stopped = [f for f in frames if end_phase and f['time'] >= min(c['uptime_ms'] for c in end_phase)+300]
    checks['neutral_end_releases_tracking_and_fire'] = len(stopped) >= 3 and all(
        not f['rig'].get('bTracking') and not f['rig'].get('bFiring') for f in stopped)
    checks['no_reload_buttons_in_protocol'] = bool(sent) and all(
        not c.get('left_secondary') and not c.get('right_secondary') for c in sent)
    return dict(passed=all(checks.values()), checks=checks, expected_secondary_weapon=secondary_weapon, baseline_shot_serial=baseline_serial,
                final_shot_serial=end_serial, trace_delta=trace_delta, coherent_samples=len(frames),
                per_weapon=per_weapon, shots=shots, malformed_records=malformed, mutated_records=mutations, preflight=preflight,
                synthetic_controllers=True, live_game=True, headset_presented=False,
                comparison='LastShotAimDirection versus LastShotDirection from the same immutable DoFireEffect record',
                limitations=['No current hand or visual transform is substituted for a shot-time pose.',
                             'This checks firing direction and stock cadence/ammunition; sight geometry, impact effects and wearer feel remain separate.'])


def validate(folder, secondary_weapon='KFVRTrackedDeagle'):
    folder = Path(folder)
    controls = [json.loads(line) for line in (folder/'controls.jsonl').read_text().splitlines()]
    rows = [json.loads(line) for line in (folder/'weapon-observations.jsonl').read_text().splitlines()]
    preflight_path = folder/'pistol-aim-preflight.json'
    preflight = json.loads(preflight_path.read_text()) if preflight_path.exists() else {}
    report = analyze(controls, rows, secondary_weapon=secondary_weapon, preflight=preflight)
    (folder/'pistol-aim-validation.json').write_text(json.dumps(report, indent=2)+'\n')
    if not report['passed']:
        raise ValueError('Pistol aim checks failed: '+', '.join(k for k, passed in report['checks'].items() if not passed))
    return report


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('folder', type=Path)
    parser.add_argument('--secondary-weapon', choices=SECONDARY_WEAPONS, default='KFVRTrackedDeagle')
    args = parser.parse_args()
    print(json.dumps(validate(args.folder, secondary_weapon=args.secondary_weapon), indent=2))
