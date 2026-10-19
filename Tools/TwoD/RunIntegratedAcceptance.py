"""Fixed original-load screen plus integrated native soak; retain every run and failure.

This is a current-host fixture gate, not a declaration of product minimum hardware.
Run on an idle build tree; no local compilation/other native fixtures during measurement.
"""
import argparse
import csv
import datetime
import hashlib
import json
import math
from functools import lru_cache
from pathlib import Path
import statistics
import struct
import subprocess
import time

ROOT = Path(__file__).resolve().parents[2]
LIMITS = {'p95_ms': 1000 / 30, 'p99_ms': 1000 / 30, 'max_ms': 2000 / 30}
MIB = 1024 * 1024
SOURCES = ['Tools/TwoD/Benchmark.cpp', 'Tools/TwoD/IntegratedStress.cpp',
           'Tools/TwoD/IntegratedStress.h', 'Tools/TwoD/RunIntegratedAcceptance.py',
           'Engine/TwoD/src/Scene.cpp', 'Engine/TwoD/src/Queries.cpp',
           'Engine/TwoD/src/GameplayUi.cpp', 'Engine/Presentation/src/Dx12Surface.cpp',
           'Engine/Presentation/src/VulkanSurface.cpp']


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def save(path, data):
    path.write_text(json.dumps(data, indent=2) + '\n', encoding='utf-8')


def distribution(values):
    values = sorted(values)
    if not values or not all(math.isfinite(v) and v >= 0 for v in values):
        raise ValueError('missing/invalid timing samples')
    return dict(samples=len(values), average_ms=statistics.fmean(values),
                p95_ms=values[math.ceil(.95 * len(values)) - 1],
                p99_ms=values[math.ceil(.99 * len(values)) - 1], max_ms=values[-1])


def within(stats):
    return all(stats[key] <= limit for key, limit in LIMITS.items())


