"""Validate saved native-renderer trials; never launch a process or touch the game.

Comparison is restricted to equal-size, equal-duration desktop cadence trials.
Counters establish work/copy accounting, not identical scenes or headset comfort.
"""
import argparse
import json
import math
import struct
from pathlib import Path

BACKENDS = ('d3d9_cpu', 'd3d9on12_gpu')
VIEWS = ('outside_world', 'desktop', 'left', 'right')
LIMITS = [
    'Scene, camera, lighting, native graphics settings and fixture equivalence require manual inspection; folder names do not establish them.',
    'Render/transfer/QPC timings are CPU wall time around calls; GPU submission time is not GPU execution time.',
    'Receiver pairs per second measure this desktop diagnostic, including its waits/copies; they are not headset refresh rate or motion-to-photon latency.',
    'Saved eye pairs are sparse pixel checks, not proof of every frame, temporal effects, mirror appearance, or exact per-pair desktop-suppression provenance.',
    'Both native eye renders remain; this does not establish a single world traversal.',
    'Producer render/transfer stage timings exclude the desktop mirror; their sum is not total frame time.',
]


def number(x):
    return type(x) in (float, int) and math.isfinite(x) and x >= 0


def integer(x):
    return type(x) is int and x >= 0


def positive(x):
    return number(x) and x > 0


def ratio(a, b):
    return a / b if number(a) and positive(b) else None


def bitmap_pixels(data):
    if len(data) < 54 or data[:2] != b'BM':
        raise ValueError('Not a complete BMP')
    offset = struct.unpack_from('<I', data, 10)[0]
    header, width, height, planes, bits, compression = struct.unpack_from('<IiiHHI', data, 14)
    if header < 40 or not 1 <= width <= 4096 or not 1 <= abs(height) <= 4096 or planes != 1 or bits != 32 or compression != 0 or offset < 54:
        raise ValueError('Expected an uncompressed 32-bit native capture')
    size = width * abs(height) * 4
    if offset + size > len(data):
        raise ValueError('Truncated native capture')
    pixels = memoryview(data)[offset:offset + size].cast('I')
    def ordered():
        for y in range(abs(height)):
            row = y if height < 0 else abs(height) - 1 - y
            for pixel in pixels[row * width:(row + 1) * width]:
                yield pixel & 0xffffff
    return (width, abs(height)), ordered


def check_bitmaps(left, right):
    dimensions, lp = bitmap_pixels(left)
    right_dimensions, rp = bitmap_pixels(right)
    if dimensions != right_dimensions:
        raise ValueError('Eye bitmap dimensions differ')
    colors = [set(), set()]
    differences = 0
    for a, b in zip(lp(), rp()):
        if len(colors[0]) <= 64:
            colors[0].add(a)
        if len(colors[1]) <= 64:
            colors[1].add(b)
        if differences <= 1024:
            differences += a != b
        if min(map(len, colors)) > 64 and differences > 1024:
            break
    return dict(dimensions=list(dimensions), minimum_rgb_colors_lower_bound=min(map(len, colors)),
                differing_rgb_pixels_lower_bound=differences,
                passed=min(map(len, colors)) > 64 and differences > 1024)


