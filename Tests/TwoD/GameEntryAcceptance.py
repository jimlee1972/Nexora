"""Exercise the engine-built game entry without a project argument from a foreign CWD."""
import argparse
import importlib.util
import json
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('game_export', ROOT / 'Tools/TwoD/ExportGame.py')
exporter = importlib.util.module_from_spec(spec)
spec.loader.exec_module(exporter)

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--app', type=Path, required=True)
    parser.add_argument('--module', type=Path, required=True)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix='nexora-game-entry-') as temporary:
        root = Path(temporary)
        source = root / 'source'
        source.mkdir()
        shutil.copy2(args.module, source / args.module.name)
        descriptor = source / 'game.json'
        descriptor.write_text(json.dumps(dict(schema=1, id='com.nexora.entry', title='Entry',
            scene='Main', module=args.module.name, audio='off', assets=[])))
        package = root / 'game with spaces 遊戲'
        exporter.export(descriptor, args.app, package, allow_development=True)
        binary = package / args.app.name
        inventory = exporter.checksum(package / 'game.nx2game')
        for i in range(2):
            report = root / f'report-{i}.json'
            result = subprocess.run([str(binary), '--frames', '60', '--report', str(report),
                '--save-directory', str(root / 'saves')], cwd=root, capture_output=True, timeout=30)
            assert result.returncode == 0, (result.stdout, result.stderr)
            observed = json.loads(report.read_text())
            assert observed['status'] == 'PASS' and observed['project_mode'] and observed['allocations_balanced']
        assert exporter.checksum(package / 'game.nx2game') == inventory
        (package / 'game.nx2game').rename(package / 'saved.nx2game')
        rejected = root / 'missing-report.json'
        result = subprocess.run([str(binary), '--frames', '1', '--report', str(rejected)],
            cwd=root, capture_output=True, timeout=30)
        assert result.returncode != 0 and not rejected.exists()
    print('Named game entry: implicit project, foreign CWD, Unicode, restart and missing-project rejection passed')

if __name__ == '__main__':
    main()
