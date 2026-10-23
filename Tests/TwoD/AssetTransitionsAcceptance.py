#!/usr/bin/env python3
"""A separately compiled Zig module, >64 MiB package and real bounded Player IO."""
import argparse
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('export_game', ROOT / 'Tools/TwoD/ExportGame.py')
exporter = importlib.util.module_from_spec(spec)
spec.loader.exec_module(exporter)
parser = argparse.ArgumentParser()
parser.add_argument('--app', type=Path, required=True)
parser.add_argument('--module', type=Path, required=True)
parser.add_argument('--fixture', type=Path, required=True)
parser.add_argument('--shipping', action='store_true')
args = parser.parse_args()
with tempfile.TemporaryDirectory(prefix='nexora-asset-transitions-') as temporary:
    root = Path(temporary)
    subprocess.run([str(args.fixture), '--fixture', str(root)], check=True, timeout=60)
    shutil.copy2(args.module, root / args.module.name)
    assets = []
    for index in range(5):
        for kind in ('atlas', 'clip'):
            assets.append(dict(kind=kind, path=f'{kind}{index}.nxcook', preload=False))
    assets.append(dict(kind='blob', path='raw.bin'))
    capacity = dict(active_assets=32, active_bytes=32*1024**2, live_bytes=48*1024**2,
                    staging_bytes=20*1024**2, pool_instances=2400)
    config = dict(schema=1, id='com.nexora.assets', title='Zig asset transitions', scene='Main',
                  module=args.module.name, audio='off', assets=assets, capacity=capacity,
                  display=dict(width=1920, height=1080, mode='windowed', resizable=True))
    descriptor = root / 'game.json'
    descriptor.write_text(json.dumps(config), encoding='utf-8')
    output = root / 'package'
    exporter.export(descriptor, args.app, output, allow_development=not args.shipping)
    assert sum(p.stat().st_size for p in (output / 'assets').iterdir()) > 80*1024**2
    assert 'capacity 32 33554432 50331648 20971520 2400' in (output / 'game.nx2game').read_text()
    assert json.loads((output / 'inventory.json').read_text())['development_test_copy'] == (not args.shipping)
    env = os.environ.copy()
    if sys.platform.startswith('linux'):
        env['LD_LIBRARY_PATH'] = str(output) + os.pathsep + env.get('LD_LIBRARY_PATH', '')
    result = subprocess.run([str(output / args.app.name), '--run-2d-project',
                             str(output / 'game.nx2game'), '--frames', '12000'],
                            capture_output=True, text=True, encoding='utf-8', env=env, timeout=120)
    assert result.returncode == 0, (result.stdout, result.stderr)
    assert 'SP-03 PASS: 15 transitions, 1800 pooled instances, 30 animation events' in result.stderr, result.stderr
    report = json.loads(result.stdout)
    assert report['allocations_balanced'] and report['pool_capacity'] == 2400
    assert report['asset_active_bytes'] == report['asset_retired_bytes'] == report['asset_staging_bytes'] == 0
    assert 16*1024**2 < report['asset_peak_content_bytes'] < 17*1024**2
    cooked_file = output / 'assets/0000.nxcook'
    original = cooked_file.read_bytes()
    cooked_file.write_bytes(original[:-1] + bytes([original[-1] ^ 1]))
    corrupt = subprocess.run([str(output / args.app.name), '--run-2d-project',
                              str(output / 'game.nx2game'), '--frames', '12000'],
                             capture_output=True, text=True, encoding='utf-8', env=env, timeout=30)
    assert corrupt.returncode != 0 and 'SP-03 PASS' not in corrupt.stderr
    assert not corrupt.stdout, 'corrupt deferred content published a success report'
    cooked_file.write_bytes(original)
    # Rejected settings must not publish an output, including explicit null/bool/unknown fields.
    for index, bad in enumerate([None, dict(capacity, active_assets=True), dict(capacity, live_bytes=1),
                                  dict(capacity, pool_instances=100001), dict(capacity, unknown=1)]):
        descriptor.write_text(json.dumps(dict(config, capacity=bad)), encoding='utf-8')
        rejected = root / f'rejected-{index}'
        try:
            exporter.export(descriptor, args.app, rejected, allow_development=not args.shipping)
        except ValueError:
            pass
        else:
            raise AssertionError('invalid capacity admitted')
        assert not rejected.exists()
print('PASS: >80 MiB package, asynchronous local IO, 15 Zig transitions, 1800 instances, event/byte baseline')
