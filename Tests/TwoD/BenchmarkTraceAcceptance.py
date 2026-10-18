"""Verify the public trace artifact against the unchanged aggregate/workload contract."""
import argparse
import csv
import math
import pathlib
import subprocess
import tempfile


def check(value, message):
    if not value:
        raise RuntimeError(message)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--benchmark', type=pathlib.Path, required=True)
    args = parser.parse_args()
    benchmark = args.benchmark.resolve()
    with tempfile.TemporaryDirectory(prefix='nexora-benchmark-trace-') as directory:
        work = pathlib.Path(directory)
        report, trace = work / 'report.csv', work / 'frames.csv'
        command = [str(benchmark), '--frames', '8', '--report', str(report)]
        subprocess.run(command + ['--frame-trace', str(trace)], check=True, timeout=45)
        aggregate = list(csv.DictReader(report.open(encoding='utf-8')))
        samples = list(csv.DictReader(trace.open(encoding='utf-8')))
        cases = [(100, 0, 0), (1000, 0, 0), (5000, 0, 0), (10000, 0, 0),
                 (1000, 0, 1), (10000, 1, 0)]
        check(len(aggregate) == 6 and len(samples) == 72, 'trace omitted workload/warm-up frames')
        stages = ['reload_ms', 'begin_frame_ms', 'populate_ms', 'index_ms', 'query_ms',
                  'extract_ms', 'draw_ms', 'end_frame_ms']
        for case_index, (objects, sparse, overlap) in enumerate(cases):
            row = aggregate[case_index]
            case = samples[case_index * 12:(case_index + 1) * 12]
            for frame, sample in enumerate(case):
                check((int(sample['objects']), int(sample['sparse']), int(sample['overlap'])) ==
                      (objects, sparse, overlap), 'trace case identity/order changed')
                check(int(sample['frame']) == frame and int(sample['warmup']) == (frame < 4)
                      and sample['native'] == '0', 'trace frame/warm-up identity changed')
                values = [float(sample[k]) for k in stages]
                check(all(math.isfinite(v) and v >= 0 for v in values), 'invalid stage interval')
                check(math.isclose(sum(values), float(sample['frame_ms']), abs_tol=1e-6),
                      'stage partition does not match the same full frame')
                check(0 <= float(sample['cpu_ms']) <= float(sample['frame_ms']),
                      'CPU interval is not contained in its full frame')
            measured = case[4:]
            for prefix, field in [('frame', 'frame_ms'), ('cpu', 'cpu_ms')]:
                values = sorted(float(s[field]) for s in measured)
                for suffix, value in [('p95_ms', values[math.ceil(.95 * len(values)) - 1]),
                                      ('p99_ms', values[math.ceil(.99 * len(values)) - 1]),
                                      ('max_ms', values[-1])]:
                    check(math.isclose(float(row[f'{prefix}_{suffix}']), value,
                                       rel_tol=1e-5, abs_tol=1e-5), 'trace/aggregate disagree')
        plain = work / 'plain.csv'
        subprocess.run([str(benchmark), '--frames', '4', '--report', str(plain)],
                       check=True, timeout=45)
        plain_rows = list(csv.DictReader(plain.open(encoding='utf-8')))
        check(len(plain_rows) == 6 and plain_rows[0].keys() == aggregate[0].keys(),
              'optional trace changed ordinary report schema')
        rejected = subprocess.run(command + ['--frame-trace', str(report)],
                                  capture_output=True, text=True, timeout=10)
        check(rejected.returncode != 0 and 'frame trace must differ' in rejected.stderr,
              'trace/report alias did not reject')
        rejected = subprocess.run(command + ['--frame-trace', str(work / 'missing' / 'trace.csv')],
                                  capture_output=True, text=True, timeout=10)
        check(rejected.returncode != 0 and 'frame trace output' in rejected.stderr,
              'invalid trace output silently succeeded')
    print('All six benchmark cases, warm-up, same-frame partitions and output failures passed')


if __name__ == '__main__':
    main()
