#!/usr/bin/env python3
"""Stage the independent host fixture; headless checks never accept visual/device behavior."""
import argparse
import hashlib
import importlib.util
import json
import math
import os
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import tempfile
import wave
import zlib

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('export_game', ROOT / 'Tools/TwoD/ExportGame.py')
exporter = importlib.util.module_from_spec(spec)
spec.loader.exec_module(exporter)


def corpus():
    # Covers every fixture label/status, including all digits and both rebinding modes.
    source = Path(__file__).with_name('HostModule.cpp').read_text(encoding='utf-8')
    return ''.join(sorted(set(c for c in source if 32 <= ord(c) <= 0xFFFF
                              and not 0xD800 <= ord(c) <= 0xDFFF)))


def synthetic_font(path):
    # Headless contract fixture only: identical solid glyphs are NOT visual/font evidence.
    chars = corpus()
    header = struct.pack('<8sIIffI', b'NXFONT1\0', 16, 16, 18., 22., len(chars))
    glyphs = b''.join(struct.pack('<I9f', ord(c), 10., 0., 0., 8., 12., 0., 0., 1., 1.) for c in chars)
    path.write_bytes(header + glyphs + b'\xff' * (16 * 16 * 4))


def stage(args, root):
    source = root / 'source'
    source.mkdir()
    shutil.copy2(args.module, source / args.module.name)
    font = source / 'font.nxfont'
    if args.real_font:
        shutil.copy2(args.real_font, font)
    else:
        synthetic_font(font)
    for name, frequency in [('sfx', 440), ('music', 660)]:
        with wave.open(str(source / (name + '.wav')), 'wb') as audio:
            audio.setparams((1, 2, 48000, 0, 'NONE', 'not compressed'))
            audio.writeframes(b''.join(struct.pack('<h', int(3276 * math.sin(i * 2 * math.pi * frequency / 48000)))
                                      for i in range(48000)))
    config = {'schema': 1, 'id': 'com.nexora.hostacceptance.20261017',
              'title': 'Nexora M6/M7 主機驗收', 'scene': 'Main', 'module': args.module.name,
              'audio': 'optional', 'assets': [{'kind': 'font', 'id': 1, 'path': 'font.nxfont'},
                                             {'kind': 'wave', 'id': 1, 'path': 'sfx.wav'},
                                             {'kind': 'wave', 'id': 2, 'path': 'music.wav'}]}
    descriptor = source / 'game.json'
    descriptor.write_text(json.dumps(config, ensure_ascii=False), encoding='utf-8')
    package = root / 'package'
    exporter.export(descriptor, args.app, package, allow_development=not args.shipping,
                    redist=args.redist)
    return package


def verify(args, root, package):
    saves = root / 'saves Unicode 進度'
    env = os.environ.copy()
    if sys.platform.startswith('linux'):
        # Match ProductAcceptance: service/storage tests never use host speakers/configuration.
        config = root / 'alsa-null.conf'
        config.write_text('pcm.!default { type null }\n', encoding='utf-8')
        env['ALSA_CONFIG_PATH'] = str(config)
    hashes = {p: hashlib.sha256(p.read_bytes()).hexdigest() for p in package.rglob('*') if p.is_file()}
    def run(name, expected=True):
        result = subprocess.run([str(package / args.app.name), '--run-2d-project', str(package / 'game.nx2game'),
                                 '--frames', '3', '--save-directory', str(saves)], env=env,
                                capture_output=True, timeout=30)
        (root / (name + '.stdout')).write_bytes(result.stdout)
        (root / (name + '.stderr')).write_bytes(result.stderr)
        assert (result.returncode == 0) == expected, result.stderr
        if expected:
            report = json.loads(result.stdout)
            assert report['project_mode'] and report['allocations_balanced'] and report['native_frames'] == 0
            if sys.platform.startswith('linux'):
                assert report['audio_open_attempts'] == report['audio_open_successes'] == 1, report
                assert report['audio_pump_failures'] == 0 and report['audible_acceptance'] == 'NOT VERIFIED', report
            line = next(x for x in result.stderr.decode('utf-8').splitlines() if x.startswith('host_fixture '))
            fixture = json.loads(line[len('host_fixture '):])
            assert fixture['final_save_ok'] and not fixture['buttons_seen']
            assert fixture['keyboard_movement_frames'] == fixture['gamepad_movement_frames'] == 0, fixture
            assert fixture['axes_min'] == fixture['axes_max'] == [0] * 6, fixture
            return fixture
    first = run('first')
    assert first['position'] == 0 and first['rebound'] == 0, (
        first, (root / 'first.stderr').read_text(encoding='utf-8'))
    slot = saves / 'host.nxsav'
    data = bytearray(slot.read_bytes())
    assert data[:8] == b'NXSAVE\x01\x00'
    schema, scope_size, key_size, size = struct.unpack_from('<4I', data, 8)
    assert schema == 1 and size == 12 and len(data) == 28 + scope_size + key_size + size
    offset = 24 + scope_size + key_size
    struct.pack_into('<d', data, offset, 17.25)
    data[offset + 8] = 1
    struct.pack_into('<I', data, len(data) - 4, zlib.crc32(data[:-4]))
    slot.write_bytes(data)
    second = run('resume')
    assert second['position'] == 17.25 and second['rebound'] == 1, (
        second, (root / 'resume.stderr').read_text(encoding='utf-8'))
    damaged = bytearray(slot.read_bytes())
    damaged[-1] ^= 1
    slot.write_bytes(damaged)
    run('corrupt', expected=False)
    assert slot.read_bytes() == damaged, 'corrupt prior state was overwritten'
    assert all(hashlib.sha256(p.read_bytes()).hexdigest() == h for p, h in hashes.items())
    (root / 'contracts.json').write_text(json.dumps({'status': 'PASS', 'first': first, 'resume': second,
        'corrupt_save_preserved': True, 'package_unchanged': True,
        'scope': 'headless service/storage wiring; no physical input, font appearance or audible acceptance'}, indent=2) + '\n', encoding='utf-8')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--app', type=Path, required=True)
    parser.add_argument('--module', type=Path, required=True)
    parser.add_argument('--output', type=Path)
    parser.add_argument('--real-font', type=Path)
    parser.add_argument('--redist', type=Path)
    parser.add_argument('--shipping', action='store_true')
    parser.add_argument('--stage-only', action='store_true')
    args = parser.parse_args()
    if args.stage_only and (not args.output or not args.real_font):
        parser.error('--stage-only requires --output and --real-font; synthetic glyphs are headless-only')
    args.app, args.module = args.app.resolve(), args.module.resolve()
    if args.output:
        root = args.output.resolve()
        root.mkdir(parents=True, exist_ok=False)
        package = stage(args, root)
        if not args.stage_only:
            verify(args, root, package)
        print(package)
    else:
        with tempfile.TemporaryDirectory(prefix='nexora-host-fixture-') as temporary:
            root = Path(temporary)
            package = stage(args, root)
            verify(args, root, package)
        print('Headless host-fixture storage/rebinding/strict font admission contracts pass')


if __name__ == '__main__':
    main()
