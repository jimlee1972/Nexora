#!/usr/bin/env python3
"""Retain Linux native mixed-stress and 2D Studio evidence on an isolated display."""
import argparse
import csv
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import time
sys.path.insert(0, str(Path(__file__).parents[1]/'Showcase'))
from LinuxVirtualDisplaySmoke import start_xvfb, unavailable
from LinuxShowcaseInteraction import request_window_close, screenshot

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--benchmark', required=True)
    p.add_argument('--studio')
    p.add_argument('--compiler', required=True)
    p.add_argument('--evidence-dir', required=True, type=Path)
    args = p.parse_args()
    output = args.evidence_dir.resolve()
    output.mkdir(parents=True, exist_ok=True)
    xvfb = shutil.which('Xvfb')
    if not xvfb: return unavailable('Xvfb missing for 2D acceptance')
    xdotool = shutil.which('xdotool')
    if args.studio and not xdotool:
        return unavailable('xdotool missing for Studio close acceptance')
    server, display = start_xvfb(xvfb, '1280x900x24')
    try:
        if display is None: return unavailable('2D acceptance display unavailable')
        env = os.environ.copy()
        env['DISPLAY'] = display
        source_root = Path(__file__).parents[2]
        for kind in ('atlas', 'clip'):
            subprocess.run([sys.executable, str(source_root/'Tools/TwoD/PrepareAssets.py'),
                str(source_root/f'Tools/TwoD/Fixtures/{kind}.json'), '--compiler', args.compiler,
                '--output', str(output/f'{kind}.nx2{kind}')], env=env, check=True, timeout=10)
            subprocess.run([args.compiler, 'cook', str(output/f'{kind}.nx2{kind}'),
                str(output/f'{kind}.nxcook')], env=env, check=True, timeout=10)
        commands = [[args.benchmark, '--native', '--frames', '24', '--atlas', str(output/'atlas.nxcook'),
                     '--clip', str(output/'clip.nxcook'), '--report', str(output/'stress.csv')]]
        if args.studio:
            workspace = output/'studio-workspace'
            workspace.mkdir(exist_ok=True)
            (workspace/'scene.nx2doc').unlink(missing_ok=True)
            shutil.copy2(output/'atlas.nx2atlas', workspace/'atlas.nx2atlas')
            shutil.copy2(output/'clip.nx2clip', workspace/'clip.nx2clip')
            commands.append([args.studio, '--workspace', str(workspace), '--atlas', 'atlas.nx2atlas',
                             '--clip', 'clip.nx2clip', '--smoke', '--frames', '12'])
        for i, command in enumerate(commands):
            run = subprocess.run(command, env=env, capture_output=True, text=True, timeout=60)
            (output/f'run-{i}.log').write_text(run.stdout+run.stderr)
            if run.returncode or 'Validation Error' in run.stdout+run.stderr:
                print(run.stdout+run.stderr)
                raise RuntimeError(f'2D native acceptance failed: {run.returncode}')
        if args.studio:
            command = [args.studio, '--workspace', str(workspace), '--atlas', 'atlas.nx2atlas',
                       '--clip', 'clip.nx2clip']
            close_log = output/'studio-close.log'
            close_log.unlink(missing_ok=True)
            process = subprocess.Popen(command, env=env, stdout=subprocess.PIPE,
                                       stderr=subprocess.PIPE, text=True)
            try:
                window = subprocess.check_output([xdotool, 'search', '--sync', '--onlyvisible',
                    '--name', '^Nexora 2D Studio$'], env=env, text=True, timeout=10).splitlines()[0]
                previous_display = os.environ.get('DISPLAY')
                os.environ['DISPLAY'] = display  # The existing X11 capture helper opens this display.
                try:
                    deadline = time.monotonic() + 15
                    while True:
                        if process.poll() is not None:
                            raise RuntimeError('Studio exited before its close request')
                        captured = screenshot(int(window), 1100, 800, output/'studio-before-close.png')
                        # Initial black/white window buffers are uniform; wait for rendered UI.
                        if len(set(captured)) > 16:
                            break
                        if time.monotonic() >= deadline:
                            raise RuntimeError('Studio did not present UI before its close request')
                        time.sleep(0.1)
                finally:
                    if previous_display is None:
                        os.environ.pop('DISPLAY', None)
                    else:
                        os.environ['DISPLAY'] = previous_display
                request_window_close(int(window), display)
                stdout, stderr = process.communicate(timeout=10)
                close_log.write_text(stdout + stderr, encoding='utf-8')
                if (process.returncode != 0 or 'Validation Error' in stdout + stderr or
                        '2D Studio native preview and document lifecycle passed' not in stdout):
                    raise RuntimeError(f'Studio normal close failed ({process.returncode}): {stderr}')
                commands.append(command)
            finally:
                if process.poll() is None:
                    process.kill()
                if not close_log.exists():
                    stdout, stderr = process.communicate(timeout=3)
                    close_log.write_text(stdout + stderr, encoding='utf-8')
        rows = list(csv.DictReader((output/'stress.csv').open()))
        assert len(rows) == 6 and all(row['native']=='1' for row in rows)
        assert [int(row['objects']) for row in rows] == [100,1000,5000,10000,1000,10000]
        assert all(int(row['visible']) > 0 and int(row['sprite_batches']) == 1 for row in rows)
        assert all(float(row['cpu_p95_ms']) >= 0 and float(row['loading_ms']) >= 0 for row in rows)
        assert float(rows[-2]['covered_aabb_ratio']) > float(rows[1]['covered_aabb_ratio'])
        assert int(rows[-1]['culled']) > int(rows[-1]['visible'])
        report = {'schema':'nexora.twod.cloud.v1', 'status':'PASS', 'platform':'Linux/X11/Vulkan',
                  'display':'virtual', 'physical_minimum_pc_60fps':'NOT RUN',
                  'windows_clean_machine_shipping':'NOT RUN', 'commands':commands,
                  'mixed_workloads':rows, 'studio_executed':bool(args.studio),
                  'studio_normal_close_executed':bool(args.studio)}
        (output/'acceptance.json').write_text(json.dumps(report, indent=2)+'\n')
        print('Linux 2D native mixed stress and authoring acceptance passed')
        return 0
    finally:
        server.terminate()
        server.communicate(timeout=3)

if __name__ == '__main__':
    sys.exit(main())
