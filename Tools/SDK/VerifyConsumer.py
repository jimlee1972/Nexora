#!/usr/bin/env python3
"""Test the relocated SDK from a source-free Windows consumer worker."""
import argparse
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
    export = load('game_export', sdk/'Tools/TwoD/ExportGame.py')
    package = output/'relocated game 陰陽'
    export.export(source/'game.json', sdk/'bin/NexoraGameHost.exe', package, sdk/'bin/NexoraTwoDAssetCompiler.exe', redist=sdk/'bin', executable_name='SdkFixture.exe')
    for i in range(2):
        report = output/f'report-{i}.json'
        run(package/'SdkFixture.exe', '--frames', '600', '--report', report, '--save-directory', output/'saves', cwd=output)
        observed = json.loads(report.read_text(encoding='utf-8'))
        if observed['status'] != 'PASS' or not observed['allocations_balanced'] or not observed['project_mode']:
            raise ValueError('headless host regression')
    if check.verify(sdk) != manifest:
        raise ValueError('consumer modified the SDK')
    (output/'acceptance.json').write_text(json.dumps(dict(status='PASS', source_commit=manifest['source_commit'],
        two_processes=True, canonical_atlas_clip=True, physical_windows='NOT_RUN', native_dx12='NOT_RUN'), indent=2)+'\n', encoding='utf-8')


if __name__ == '__main__':
    main()
