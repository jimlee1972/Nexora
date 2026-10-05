#!/usr/bin/env python3
"""Deterministically bake bounded linear RGBA16F IBL from the pinned CC0 Radiance HDRI.

No third-party Python or runtime image library is needed. The original HDRI, license evidence,
conversion parameters and derived hashes travel with the Showcase content package.
"""
import argparse
from array import array
import hashlib
import json
import math
from pathlib import Path
import re
import struct

ROOT = Path(__file__).resolve().parents[2]
CONTENT = ROOT / 'Content/Showcase/Courtyard/Environment'
SAMPLES = 128
PI = math.pi


def decode_rgbe(data):
    """Accept only bounded -Y/+X Radiance scanline RLE, not arbitrary image formats."""
    end = data.find(b'\n\n')
    if end < 0 or end > 4096 or not data.startswith(b'#?RADIANCE\n'):
        raise ValueError('Unsupported Radiance header')
    header = data[:end].splitlines()
    if b'FORMAT=32-bit_rle_rgbe' not in header:
        raise ValueError('Expected linear RGBE radiance')
    if any(x.startswith(b'GAMMA=') and x != b'GAMMA=1' for x in header):
        raise ValueError('Unexpected source gamma')
    pos = end + 2
    line_end = data.find(b'\n', pos)
    dimensions = re.fullmatch(rb'-Y (\d+) \+X (\d+)', data[pos:line_end])
    if not dimensions:
        raise ValueError('Unsupported Radiance orientation')
    height, width = map(int, dimensions.groups())
    if not (8 <= width <= 4096 and 1 <= height <= 2048 and width * height <= 2097152):
        raise ValueError('Radiance dimensions exceed the content bound')
    pos = line_end + 1
    pixels = array('f')
    for _ in range(height):
        if data[pos:pos + 4] != bytes((2, 2, width >> 8, width & 255)):
            raise ValueError('Invalid Radiance scanline')
        pos += 4
        channels = []
        for _ in range(4):
            channel = bytearray()
            while len(channel) < width:
                if pos >= len(data):
                    raise ValueError('Truncated Radiance run')
                count = data[pos]
                pos += 1
                if count == 0:
                    raise ValueError('Empty Radiance run')
                if count > 128:
                    count -= 128
                    if pos >= len(data):
                        raise ValueError('Truncated Radiance repeated run')
                    channel.extend(bytes((data[pos],)) * count)
                    pos += 1
                else:
                    if pos + count > len(data):
                        raise ValueError('Truncated Radiance literal run')
                    channel.extend(data[pos:pos + count])
                    pos += count
                if len(channel) > width:
                    raise ValueError('Radiance scanline overflow')
            channels.append(channel)
        for x in range(width):
            factor = math.ldexp(1.0, channels[3][x] - 136) if channels[3][x] else 0.0
            values = [channels[c][x] * factor for c in range(3)]
            if any(not math.isfinite(v) or v > 65504 for v in values):
                raise ValueError('Source radiance exceeds finite RGBA16F range')
            pixels.extend(values)
    if pos != len(data):
        raise ValueError('Unexpected Radiance suffix')
    return width, height, pixels


def normalize(v):
    scale = math.sqrt(sum(x * x for x in v))
    return tuple(x / scale for x in v)


def dot(a, b):
    return sum(x * y for x, y in zip(a, b))


def cross(a, b):
    return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2],
            a[0] * b[1] - a[1] * b[0])


def direction(x, y, width, height):
    theta, phi = PI * (y + 0.5) / height, 2 * PI * ((x + 0.5) / width - 0.5)
    return (math.sin(theta) * math.cos(phi), math.cos(theta), math.sin(theta) * math.sin(phi))


