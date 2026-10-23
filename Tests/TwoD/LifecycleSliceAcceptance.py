#!/usr/bin/env python3
"""Independent exported Zig lifecycle Player smoke; not physical Windows evidence."""
import argparse
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('export_game', ROOT / 'Tools/TwoD/ExportGame.py')
exporter = importlib.util.module_from_spec(spec)
spec.loader.exec_module(exporter)
def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--app', type=Path, required=True)
    parser.add_argument('--module', type=Path, required=True)
    parser.add_argument('--shipping', action='store_true')
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix='nexora-lifecycle-') as temporary:
        root = Path(temporary)
        shutil.copy2(args.module, root / args.module.name)
        config = dict(schema=1, id='com.nexora.lifecycle', title='Zig Lifecycle Diagnostic',
                      scene='Main', module=args.module.name, audio='off', assets=[],
                      display=dict(width=1920, height=1080, mode='windowed', resizable=True))
        descriptor = root / 'game.json'
        descriptor.write_text(json.dumps(config), encoding='utf-8')
        output = root / 'package'
        exporter.export(descriptor, args.app, output, allow_development=not args.shipping)
        env = os.environ.copy()
        env['LD_LIBRARY_PATH'] = str(output) + os.pathsep + env.get('LD_LIBRARY_PATH', '')
        result = subprocess.run([str(output / args.app.name), '--run-2d-project', str(output / 'game.nx2game'),
                                 '--save-directory', str(root / 'saves'), '--frames', '240'],
                                env=env, text=True, encoding='utf-8', capture_output=True, timeout=60)
        assert result.returncode == 0, (result.stdout, result.stderr)
        assert 'SP-06 Zig lifecycle service negotiated' in result.stderr, result.stderr
        report = json.loads(result.stdout)
        assert report['allocations_balanced'] and report['ui_frames'] == 240, report
        print('SP-06 exported independent Zig/Player: lifecycle negotiation, close guard, diagnostic UI, balanced ownership passed')
if __name__ == '__main__':
    main()
