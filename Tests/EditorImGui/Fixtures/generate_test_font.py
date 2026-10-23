#!/usr/bin/env python3
"""Generate an original test-only TrueType outline fixture using Python's stdlib.

All glyph outlines and tables in this file are authored for the Nexora tests.
No third-party font, font package, or operating-system font is copied or read.
The mapped code points deliberately draw synthetic geometry, not real
Latin/Chinese/emoji typography. U+0080 aliases the triangle to test a missing
built-in Latin-range mapping. This asset tests cmap/atlas loading only.
The generated font and this script may be used, modified, and redistributed
without restriction under CC0-1.0; no attribution is required.
"""
import argparse
from pathlib import Path
import struct

MAGIC_CHECKSUM = 0xB1B0AFBA
CODEPOINTS = ((0x0041, 1), (0x0080, 1), (0x4E2D, 2), (0x1F642, 3))


def pack(fmt, *values):
    return struct.pack('>' + fmt, *values)


def checksum(data):
    data += b'\0' * (-len(data) % 4)
    return sum(struct.unpack('>' + 'I' * (len(data) // 4), data)) & 0xFFFFFFFF


def glyph(contours):
    points = [point for contour in contours for point in contour]
    xs, ys = zip(*points)
    data = pack('hhhhh', len(contours), min(xs), min(ys), max(xs), max(ys))
    end = -1
    for contour in contours:
        end += len(contour)
        data += pack('H', end)
    data += pack('H', 0)  # No TrueType instructions/hinting.
    data += b'\x01' * len(points)  # On-curve points; signed 16-bit deltas follow.
    for coordinates in (xs, ys):
        previous = 0
        for coordinate in coordinates:
            data += pack('h', coordinate - previous)
            previous = coordinate
    return data + b'\0' * (-len(data) % 2)


def cmap(wide=False):
    if wide:
        groups = ((0x20, 0xFF, 1), (0x2000, 0xFFEF, 2), (0x1F600, 0x1F642, 3))
        subtable = pack('HHIII', 13, 0, 16 + 12 * len(groups), 0, len(groups))
        subtable += b''.join(pack('III', *group) for group in groups)
        return pack('HHHHI', 0, 1, 3, 10, 12) + subtable

    # Microsoft BMP Unicode format 4, one singleton segment per mapped BMP code point.
    bmp = [(code, index) for code, index in CODEPOINTS if code <= 0xFFFF]
    segments = bmp + [(0xFFFF, 0)]
    count = len(segments)
    power = 1 << (count.bit_length() - 1)
    format4 = pack('7H', 4, 16 + 8 * count, 0, 2 * count,
                   2 * power, power.bit_length() - 1, 2 * count - 2 * power)
    format4 += b''.join(pack('H', code) for code, _ in segments) + pack('H', 0)
    format4 += b''.join(pack('H', code) for code, _ in segments)
    format4 += b''.join(pack('H', (index - code) & 0xFFFF) for code, index in segments)
    format4 += pack('H', 0) * count
    # Microsoft full Unicode format 12, including the supplementary-plane mapping.
    format12 = pack('HHIII', 12, 0, 16 + 12 * len(CODEPOINTS), 0, len(CODEPOINTS))
    format12 += b''.join(pack('III', code, code, index) for code, index in CODEPOINTS)
    offset4 = 4 + 8 * 2
    return (pack('HH', 0, 2) + pack('HHI', 3, 1, offset4) +
            pack('HHI', 3, 10, offset4 + len(format4)) + format4 + format12)


def name(family="Nexora Synthetic Glyph Test"):
    names = {1: family, 2: 'Regular',
             3: 'NexoraSyntheticGlyphTest-1.0', 4: family + ' Regular',
             5: 'Version 1.0', 6: 'NexoraSyntheticGlyphTest-Regular',
             13: 'Original synthetic test outlines. CC0-1.0. Not real typography.',
             14: 'https://creativecommons.org/publicdomain/zero/1.0/'}
    records, strings = b'', b''
    for identifier, text in sorted(names.items()):
        encoded = text.encode('utf-16-be')
        records += pack('6H', 3, 1, 0x0409, identifier, len(encoded), len(strings))
        strings += encoded
    return pack('3H', 0, len(names), 6 + len(records)) + records + strings


def make_font(family="Nexora Synthetic Glyph Test", magnify=1, wide=False):
    outlines = [
        # .notdef: a hollow box, with oppositely wound inner/outer contours.
        [[(0, 0), (0, 800), (800, 800), (800, 0)],
         [(200, 200), (600, 200), (600, 600), (200, 600)]],
        # U+0041: solid triangle.
        [[(0, 0), (400, 800), (800, 0)]],
        # U+4E2D: solid cross, intentionally not a Chinese character drawing.
        [[(300, 0), (500, 0), (500, 300), (800, 300), (800, 500), (500, 500),
          (500, 800), (300, 800), (300, 500), (0, 500), (0, 300), (300, 300)]],
        # U+1F642: solid diamond, intentionally not an emoji face drawing.
        [[(400, 0), (800, 400), (400, 800), (0, 400)]],
    ]
    if magnify != 1:
        outlines = [[[(x * magnify, y * magnify) for x, y in contour]
                     for contour in outline] for outline in outlines]
    bound = 800 * magnify
    glyf, offsets = b'', [0]
    for outline in outlines:
        glyf += glyph(outline)
        offsets.append(len(glyf))
    # Long loca offsets; no ambiguous 16-bit offset truncation.
    head = (pack('IIIIHH', 0x00010000, 0x00010000, 0, 0x5F0F3CF5, 3, 1024) +
            pack('QQ', 0, 0) + pack('hhhhHHhhh', 0, 0, bound, bound, 0, 8, 2, 1, 0))
    hhea = (pack('IhhhHhhhh', 0x00010000, 800, -200, 0, 1000, 0, 1000 - bound, bound, 1) +
            pack('h', 0) + pack('h', 0) * 5 + pack('hH', 0, 4))
    # maxp 1.0: points=12, contours=2, no composite glyphs or instructions.
    maxp = pack('I14H', 0x00010000, 4, 12, 2, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0)
    # OS/2 version 0 (78 bytes): regular, unrestricted embedding, 1000 advance.
    os2 = (pack('Hh3H', 0, 1000, 400, 5, 0) + pack('11h', *([0] * 11)) +
           bytes(10) + pack('4I', 1, (1 << 25) | (1 << 27), 0, 0) + b'NXRT' +
           pack('3H3h2H', 0x40, 0x0041, 0xFFFF, 800, -200, 0, 800, 200))
    tables = {'OS/2': os2, 'cmap': cmap(wide), 'glyf': glyf, 'head': head, 'hhea': hhea,
              'hmtx': pack('Hh', 1000, 0) * 4,
              'loca': b''.join(pack('I', offset) for offset in offsets), 'maxp': maxp,
              'name': name(family), 'post': pack('IIhhIIIII', 0x00030000, 0, -75, 50, 0, 0, 0, 0, 0)}
    expected = {'head': 54, 'hhea': 36, 'maxp': 32, 'OS/2': 78, 'post': 32}
    for tag, length in expected.items():
        if len(tables[tag]) != length:
            raise ValueError(f'{tag} length {len(tables[tag])}, expected {length}')
    count = len(tables)
    power = 1 << (count.bit_length() - 1)
    header = pack('I4H', 0x00010000, count, 16 * power,
                  power.bit_length() - 1, 16 * count - 16 * power)
    directory, body = b'', b''
    offset = len(header) + 16 * count
    head_offset = None
    for tag, data in sorted(tables.items()):
        directory += tag.encode('ascii') + pack('III', checksum(data), offset, len(data))
        if tag == 'head':
            head_offset = offset
        padded = data + b'\0' * (-len(data) % 4)
        body += padded
        offset += len(padded)
    result = bytearray(header + directory + body)
    adjustment = (MAGIC_CHECKSUM - checksum(bytes(result))) & 0xFFFFFFFF
    struct.pack_into('>I', result, head_offset + 8, adjustment)
    assert checksum(bytes(result)) == MAGIC_CHECKSUM
    return bytes(result)


def make_collection():
    # Every face is an original synthetic font. Only face 3 has the TC family identity.
    fonts = [bytearray(make_font('Synthetic CJK ' + locale)) for locale in ('JP', 'KR', 'SC', 'TC')]
    offset = 12 + 4 * len(fonts)
    offsets, parts = [], []
    for font in fonts:
        offsets.append(offset)
        count = struct.unpack_from('>H', font, 4)[0]
        for index in range(count):
            record = 12 + index * 16
            table_offset = struct.unpack_from('>I', font, record + 8)[0]
            if font[record:record + 4] == b'head':
                struct.pack_into('>I', font, table_offset + 8, 0) # TTC has no per-face adjustment.
            struct.pack_into('>I', font, record + 8, table_offset + offset)
        font += bytes(-len(font) % 4)
        parts.append(font)
        offset += len(font)
    return b'ttcf' + pack('II', 0x00010000, len(fonts)) + pack('I' * len(fonts), *offsets) + b''.join(parts)


def make_bitmap_container():
    # A minimal bitmap-table-only sfnt container, not a working color emoji font.
    # It deliberately has no glyf/loca/CFF outlines. Nothing should rasterize this fixture.
    original = make_font()
    count = struct.unpack_from('>H', original, 4)[0]
    tables = {}
    for index in range(count):
        record = 12 + index * 16
        tag = original[record:record + 4].decode('ascii')
        start, length = struct.unpack_from('>II', original, record + 8)
        if tag not in ('glyf', 'loca'):
            tables[tag] = original[start:start + length]
    tables['head'] = tables['head'][:8] + bytes(4) + tables['head'][12:]
    tables['CBDT'] = pack('I', 0x00030000)
    tables['CBLC'] = pack('II', 0x00030000, 0)
    count = len(tables)
    power = 1 << (count.bit_length() - 1)
    header = pack('I4H', 0x10000, count, power * 16, power.bit_length() - 1, count * 16 - power * 16)
    offset, directory, body, head_offset = 12 + count * 16, b'', b'', 0
    for tag, data in sorted(tables.items()):
        directory += tag.encode('ascii') + pack('III', checksum(data), offset, len(data))
        if tag == 'head': head_offset = offset
        padded = data + bytes(-len(data) % 4)
        body += padded
        offset += len(padded)
    result = bytearray(header + directory + body)
    struct.pack_into('>I', result, head_offset + 8, (MAGIC_CHECKSUM - checksum(bytes(result))) & 0xFFFFFFFF)
    return bytes(result)


def font_tables(font):
    result = {}
    for index in range(struct.unpack_from('>H', font, 4)[0]):
        record = 12 + index * 16
        tag = font[record:record + 4].decode('ascii')
        offset, length = struct.unpack_from('>II', font, record + 8)
        result[tag] = font[offset:offset + length]
    result['head'] = result['head'][:8] + bytes(4) + result['head'][12:]
    return result


def sfnt(tables, version=0x10000):
    count = len(tables)
    power = 1 << (count.bit_length() - 1)
    header = pack('I4H', version, count, power * 16, power.bit_length() - 1, count * 16 - power * 16)
    offset, directory, body, head = 12 + count * 16, b'', b'', 0
    for tag, table in sorted(tables.items()):
        directory += tag.encode('ascii') + pack('III', checksum(table), offset, len(table))
        if tag == 'head': head = offset
        padded = table + bytes(-len(table) % 4)
        body += padded
        offset += len(padded)
    result = bytearray(header + directory + body)
    struct.pack_into('>I', result, head + 8, (MAGIC_CHECKSUM - checksum(bytes(result))) & 0xFFFFFFFF)
    return bytes(result)


def cff_index(objects):
    if not objects: return pack('H', 0)
    offsets, offset = [1], 1
    for obj in objects:
        offset += len(obj)
        offsets.append(offset)
    width = max(1, (offset.bit_length() + 7) // 8)
    return pack('HB', len(objects), width) + b''.join(x.to_bytes(width, 'big') for x in offsets) + b''.join(objects)


def cff_integer(value):
    if -107 <= value <= 107: return bytes((value + 139,))
    if 108 <= value <= 1131:
        value -= 108
        return bytes((247 + value // 256, value % 256))
    if -1131 <= value <= -108:
        value = -value - 108
        return bytes((251 + value // 256, value % 256))
    return bytes((28,)) + pack('h', value)


def make_cff():
    def shape(points):
        result = cff_integer(points[0][0]) + cff_integer(points[0][1]) + bytes((21,))
        previous = points[0]
        for point in points[1:]:
            result += cff_integer(point[0] - previous[0]) + cff_integer(point[1] - previous[1]) + bytes((5,))
            previous = point
        return result + bytes((14,))
    chars = cff_index([
        shape([(0, 0), (0, 800), (800, 800), (800, 0)]),
        shape([(0, 0), (400, 800), (800, 0)]),
        shape([(300, 0), (500, 0), (500, 300), (800, 300), (800, 500), (500, 500),
               (500, 800), (300, 800), (300, 500), (0, 500), (0, 300), (300, 300)]),
        shape([(400, 0), (800, 400), (400, 800), (0, 400)]),
    ])
    header = bytes((1, 0, 4, 1))
    names = cff_index([b'NexoraSyntheticCFF'])
    # Fixed-width DICT integer avoids a circular size dependency.
    template = cff_index([bytes((29,)) + pack('I', 0) + bytes((17,))])
    empty = cff_index([])
    offset = len(header + names + template + empty + empty)
    top = cff_index([bytes((29,)) + pack('I', offset) + bytes((17,))])
    cff = header + names + top + empty + empty + chars
    tables = font_tables(make_font())
    del tables['glyf'], tables['loca']
    tables['CFF '] = cff
    tables['maxp'] = pack('IH', 0x00005000, 4)
    return sfnt(tables, 0x4F54544F)


def ordered_compound_chain():
    # Glyph order primes the memoization cache: 0 is simple, 1->0, ... 17->16.
    # Source maxima intentionally describe the real 17-level chain; admission must reject it.
    tables = font_tables(make_font())
    first_end = struct.unpack_from('>I', tables['loca'], 4)[0]
    outlines = tables['glyf'][:first_end]
    offsets = [0, len(outlines)]
    for index in range(1, 18):
        # Compound glyph; one child, signed-word XY args (flags 0x0003), identity transform.
        outlines += pack('5h4H', -1, 0, 0, 800, 800, 3, index - 1, 0, 0)
        offsets.append(len(outlines))
    tables['glyf'] = outlines
    tables['loca'] = pack('I' * len(offsets), *offsets)
    maxp = bytearray(tables['maxp'])
    struct.pack_into('>H', maxp, 4, 18)
    struct.pack_into('>H', maxp, 10, 8) # maxCompositePoints
    struct.pack_into('>H', maxp, 12, 2) # maxCompositeContours
    struct.pack_into('>H', maxp, 28, 1) # maxComponentElements
    struct.pack_into('>H', maxp, 30, 17) # maxComponentDepth
    tables['maxp'] = maxp
    hhea = bytearray(tables['hhea']); struct.pack_into('>H', hhea, 34, 18)
    tables['hhea'] = hhea
    tables['hmtx'] = pack('Hh', 1000, 0) * 18
    # The ordinary mapping for CJK uses glyph 2; inspect must reject the container even though
    # that one glyph is shallow. This directly exercises all-glyph depth admission.
    return sfnt(tables)


def malformed_fonts(directory):
    tables = font_tables(make_font())
    damaged = bytearray(tables['glyf'])
    # Selected glyph 2's header claims 32767 contours; all table bounds and checksums stay valid.
    glyph_offset = struct.unpack_from('>I', tables['loca'], 8)[0]
    struct.pack_into('>h', damaged, glyph_offset, 32767)
    tables['glyf'] = damaged
    (directory / 'NexoraInvalidContours.ttf').write_bytes(sfnt(tables))
    tables = font_tables(make_font())
    # Keep only format 4 so stb actually selects the malformed search metadata.
    original = tables['cmap']; offset = struct.unpack_from('>I', original, 8)[0]
    length = struct.unpack_from('>H', original, offset + 2)[0]
    format4 = bytearray(original[offset:offset + length])
    struct.pack_into('>H', format4, 12, 65534)
    tables['cmap'] = pack('HHHHI', 0, 1, 3, 1, 12) + format4
    (directory / 'NexoraInvalidCmap.ttf').write_bytes(sfnt(tables))
    cff = font_tables(make_cff())
    damaged = bytearray(cff['CFF '])
    damaged[6] = 0  # Name INDEX offSize must be 1..4.
    cff['CFF '] = damaged
    (directory / 'NexoraInvalidCff.otf').write_bytes(sfnt(cff, 0x4F54544F))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('output', nargs='?', type=Path,
                        default=Path(__file__).with_name('NexoraSyntheticGlyphTest.ttf'))
    args = parser.parse_args()
    data = make_font()
    args.output.write_bytes(data)
    print(f'{args.output}: {len(data)} bytes; 4 glyphs; cmap U+0041/U+0080/U+4E2D/U+1F642')
    args.output.with_name('NexoraSyntheticFaces.ttc').write_bytes(make_collection())
    args.output.with_name('NexoraBitmapOnlyContainer.ttf').write_bytes(make_bitmap_container())
    args.output.with_name('NexoraAtlasAdmission.ttf').write_bytes(
        make_font('Synthetic Atlas Admission', magnify=8, wide=True))
    args.output.with_name('NexoraSyntheticCff.otf').write_bytes(make_cff())
    malformed_fonts(args.output.parent)
    args.output.with_name('NexoraInvalidCompoundDepth.ttf').write_bytes(ordered_compound_chain())


if __name__ == '__main__':
    main()