def analyze_evidence(receiver, summary, bridge, views_report, trace, pixels,
                     expected_backend, expect_suppression=None, last_pixels=None, require_last_pair=False):
    """Pure checks shared by saved-file validation and in-memory negative tests."""
    checks = {}
    frames = trace.get('frames') if isinstance(trace.get('frames'), list) else []
    view_rows = views_report.get('views') if isinstance(views_report.get('views'), list) else []
    def check(name, condition):
        checks[name] = bool(condition)
    check('desktop_receiver_identity', receiver.get('synthetic_tracking') is True and receiver.get('headset_presented') is False)
    check('expected_backend', expected_backend in BACKENDS and bridge.get('transfer_backend') == expected_backend)
    check('backend_readback_policy', bridge.get('per_frame_cpu_image_readback') is (expected_backend == 'd3d9_cpu'))
    check('closed_and_restored', all(bridge.get(k) is True for k in ('closed', 'reset_hook_restored', 'resources_released'))
          and bridge.get('reason') in ('host_closed', 'duration_finished'))
    check('launcher_and_hooks_clean', summary.get('launcher_exit') == 0 and summary.get('entry_bytes_restored') is True
          and summary.get('hooks_disabled') is True and trace.get('status') == 0 and trace.get('hooks_disabled') is True)
    check('native_preflights_passed', all(summary.get(k) is True for k in ('target_trial_passed', 'camera_trial_passed', 'bridge_trial_passed', 'body_anchor_passed')))
    check('summary_matches_native_bridge', summary.get('bridge') == bridge)
    check('summary_matches_render_views', summary.get('render_views') == views_report and summary.get('render_view_counters_passed') is True)
    failure_fields = ('device_resets', 'mirror_failures', 'mirror_state_failures', 'restored_render_failures', 'unsafe_render_failures')
    check('no_native_or_mirror_failures', all(type(bridge.get(k)) is int and bridge[k] == 0 for k in failure_fields))
    check('transfer_succeeded', type(bridge.get('transfer_hresult')) is int and bridge['transfer_hresult'] == 0)
    check('no_unsafe_world_fallback', views_report.get('unsafe_state_latched') is False and views_report.get('unsafe_aborts') == 0)
    check('no_engine_tick_inside_draw', views_report.get('draws_with_engine_tick') == 0 and summary.get('engine_ticks_observed_inside_draw') is False
          and bool(frames) and all(isinstance(f, dict) and integer(f.get('ticks_before')) and integer(f.get('ticks_after'))
              and f['ticks_before'] == f['ticks_after'] for f in frames))
    count_fields = ('requests', 'rendered_pairs', 'published_pairs', 'consumed_pairs', 'discarded_pairs', 'expired_before_transfer', 'transfer_busy_pairs',
                    'mirror_attempts', 'mirror_copies', 'desktop_replacements', 'mirrored_replacements')
    counts_valid = all(integer(bridge.get(k)) for k in count_fields)
    check('pair_counters_complete', counts_valid)
    pairs = receiver.get('pairs')
    check('at_least_twelve_complete_receiver_pairs', integer(pairs) and pairs >= 12)
    check('consumed_pairs_match_receiver', integer(pairs) and pairs == bridge.get('consumed_pairs'))
    if counts_valid:
        req, rendered, published, consumed, discarded = (bridge[k] for k in count_fields[:5])
        check('pair_counter_order', req >= rendered >= published >= consumed and consumed + discarded <= rendered)
        # Expiry is recorded at two boundaries; not all expired requests rendered.
        # At most one pair may remain between rendering/publication and close.
        check('unaccounted_pairs_bounded_by_drop_reasons', 0 <= rendered - consumed - discarded <= bridge['expired_before_transfer'] + bridge['transfer_busy_pairs'] + 1)
    else:
        check('pair_counter_order', False)
        check('unaccounted_pairs_bounded_by_drop_reasons', False)
    width, height = receiver.get('width'), receiver.get('height')
    check('valid_receiver_dimensions', integer(width) and integer(height) and 64 <= width <= 4096 and 64 <= height <= 4096)
    checked = receiver.get('pixel_checked_pairs')
    check('receiver_pixel_checks_nonflat_and_distinct', integer(checked) and integer(pairs) and 1 <= checked <= pairs
          and integer(receiver.get('minimum_eye_colors')) and receiver['minimum_eye_colors'] > 64
          and integer(receiver.get('minimum_differing_eye_pixels')) and receiver['minimum_differing_eye_pixels'] > 1024)
    check('saved_first_eye_bitmaps_independently_pass', pixels.get('passed') is True and pixels.get('dimensions') == [width, height])
    last_declared = 'last_pixel_checked' in receiver or 'last_frame_id' in receiver or last_pixels is not None
    last_complete = (receiver.get('last_pixel_checked') is True and integer(receiver.get('last_frame_id')) and receiver['last_frame_id'] > 0
                     and integer(checked) and checked >= 2 and isinstance(last_pixels, dict)
                     and last_pixels.get('passed') is True and last_pixels.get('dimensions') == [width, height])
    check('recorded_last_pair_pixels_independently_pass', not last_declared or last_complete)
    check('required_last_pair_coverage', not require_last_pair or last_complete)

    check('valid_receiver_wall_timings', positive(receiver.get('trial_elapsed_ms')) and positive(receiver.get('request_to_copy_average_ms'))
          and number(receiver.get('request_to_copy_max_ms')) and receiver['request_to_copy_max_ms'] >= receiver['request_to_copy_average_ms'])
    if receiver.get('diagnostic_mode') == 'cadence':
        check('complete_requested_cadence_duration', positive(receiver.get('requested_duration_ms')) and receiver['requested_duration_ms'] >= 20000
              and number(receiver.get('trial_elapsed_ms')) and receiver['trial_elapsed_ms'] >= receiver['requested_duration_ms'])
    timings = ('render_total_ms', 'render_max_ms', 'transfer_total_ms', 'transfer_max_ms', 'resolve_total_ms', 'readback_total_ms', 'convert_total_ms', 'upload_total_ms', 'publish_total_ms', 'elapsed_ms')
    check('valid_native_wall_timings', all(number(bridge.get(k)) for k in timings) and positive(bridge.get('elapsed_ms'))
          and bridge.get('render_total_ms', 0) >= bridge.get('render_max_ms', 0) and bridge.get('transfer_total_ms', 0) >= bridge.get('transfer_max_ms', 0))
    if expected_backend == 'd3d9on12_gpu':
        check('gpu_path_has_no_cpu_image_readback_or_upload', bridge.get('readback_total_ms') == 0 and bridge.get('upload_total_ms') == 0)
    views = {v['name']: v for v in view_rows if isinstance(v, dict) and isinstance(v.get('name'), str)}
    view_fields = ('outer_views', 'level_original_calls', 'nested_level_original_calls', 'script_calls', 'getframe_calls', 'actor_animation_calls', 'skeletal_animation_calls', 'script_overflow', 'outer_level_qpc_ticks', 'outer_level_max_qpc_ticks')
    valid_views = set(views) == set(VIEWS) and len(view_rows) == 4 and all(integer(v.get(k)) for v in views.values() for k in view_fields)
    check('all_per_view_counters_present', valid_views)
    check('whole_session_counter_coverage', positive(views_report.get('draws')) and positive(views_report.get('qpc_frequency'))
          and integer(views_report.get('root_world_callbacks')) and integer(views_report.get('desktop_world_skipped'))
          and integer(views_report.get('desktop_fallbacks')) and views_report.get('draws', 0) >= len(frames))
    if valid_views and counts_valid:
        check('both_native_eyes_rendered_per_pair', views['left']['outer_views'] == views['right']['outer_views'] == bridge['rendered_pairs'])
        check('all_root_world_calls_accounted', integer(views_report.get('desktop_world_skipped')) and integer(views_report.get('root_world_callbacks'))
              and views['desktop']['outer_views'] + views_report['desktop_world_skipped'] == views_report['root_world_callbacks'])
        check('native_level_and_script_counters_consistent', all(v['nested_level_original_calls'] <= v['level_original_calls']
              and v['outer_views'] <= v['level_original_calls'] and v['outer_level_max_qpc_ticks'] <= v['outer_level_qpc_ticks']
              and v['script_overflow'] == 0 and isinstance(v.get('scripts'), list)
              and all(isinstance(c, dict) and integer(c.get('calls')) for c in v['scripts'])
              and sum(c['calls'] for c in v['scripts']) == v['script_calls'] for v in views.values()))
        check('no_animation_advance_inside_draw', all(v['actor_animation_calls'] == v['skeletal_animation_calls'] == 0 for v in views.values()))
    else:
        for key in ('both_native_eyes_rendered_per_pair', 'all_root_world_calls_accounted', 'native_level_and_script_counters_consistent', 'no_animation_advance_inside_draw'):
            check(key, False)
    suppression = bridge.get('desktop_suppression_opt_in')
    check('suppression_flags_agree', type(suppression) is bool and views_report.get('desktop_suppression_opt_in') is suppression
          and summary.get('desktop_suppression_opt_in') is suppression)
    check('expected_suppression', expect_suppression is None or suppression is expect_suppression)
    if suppression is True:
        check('suppression_replaces_world_with_successful_mirror', counts_valid and bridge['desktop_replacements'] > 0
              and bridge['desktop_replacements'] == bridge['mirrored_replacements'] == views_report.get('desktop_world_skipped')
              and bridge['mirror_copies'] > bridge['desktop_replacements'] and bridge['mirror_attempts'] == bridge['mirror_copies']
              and bridge.get('mirror_hresult') == 0 and positive(bridge.get('mirror_width')) and positive(bridge.get('mirror_height'))
              and bridge.get('mirror_format') in (21, 22))
        check('suppression_fallbacks_accounted', valid_views and views_report.get('desktop_fallbacks') == views['desktop']['outer_views'])
        diagnostic = bridge.get('mirror_diagnostic') if isinstance(bridge.get('mirror_diagnostic'), dict) else {}
        target = diagnostic.get('target') if isinstance(diagnostic.get('target'), dict) else {}
        source = diagnostic.get('source') if isinstance(diagnostic.get('source'), dict) else {}
        path = diagnostic.get('path')
        check('successful_mirror_path_measured', path in ('direct_stretch', 'msaa_textured_quad')
              and diagnostic.get('stage') == 'complete' and diagnostic.get('first_failure_stage') == 'none'
              and diagnostic.get('first_failure_hresult') == 0 and integer(target.get('samples'))
              and [source.get('width'), source.get('height')] == [width, height])
        if path == 'msaa_textured_quad':
            quad_fields = ('quad_attempts', 'quad_copies', 'verified_state_restores', 'scene_begins', 'scene_ends', 'texture_copies')
            check('msaa_quad_scene_and_state_restoration_verified', all(integer(diagnostic.get(k)) for k in quad_fields)
                  and positive(target.get('samples')) and integer(diagnostic.get('render_target_slots')) and 1 <= diagnostic['render_target_slots'] <= 4
                  and diagnostic['quad_attempts'] == diagnostic['quad_copies'] == diagnostic['verified_state_restores']
                  == diagnostic['scene_begins'] == diagnostic['scene_ends'] == bridge.get('mirror_copies')
                  and diagnostic['quad_copies'] > 0 and diagnostic['texture_copies'] <= diagnostic['quad_copies']
                  and diagnostic.get('state_restore_hresult') == 0 and diagnostic.get('texture_source') in ('native_level_zero', 'owned_rt_copy'))
        elif path == 'direct_stretch':
            check('direct_copy_destination_is_not_multisampled', target.get('samples') == 0)

    elif suppression is False:
        check('ordinary_path_preserved', counts_valid and bridge['desktop_replacements'] == bridge['mirrored_replacements'] == bridge['mirror_attempts'] == bridge['mirror_copies'] == views_report.get('desktop_world_skipped') == 0)
    else:
        check('suppression_policy_measured', False)
    per_view = {}
    for name, view in views.items():
        elapsed = ratio(view.get('outer_level_qpc_ticks'), views_report.get('qpc_frequency'))
        per_view[name] = dict(outer_views=view.get('outer_views'), native_level_calls=view.get('level_original_calls'),
            nested_level_calls=view.get('nested_level_original_calls'), script_calls=view.get('script_calls'), getframe_calls=view.get('getframe_calls'),
            outer_level_wall_total_ms=elapsed * 1000 if elapsed is not None else None,
            outer_level_wall_average_ms=ratio(elapsed * 1000, view.get('outer_views')) if elapsed is not None else None)
    metrics = dict(dimensions=[width, height], diagnostic_mode=receiver.get('diagnostic_mode'), requested_duration_ms=receiver.get('requested_duration_ms'),
        backend=bridge.get('transfer_backend'), desktop_suppression=suppression, receiver_complete_pairs=pairs,
        last_pair_pixel_checked=bool(last_complete), last_received_frame_id=receiver.get('last_frame_id'),
        observed_desktop_receiver_pairs_per_second=ratio(pairs * 1000, receiver.get('trial_elapsed_ms')) if integer(pairs) else None,
        receiver_interval_pairs_per_second=ratio((pairs - 1) * 1000, receiver.get('trial_elapsed_ms')) if integer(pairs) and pairs else None,
        receiver_request_to_copy_wall_average_ms=receiver.get('request_to_copy_average_ms'), receiver_request_to_copy_wall_max_ms=receiver.get('request_to_copy_max_ms'),
        producer_render_wall_average_ms=ratio(bridge.get('render_total_ms'), bridge.get('rendered_pairs')),
        # Publish failures may have completed a transfer: the denominator is the
        # counted published pairs, never represented as per-GPU-work duration.
        producer_transfer_wall_total_ms=bridge.get('transfer_total_ms'), producer_transfer_wall_per_published_pair_ms=ratio(bridge.get('transfer_total_ms'), bridge.get('published_pairs')),
        producer_transfer_wall_max_ms=bridge.get('transfer_max_ms'), producer_elapsed_ms=bridge.get('elapsed_ms'),
        rendered_pairs=bridge.get('rendered_pairs'), published_pairs=bridge.get('published_pairs'), consumed_pairs=bridge.get('consumed_pairs'), discarded_pairs=bridge.get('discarded_pairs'),
        transfer_busy_pairs=bridge.get('transfer_busy_pairs'), expired_pairs=bridge.get('expired_before_transfer'),
        desktop_world_skipped=views_report.get('desktop_world_skipped'), root_world_callbacks=views_report.get('root_world_callbacks'),
        desktop_world_suppression_fraction=ratio(views_report.get('desktop_world_skipped'), views_report.get('root_world_callbacks')),
        detailed_draw_samples=len(frames), aggregate_draws=views_report.get('draws'), per_view=per_view)
    return dict(passed=all(checks.values()), checks=checks, failed_checks=[k for k, v in checks.items() if not v], metrics=metrics,
                native_outcome={k: bridge.get(k) for k in ('reason', 'closed', 'reset_hook_restored', 'resources_released', 'transfer_hresult',
                    'device_resets', 'restored_render_failures', 'unsafe_render_failures', 'mirror_attempts', 'mirror_copies', 'mirror_failures',
                    'mirror_state_failures', 'mirror_hresult', 'mirror_diagnostic')}, pixel_evidence=pixels, last_pixel_evidence=last_pixels,
                limitations=LIMITS + ([] if last_complete else ['Only the first received pair has independent saved pixel evidence.']))


