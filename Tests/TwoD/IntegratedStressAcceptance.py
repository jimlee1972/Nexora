"""Check the integrated load against independent geometry/lifecycle observations."""
import argparse
import csv
import importlib.util
import math
from pathlib import Path
import subprocess
import tempfile


def check(value, message):
    if not value:
        raise RuntimeError(message)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--benchmark', type=Path, required=True)
    args = parser.parse_args()
    spec = importlib.util.spec_from_file_location(
        'integrated_gate', Path(__file__).resolve().parents[2] / 'Tools/TwoD/RunIntegratedAcceptance.py')
    gate = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(gate)
    with tempfile.TemporaryDirectory(prefix='nexora-integrated-') as directory:
        work = Path(directory)
        report, trace = work / 'report.csv', work / 'frames.csv'
        for objects in [10000, 5000, 1000]:
            command = [str(args.benchmark.resolve()), '--integrated', '--frames', '128',
                       '--reload-every', '120', '--report', str(report), '--frame-trace', str(trace)]
            if objects != 10000:
                command += ['--objects', str(objects)]
            subprocess.run(command, check=True, timeout=45)
            summary = list(csv.DictReader(report.open(encoding='utf-8')))
            rows = list(csv.DictReader(trace.open(encoding='utf-8')))
            check(len(summary) == 1 and len(rows) == 132, 'omitted frames/summary')
            header = summary[0]
            check((header['objects'], header['vfx'], header['frames'], header['warmup_frames']) ==
                  (str(objects), '256', '128', '4'), 'workload count changed')
            check(header['steps'] == '132' and header['reloads'] == '1', 'lost update/reload')
            check(int(header['animation_events']) == 22 and int(header['activations']) >= 1,
                  'animation or UI input did not advance')
            stages = ['reload_ms', 'begin_frame_ms', 'populate_ms', 'index_ms', 'query_ms',
                      'extract_ms', 'ui_ms', 'draw_ms', 'end_frame_ms']
            for frame, row in enumerate(rows):
                sparse = (frame // 120) % 2 == 1
                check(int(row['frame']) == frame and int(row['warmup']) == (frame < 4),
                      'frame identity/dropped sample')
                check(int(row['sparse']) == sparse and int(row['reloaded']) ==
                      (frame > 0 and frame % 120 == 0), 'phase/reload cadence')
                spacing = 2 if sparse else .25
                shift = (frame % 120 + 1) * .0001
                centers = [(i % 100 * spacing - 12 + shift, i // 100 * spacing - 12)
                           for i in range(objects)]
                query = sum(x*x + y*y <= 2.05**2 for x, y in centers)
                sweep = sum((x - min(1, max(-1, x)))**2 + y*y <= .1**2
                            for x, y in centers)
                check(int(row['query_hits']) == query and int(row['sweep_hits']) == sweep,
                      'query/sweep differs from independent all-object oracle')
                check(gate.query_oracle(sparse, frame % 120 + 1, objects) == (query, sweep),
                      'acceptance runner failed to account for moving query boundaries')
                # Orthographic bounds: x in [-16,16], y in [-12,12], sprite half size .075.
                visible = sum(-16.075 <= x <= 16.075 and -12.075 <= y <= 12.075
                              for x, y in centers) + 256
                check(int(row['visible']) == visible and int(row['culled']) == objects + 256 - visible,
                      'CPU culling differs from independent camera oracle')
                check(gate.visible_oracle(sparse, frame % 120 + 1, objects) == visible,
                      'acceptance runner culling oracle differs')
                check(row['sprite_batches'] == '2' and row['ui_quads'] == '120' and
                      row['ui_commands'] == '120', 'missing integrated render/UI load')
                values = [float(row[key]) for key in stages]
                check(all(math.isfinite(v) and v >= 0 for v in values), 'invalid intervals')
                check(sum(values) <= float(row['frame_ms']) + 1e-6 and
                      float(row['cpu_ms']) <= float(row['frame_ms']) <= float(row['cycle_ms']),
                      'timing containment')
                check(row['fixture_alloc_live'] == '0' and
                      row['fixture_allocations'] == row['fixture_frees'], 'allocation imbalance')
                check(int(row['fixture_allocations']) == frame // 120 and
                      int(row['fixture_alloc_peak']) == (8388608 if frame >= 120 else 0),
                      'declared loading allocation missing')
            # The native runner must reject a report from a different selected workload.
            result = gate.integrated_result(report, trace, 0, 120, objects)
            native_only = {'acquired', 'presented', 'instanced_draws', 'gpu_instances',
                           'ui_draws', 'texture_uploads'}
            check(all(set(error.get('mismatches', {})) <= native_only and
                      error.get('error') in (None, 'missing completed GPU timing samples')
                      for error in result['correctness_errors']),
                  'runner rejected matching portable load geometry')
            wrong = gate.integrated_result(report, trace, 0, 120,
                                           5000 if objects == 10000 else 10000)
            check(any('summary workload' in str(error) for error in wrong['correctness_errors']),
                  'runner accepted another workload summary')
            rows[0]['objects'] = '5000' if objects == 10000 else '10000'
            with trace.open('w', newline='', encoding='utf-8') as out:
                writer = csv.DictWriter(out, fieldnames=list(rows[0]))
                writer.writeheader()
                writer.writerows(rows)
            corrupt = gate.integrated_result(report, trace, 0, 120, objects)
            check(any('objects' in error.get('mismatches', {})
                      for error in corrupt['correctness_errors']),
                  'runner accepted another workload in per-frame trace')
        # Larger historical stress still reports failure, while the selected gate stays scoped.
        cases = [(100, 0, 0), (1000, 0, 0), (5000, 0, 0), (10000, 0, 0),
                 (1000, 0, 1), (10000, 1, 0)]
        with report.open('w', newline='', encoding='utf-8') as out, \
                trace.open('w', newline='', encoding='utf-8') as frames:
            aggregate = csv.DictWriter(out, fieldnames=['objects', 'sparse', 'overlap', 'frames',
                'warmup_frames', 'frame_p95_ms', 'frame_p99_ms', 'frame_max_ms'])
            samples = csv.DictWriter(frames, fieldnames=['objects', 'sparse', 'overlap', 'frame',
                                                        'warmup', 'frame_ms'])
            aggregate.writeheader()
            samples.writeheader()
            for count, sparse, overlap in cases:
                duration = 100 if count == 10000 else 10
                identity = dict(objects=count, sparse=sparse, overlap=overlap)
                aggregate.writerow(dict(identity, frames=600, warmup_frames=4,
                    frame_p95_ms=duration, frame_p99_ms=duration, frame_max_ms=duration))
                for frame in range(604):
                    samples.writerow(dict(identity, frame=frame, warmup=int(frame < 4),
                                          frame_ms=duration))
        selected = gate.original_result(report, trace, 5000)
        check(selected['correctness_pass'] and selected['performance_pass'],
              'additional stress timing blocked selected gate')
        check(sum(not case['pass_budget'] for case in selected['cases']) == 2,
              'historical stress failures were hidden')
        check(not gate.original_result(report, trace)['performance_pass'],
              'default historical 10k gate weakened')
        text = trace.read_text(encoding='utf-8')
        trace.write_text(text.replace('10000,', '10001,', 1), encoding='utf-8')
        check(not gate.original_result(report, trace, 5000)['correctness_pass'],
              'additional stress correctness failure was waived')
        for option in [['--integrated'], ['--soak-seconds', '1'], ['--reload-every', '0'],
                       ['--integrated', '--soak-seconds', '14401'],
                       ['--objects', '5000'], ['--objects', '0'], ['--objects', '5001'],
                       ['--objects', '5000oops'], ['--objects', '-1000'], ['--objects']]:
            if option[0] == '--objects' and option != ['--objects', '5000']:
                option = ['--integrated', '--frame-trace', str(trace)] + option
            failed = subprocess.run([str(args.benchmark.resolve()), '--report', str(report)] +
                                    option, capture_output=True, timeout=10)
            check(failed.returncode != 0, f'invalid invocation accepted: {option}')
    print('Integrated 1k/5k/10k updates, independent queries/culling, UI/VFX, reload and trace pass')


if __name__ == '__main__':
    main()
