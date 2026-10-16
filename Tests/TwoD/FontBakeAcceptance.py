#!/usr/bin/env python3
"""Windows GDI bake, immutable publication, metrics and real glyph coverage checks."""
from pathlib import Path
import struct
import subprocess
import tempfile

script = Path(__file__).resolve().parents[2] / 'Tools/TwoD/BakeWindowsFont.ps1'
with tempfile.TemporaryDirectory(prefix='nexora-font-') as temporary:
    root = Path(temporary)
    corpus, output = root / 'corpus.txt', root / 'ui.nxfont'
    corpus.write_text('SAVE OPEN 123', encoding='utf-8')
    command = ['powershell.exe', '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', str(script),
               '-TextFile', str(corpus), '-Output', str(output), '-FontName', 'Segoe UI']
    result = subprocess.run(command, capture_output=True, text=True)
    assert result.returncode == 0, result.stderr
    data = output.read_bytes()
    magic, width, height, pixels, line, count = struct.unpack_from('<8sIIffI', data)
    assert magic == b'NXFONT1\0' and width and height and pixels == 32 and line > 0
    assert len(data) == 28 + count * 40 + width * height * 4
    assert any(data[28 + count * 40 + 3::4]), 'empty alpha raster'
    codes = [struct.unpack_from('<I', data, 28 + i * 40)[0] for i in range(count)]
    assert codes == sorted(set(map(ord, ' SAVEOPN123')))
    again = subprocess.run(command, capture_output=True, text=True)
    assert again.returncode != 0 and output.read_bytes() == data, 'overwrote atlas'
    corpus.write_text('\U0001f600', encoding='utf-8')
    output.unlink()
    rejected = subprocess.run(command, capture_output=True, text=True)
    assert rejected.returncode != 0 and not output.exists(), 'non-BMP input admitted'
    corpus.write_text('\u4e2d', encoding='utf-8')
    command[-1] = 'Arial'
    rejected = subprocess.run(command, capture_output=True, text=True)
    assert rejected.returncode != 0 and not output.exists(), 'missing font glyph admitted'
    assert not list(root.glob('*.tmp')), 'partial atlas retained'
print('Windows GDI font corpus bake, alpha, metrics, overwrite and missing-glyph checks passed')
