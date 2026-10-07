#!/usr/bin/env python3
"""Benchmark evidence policy tests using a retained real native report as the baseline."""

import copy
import json
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch

from BenchmarkShowcase import collect, markdown, validate_report, validate_repeats

ROOT = Path(__file__).resolve().parents[2]
BASELINE = ROOT / 'Apps/Showcase/evidence/VIS-M6-Linux-Release-2026-10-05/benchmark/standard/run-1.json'


class BenchmarkPolicyTests(unittest.TestCase):
    def setUp(self):
        self.report = json.loads(BASELINE.read_text(encoding='utf-8'))

    def validate(self, report):
        return validate_report(report, 'vulkan', 'standard', 360)

    def test_retained_native_report_and_unavailable_host_metrics(self):
        self.validate(self.report)
        self.report['performance']['average_process_cpu_ms'] = None
        self.report['performance']['peak_resident_bytes'] = None
        self.validate(self.report)

    def test_reject_invalid_evidence(self):
        changes = [
            (('status',), 'FAIL'), (('scene',), 'hub'), (('mode',), 'tour'),
            (('headless_evidence', 'executed'), True),
            (('windowed_evidence', 'backend'), 'dx12'),
            (('windowed_evidence', 'backend_fallback'), True),
            (('windowed_evidence', 'surface_presents'), 359),
            (('windowed_evidence', 'native_scene_composites'), 0),
            (('windowed_evidence', 'overlay_frames'), 1),
            (('windowed_evidence', 'software_rasterizer'), None),
            (('render_settings', 'quality'), 'basic'), (('render_settings', 'width'), 960),
            (('render_settings', 'present_mode'), 'vsync'),
            (('runtime_rooms', 'courtyard', 'animation_seconds'), 1),
            (('runtime_rooms', 'courtyard', 'device_active'), False),
            (('runtime_rooms', 'courtyard', 'hero_asset_loaded'), False),
            (('performance', 'sample_count'), 299), (('performance', 'warmup_frames'), 0),
            (('performance', 'gpu_timing_ms'), 1.2),
            (('performance', 'average_frame_ms'), float('nan')),
            (('performance', 'p99_frame_ms'), float('inf')),
            (('performance', 'average_fps'), True), (('performance', 'average_fps'), 60),
            (('performance', 'p95_frame_ms'), 100),
            (('performance', 'average_process_cpu_ms'), -1),
            (('performance', 'peak_resident_bytes'), 0),
            (('build', 'configuration'), 'Development'), (('build', 'build_id'), ''),
        ]
        for path, value in changes:
            with self.subTest(path=path, value=value):
                report = copy.deepcopy(self.report)
                target = report
                for key in path[:-1]:
                    target = target[key]
                target[path[-1]] = value
                with self.assertRaises(ValueError):
                    self.validate(report)

    def test_reject_mixed_build_and_same_quality_scene_drift(self):
        first = self.validate(self.report)
        for path, value in [(('build', 'build_id'), 'another-version'),
                            (('native', 'software_rasterizer'), False),
                            (('courtyard', 'geometry_vertex_count'), 3)]:
            second = copy.deepcopy(first)
            second[path[0]][path[1]] = value
            with self.assertRaises(ValueError):
                validate_repeats([first, second])

    def test_collect_retains_provenance_without_hardware_acceptance(self):
        self.report['windowed_evidence']['device_identity'] = {
            'name': 'Observed Vulkan device', 'vendor_id': 65541, 'device_id': 0,
            'driver_version': '0', 'driver_version_format': 'vulkan.raw'}
        with tempfile.TemporaryDirectory() as temporary:
            output = Path(temporary)
            executable = output / 'showcase'
            executable.write_bytes(b'test executable, never launched')

            def launch(command, **kwargs):
                report_path = Path(next(arg.split('=', 1)[1] for arg in command if arg.startswith('--report=')))
                report_path.write_text(json.dumps(self.report), encoding='utf-8')
                return subprocess.CompletedProcess(command, 0)

            with patch('BenchmarkShowcase.subprocess.run', side_effect=launch), \
                 patch('BenchmarkShowcase.platform.platform', return_value='test host'), \
                 patch('BenchmarkShowcase.platform.processor', return_value='test CPU'):
                summary = collect(executable, output, 'vulkan', ['standard'], 2, 360, {}, 10)
            self.assertEqual(len(summary['runs']), 2)
            self.assertFalse(summary['hardware_budget_accepted'])
            self.assertFalse(summary['physical_display_verified'])
            self.assertEqual(summary['driver_identity'], 'vulkan.raw:0')
            self.assertEqual(summary['device_identity']['name'], 'Observed Vulkan device')
            self.assertEqual(len(summary['executable_sha256']), 64)
            self.assertEqual(len(summary['runs'][0]['report_sha256']), 64)
            self.assertTrue((output / 'benchmark.md').is_file())
            self.assertIn('GPU timestamps are unavailable', markdown(summary))

    def test_device_observations_and_missing_historical_identity(self):
        original = copy.deepcopy(self.validate(self.report))
        self.assertIsNone(original['native'].get('device_identity'))
        identity = {'name': 'Observed device', 'vendor_id': 0, 'device_id': 0,
                    'driver_version': '4294967295', 'driver_version_format': 'vulkan.raw'}
        self.report['windowed_evidence']['device_identity'] = identity
        observed = self.validate(self.report)
        for key, value in [('name', ''), ('name', 'bad\nname'), ('name', 'a' * 512),
                           ('vendor_id', True), ('device_id', -1), ('vendor_id', None),
                           ('driver_version', 123), ('driver_version', '01'),
                           ('driver_version', '4294967296'), ('driver_version', '１'),
                           ('driver_version', None), ('driver_version_format', 'dxgi.umd')]:
            with self.subTest(key=key, value=value):
                changed = copy.deepcopy(self.report)
                changed['windowed_evidence']['device_identity'][key] = value
                with self.assertRaises(ValueError):
                    self.validate(changed)
        for changed in (original, copy.deepcopy(observed)):
            if changed is not original:
                changed['native']['device_identity']['driver_version'] = '0'
            with self.assertRaises(ValueError):
                validate_repeats([observed, changed])
        identity.update(driver_version=None, driver_version_format=None)
        self.validate(self.report)
        identity.update(name=None, vendor_id=None, device_id=None)
        self.validate(self.report)

    def test_dxgi_version_precision_and_markdown_device_text(self):
        self.report['windowed_evidence'].update(backend='dx12', device_identity={
            'name': '<device> [link](url) `code` | *bold*', 'vendor_id': 0, 'device_id': 0,
            'driver_version': '18446744073709551615', 'driver_version_format': 'dxgi.umd'})
        self.report['render_settings']['backend_requested'] = 'dx12'
        run = validate_report(self.report, 'dx12', 'standard', 360)
        text = markdown({'runs': [dict(run, repeat=1)], 'backend': 'dx12', 'scope': 'test',
                         'software_rasterizer': True, 'device_identity': run['native']['device_identity'],
                         'driver_identity': 'dxgi.umd:18446744073709551615'})
        self.assertIn('18446744073709551615', text)
        for fragment in ('<device>', '[link]', '`code`', '*bold*', ' | '):
            self.assertNotIn(fragment, text.split('Device: ', 1)[1].split('\n', 1)[0])

    def test_failed_or_missing_report_cannot_reuse_stale_pass(self):
        with tempfile.TemporaryDirectory() as temporary:
            output = Path(temporary)
            executable = output / 'showcase'
            executable.write_bytes(b'test executable, never launched')
            for returncode in (0, 1):
                for name in ('benchmark.json', 'benchmark.md', 'standard-1.json'):
                    (output / name).write_text(json.dumps(self.report), encoding='utf-8')
                with patch('BenchmarkShowcase.subprocess.run', return_value=
                           subprocess.CompletedProcess([], returncode)):
                    with self.assertRaises((OSError, ValueError)):
                        collect(executable, output, 'vulkan', ['standard'], 1, 360, {}, 10)
                self.assertFalse((output / 'benchmark.json').exists())
                self.assertFalse((output / 'benchmark.md').exists())
                self.assertFalse((output / 'standard-1.json').exists())
                self.assertTrue((output / 'standard-1.stderr.log').exists())


if __name__ == '__main__':
    unittest.main()
