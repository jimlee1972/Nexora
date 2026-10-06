#!/usr/bin/env python3
"""Measure a fixed native courtyard across quality tiers; never certify hardware acceptance."""

import argparse
import hashlib
import json
import math
import os
from pathlib import Path
import platform
import shutil
import subprocess
import sys
import time


def require(condition, message):
    if not condition:
        raise ValueError(message)


def positive(value):
    return type(value) in (int, float) and math.isfinite(value) and value > 0


def validate_report(report, backend, quality, frames):
    """Reject missing/fallback/paced/drifting evidence even under python -O."""
    require(report.get('schema') == 'nexora.zig_showcase.v1' and report.get('status') == 'PASS',
            'native Showcase run did not pass')
    require(report.get('scene') == 'courtyard' and report.get('mode') == 'interactive',
            'expected the interactive courtyard')
    require(report.get('headless_evidence', {}).get('executed') is False, 'headless evidence')
    native = report['windowed_evidence']
    require(native.get('executed') is True and native.get('backend') == backend and
            native.get('backend_fallback') is False, 'native backend mismatch or fallback')
    require(type(native.get('software_rasterizer')) is bool, 'missing rasterizer identity')
    for field in ('surface_acquires', 'surface_presents', 'native_graph_frames',
                  'native_scene_draws', 'native_offscreen_draws', 'native_scene_composites'):
        require(type(native.get(field)) is int and native[field] == frames,
                f'incomplete native frames: {field}')
    require(native.get('overlay_frames') == 0, 'diagnostics were visible')
    lifecycle = report['lifecycle']
    require(lifecycle.get('frames') == frames and
            lifecycle.get('module_unloaded_before_engine_shutdown') is True, 'lifecycle mismatch')
    settings = report['render_settings']
    require(settings.get('quality') == quality and settings.get('width') == 1280 and
            settings.get('height') == 720, 'quality or viewport mismatch')
    require(settings.get('vsync_requested') == 'off' and settings.get('present_mode') == 'immediate',
            'benchmark requires unpaced immediate presentation')
    courtyard = report['runtime_rooms']['courtyard']
    require(courtyard.get('shot') == 0 and courtyard.get('camera_mode') == 'orbit' and
            courtyard.get('animation_paused') is True and courtyard.get('animation_seconds') == 0 and
            courtyard.get('device_active') is True and courtyard.get('screenshot_mode') is True,
            'fixed camera/effect timeline mismatch')
    require(courtyard.get('hero_asset_loaded') is True and
            courtyard.get('scene_color_format') == 'RGBA16F', 'hero/HDR content missing')
    metrics = report['performance']
    require(metrics.get('schema') == 'nexora.showcase.performance.v1' and
            metrics.get('status') == 'MEASURED' and metrics.get('warmup_frames') == 60 and
            metrics.get('observed_frames') == frames and metrics.get('sample_count') == frames - 60,
            'missing warm-up or incomplete sample window')
    for field in ('average_fps', 'average_frame_ms', 'p95_frame_ms', 'p99_frame_ms'):
        require(positive(metrics.get(field)), f'invalid timing: {field}')
    require(metrics['p95_frame_ms'] <= metrics['p99_frame_ms'], 'percentile order mismatch')
    require(math.isclose(metrics['average_fps'] * metrics['average_frame_ms'], 1000, rel_tol=1e-6),
            'FPS/frame-time mismatch')
    require(metrics.get('gpu_timing_ms') is None and
            metrics.get('gpu_timing_status') == 'UNAVAILABLE: no GPU timestamps',
            'unsupported GPU timestamp claim')
    cpu = metrics.get('average_process_cpu_ms')
    require(cpu is None or (type(cpu) in (int, float) and math.isfinite(cpu) and cpu >= 0),
            'invalid process CPU time')
    memory = metrics.get('peak_resident_bytes')
    require(memory is None or positive(memory), 'invalid resident memory')
    build = report['build']
    require(build.get('configuration') == 'Shipping' and build.get('link_mode') == 'Monolithic' and
            bool(build.get('build_id')), 'benchmark requires identified Shipping/Monolithic build')
    return {'build': build, 'native': native, 'render_settings': settings,
            'courtyard': courtyard, 'performance': metrics}


def validate_repeats(runs):
    require(bool(runs), 'no benchmark runs')
    for run in runs[1:]:
        require(run['build'] == runs[0]['build'], 'mixed build versions')
        require(run['native']['software_rasterizer'] == runs[0]['native']['software_rasterizer'],
                'rasterizer changed between runs')
        same_quality = next((r for r in runs if r['render_settings']['quality'] ==
                             run['render_settings']['quality']), None)
        require(run['render_settings'] == same_quality['render_settings'] and
                run['courtyard'] == same_quality['courtyard'], 'scene/settings changed between repeats')


