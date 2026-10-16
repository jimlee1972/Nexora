#!/usr/bin/env python3
"""Verify one packaged 2D host's cooked assets, UI, audio submission and durable saves."""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('product_stage', ROOT / 'Tools/TwoD/StageProductValidation.py')
stager = importlib.util.module_from_spec(spec)
spec.loader.exec_module(stager)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--app', type=Path, required=True)
    parser.add_argument('--compiler', type=Path, required=True)
    parser.add_argument('--evidence-dir', type=Path, required=True)
    parser.add_argument('--shipping', action='store_true')
    parser.add_argument('--native', action='store_true')
    args = parser.parse_args()
    if args.native and (not shutil.which('Xvfb') or not shutil.which('xdotool')):
        sys.path.insert(0, str(ROOT / 'Tests/Showcase'))
        from LinuxVirtualDisplaySmoke import unavailable
        return unavailable('native product acceptance requires Xvfb and xdotool')
    root = args.evidence_dir.resolve()
    root.mkdir(parents=True, exist_ok=True)
    package = Path(tempfile.mkdtemp(prefix='package-', dir=root))
    manifest = stager.stage(args.app, args.compiler, package, allow_development=not args.shipping)
    app = package / manifest['runtime']
    env = os.environ.copy()
    env.pop('LD_LIBRARY_PATH', None)
    env.pop('DYLD_LIBRARY_PATH', None)
    audio = []
    if sys.platform.startswith('linux'):
        config = package / 'null.conf'
        config.write_text('pcm.!default { type null }\n', encoding='utf-8')
        env['ALSA_CONFIG_PATH'] = str(config)
        audio = ['--audio']
    saves = package / 'saves'
    results = {}

    def run(name, extra=(), expected=True):
        report = package / f'{name}.json'
        command = [str(app), '--validation-package', str(package), '--save-directory', str(saves),
                   '--frames', '60', '--report', str(report), *audio, *extra]
        process = subprocess.run(command, env=env, cwd=package, capture_output=True,
                                 text=True, timeout=30)
        (package / f'{name}.log').write_text(process.stdout + process.stderr, encoding='utf-8')
        if expected:
            if process.returncode:
                raise RuntimeError(f'{name} failed: {process.stderr}')
            value = json.loads(report.read_text())
            assert value['status'] == 'PASS' and value['validation_package'] is True
            assert value['ui_frames'] == 60 and value['save_reopened'] is True
            assert value['cooked_reloads'] == 1 and value['allocations_balanced'] is True
            assert value['audio_submitted_frames'] == (48000 if audio else 0)
            assert value['audible_acceptance'] == 'NOT VERIFIED'
            results[name] = value
            return value
        assert process.returncode != 0 and not report.exists(), f'{name} admitted invalid input'
        results[name] = {'rejected': True, 'exit_code': process.returncode}

    first = run('first')
    second = run('resume')
    assert first['resumed'] is False and first['initial_x'] == 0 and first['entity_x'] == 1
    assert second['resumed'] is True and second['initial_x'] == 1 and second['entity_x'] == 2
    atlas = package / 'assets/atlas.nxcook'
    original = atlas.read_bytes()
    broken = bytearray(original)
    broken[-1] ^= 1
    atlas.write_bytes(broken)
    run('corrupt-cook', expected=False)
    atlas.write_bytes(original)
    clip = package / 'assets/clip.nxcook'
    original_clip = clip.read_bytes()
    clip.write_bytes(original)
    run('wrong-cook-type', expected=False)
    clip.write_bytes(original_clip)
    tone = package / 'assets/tone.wav'
    tone.rename(tone.with_suffix('.hidden'))
    run('missing-wave', expected=False)
    tone.with_suffix('.hidden').rename(tone)
    slot = saves / 'position.nxsav'
    good_save = slot.read_bytes()
    corrupt = bytearray(good_save)
    corrupt[-1] ^= 1
    slot.write_bytes(corrupt)
    run('corrupt-save', expected=False)
    assert slot.read_bytes() == corrupt, 'corrupt save was silently overwritten'
    slot.write_bytes(good_save)
    for name, expected in manifest['files'].items():
        assert hashlib.sha256((package / name).read_bytes()).hexdigest() == expected
    if args.native:
        results['native'] = native(app, args.compiler, package, env, audio)
    results['scope'] = ('Monolithic Shipping' if args.shipping else 'Development modular test copy')
    results['physical_windows'] = 'NOT RUN'
    (root / 'acceptance.json').write_text(json.dumps(results, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(results))


def native(app, compiler, package, base_env, audio):
    # Reuse existing native screenshot/display helpers, not a second graphics test backend.
    sys.path.insert(0, str(ROOT / 'Tests/Showcase'))
    from LinuxVirtualDisplaySmoke import start_xvfb
    from LinuxShowcaseInteraction import screenshot
    import time
    xvfb, xdotool = shutil.which('Xvfb'), shutil.which('xdotool')
    assert xvfb and xdotool, 'native product acceptance requires Xvfb and xdotool'
    server, display = start_xvfb(xvfb, '1024x768x24')
    process = None
    old_display = os.environ.get('DISPLAY')
    try:
        assert display, 'native product display did not start'
        env = dict(base_env, DISPLAY=display)
        # Only the software Vulkan driver needs the task-local library path in cloud.
        if 'LD_LIBRARY_PATH' in os.environ:
            env['LD_LIBRARY_PATH'] = os.environ['LD_LIBRARY_PATH']
        os.environ['DISPLAY'] = display
        report = package / 'native.json'
        process = subprocess.Popen([str(app), '--validation-package', str(package),
                                   '--save-directory', str(package / 'native-saves'),
                                   '--interactive', '--frames', '900', '--require-movement',
                                   '--report', str(report), *audio], env=env, cwd=package,
                                   stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        window = subprocess.check_output([xdotool, 'search', '--sync', '--onlyvisible',
                                          '--name', '^Nexora Zig 2D$'], env=env, text=True,
                                         timeout=8).splitlines()[0]

        def send(*arguments):
            subprocess.run([xdotool, *arguments], env=env, check=True, capture_output=True, timeout=3)

        send('windowfocus', '--sync', window)
        send('keydown', '--window', window, 'd')
        time.sleep(.15)
        capture = screenshot(int(window), 640, 480, package / 'native-ui.png')
        def bright(x, y):
            offset = y * (640 * 3 + 1) + 1 + x * 3
            return sum(capture[offset:offset + 3]) / 3
        assert bright(12, 12) < 150 and bright(15, 12) > 230
        # The UI font uses linear sampling; the one-pixel stroke edge blends with the panel.
        assert bright(12, 15) > bright(15, 15) + 40 and bright(15, 15) < 150, \
            'native supplied 2 glyph mismatch'
        for x in (200, 280):  # SAVE then OPEN, each with distinct native press/release frames.
            send('mousemove', '--window', window, str(x), '20')
            send('mousedown', '1')
            time.sleep(.08)
            send('mouseup', '1')
            time.sleep(.08)
        send('windowsize', window, '800', '600')
        send('keyup', 'd')
        stdout, stderr = process.communicate(timeout=35)
        (package / 'native.log').write_text(stdout + stderr, encoding='utf-8')
        assert process.returncode == 0, stderr
        value = json.loads(report.read_text())
        assert value['native_frames'] == 900 and value['ui_frames'] == 900
        assert value['native_ui_draws'] >= 900 * 4 and value['native_ui_texture_uploads'] >= 2
        assert value['resize_generations'] >= 1 and value['entity_x'] > 0
        assert value['final_width'] == 800 and value['final_height'] == 600
        assert value['ui_saves'] > 0 and value['ui_opens'] > 0 and value['save_reopened']
        assert value['audio_submitted_frames'] > 0 and value['cooked_reloads'] == 1
        assert value['audible_acceptance'] == 'NOT VERIFIED' and value['physical_windows'] == 'NOT RUN'
        return value
    finally:
        if process and process.poll() is None:
            process.kill()
            process.communicate(timeout=3)
        server.terminate()
        server.communicate(timeout=3)
        if old_display is None:
            os.environ.pop('DISPLAY', None)
        else:
            os.environ['DISPLAY'] = old_display


if __name__ == '__main__':
    raise SystemExit(main())