def sample(image, vector):
    width, height, pixels = image
    u = math.atan2(vector[2], vector[0]) / (2 * PI) + 0.5
    v = math.acos(max(-1, min(1, vector[1]))) / PI
    fx, fy = u * width - 0.5, v * height - 0.5
    x, y = math.floor(fx), math.floor(fy)
    tx, ty = fx - x, fy - y
    result = [0.0, 0.0, 0.0]
    for dx, dy, weight in [(0, 0, (1-tx)*(1-ty)), (1, 0, tx*(1-ty)),
                            (0, 1, (1-tx)*ty), (1, 1, tx*ty)]:
        index = (max(0, min(height - 1, y + dy)) * width + (x + dx) % width) * 3
        for c in range(3):
            result[c] += pixels[index + c] * weight
    return result


def radical_inverse(value):
    result, weight = 0.0, 0.5
    while value:
        result += (value & 1) * weight
        value >>= 1
        weight *= 0.5
    return result


POINTS = [(i / SAMPLES, radical_inverse(i)) for i in range(SAMPLES)]


def basis(normal):
    tangent = normalize(cross((0, 1, 0) if abs(normal[1]) < 0.99 else (1, 0, 0), normal))
    return tangent, cross(normal, tangent)


def to_world(local, normal, tangent, bitangent):
    return tuple(tangent[i] * local[0] + bitangent[i] * local[1] + normal[i] * local[2]
                 for i in range(3))


def importance_ggx(point, roughness):
    phi = 2 * PI * point[0]
    alpha2 = max(roughness, 0.001) ** 4
    cos_theta = math.sqrt((1 - point[1]) / (1 + (alpha2 - 1) * point[1]))
    sin_theta = math.sqrt(max(0, 1 - cos_theta * cos_theta))
    return (math.cos(phi) * sin_theta, math.sin(phi) * sin_theta, cos_theta)


def irradiance(image, normal):
    tangent, bitangent = basis(normal)
    result = [0.0, 0.0, 0.0]
    for u, v in POINTS:
        # Cosine-weighted hemisphere: integral is pi times the sample mean.
        radius, phi = math.sqrt(v), 2 * PI * u
        local = (radius * math.cos(phi), radius * math.sin(phi), math.sqrt(1 - v))
        color = sample(image, to_world(local, normal, tangent, bitangent))
        for c in range(3):
            result[c] += color[c] * PI / SAMPLES
    return result


def prefilter(image, normal, roughness):
    if roughness == 0:
        return sample(image, normal)
    tangent, bitangent = basis(normal)
    result, total = [0.0, 0.0, 0.0], 0.0
    for point in POINTS:
        half = to_world(importance_ggx(point, roughness), normal, tangent, bitangent)
        n_dot_h = max(0, dot(normal, half))
        light = tuple(2 * n_dot_h * half[i] - normal[i] for i in range(3))
        weight = max(0, dot(normal, light))
        if weight > 0:
            color = sample(image, normalize(light))
            total += weight
            for c in range(3):
                result[c] += color[c] * weight
    return [c / total for c in result]


def integrate_brdf(n_dot_v, roughness):
    view = (math.sqrt(1 - n_dot_v * n_dot_v), 0.0, n_dot_v)
    a, b = 0.0, 0.0
    k = roughness * roughness / 2
    for point in POINTS:
        half = importance_ggx(point, roughness)
        v_dot_h = max(0, dot(view, half))
        light = tuple(2 * v_dot_h * half[i] - view[i] for i in range(3))
        n_dot_l, n_dot_h = max(0, light[2]), max(0, half[2])
        if n_dot_l > 0:
            g_l = n_dot_l / (n_dot_l * (1-k) + k)
            g_v = n_dot_v / (n_dot_v * (1-k) + k)
            visible = g_l * g_v * v_dot_h / max(n_dot_h * n_dot_v, 1e-8)
            fresnel = (1-v_dot_h) ** 5
            a += (1-fresnel) * visible / SAMPLES
            b += fresnel * visible / SAMPLES
    return (a, b, 0.0)


def half_pixel(color):
    if any(not math.isfinite(v) or v < 0 or v > 65504 for v in color):
        raise ValueError('Derived radiance exceeds finite RGBA16F range')
    return struct.pack('<4e', *color, 1.0)


