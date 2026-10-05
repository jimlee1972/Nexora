#!/usr/bin/env python3
"""Analytical IBL energy/orientation and malformed Radiance input contracts."""
from array import array
import importlib.util
import math
import json
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('courtyard_ibl', ROOT/'Tools/Build/PrepareCourtyardEnvironment.py')
ibl = importlib.util.module_from_spec(spec)
spec.loader.exec_module(ibl)


class EnvironmentContracts(unittest.TestCase):
    def fixture(self):
        header = b'#?RADIANCE\nFORMAT=32-bit_rle_rgbe\n\n-Y 1 +X 8\n'
        return header + bytes((2, 2, 0, 8, 136, 128, 136, 64, 136, 32, 136, 129))

    def test_rgbe_linear_radiance(self):
        width, height, pixels = ibl.decode_rgbe(self.fixture())
        self.assertEqual((width, height), (8, 1))
        self.assertEqual(list(pixels), [1.0, 0.5, 0.25] * 8)

    def test_malformed_radiance(self):
        source = self.fixture()
        for invalid in [source[:-1], source+b'extra', source.replace(b'+X 8', b'+X 0'),
                        source.replace(b'-Y 1', b'+Y 1'), source.replace(b'rgbe', b'xyze'),
                        source.replace(b'\x88\x80', b'\x00\x80', 1),
                        source.replace(b'FORMAT=', b'GAMMA=2\nFORMAT=', 1),
                        source.replace(b'-Y 1 +X 8', b'-Y 2048 +X 4096')]:
            with self.subTest(invalid=invalid[:80]), self.assertRaises(ValueError):
                ibl.decode_rgbe(invalid)

    def test_constant_environment_energy(self):
        color = [1.0, 2.0, 0.25]
        image = (8, 4, array('f', color * 32))
        for normal in [(0, 1, 0), (0, -1, 0), (1, 0, 0), ibl.normalize((1, 2, 3))]:
            irradiance = ibl.irradiance(image, normal)
            for actual, expected in zip(irradiance, color):
                self.assertAlmostEqual(actual, expected * math.pi, places=10)
            for roughness in [0.0, 0.1, 0.5, 1.0]:
                for actual, expected in zip(ibl.prefilter(image, normal, roughness), color):
                    self.assertAlmostEqual(actual, expected, places=10)

    def test_directional_orientation_and_u_wrap(self):
        # Positive Z selects the green half; negative Z the red half.
        image = (4, 2, array('f', [1,0,0, 1,0,0, 0,1,0, 0,1,0] * 2))
        self.assertEqual(ibl.sample(image, (0,0,1)), [0,1,0])
        self.assertEqual(ibl.sample(image, (0,0,-1)), [1,0,0])
        self.assertEqual(ibl.sample(image, (-1,0,0)), [0.5,0.5,0])

    def test_golden_sky_hdr_and_solar_direction(self):
        sky = json.loads((ROOT/'Content/Showcase/Courtyard/Hero/source.json').read_text())['sky']
        sun = ibl.normalize(sky['sun_direction'])
        self.assertEqual(ibl.golden_sky(sun,sky,True),tuple(sky['sun_radiance']))
        self.assertGreater(max(ibl.golden_sky(sun,sky,True)),1)
        self.assertLessEqual(max(ibl.golden_sky(sun,sky,False)),1)
        away = tuple(-d for d in sun)
        self.assertLessEqual(max(ibl.golden_sky(away,sky,True)),1)
        # A face boundary has the same world direction/radiance from either side.
        edge = ibl.normalize((1,0.4,1))
        for a,b in zip(ibl.golden_sky(edge,sky),ibl.golden_sky((1,0.4,1),sky)):
            self.assertAlmostEqual(a,b,places=12)

    def test_brdf_is_bounded(self):
        for n_dot_v in [0.02, 0.2, 0.5, 0.99]:
            for roughness in [0.02, 0.2, 0.5, 0.99]:
                a, b, _ = ibl.integrate_brdf(n_dot_v, roughness)
                self.assertTrue(math.isfinite(a+b) and a >= 0 and b >= 0 and a+b <= 1.05)


if __name__ == '__main__':
    unittest.main()