@lru_cache(maxsize=28800)
def query_oracle(sparse, step, objects=10000):
    """Independent disc/segment distance counts for the declared moving regular grid."""
    def f32(value):
        return struct.unpack('<f', struct.pack('<f', value))[0]
    spacing = 2 if sparse else .25
    xs = [f32(column * spacing - 12 + step * .0001) for column in range(100)]
    ys = [row * spacing - 12 for row in range(objects // 100)]
    radius = f32(.05)
    query = sum(x*x + y*y <= (2 + radius)**2 for y in ys if abs(y) <= 2 + radius for x in xs)
    sweep = sum((x - min(1, max(-1, x)))**2 + y*y <= (2 * radius)**2
                for y in ys if abs(y) <= 2 * radius for x in xs)
    return query, sweep


def visible_oracle(sparse, step, objects):
    spacing = 2 if sparse else .25
    columns = sum(-16.075 <= column * spacing - 12 + step * .0001 <= 16.075
                  for column in range(100))
    rows = sum(-12.075 <= row * spacing - 12 <= 12.075
               for row in range(objects // 100))
    return columns * rows + 256


def original_result(report, trace, objects=10000):
    rows = list(csv.DictReader(report.open(encoding='utf-8')))
    samples = list(csv.DictReader(trace.open(encoding='utf-8')))
    expected = [('100', '0', '0'), ('1000', '0', '0'), ('5000', '0', '0'),
                ('10000', '0', '0'), ('1000', '0', '1'), ('10000', '1', '0')]
    valid = len(rows) == 6 and len(samples) == 3624
    cases = []
    for index, (row, case) in enumerate(zip(rows, expected)):
        chunk = samples[index * 604:(index + 1) * 604]
        valid &= ((row['objects'], row['sparse'], row['overlap']) == case and
                  row['frames'] == '600' and row['warmup_frames'] == '4')
        valid &= all((s['objects'], s['sparse'], s['overlap']) == case and
                     int(s['frame']) == frame and int(s['warmup']) == (frame < 4)
                     for frame, s in enumerate(chunk))
        stats = distribution([float(s['frame_ms']) for s in chunk[4:]])
        for key in LIMITS:
            valid &= math.isclose(stats[key], float(row[f'frame_{key}']),
                                  rel_tol=1e-5, abs_tol=1e-5)
        cases.append(dict(case=case, timing=stats, pass_budget=within(stats),
                          required=int(case[0]) <= objects))
    return dict(rows=rows, cases=cases, frame_samples=len(samples), correctness_pass=bool(valid),
                performance_pass=len(cases) == 6 and all(c['pass_budget'] for c in cases if c['required']),
                resource_pass=None)


def integrated_result(report, trace, seconds, cadence, objects=10000):
    aggregate = list(csv.DictReader(report.open(encoding='utf-8')))
    if len(aggregate) != 1:
        raise ValueError('missing integrated summary')
    summary = aggregate[0]
    timing = {key: [] for key in ['frame_ms', 'cycle_ms', 'cpu_ms', 'gpu_ms']}
    phase = {False: [], True: []}
    memory = []
    worst = []
    errors = []
    if (int(summary['objects']) != objects or int(summary['vfx']) != 256 or
            int(summary['reload_every']) != cadence or
            int(summary['warmup_frames']) != (1200 if seconds else 4)):
        errors.append({'error': 'summary workload/cadence/warmup mismatch'})
    frames = 0
    visible_total = 0
    warmup = int(summary['warmup_frames'])
    previous_gpu = 0
    unique_gpu = 0
    stages = ['reload_ms', 'begin_frame_ms', 'populate_ms', 'index_ms', 'query_ms',
              'extract_ms', 'ui_ms', 'draw_ms', 'end_frame_ms']
    for row in csv.DictReader(trace.open(encoding='utf-8')):
        frame = int(row['frame'])
        sparse = (frame // cadence) % 2 == 1
        expected_visible = visible_oracle(sparse, frame % cadence + 1, objects)
        visible_total += expected_visible
        expected_reload = frame // cadence
        query, sweep = query_oracle(sparse, frame % cadence + 1, objects)
        expected = {
            'frame': frames, 'warmup': int(frame < warmup), 'objects': objects, 'vfx': 256,
            'sparse': int(sparse), 'reloaded': int(frame > 0 and frame % cadence == 0),
            'visible': expected_visible, 'culled': objects + 256 - expected_visible,
            'sprite_batches': 2, 'ui_quads': 120, 'ui_commands': 120,
            'query_hits': query, 'sweep_hits': sweep,
            'reloads': expected_reload, 'fixture_alloc_live': 0,
            'fixture_allocations': expected_reload, 'fixture_frees': expected_reload,
            'acquired': frame + 1, 'presented': frame + 1,
            'instanced_draws': (frame + 1) * 2, 'gpu_instances': visible_total,
            'ui_draws': (frame + 1) * 123, 'texture_uploads': 5 + expected_reload * 2,
            'rejected_textures': 0, 'memory_sampled': int(frame % 60 == 0),
        }
        if any(int(row[key]) != value for key, value in expected.items()) and len(errors) < 20:
            errors.append({'frame': frame, 'mismatches': {
                key: [value, row[key]] for key, value in expected.items()
                if int(row[key]) != value}})
        values = [float(row[key]) for key in stages]
        full, cycle = float(row['frame_ms']), float(row['cycle_ms'])
        if (not all(math.isfinite(v) and v >= 0 for v in values) or
                sum(values) > full + 1e-5 or not 0 <= float(row['cpu_ms']) <= full <= cycle):
            if len(errors) < 20:
                errors.append({'frame': frame, 'error': 'timing containment'})
        if row['gpu_ms']:
            submission = int(row['gpu_submission'])
            if submission <= previous_gpu and len(errors) < 20:
                errors.append({'frame': frame, 'error': 'duplicate GPU sample'})
            previous_gpu = submission
        if frame >= warmup:
            for key in timing:
                if row[key] and (key != 'gpu_ms' or int(row['gpu_submission']) > warmup):
                    timing[key].append(float(row[key]))
            phase[sparse].append(cycle)
            worst.append(row)
            worst.sort(key=lambda sample: float(sample['cycle_ms']), reverse=True)
            del worst[20:]
            unique_gpu += bool(row['gpu_ms']) and int(row['gpu_submission']) > warmup
            if row['memory_sampled'] == '1':
                memory.append(row)
        frames += 1
    measured = frames - warmup
    if measured != int(summary['frames']) or frames != int(summary['steps']):
        errors.append({'error': 'omitted frame/update'})
    if seconds and float(summary['measured_seconds']) < seconds:
        errors.append({'error': 'duration below fixed minimum'})
    if unique_gpu != int(summary['gpu_samples']) or unique_gpu != measured - 2:
        errors.append({'error': 'missing completed GPU timing samples'})
    resources = {}
    # These are observed-process budgets; fixture allocation is scoped, not all engine new/malloc.
    for key, cap, growth in [('rss_bytes', 512 * MIB, 32 * MIB),
                             ('private_bytes', 512 * MIB, 32 * MIB),
                             ('gpu_local_usage_bytes', 512 * MIB, 16 * MIB),
                             ('process_handles', 4096, 8),
                             ('buffer_reallocations', 8, 0)]:
        observed = [sample for sample in memory if sample[key] != '']
        item = {'available': bool(observed), 'peak_limit': cap, 'growth_limit': growth}
        if observed:
            start, end = float(observed[0]['elapsed_seconds']), float(observed[-1]['elapsed_seconds'])
            first = [int(sample[key]) for sample in observed
                     if float(sample['elapsed_seconds']) <= start + 300]
            last = [int(sample[key]) for sample in observed
                    if float(sample['elapsed_seconds']) >= end - 300]
            item.update(peak=max(int(s[key]) for s in observed),
                        first_window_median=statistics.median(first),
                        last_window_median=statistics.median(last))
            item['median_growth'] = item['last_window_median'] - item['first_window_median']
            item['pass_budget'] = item['peak'] <= cap and item['median_growth'] <= growth
        resources[key] = item
    stats = {key: distribution(values) for key, values in timing.items() if values}
    phases = {('sparse' if sparse else 'mixed'): distribution(values)
              for sparse, values in phase.items() if values}
    return {'summary': summary, 'frames': frames, 'measured_frames': measured,
            'correctness_errors': errors, 'correctness_pass': not errors,
            'timing': stats, 'phases_cycle': phases, 'worst_frames': worst,
            'performance_pass': within(stats['cycle_ms']) and within(stats['frame_ms']) and
                                all(within(s) for s in phases.values()),
            'resources': resources,
            'resource_pass': all(item.get('pass_budget', False) for item in resources.values()),
            'memory_samples': len(memory), 'pending_gpu_submissions': frames - previous_gpu,
            'dropped_gpu_samples': max(0, measured - 2 - unique_gpu),
            'dropped_frame_samples': max(0, int(summary['steps']) - frames)}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--benchmark', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--seconds', type=int, default=1800)
    parser.add_argument('--objects', type=int, choices=[1000, 5000, 10000], default=10000,
                        help='1000 functional, 5000 current-PC M6, 10000 additional stress; default preserves historical plan')
    parser.add_argument('--backends', nargs='+', choices=['dx12', 'vulkan'], default=['dx12', 'vulkan'])
    parser.add_argument('--vendor-id', type=int, default=4318)
    parser.add_argument('--device-id', type=int, default=5121)
    args = parser.parse_args()
    if not 1 <= args.seconds <= 14400:
        parser.error('seconds 1..14400 required')
    exe, output = args.benchmark.resolve(), args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    runs = []
    for repeat in [1, 2]:
        for backend in args.backends:
            runs.append(dict(stem=f'{backend}-original-{repeat}', backend=backend, mode='original',
                             frames=600, seconds=0, reload_every=None))
            runs.append(dict(stem=f'{backend}-integrated-{repeat}', backend=backend, mode='integrated',
                             frames=600, seconds=0, reload_every=300))
    for backend in args.backends:
        runs.append(dict(stem=f'{backend}-soak', backend=backend, mode='integrated', frames=48,
                         seconds=args.seconds, reload_every=600))
    for run in runs:
        stem = run['stem']
        command = [str(exe), '--native', '--backend', run['backend'], '--frames', str(run['frames']),
                   '--report', str(output / f'{stem}.csv'),
                   '--frame-trace', str(output / f'{stem}-frames.csv')]
        if run['mode'] == 'integrated':
            command += ['--integrated', '--objects', str(args.objects),
                        '--reload-every', str(run['reload_every'])]
            if run['seconds']:
                command += ['--soak-seconds', str(run['seconds'])]
        run['command'] = command
    plan = dict(declared_at_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),
                source=subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
                executable=str(exe), executable_sha256=digest(exe),
                source_sha256={path: digest(ROOT / path) for path in SOURCES},
                viewport=[640, 480], present_mode='VSync', frames_in_flight=2,
                target_fps=30, frame_budgets=LIMITS, runs=runs,
                integrated=dict(updated_objects=args.objects, query_shapes=args.objects, vfx=256,
                                sprite_size=.15, vfx_size=.6, vfx_alpha=.125,
                                fixed_step_seconds=1/60, animation_frame_seconds=.1,
                                sprite_atlases=2, sprite_batches=2, ui_panels=16, ui_buttons=8,
                                material='one unlit Sprite program, one indexed UI program; no material asset switches',
                                ui_labels=8, ui_quads=120, ui_commands=120,
                                visible_mixed=visible_oracle(False, 1, args.objects),
                                visible_sparse=visible_oracle(True, 1, args.objects),
                                allocation_per_reload_bytes=8388608,
                                loading='pool recycle; touched scoped allocation; atlas codec decode; GPU retirement/reupload; no disk IO'),
                soak=dict(minimum_measured_seconds=args.seconds, warmup_frames=1200,
                          memory_scope='Post-frame sampled peaks, not lifetime/intraframe peaks; explicit fixture allocation has exact scoped accounting.',
                          phase_and_reload_every_frames=600, resource_sample_every_frames=60,
                          process_bytes_peak=512*MIB, process_median_growth_bytes=32*MIB,
                          gpu_local_usage_peak_bytes=512*MIB, gpu_local_median_growth_bytes=16*MIB,
                          process_handles_peak=4096, process_handles_median_growth=8,
                          buffer_reallocations_peak=8, buffer_reallocations_growth=0,
                          comparison_windows_seconds=300),
                gpu_sample_policy='Every measured submission except the final two in-flight submissions; no duplicate or missing completed samples.',
                scope='Current physical developer-PC integrated fixture. Product minimum spec, total allocator accounting, physical VRAM residency and M7 device gates remain separate.',
                timing='Integrated full includes preceding buffered trace output, frame destruction and resource polls. Cycle is start-to-next-start; both must pass. GPU samples are earlier completed submissions, not same-frame or display latency.',
                policy='No build/other native fixture during timing; keep ordinary desktop apps. All rows/runs/frames retained. No retries after performance failure, no excluded measured spikes. Any missing sample, wrong native device or nonzero exit fails.')
    plan['original_screen_policy'] = ('All six historical cases retain correctness checks and timing verdicts; '
                                      'only cases with objects <= selected integrated load gate performance.')
    plan['load_role'] = {1000: 'functional smoke only', 5000: 'current-PC M6 required load',
                         10000: 'additional stress'}[args.objects]
    save(output / 'plan.json', plan)
    executions = []
    for run in runs:
        stem = run['stem']
        print(f'Start {stem}: fixed minimum seconds={run["seconds"]}', flush=True)
        started = time.monotonic()
        with (output / f'{stem}.stdout').open('wb') as out, (output / f'{stem}.stderr').open('wb') as err:
            process = subprocess.Popen(run['command'], cwd=ROOT, stdout=out, stderr=err)
            deadline = started + run['seconds'] + 600
            while True:
                try:
                    code = process.wait(timeout=min(45, max(.01, deadline-time.monotonic())))
                    break
                except subprocess.TimeoutExpired:
                    if time.monotonic() >= deadline:
                        process.kill()
                        process.wait()
                        code = 'timeout'
                        break
                    # Only a bounded tail read, never percentile analysis during native timing.
                    tail = output / f'{stem}-frames.csv'
                    latest = ''
                    if tail.exists():
                        with tail.open('rb') as stream:
                            stream.seek(max(0, tail.stat().st_size - 4096))
                            lines = stream.read().splitlines()
                            if len(lines) > 2:
                                latest = lines[-2].decode('ascii').split(',', 5)[0]
                    print(f'Progress {stem}: wall={time.monotonic()-started:.0f}s last_flushed_frame={latest}',
                          flush=True)
        executions.append(dict(stem=stem, exit_code=code, wall_seconds=time.monotonic()-started))
        save(output / 'executions.json', executions)
        print(f'End {stem}: exit={code}', flush=True)
    # Analyse after every native process has finished, avoiding analysis load during measurement.
    results = []
    for run, execution in zip(runs, executions):
        stem = run['stem']
        item = dict(execution)
        if execution['exit_code'] != 0:
            item['correctness_pass'] = False
        else:
            report, trace = output / f'{stem}.csv', output / f'{stem}-frames.csv'
            if run['mode'] == 'original':
                item.update(original_result(report, trace, args.objects))
                rows = item['rows']
            else:
                item.update(integrated_result(report, trace, run['seconds'], run['reload_every'], args.objects))
                rows = [item['summary']]
            item['device_pass'] = all(r['vendor_id'] == str(args.vendor_id) and
                                      r['device_id'] == str(args.device_id) and
                                      r['backend_requested'] == run['backend'] for r in rows)
        results.append(item)
    save(output / 'results.json', results)
    identity = dict(executable_sha256_before=plan['executable_sha256'], executable_sha256_after=digest(exe),
                    source_sha256_after={path:digest(ROOT/path) for path in SOURCES},
                    artifacts={p.name:digest(p) for p in sorted(output.iterdir()) if p.is_file()})
    save(output / 'identity.json', identity)
    passed = (identity['executable_sha256_before'] == identity['executable_sha256_after'] and
              plan['source_sha256'] == identity['source_sha256_after']) and all(
        item.get('correctness_pass') and item.get('device_pass') and item.get('performance_pass') and
        (item.get('resource_pass') is not False) for item in results)
    print(f'Fixed current-host gate passed={passed}. Product M6 acceptance requires scope review.', flush=True)
    return 0 if passed else 1


if __name__ == '__main__':
    raise SystemExit(main())