def bake(check):
    source = (CONTENT / 'forest_slope_1k.hdr').read_bytes()
    meta = json.loads((CONTENT / 'source.json').read_text())
    if hashlib.sha256(source).hexdigest() != meta['source_sha256']:
        raise ValueError('Pinned HDRI source hash mismatch')
    for filename, key in [('CC0.txt', 'license_sha256'),
                          ('polyhaven-license.json', 'license_evidence_sha256'),
                          ('drei-attribution.md', 'attribution_sha256')]:
        if hashlib.sha256((CONTENT / filename).read_bytes()).hexdigest() != meta[key]:
            raise ValueError(f'Pinned environment license/attribution hash mismatch: {filename}')
    image = decode_rgbe(source)
    resources = []
    diffuse = b''.join(half_pixel(irradiance(image, direction(x, y, 16, 8)))
                       for y in range(8) for x in range(16))
    resources.append(('diffuse', 16, 8, 1, diffuse))
    specular = bytearray()
    for mip in range(7):
        width, height = max(1, 64 >> mip), max(1, 32 >> mip)
        specular.extend(b''.join(half_pixel(prefilter(image, direction(x, y, width, height), mip/6))
                                for y in range(height) for x in range(width)))
    resources.append(('specular', 64, 32, 7, bytes(specular)))
    lut = b''.join(half_pixel(integrate_brdf((x+0.5)/32, (y+0.5)/32))
                   for y in range(32) for x in range(32))
    resources.append(('brdf', 32, 32, 1, lut))
    text = '// Generated by Tools/Build/PrepareCourtyardEnvironment.py; source license CC0-1.0.\n'
    text += '// clang-format off\n#pragma once\n#include <array>\n#include <cstdint>\n'
    text += 'namespace nexora::showcase::courtyard_environment {\n'
    derived = {'schema': 'nexora.showcase.ibl-derived.v1', 'source_sha256': meta['source_sha256'],
               'samples': SAMPLES, 'sequence': 'Hammersley',
               'converter_sha256': hashlib.sha256(Path(__file__).read_bytes()).hexdigest(), 'format': 'RGBA16F little-endian',
               'orientation': 'latlong +Y up, atan2(z,x); U wrap/V clamp',
               'diffuse': 'cosine-weighted irradiance integral including pi',
               'specular': 'GGX importance sampling, N=V, NdotL weighted mean; roughness=mip/6',
               'brdf': 'split sum GGX / Schlick geometry k=roughness^2/2; x=NdotV, y=roughness',
               'resources': []}
    for name, width, height, levels, payload in resources:
        filename = f'{name}.rgba16f'
        target = CONTENT / filename
        if check:
            if not target.exists() or target.read_bytes() != payload:
                raise ValueError(f'Derived IBL payload differs: {target}')
        else:
            target.write_bytes(payload)
        derived['resources'].append({'name': name, 'path': filename, 'width': width, 'height': height,
                                     'mip_levels': levels, 'bytes': len(payload),
                                     'sha256': hashlib.sha256(payload).hexdigest()})
        text += f'inline constexpr std::array<std::uint8_t, {len(payload)}> {name}{{{{\n'
        for offset in range(0, len(payload), 24):
            text += '  ' + ', '.join(f'0x{x:02x}' for x in payload[offset:offset+24]) + ',\n'
        text += '}};\n'
    metadata = json.dumps({'source': meta, 'derived': derived}, sort_keys=True, separators=(',', ':'))
    text += f'inline constexpr char metadata[] = R"NEXORA_IBL({metadata})NEXORA_IBL";\n'
    text += '} // namespace nexora::showcase::courtyard_environment\n// clang-format on\n'
    for target, value in [(ROOT/'Apps/Showcase/CourtyardEnvironment.h', text),
                           (CONTENT/'derived.json', json.dumps(derived, indent=2)+'\n')]:
        if check:
            if not target.exists() or target.read_text() != value:
                raise ValueError(f'Derived IBL metadata/header differs: {target}')
        else:
            target.write_text(value)
    print(json.dumps({'source': image[:2], 'resources': derived['resources']}))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true')
    bake(parser.parse_args().check)
