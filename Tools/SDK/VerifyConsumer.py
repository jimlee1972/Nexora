#!/usr/bin/env python3
"""Test the relocated SDK from a source-free Windows consumer worker."""
import argparse
import hashlib
import re
import importlib.util
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
import sys
sys.dont_write_bytecode = True
import zlib


def load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def run(*args, cwd=None):
    subprocess.run([str(a) for a in args], cwd=cwd, check=True, timeout=300)


def main():
    p = argparse.ArgumentParser()
    p.add_argument('--sdk', type=Path, required=True)
    p.add_argument('--output', type=Path, required=True)
    a = p.parse_args()
    sdk, output = a.sdk.resolve(), a.output.resolve()
    check = load('sdk_check', sdk/'Tools/SDK/SDK.py')
    manifest = check.verify(sdk)
    output.mkdir(parents=True, exist_ok=False)
    source = output/'source 陰陽 workspace'
    source.mkdir()
    (source/'game.zig').write_text('pub const Game = struct {};\ntest "script test" {}\n', encoding='utf-8')
    (source/'project.json').write_text(json.dumps(dict(schema=1, module_name='SdkFixture', script='game.zig')), encoding='utf-8')
    (source/'CMakeLists.txt').write_text('''cmake_minimum_required(VERSION 3.25)
project(SdkConsumer LANGUAGES NONE)
find_program(ZIG_EXECUTABLE zig REQUIRED)
include("${SDK}/CMake/Modules/NexoraGameProject.cmake")
nexora_add_game_module(game_module PROJECT "${CMAKE_SOURCE_DIR}/project.json" OUTPUT "${CMAKE_BINARY_DIR}/module" ZIG "${ZIG_EXECUTABLE}")
''', encoding='utf-8')
    build = output/'build'
    run('cmake', '-S', source, '-B', build, '-G', 'Ninja', '-DSDK='+str(sdk), '-DCMAKE_MAKE_PROGRAM='+str(sdk/'Tools/Ninja/ninja.exe'))
    run('cmake', '--build', build, '--target', 'game_module')
    shutil.copy2(build/'module/bin/SdkFixture.dll', source/'module.dll')
    prepare = load('prepare', sdk/'Tools/TwoD/PrepareAssets.py')
    # Explicitly procedural RGBA8 test artwork; exercise canonical preparation AND native cook.
    def chunk(kind, data):
        return struct.pack('>I', len(data))+kind+data+struct.pack('>I', zlib.crc32(kind+data))
    (source/'sheet.png').write_bytes(b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR', struct.pack('>IIBBBBB', 2,2,8,6,0,0,0))+chunk(b'IDAT', zlib.compress((b'\0'+b'\xff\0\xff\xff'*2)*2))+chunk(b'IEND',b''))
    (source/'atlas.json').write_text(json.dumps(dict(type='atlas', id=[501,502], width=16, images=[dict(path='sheet.png', grid=[1,1])])), encoding='utf-8')
    (source/'clip.json').write_text(json.dumps(dict(type='clip', id=[503,504], atlas=[501,502], frames=[dict(region=1, seconds=0.1)])), encoding='utf-8')
    for kind in ('atlas','clip'):
        run(sys.executable, sdk/'Tools/TwoD/PrepareAssets.py', source/(kind+'.json'), '--compiler', sdk/'bin/NexoraTwoDAssetCompiler.exe', '--output', source/('asset.nx2'+kind))
    (source/'game.json').write_text(json.dumps(dict(schema=1, id='com.nexora.sdk-fixture', title='SDK TEST FIXTURE', scene='Main', module='module.dll', audio='off',
        assets=[dict(kind=k, path='asset.nx2'+k) for k in ('atlas','clip')])), encoding='utf-8')
    package = output/'relocated game 陰陽'
    run(sys.executable, sdk/'Tools/TwoD/ExportGame.py', source/'game.json',
        '--runtime', sdk/'bin/NexoraGameHost.exe', '--output', package,
        '--compiler', sdk/'bin/NexoraTwoDAssetCompiler.exe', '--vc-redist', sdk/'bin',
        '--executable-name', 'SdkFixture.exe')
    for i in range(2):
        report = output/f'report-{i}.json'
        run(package/'SdkFixture.exe', '--frames', '600', '--report', report, '--save-directory', output/'saves', cwd=output)
        observed = json.loads(report.read_text(encoding='utf-8'))
        if observed['status'] != 'PASS' or not observed['allocations_balanced'] or not observed['project_mode']:
            raise ValueError('headless host regression')
    # Build the real reusable Zig diagnostic from the exact SDK bytes, not producer sources.
    (source/'game.zig').write_text("""const nx = @import("nexora");
pub const Game = struct {
    diagnostic: nx.hd2d_diagnostic.State = .{},
    pub fn start(self: *Game, ctx: *const nx.game.Context) !void {
        try ctx.activateScene("Main");
        try self.diagnostic.start(&ctx.host, .{ .automated = true });
    }
    pub fn update(self: *Game, _: *const nx.game.Context, delta: f64) !void {
        try self.diagnostic.update(delta);
    }
    pub fn stop(self: *Game, ctx: *const nx.game.Context) void {
        self.diagnostic.stop() catch |err| ctx.log("HD2D STOP FAIL {s}", .{@errorName(err)});
    }
};
test "diagnostic defaults are interactive" {
    const std = @import("std");
    try std.testing.expect(!(nx.hd2d_diagnostic.Options{}).automated);
}
""", encoding='utf-8')
    run('cmake', '--build', build, '--target', 'game_module')
    shutil.copy2(build/'module/bin/SdkFixture.dll', source/'module.dll')
    hd2d = output/'HD2D relocated 陰陽'
    run(sys.executable, sdk/'Tools/TwoD/ExportGame.py', source/'game.json',
        '--runtime', sdk/'bin/NexoraGameHost.exe', '--output', hd2d,
        '--compiler', sdk/'bin/NexoraTwoDAssetCompiler.exe', '--vc-redist', sdk/'bin',
        '--executable-name', 'SdkFixture.exe')
    inventory_path = hd2d/'inventory.json'
    inventory = json.loads(inventory_path.read_text(encoding='utf-8'))
    for name in ('Noto-CJK-OFL.txt', 'Noto-CJK-NOTICE.md'):
        target = hd2d/'licenses'/name
        target.parent.mkdir(exist_ok=True)
        shutil.copy2(sdk/'licenses'/name, target)
        inventory['files'][target.relative_to(hd2d).as_posix()] = hashlib.sha256(target.read_bytes()).hexdigest()
    inventory_path.write_text(json.dumps(inventory, indent=2)+'\n', encoding='utf-8')
    for name, digest in inventory['files'].items():
        if hashlib.sha256((hd2d/name).read_bytes()).hexdigest() != digest:
            raise ValueError('HD2D inventory mismatch')
    for native in (False, True):
        label = 'native' if native else 'fallback'
        report = output/f'hd2d-{label}.json'
        command = [str(hd2d/'SdkFixture.exe'), '--frames', '144', '--report', str(report),
                   '--save-directory', str(output/'hd2d-saves')]
        if native: command += ['--native', '--backend', 'dx12', '--gpu-timing']
        result = subprocess.run(command, cwd=output, capture_output=True, timeout=180)
        (output/f'hd2d-{label}.stdout.log').write_bytes(result.stdout)
        (output/f'hd2d-{label}.stderr.log').write_bytes(result.stderr)
        if result.returncode: raise ValueError('real SDK Zig diagnostic failed: '+label)
        observed = json.loads(report.read_text(encoding='utf-8'))
        logs = (result.stdout+result.stderr).decode('utf-8', 'replace')
        if (observed['status'] != 'PASS' or not observed['allocations_balanced'] or
            observed['asset_active_bytes'] or observed['asset_staging_bytes'] or
            'HD2D STOP PASS phase_mask=4095 reloads=2' not in logs or 'FAIL' in logs):
            raise ValueError('HD2D lifecycle/fallback regression: '+label)
        if native:
            if observed['native_frames'] != 144 or 'features=7' not in logs:
                raise ValueError('SDK did not execute native HD2D effects')
            if any(observed[k] for k in ('effects_scene_bytes','effects_shadow_bytes',
                                         'effects_bloom_bytes','effects_upload_capacity_bytes')):
                raise ValueError('effects-off resources retained')
        elif 'features=0' not in logs:
            raise ValueError('headless claimed native effects')
    # Rebuild through the same source-free SDK with an unreachable proxy. No SDK mutation,
    # source fallback, producer cache or compiler download is allowed in this consumer job.
    proxy_keys = ('HTTP_PROXY', 'HTTPS_PROXY', 'http_proxy', 'https_proxy')
    previous = {key: os.environ.get(key) for key in proxy_keys}
    try:
        for key in proxy_keys: os.environ[key] = 'http://127.0.0.1:9'
        with (source/'game.zig').open('a', encoding='utf-8') as script:
            script.write('\n// Offline independent SDK rebuild.\n')
        run('cmake', '--build', build, '--target', 'game_module')
    finally:
        for key, value in previous.items():
            if value is None: os.environ.pop(key, None)
            else: os.environ[key] = value
    if check.verify(sdk) != manifest:
        raise ValueError('consumer modified the SDK')
    (output/'acceptance.json').write_text(json.dumps(dict(status='PASS', source_commit=manifest['source_commit'],
        two_processes=True, canonical_atlas_clip=True, real_zig_hd2d=True, hd2d_reload_cycles=2, offline_rebuild=True,
        physical_windows='NOT_RUN', native_dx12='PASS'), indent=2)+'\n', encoding='utf-8')



if __name__ == '__main__':
    main()
