"""Retain three fixed-camera native courtyard runs; software results are never hardware targets."""
import argparse
import json
import os
from pathlib import Path
import platform
import shutil
import subprocess

from LinuxVirtualDisplaySmoke import start_xvfb, unavailable


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('executable', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    xvfb = shutil.which('Xvfb')
    if not xvfb:
        return unavailable('Xvfb is not installed')
    args.output.mkdir(parents=True, exist_ok=True)
    executable = args.executable.resolve()
    server, display = start_xvfb(xvfb, '1280x720x24')
    try:
        if display is None:
            return unavailable('Xvfb did not start')
        environment = os.environ.copy()
        environment['DISPLAY'] = display
        reports = []
        for repeat in range(3):
            target = (args.output / f'run-{repeat + 1}.json').resolve()
            command = [str(executable), '--scene=courtyard', '--backend=vulkan', '--vsync=off',
                       '--clean-view', '--frames=360', '--no-reload', '--gameplay-module=static',
                       f'--report={target}']
            result = subprocess.run(command, env=environment, capture_output=True, text=True,
                                    timeout=120)
            if result.returncode != 0:
                raise RuntimeError(result.stderr)
            report = json.loads(target.read_text())
            native, metrics = report['windowed_evidence'], report['performance']
            assert report['status'] == 'PASS' and native['backend'] == 'vulkan'
            assert not native['backend_fallback'] and native['surface_presents'] == 360
            assert native['overlay_frames'] == 0 and report['runtime_rooms']['courtyard']['screenshot_mode']
            assert report['render_settings']['present_mode'] == 'immediate'
            assert metrics['sample_count'] == 300 and metrics['status'] == 'MEASURED'
            assert 0 < metrics['p95_frame_ms'] <= metrics['p99_frame_ms']
            assert metrics['average_process_cpu_ms'] is not None
            assert metrics['gpu_timing_ms'] is None and metrics['peak_resident_bytes'] > 0
            reports.append({'performance': metrics, 'build': report['build'], 'native': native,
                            'render_settings': report['render_settings'],
                            'shading': report['runtime_rooms']['courtyard'].get('shading', 'lambert'),
                            'scene_color_format': report['runtime_rooms']['courtyard'].get('scene_color_format', 'RGBA8'),
                            'exposure': report['runtime_rooms']['courtyard'].get('exposure', 1),
                            'shadows_enabled': report['runtime_rooms']['courtyard'].get('shadows_enabled', False),
                            'stylized_enabled': report['runtime_rooms']['courtyard'].get('stylized_enabled', False),
                            'bloom_enabled': report['runtime_rooms']['courtyard'].get('bloom_enabled', False),
                            'hero_asset_loaded': report['runtime_rooms']['courtyard'].get('hero_asset_loaded', False),
                            'shadow_bias': report['runtime_rooms']['courtyard'].get('shadow_bias', 0)})
        assert all(r['shading'] == reports[0]['shading'] and
                   r['render_settings'] == reports[0]['render_settings'] for r in reports)
        cpu = next((line.split(':', 1)[1].strip() for line in Path('/proc/cpuinfo').read_text().splitlines()
                    if line.startswith('model name')), 'unknown')
        summary = {'schema': 'nexora.showcase.courtyard-baseline.v1', 'resolution': [1280, 720],
                   'quality': reports[0]['render_settings']['quality'],
                   'shading': reports[0]['shading'], 'scene_color_format': reports[0]['scene_color_format'],
                   'exposure': reports[0]['exposure'], 'camera': 'fixed wide / shot 0',
                   'cpu': cpu, 'host': platform.platform(), 'vsync_requested': False,
                   'refresh_rate_hz': None, 'driver_identity': 'retained VK_ICD_FILENAMES selection',
                   'software_rasterizer': reports[0]['native']['software_rasterizer'],
                   'hardware_budget_accepted': False,
                   'scope': 'Linux Xvfb native baseline; does not establish GTX 960 performance',
                   'runs': reports}
        (args.output / 'baseline.json').write_text(json.dumps(summary, indent=2) + '\n')
        print(json.dumps({'runs': [r['performance']['average_fps'] for r in reports],
                          'software_rasterizer': summary['software_rasterizer']}))
        return 0
    finally:
        server.terminate()
        server.wait(timeout=3)


if __name__ == '__main__':
    raise SystemExit(main())