def analyze_trial(folder, expected_backend, expect_suppression=None, require_last_pair=False):
    folder = Path(folder).resolve()
    errors = []
    def read(path):
        try:
            value = json.loads(path.read_text(encoding='utf-8-sig'), parse_constant=lambda value: (_ for _ in ()).throw(ValueError('Nonfinite JSON value: ' + value)))
            if not isinstance(value, dict):
                raise ValueError('Expected a JSON object')
            return value
        except (OSError, ValueError) as error:
            errors.append(dict(path=str(path), error=str(error)))
            return {}
    receiver = read(folder / 'receiver.json')
    summary = read(folder / 'trace-summary.json')
    trace_dir = Path(summary['report']).parent if isinstance(summary.get('report'), str) else folder
    bridge = read(trace_dir / 'native-bridge.json')
    views = read(trace_dir / 'native-render-views.json')
    trace = read(trace_dir / 'trace.json')
    try:
        pixels = check_bitmaps((folder / 'first-left.bmp').read_bytes(), (folder / 'first-right.bmp').read_bytes())
    except (OSError, ValueError, struct.error) as error:
        pixels = dict(passed=False, error=str(error))
    last_pixels = None
    if 'last_pixel_checked' in receiver or 'last_frame_id' in receiver or (folder / 'last-left.bmp').exists() or (folder / 'last-right.bmp').exists():
        try:
            last_pixels = check_bitmaps((folder / 'last-left.bmp').read_bytes(), (folder / 'last-right.bmp').read_bytes())
        except (OSError, ValueError, struct.error) as error:
            last_pixels = dict(passed=False, error=str(error))
    result = analyze_evidence(receiver, summary, bridge, views, trace, pixels, expected_backend, expect_suppression, last_pixels, require_last_pair)
    result.update(folder=str(folder), trace_folder=str(trace_dir), file_errors=errors)
    if errors:
        result['passed'] = False
    if (folder / 'host.json').exists():
        host = read(folder / 'host.json')
        result['headset_summary'] = {k: host.get(k) for k in ('submitted_pairs', 'reprojected_frames', 'empty_frames', 'rejected_pairs', 'ended_frames', 'eye_width', 'eye_height')}
        result['limitations'] = LIMITS + ['Headset metadata is summarized separately; this desktop pixel/cadence acceptance contract cannot validate a headset-only recording.']
    return result


