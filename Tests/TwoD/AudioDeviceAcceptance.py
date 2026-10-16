"""Exercise the actual ALSA adapter without routing a tone to physical speakers."""
import argparse
import json
import os
from pathlib import Path
import subprocess


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--probe', type=Path, required=True)
    parser.add_argument('--evidence-dir', type=Path, required=True)
    args = parser.parse_args()
    root = args.evidence_dir.resolve()
    root.mkdir(parents=True, exist_ok=True)
    reports = {}
    for name, config, extra in (
        ('null', 'pcm.!default { type null }\n', []),
        ('missing', 'pcm.!default { type hw card "NexoraMissingDevice" }\n',
         ['--expect-unavailable']),
    ):
        path = root / f'{name}.conf'
        path.write_text(config, encoding='utf-8')
        env = dict(os.environ, ALSA_CONFIG_PATH=str(path))
        result = subprocess.run([str(args.probe.resolve()), '--frames', '8193', *extra],
                                env=env, capture_output=True, text=True, timeout=10)
        (root / f'{name}.stdout').write_text(result.stdout, encoding='utf-8')
        (root / f'{name}.stderr').write_text(result.stderr, encoding='utf-8')
        if result.returncode:
            raise RuntimeError(f'{name}: probe exited {result.returncode}: {result.stderr}')
        report = json.loads(result.stdout)
        assert report['device_output'] == ('PASS' if name == 'null' else 'UNAVAILABLE')
        assert report['audible_acceptance'] == 'NOT VERIFIED'
        if name == 'null':
            assert report['submitted_frames'] == 8194 and report['reopened'] is True
        reports[name] = report
    # Misconfiguration is never silently accepted as a successful device run.
    env = dict(os.environ, ALSA_CONFIG_PATH=str(root / 'missing.conf'))
    result = subprocess.run([str(args.probe.resolve()), '--frames', '1'], env=env,
                            capture_output=True, text=True, timeout=10)
    assert result.returncode != 0 and 'PASS' not in result.stdout
    reports['missing_requires_failure'] = True
    reports['scope'] = 'ALSA null device lifecycle; physical audio and Windows NOT RUN'
    (root / 'acceptance.json').write_text(json.dumps(reports, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(reports))


if __name__ == '__main__':
    main()
