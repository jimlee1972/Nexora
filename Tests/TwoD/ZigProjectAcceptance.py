#!/usr/bin/env python3
"""Export and launch a real Zig DLL/SO through the general project path, using labeled fixtures."""
import argparse
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]


def load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--app', required=True, type=Path)
    parser.add_argument('--module', required=True, type=Path)
    parser.add_argument('--compiler', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--shipping', action='store_true')
    parser.add_argument('--vc-redist', type=Path)
    parser.add_argument('--vulkan-loader', type=Path)
    args = parser.parse_args()
    output = args.output.resolve()
    if output.exists():
        raise ValueError('use a fresh evidence output')
    output.mkdir(parents=True)
    source = output / 'fixture-source'
    stage = load('product_stage', ROOT / 'Tools/TwoD/StageProductValidation.py')
    exporter = load('export_game', ROOT / 'Tools/TwoD/ExportGame.py')
    stage.stage(args.app, args.compiler, source, allow_development=not args.shipping)
    shutil.copy2(args.module, source / args.module.name)
    config = {'schema': 1, 'id': 'com.nexora.zig-fixture', 'title': 'Zig project validation fixture',
              'scene': 'Zig 2D', 'module': args.module.name, 'audio': 'off', 'tile_atlas': [1, 2],
              'assets': [{'kind': 'clip', 'path': 'assets/clip.nxcook'},
                         {'kind': 'atlas', 'path': 'assets/atlas.nxcook'},
                         {'kind': 'wave', 'id': 1, 'path': 'assets/tone.wav'}]}
    descriptor = source / 'game.json'
    descriptor.write_text(json.dumps(config), encoding='utf-8')
    package = output / 'ZigGame-Shipping-Fixture'
    exporter.export(descriptor, args.app, package, allow_development=not args.shipping,
                    redist=args.vc_redist, vulkan_loader=args.vulkan_loader)
    (output / 'README-fixture.txt').write_text('Engine validation fixture, not production game art.\n'
        'Run.cmd / run.sh starts the real separate Zig module and cooked assets.\n'
        'WASD / controller moves. Close the window to exit; no permanent scene state is invented.\n'
        'No physical controller, sound, DPI or minimum-PC acceptance is claimed.\n', encoding='ascii')
    environment = os.environ.copy()
    if sys.platform.startswith('linux'):
        if args.shipping:
            environment.pop('LD_LIBRARY_PATH', None)
        else:
            environment['LD_LIBRARY_PATH'] = str(package)
    command = [str(package / args.app.name), '--run-2d-project', str(package / 'game.nx2game'),
               '--frames', '60', '--save-directory', str(output / 'saves')]
    run = subprocess.run(command, env=environment, capture_output=True, text=True)
    (output / 'stdout.log').write_text(run.stdout, encoding='utf-8')
    (output / 'stderr.log').write_text(run.stderr, encoding='utf-8')
    if run.returncode:
        raise RuntimeError(run.stderr)
    report = json.loads(run.stdout)
    assert report['project_mode'] and report['visible'] >= 32 and not report['reloaded']
    assert report['allocations_balanced'] and not report['validation_package']
    cook = next((package / 'assets').glob('*.nxcook'))
    original = cook.read_bytes()
    corrupt = bytearray(original)
    corrupt[-1] ^= 1
    cook.write_bytes(corrupt)
    rejected = subprocess.run(command, env=environment, capture_output=True, text=True)
    cook.write_bytes(original)
    assert rejected.returncode != 0 and not rejected.stdout, 'corrupt project cook admitted'
    (output / 'acceptance.json').write_text(json.dumps({'status': 'PASS', 'shipping': args.shipping,
        'real_zig_project': True, 'fixed_sample_host_mode': False, 'corruption_rejected': True,
        'physical_windows': 'NOT RUN', 'audible_acceptance': 'NOT VERIFIED',
        'report': report}, indent=2) + '\n', encoding='utf-8')
    print('Real Zig project export, dependency closure and corruption rejection passed')


if __name__ == '__main__':
    main()