def compare(baseline, candidate):
    a, b = baseline['metrics'], candidate['metrics']
    checks = dict(both_trials_pass=baseline['passed'] and candidate['passed'],
        dimensions_match=a['dimensions'] == b['dimensions'] and all(positive(v) for v in a['dimensions']),
        controlled_cadence_metadata=a['diagnostic_mode'] == b['diagnostic_mode'] == 'cadence',
        requested_duration_matches=positive(a['requested_duration_ms']) and a['requested_duration_ms'] == b['requested_duration_ms'])
    deltas = {}
    if all(checks.values()):
        for key in ('observed_desktop_receiver_pairs_per_second', 'receiver_request_to_copy_wall_average_ms', 'producer_render_wall_average_ms', 'producer_transfer_wall_per_published_pair_ms', 'desktop_world_suppression_fraction'):
            av, bv = a.get(key), b.get(key)
            if number(av) and number(bv):
                deltas[key] = dict(baseline=av, candidate=bv, absolute_change=bv - av, candidate_over_baseline=ratio(bv, av))
    return dict(passed=all(checks.values()), checks=checks, baseline=baseline, candidate=candidate, comparable_metrics=deltas,
                identical_scene_verified=False, headset_performance_verified=False, limitations=LIMITS)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--baseline', type=Path, required=True)
    parser.add_argument('--candidate', type=Path, required=True)
    parser.add_argument('--baseline-backend', choices=BACKENDS, default='d3d9_cpu')
    parser.add_argument('--candidate-backend', choices=BACKENDS, required=True)
    parser.add_argument('--baseline-suppression', choices=('on', 'off'), default='off')
    parser.add_argument('--candidate-suppression', choices=('on', 'off'))
    parser.add_argument('--require-last-pair', action='store_true', help='Require independently checked saved last-pair bitmaps in both trials')
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    baseline = analyze_trial(args.baseline, args.baseline_backend, args.baseline_suppression == 'on', args.require_last_pair)
    candidate = analyze_trial(args.candidate, args.candidate_backend, None if args.candidate_suppression is None else args.candidate_suppression == 'on', args.require_last_pair)
    result = compare(baseline, candidate)
    args.output.write_text(json.dumps(result, indent=2, allow_nan=False) + '\n', encoding='utf-8')
    print(json.dumps(dict(passed=result['passed'], comparison_checks=result['checks'], baseline_failed=baseline['failed_checks'], candidate_failed=candidate['failed_checks'], output=str(args.output)), indent=2))
    return 0 if result['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
