#!/usr/bin/env python3
"""Reject ambiguous backend requests before work and retain headless provenance."""
import argparse
import csv
from pathlib import Path
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--app', required=True, type=Path)
    parser.add_argument('--benchmark', required=True, type=Path)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix='nexora-backend-') as temporary:
        root = Path(temporary)
        for app in (args.app.resolve(), args.benchmark.resolve()):
            for name in ('dx12', 'vulkan', 'metal', 'invalid'):
                report = root / (app.stem + '-' + name + '.json')
                result = subprocess.run([str(app), '--backend', name, '--frames', '4',
                                         '--report', str(report)], capture_output=True, timeout=15)
                message = b'backend must be' if name == 'invalid' else b'explicit backend requires native'
                assert result.returncode != 0 and message in result.stderr, result.stderr
                assert not report.exists(), 'rejected request published a report'
        report = root / 'cpu.csv'
        result = subprocess.run([str(args.benchmark.resolve()), '--backend', 'auto', '--frames', '4',
                                 '--report', str(report)], capture_output=True, timeout=20)
        assert result.returncode == 0, result.stderr
        with report.open(encoding='utf-8', newline='') as file:
            rows = list(csv.DictReader(file))
        assert len(rows) == 6 and all(r['backend_requested'] == 'none' and r['native'] == '0' for r in rows)
    print('Backend admission and headless provenance pass')


if __name__ == '__main__':
    main()
