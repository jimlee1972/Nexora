"""Real ALSA owner isolation and repeated failed/successful open lifetimes."""
import argparse
import json
import os
from pathlib import Path
import subprocess


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--test', type=Path, required=True)
    parser.add_argument('--evidence-dir', type=Path, required=True)
    args = parser.parse_args()
    root = args.evidence_dir.resolve()
    root.mkdir(parents=True, exist_ok=True)
    configs = {
        'null': ('pcm.!default { type null }\n', 'available'),
        'missing': ('pcm.!default { type hw card "NexoraMissingDevice" }\n', 'unavailable'),
        'malformed': ('pcm.!default { type\n', 'unavailable'),
        'absent-file': (None, 'unavailable'),
    }
    for name, (content, _) in configs.items():
        if content is not None:
            (root / f'{name}.conf').write_text(content, encoding='utf-8')
        else:
            (root / f'{name}.conf').unlink(missing_ok=True)
    reports = {}
    for name in (*configs, 'system'):
        for peer in (False, True):
            env = dict(os.environ)
            env.pop('ALSA_CONFIG_PATH', None)
            mode = 'system'
            if name in configs:
                env['ALSA_CONFIG_PATH'] = str(root / f'{name}.conf')
                mode = configs[name][1]
            command = [str(args.test.resolve()), mode]
            if peer:
                command.append(str(root / 'null.conf'))
            label = name + ('-peer' if peer else '')
            result = subprocess.run(command, env=env, capture_output=True, text=True, timeout=20)
            (root / f'{label}.stdout').write_text(result.stdout, encoding='utf-8')
            (root / f'{label}.stderr').write_text(result.stderr, encoding='utf-8')
            assert result.returncode == 0, f'{label}: {result.returncode}: {result.stderr}'
            report = json.loads(result.stdout)
            assert report['opens'] == 65 and report['peer'] == peer
            assert report['audible_acceptance'] == 'NOT VERIFIED'
            if mode != 'system':
                assert report['opened'] == (65 if mode == 'available' else 0)
            reports[label] = report
    (root / 'acceptance.json').write_text(json.dumps(reports, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(reports))


if __name__ == '__main__':
    main()
