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
        command = [str(args.benchmark.resolve()), '--integrated', '--frames', '128',
                   '--reload-every', '120', '--report', str(report), '--frame-trace', str(trace)]
        subprocess.run(command, check=True, timeout=45)
        summary = list(csv.DictReader(report.open(encoding='utf-8')))
        rows = list(csv.DictReader(trace.open(encoding='utf-8')))
        check(len(summary) == 1 and len(rows) == 132, 'omitted frames/summary')
        header = summary[0]
        check((header['objects'], header['vfx'], header['frames'], header['warmup_frames']) ==
              ('10000', '256', '128', '4'), 'workload count changed')
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
                       for i in range(10000)]
            query = sum(x*x + y*y <= 2.05**2 for x, y in centers)
            sweep = sum((x - min(1, max(-1, x)))**2 + y*y <= .1**2
                        for x, y in centers)
            check(int(row['query_hits']) == query and int(row['sweep_hits']) == sweep,
                  'query/sweep differs from independent all-object oracle')
            check(gate.query_oracle(sparse, frame % 120 + 1) == (query, sweep),
                  'acceptance runner failed to account for moving query boundaries')
            # Orthographic bounds: x in [-16,16], y in [-12,12], sprite half size .075.
            visible = sum(-16.075 <= x <= 16.075 and -12.075 <= y <= 12.075
                          for x, y in centers) + 256
            check(int(row['visible']) == visible and int(row['culled']) == 10256 - visible,
                  'CPU culling differs from independent camera oracle')
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
        for option in [['--integrated'], ['--soak-seconds', '1'], ['--reload-every', '0'],
                       ['--integrated', '--soak-seconds', '14401']]:
            failed = subprocess.run([str(args.benchmark.resolve()), '--report', str(report)] +
                                    option, capture_output=True, timeout=10)
            check(failed.returncode != 0, f'invalid invocation accepted: {option}')
    print('Integrated 10k updates, independent queries/culling, UI/VFX, reload and trace pass')


if __name__ == '__main__':
    main()
