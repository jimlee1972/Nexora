#!/usr/bin/env python3
"""Bake a strict Unicode corpus into one bounded NXFONT1 atlas using Pillow/FreeType.

Install Tools/TwoD/font-requirements.txt. No fallback fonts, downloads or synthetic glyphs.
The JSON report identifies every input, cmap coverage and the exact atlas/registration budget.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path
import struct
import sys
import unicodedata


def sha(data):
    return hashlib.sha256(data).hexdigest()


def bake(font_path, corpus_paths, output, report_path, pixels=32, width=2048, audit_only=False):
    from fontTools.ttLib import TTFont
    from PIL import Image, ImageDraw, ImageFont, __version__ as pillow_version
    if not 4 <= pixels <= 96 or not 32 <= width <= 4096:
        raise ValueError('pixel size must be 4..96 and atlas width 32..4096')
    if output.exists() or report_path.exists() or output.resolve() == report_path.resolve():
        raise ValueError('outputs must be distinct new files')
    sources = [(p, p.read_bytes()) for p in corpus_paths]
    # Strict UTF-8; preserve scalar identity, no normalization or silent removal of visible text.
    codes = {32}
    for _, data in sources:
        for c in data.decode('utf-8'):
            if c in '\n\r\t':
                continue
            if unicodedata.category(c) in ('Cc', 'Cs'):
                raise ValueError(f'unsupported control U+{ord(c):04X}')
            codes.add(ord(c))
    with TTFont(font_path) as table:
        cmap = table.getBestCmap() or {}
        missing = sorted(c for c in codes if c not in cmap or cmap[c] == '.notdef')
        family = table['name'].getDebugName(1)
    report = dict(schema=1, font=dict(path=font_path.name, sha256=sha(font_path.read_bytes()), family=family),
                  inputs=[dict(path=p.name, sha256=sha(data), utf8_bytes=len(data)) for p, data in sources],
                  unique_scalars=len(codes), missing=[f'U+{c:04X}' for c in missing],
                  pixel_size=pixels, pillow=pillow_version, limits=dict(fonts=32, bytes_per_font=16777216,
                  glyphs_per_font=65536), final_game_acceptance=False)
    if missing:
        report['status'] = 'MISSING_GLYPHS'
        report_path.write_text(json.dumps(report, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
        raise ValueError(f'{len(missing)} missing glyphs; see coverage report')
    if len(codes) > 65536:
        raise ValueError('corpus exceeds per-font glyph budget')
    font = ImageFont.truetype(str(font_path), pixels, layout_engine=ImageFont.Layout.BASIC)
    ascent, descent = font.getmetrics()
    glyphs, placements = [], []
    x = y = row_height = 1
    for cp in sorted(codes):
        char = chr(cp)
        left, top, right, bottom = font.getbbox(char, anchor='la')
        w, h = right-left, bottom-top
        if w + 2 > width:
            raise ValueError('glyph exceeds atlas width')
        if x + w + 1 > width:
            x = 1; y += row_height + 1; row_height = 0
        placements.append((char, x, y, left, top, w, h))
        glyphs.append((cp, font.getlength(char), left, top, w, h, x/width, y, (x+w)/width, y+h))
        x += w + 2; row_height = max(row_height, h+1)
    height = 1 << max(0, math.ceil(math.log2(max(1, y+row_height+1))))
    report['atlas'] = dict(width=width, height=height, rgba_bytes=width*height*4,
                           glyph_count=len(glyphs), line_height=ascent+descent,
                           remaining_font_slots=31)
    if height > 4096 or width*height*4 > 16777216:
        report['status'] = 'ATLAS_BUDGET_EXCEEDED'
        report_path.write_text(json.dumps(report, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
        raise ValueError('corpus does not fit one bounded font; split into explicit font sets')
    report['status'] = 'COVERED' if audit_only else 'BAKED'
    if not audit_only:
        image = Image.new('RGBA', (width, height), (255,255,255,0))
        draw = ImageDraw.Draw(image)
        for char, gx, gy, left, top, _, _ in placements:
            draw.text((gx-left, gy-top), char, font=font, fill=(255,255,255,255), anchor='la')
        data = bytearray(struct.pack('<8sIIffI', b'NXFONT1\0', width, height, pixels, ascent+descent, len(glyphs)))
        for cp, advance, left, top, w, h, u0, v0, u1, v1 in glyphs:
            data.extend(struct.pack('<I9f', cp, advance, left, top, w, h, u0, v0/height, u1, v1/height))
        data.extend(image.tobytes())
        report['atlas']['sha256'] = sha(data)
        # All coverage/size validation finishes before publishing either artifact.
        with output.open('xb') as stream:
            stream.write(data)
    with report_path.open('x', encoding='utf-8') as stream:
        stream.write(json.dumps(report, ensure_ascii=False, indent=2)+'\n')
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--font', type=Path, required=True)
    parser.add_argument('--text', type=Path, nargs='+', required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--report', type=Path, required=True)
    parser.add_argument('--pixels', type=int, default=32)
    parser.add_argument('--width', type=int, default=2048)
    parser.add_argument('--audit-only', action='store_true')
    args = parser.parse_args()
    try:
        result = bake(args.font, args.text, args.output, args.report, args.pixels, args.width, args.audit_only)
    except (ValueError, OSError, ImportError) as error:
        print(str(error), file=sys.stderr)
        return 1
    print(json.dumps(result, ensure_ascii=False))
    return 0


if __name__ == '__main__':
    sys.exit(main())