def markdown(summary):
    lines = ['# Native courtyard performance', '', summary['scope'], '',
             f"Build: `{summary['runs'][0]['build']['build_id']}`; backend: {summary['backend']}; "
             f"software rasterizer: {summary['software_rasterizer']}.", '',
             'Fixed wide camera, activated device, frozen animation at 0 seconds, 1280×720, VSync off.',
             'Each process discards 60 warm-up frames. CPU is process-wide; GPU timestamps are unavailable.',
             'These observations do not accept the hardware budget or physical display.', '',
             '| Quality | Repeat | FPS | Mean ms | P95 ms | P99 ms | CPU ms | Peak RSS bytes |',
             '| --- | --- | --- | --- | --- | --- | --- | --- |']
    for run in summary['runs']:
        m = run['performance']
        values = [run['render_settings']['quality'], run['repeat']] + [
            m[k] for k in ('average_fps', 'average_frame_ms', 'p95_frame_ms', 'p99_frame_ms',
                          'average_process_cpu_ms', 'peak_resident_bytes')]
        lines.append('| ' + ' | '.join('UNAVAILABLE' if v is None else
                     f'{v:.3f}' if type(v) is float else str(v) for v in values) + ' |')
    return '\n'.join(lines) + '\n'


def collect(executable, output, backend, qualities, repeats, frames, environment, timeout):
    # A failed attempt cannot inherit a previous passing matrix or process report.
    for name in ('benchmark.json', 'benchmark.md'):
        (output / name).unlink(missing_ok=True)
    executable_hash = hashlib.sha256(executable.read_bytes()).hexdigest()
    runs = []
    for quality in qualities:
        for repeat in range(1, repeats + 1):
            stem = f'{quality}-{repeat}'
            target = output / f'{stem}.json'
            target.unlink(missing_ok=True)
            command = [str(executable), '--scene=courtyard', f'--backend={backend}', '--vsync=off',
                       '--clean-view', f'--frames={frames}', '--no-reload', '--gameplay-module=static',
                       f'--report={target}', f'--quality={quality}', '--pause-animation', '--activate-device']
            started = time.monotonic()
            with (output / f'{stem}.stdout.log').open('w', encoding='utf-8') as stdout, \
                 (output / f'{stem}.stderr.log').open('w', encoding='utf-8') as stderr:
                completed = subprocess.run(command, cwd=executable.parent, env=environment,
                                           stdout=stdout, stderr=stderr, timeout=timeout)
            require(completed.returncode == 0, f'{stem} exited {completed.returncode}; see retained logs')
            run = validate_report(json.loads(target.read_text(encoding='utf-8')), backend, quality, frames)
            run.update(repeat=repeat, command=command, wall_seconds=time.monotonic() - started,
                       report_sha256=hashlib.sha256(target.read_bytes()).hexdigest())
            runs.append(run)
            validate_repeats(runs)
            require(hashlib.sha256(executable.read_bytes()).hexdigest() == executable_hash,
                    'executable changed during benchmark')
    summary = {'schema': 'nexora.showcase.quality-benchmark.v1', 'status': 'MEASURED',
               'scope': 'Sequential native fixed-scene measurements; hardware/visual acceptance remains open.',
               'backend': backend, 'host': platform.platform(), 'cpu': platform.processor() or None,
               'driver_identity': None, 'refresh_rate_hz': None,
               'executable_sha256': executable_hash, 'software_rasterizer': runs[0]['native']['software_rasterizer'],
               'hardware_budget_accepted': False, 'physical_display_verified': False, 'runs': runs}
    (output / 'benchmark.md').write_text(markdown(summary), encoding='utf-8')
    (output / 'benchmark.json').write_text(json.dumps(summary, indent=2, allow_nan=False) + '\n', encoding='utf-8')
    return summary


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('executable', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--backend', choices=('dx12', 'vulkan'), required=True)
    parser.add_argument('--quality', choices=('basic', 'standard', 'high'), action='append')
    parser.add_argument('--repeats', type=int, default=3)
    parser.add_argument('--frames', type=int, default=360)
    parser.add_argument('--timeout', type=int, default=300)
    parser.add_argument('--virtual-display', action='store_true', help='Linux Xvfb, not a physical display')
    args = parser.parse_args()
    if not 1 <= args.repeats <= 10 or not 160 <= args.frames <= 10000 or args.timeout <= 0:
        parser.error('require 1..10 repeats, 160..10000 frames (at least 100 samples), positive timeout')
    if args.backend == 'dx12' and platform.system() != 'Windows':
        parser.error('DX12 requires Windows')
    if args.virtual_display and platform.system() != 'Linux':
        parser.error('virtual-display requires Linux')
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    server = None
    try:
        environment = os.environ.copy()
        if args.virtual_display:
            sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'Tests/Showcase'))
            from LinuxVirtualDisplaySmoke import start_xvfb
            require(shutil.which('Xvfb') is not None, 'Xvfb is required')
            server, display = start_xvfb(shutil.which('Xvfb'), '1280x720x24')
            require(display is not None, 'Xvfb did not start')
            environment['DISPLAY'] = display
        summary = collect(args.executable.resolve(), output, args.backend,
                          list(dict.fromkeys(args.quality or ['basic', 'standard', 'high'])),
                          args.repeats, args.frames, environment, args.timeout)
        print(json.dumps({'status': summary['status'], 'runs': len(summary['runs']),
                          'hardware_budget_accepted': False}))
        return 0
    except (ValueError, KeyError, TypeError, OSError, subprocess.TimeoutExpired) as error:
        for name in ('benchmark.json', 'benchmark.md'):
            (output / name).unlink(missing_ok=True)
        print(f'FAIL: {error}', file=sys.stderr)
        return 1
    finally:
        if server:
            server.terminate()
            server.wait(timeout=3)


if __name__ == '__main__':
    raise SystemExit(main())
