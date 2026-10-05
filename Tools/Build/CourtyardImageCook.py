"""Bounded dependency-free PNG decoding and area filtering for authored courtyard sources."""
import struct
import zlib


def decode_png(data):
    if data[:8] != b'\x89PNG\r\n\x1a\n':
        raise ValueError('Invalid authored PNG signature')
    cursor, compressed, dimensions = 8, bytearray(), None
    while cursor + 12 <= len(data):
        size = struct.unpack_from('>I', data, cursor)[0]
        if size > len(data) - cursor - 12:
            raise ValueError('Truncated authored PNG chunk')
        kind = data[cursor + 4:cursor + 8]
        payload = data[cursor + 8:cursor + 8 + size]
        if zlib.crc32(kind + payload) != struct.unpack_from('>I', data, cursor + 8 + size)[0]:
            raise ValueError('Authored PNG checksum mismatch')
        if kind == b'IHDR':
            dimensions = struct.unpack('>IIBBBBB', payload)
        elif kind == b'IDAT':
            compressed.extend(payload)
        elif kind == b'IEND':
            break
        cursor += size + 12
    if dimensions is None:
        raise ValueError('Missing authored PNG dimensions')
    width, height, depth, color, compression, filtering, interlace = dimensions
    if not (0 < width <= 2048 and 0 < height <= 2048 and depth == 8 and color in (2, 6)
            and compression == filtering == interlace == 0):
        raise ValueError('Authored sources require bounded RGB/RGBA8 non-interlaced PNG')
    channels = 3 if color == 2 else 4
    stride = width * channels
    expected = height * (stride + 1)
    decoder = zlib.decompressobj()
    raw = decoder.decompress(compressed, expected + 1)
    if len(raw) != expected or not decoder.eof:
        raise ValueError('Invalid bounded authored PNG payload')
    pixels, previous = bytearray(), bytearray(stride)
    for y in range(height):
        offset = y * (stride + 1)
        mode = raw[offset]
        row = bytearray(raw[offset + 1:offset + 1 + stride])
        if mode > 4:
            raise ValueError('Unsupported authored PNG filter')
        for i in range(stride):
            left = row[i - channels] if i >= channels else 0
            up = previous[i]
            corner = previous[i - channels] if i >= channels else 0
            if mode == 1:
                row[i] = (row[i] + left) & 255
            elif mode == 2:
                row[i] = (row[i] + up) & 255
            elif mode == 3:
                row[i] = (row[i] + (left + up) // 2) & 255
            elif mode == 4:
                estimate = left + up - corner
                distances = [abs(estimate - value) for value in (left, up, corner)]
                row[i] = (row[i] + (left, up, corner)[distances.index(min(distances))]) & 255
        if channels == 4:
            pixels.extend(row)
        else:
            for x in range(width):
                pixels.extend((*row[x * 3:x * 3 + 3], 255))
        previous = row
    return width, height, pixels


def area_filter(image, width, height):
    """Integer overlap weights; alpha-weighted colors avoid transparent-edge halos."""
    source_width, source_height, pixels = image
    output = bytearray()
    for y in range(height):
        top, bottom = y * source_height, (y + 1) * source_height
        for x in range(width):
            left, right = x * source_width, (x + 1) * source_width
            sums, alpha = [0, 0, 0], 0
            for sy in range(top // height, (bottom + height - 1) // height):
                wy = min(bottom, (sy + 1) * height) - max(top, sy * height)
                for sx in range(left // width, (right + width - 1) // width):
                    wx = min(right, (sx + 1) * width) - max(left, sx * width)
                    index = (sy * source_width + sx) * 4
                    weight = wx * wy * pixels[index + 3]
                    alpha += weight
                    for c in range(3):
                        sums[c] += pixels[index + c] * weight
            rgb = [(value + alpha // 2) // alpha if alpha else 0 for value in sums]
            area = source_width * source_height
            output.extend((*rgb, (alpha + area // 2) // area))
    return output


def leaf_card(image, size):
    """Preserve portrait proportions; UV zero is the root used by vegetation wind."""
    width, height, _ = image
    target_width = size if width >= height else max(1, size * width // height)
    target_height = size if height >= width else max(1, size * height // width)
    filtered = area_filter(image, target_width, target_height)
    output = bytearray(size * size * 4)
    offset_x, offset_y = (size - target_width) // 2, (size - target_height) // 2
    for y in range(target_height):
        destination = ((offset_y + y) * size + offset_x) * 4
        source = (target_height - 1 - y) * target_width * 4
        output[destination:destination + target_width * 4] = filtered[source:source + target_width * 4]
    return output
