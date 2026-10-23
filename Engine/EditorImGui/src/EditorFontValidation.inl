// Private, bounded structural preflight for the pinned stb subset. This is not a font sanitizer.
bool ValidGlyf(std::span<const std::byte> bytes, Slice glyf, Slice loca, unsigned format,
               unsigned glyph_count) {
  std::vector<Slice> slices;
  for (std::size_t glyph = 0; glyph < glyph_count; ++glyph) {
    const auto get = [&](std::size_t index) {
      return format ? U32(bytes, loca.offset + index * 4)
                    : std::uint32_t(U16(bytes, loca.offset + index * 2)) * 2;
    };
    const auto first = get(glyph), last = get(glyph + 1);
    if (first > last || last > glyf.size || (last != first && last - first < 10))
      return false;
    slices.push_back({glyf.offset + first, last - first});
  }
  std::vector<std::vector<unsigned>> children(glyph_count);
  std::vector<std::uint64_t> points(glyph_count), vertices(glyph_count);
  for (std::size_t glyph = 0; glyph < slices.size(); ++glyph) {
    const auto slice = slices[glyph];
    if (!slice.size)
      continue;
    const auto data = bytes.subspan(slice.offset, slice.size);
    const auto contours = static_cast<std::int16_t>(U16(data, 0));
    std::size_t cursor = 10;
    if (contours >= 0) {
      if (!Bounds(data, cursor, std::size_t(contours) * 2 + 2))
        return false;
      unsigned count = 0;
      for (unsigned contour = 0; contour < static_cast<unsigned>(contours); ++contour) {
        const unsigned endpoint = U16(data, cursor);
        cursor += 2;
        if (endpoint < count || (contour && endpoint == count - 1))
          return false;
        count = endpoint + 1;
      }
      if (count > 8192)
        return false;
      const auto instructions = U16(data, cursor);
      cursor += 2;
      if (!Bounds(data, cursor, instructions))
        return false;
      cursor += instructions;
      std::size_t x_bytes = 0, y_bytes = 0;
      for (unsigned point = 0; point < count;) {
        if (!Bounds(data, cursor, 1))
          return false;
        const auto flag = std::to_integer<unsigned>(data[cursor++]);
        unsigned repeat = 1;
        if (flag & 8) {
          if (!Bounds(data, cursor, 1))
            return false;
          repeat += std::to_integer<unsigned>(data[cursor++]);
        }
        if (repeat > count - point)
          return false;
        x_bytes += repeat * ((flag & 2) ? 1 : (flag & 16) ? 0 : 2);
        y_bytes += repeat * ((flag & 4) ? 1 : (flag & 32) ? 0 : 2);
        point += repeat;
      }
      if (!Bounds(data, cursor, x_bytes + y_bytes))
        return false;
      points[glyph] = count;
      vertices[glyph] = count + 2ULL * contours;
    } else {
      if (contours != -1)
        return false;
      unsigned flags = 0, components = 0;
      do {
        if (!Bounds(data, cursor, 4) || ++components > 64)
          return false;
        flags = U16(data, cursor);
        const auto component = U16(data, cursor + 2);
        cursor += 4;
        if (component >= glyph_count || !(flags & 2))
          return false; // pinned stb requires XY args.
        const unsigned transforms = !!(flags & 8) + !!(flags & 64) + !!(flags & 128);
        if (transforms > 1)
          return false;
        cursor += (flags & 1) ? 4 : 2;
        cursor += (flags & 8) ? 2 : (flags & 64) ? 4 : (flags & 128) ? 8 : 0;
        if (!Bounds(data, cursor, 0))
          return false;
        children[glyph].push_back(component);
      } while (flags & 32);
      if (flags & 256) {
        if (!Bounds(data, cursor, 2))
          return false;
        const auto instructions = U16(data, cursor);
        cursor += 2;
        if (!Bounds(data, cursor, instructions))
          return false;
      }
    }
  }
  std::vector<unsigned char> visited(glyph_count);
  std::vector<unsigned> subtree_depth(glyph_count);
  const auto visit = [&](auto &&self, unsigned glyph, unsigned depth) -> bool {
    if (depth > 16 || visited[glyph] == 1)
      return false;
    if (visited[glyph] == 2)
      return subtree_depth[glyph] + depth <= 16;
    visited[glyph] = 1;
    for (const auto child : children[glyph]) {
      if (!self(self, child, depth + 1))
        return false;
      subtree_depth[glyph] = std::max(subtree_depth[glyph], subtree_depth[child] + 1);
      if (subtree_depth[glyph] > 16)
        return false;
      points[glyph] += points[child];
      vertices[glyph] += vertices[child];
      if (points[glyph] > 8192 || vertices[glyph] > 16384)
        return false;
    }
    visited[glyph] = 2;
    return true;
  };
  for (unsigned glyph = 0; glyph < glyph_count; ++glyph)
    if (!visit(visit, glyph, 0))
      return false;
  return true;
}
struct CffIndex {
  std::vector<Slice> objects;
  std::size_t next{};
};
std::optional<CffIndex> CffReadIndex(std::span<const std::byte> bytes, std::size_t cursor) {
  if (!Bounds(bytes, cursor, 2))
    return std::nullopt;
  const auto count = U16(bytes, cursor);
  cursor += 2;
  CffIndex result;
  if (!count) {
    result.next = cursor;
    return result;
  }
  if (!Bounds(bytes, cursor, 1))
    return std::nullopt;
  const auto width = std::to_integer<unsigned>(bytes[cursor++]);
  if (width < 1 || width > 4 || !Bounds(bytes, cursor, (std::size_t(count) + 1) * width))
    return std::nullopt;
  const auto offset = [&](unsigned index) {
    std::uint32_t value = 0;
    for (unsigned byte = 0; byte < width; ++byte)
      value = (value << 8) |
              std::to_integer<unsigned>(bytes[cursor + std::size_t(index) * width + byte]);
    return value;
  };
  const auto data = cursor + (std::size_t(count) + 1) * width;
  if (offset(0) != 1)
    return std::nullopt;
  for (unsigned index = 0; index < count; ++index) {
    const auto first = offset(index), last = offset(index + 1);
    if (last < first || !Bounds(bytes, data, std::uint64_t(last) - 1))
      return std::nullopt;
    result.objects.push_back({data + first - 1, last - first});
  }
  result.next = data + offset(count) - 1;
  return result;
}
using CffDict = std::map<unsigned, std::vector<std::int64_t>>;
std::optional<CffDict> CffReadDict(std::span<const std::byte> data) {
  CffDict result;
  std::vector<std::int64_t> stack;
  for (std::size_t cursor = 0; cursor < data.size();) {
    const auto byte = std::to_integer<unsigned>(data[cursor++]);
    std::int64_t value = 0;
    if (byte >= 32 && byte <= 246)
      value = static_cast<int>(byte) - 139;
    else if (byte >= 247 && byte <= 254) {
      if (!Bounds(data, cursor, 1))
        return std::nullopt;
      value = (static_cast<int>(byte <= 250 ? byte - 247 : byte - 251) * 256) +
              std::to_integer<unsigned>(data[cursor++]) + 108;
      if (byte > 250)
        value = -value;
    } else if (byte == 28 || byte == 29) {
      const auto length = byte == 28 ? 2 : 4;
      if (!Bounds(data, cursor, length))
        return std::nullopt;
      value = byte == 28 ? static_cast<std::int16_t>(U16(data, cursor))
                         : static_cast<std::int32_t>(U32(data, cursor));
      cursor += length;
    } else if (byte == 30) {
      bool ended = false;
      while (cursor < data.size()) {
        const auto digits = std::to_integer<unsigned>(data[cursor++]);
        if ((digits >> 4) == 15 || (digits & 15) == 15) {
          ended = true;
          break;
        }
        if ((digits >> 4) == 13 || (digits & 15) == 13)
          return std::nullopt;
      }
      if (!ended)
        return std::nullopt;
      value = std::numeric_limits<std::int64_t>::min(); // Real operands cannot be table offsets.
    } else {
      if (byte > 21 || stack.size() > 48)
        return std::nullopt;
      auto operation = byte;
      if (byte == 12) {
        if (!Bounds(data, cursor, 1))
          return std::nullopt;
        operation = 256 + std::to_integer<unsigned>(data[cursor++]);
      }
      if (result.contains(operation))
        return std::nullopt;
      result[operation] = stack;
      stack.clear();
      continue;
    }
    if (stack.size() == 48)
      return std::nullopt;
    stack.push_back(value);
  }
  if (!stack.empty())
    return std::nullopt;
  return result;
}
std::optional<std::uint32_t> CffOffset(const CffDict &dict, unsigned key) {
  const auto found = dict.find(key);
  if (found == dict.end())
    return 0;
  if (found->second.size() != 1 || found->second[0] < 0 ||
      found->second[0] > std::numeric_limits<std::uint32_t>::max())
    return std::nullopt;
  return static_cast<std::uint32_t>(found->second[0]);
}
std::optional<CffIndex> CffSubrs(std::span<const std::byte> data, const CffDict &dict) {
  const auto found = dict.find(18);
  if (found == dict.end())
    return CffIndex{};
  if (found->second.size() != 2 || found->second[0] < 0 || found->second[1] < 0)
    return std::nullopt;
  const auto length = static_cast<std::uint64_t>(found->second[0]);
  const auto offset = static_cast<std::uint64_t>(found->second[1]);
  if (offset > data.size() || length > data.size() - offset)
    return std::nullopt;
  const auto private_dict =
      CffReadDict(data.subspan(static_cast<std::size_t>(offset), static_cast<std::size_t>(length)));
  if (!private_dict)
    return std::nullopt;
  const auto relative = CffOffset(*private_dict, 19);
  if (!relative || *relative > data.size() - offset)
    return std::nullopt;
  return *relative ? CffReadIndex(data, static_cast<std::size_t>(offset) + *relative)
                   : std::optional<CffIndex>{CffIndex{}};
}
struct CffData {
  std::span<const std::byte> bytes;
  CffIndex charstrings, global_subrs, local_subrs;
  std::vector<CffIndex> private_subrs;
  std::vector<unsigned> selectors;
};
std::optional<CffData> CffPreflight(std::span<const std::byte> bytes, unsigned glyphs) {
  if (bytes.size() < 4 || std::to_integer<unsigned>(bytes[0]) != 1)
    return std::nullopt;
  const auto header = std::to_integer<unsigned>(bytes[2]);
  if (header < 4 || header > bytes.size())
    return std::nullopt;
  const auto names = CffReadIndex(bytes, header);
  if (!names || names->objects.empty())
    return std::nullopt;
  const auto top = CffReadIndex(bytes, names->next);
  if (!top || top->objects.empty())
    return std::nullopt;
  const auto strings = CffReadIndex(bytes, top->next);
  if (!strings)
    return std::nullopt;
  const auto global = CffReadIndex(bytes, strings->next);
  if (!global)
    return std::nullopt;
  const auto top_slice = top->objects[0];
  const auto dict = CffReadDict(bytes.subspan(top_slice.offset, top_slice.size));
  if (!dict)
    return std::nullopt;
  const auto charstrings = CffOffset(*dict, 17), fdarray = CffOffset(*dict, 292),
             fdselect = CffOffset(*dict, 293);
  const auto type = CffOffset(*dict, 262);
  if (!charstrings || !*charstrings || !fdarray || !fdselect || !type || (*type && *type != 2))
    return std::nullopt;
  const auto chars = CffReadIndex(bytes, *charstrings), local = CffSubrs(bytes, *dict);
  if (!chars || chars->objects.size() != glyphs || !local)
    return std::nullopt;
  CffData result{bytes, *chars, *global, *local, {}, {}};
  if (*fdarray) {
    if (!*fdselect || !Bounds(bytes, *fdselect, 1))
      return std::nullopt;
    const auto array = CffReadIndex(bytes, *fdarray);
    if (!array || array->objects.empty() || array->objects.size() > 256)
      return std::nullopt;
    for (const auto slice : array->objects) {
      const auto private_dict = CffReadDict(bytes.subspan(slice.offset, slice.size));
      if (!private_dict)
        return std::nullopt;
      const auto subrs = CffSubrs(bytes, *private_dict);
      if (!subrs)
        return std::nullopt;
      result.private_subrs.push_back(*subrs);
    }
    auto cursor = std::size_t(*fdselect);
    const auto format = std::to_integer<unsigned>(bytes[cursor++]);
    result.selectors.resize(glyphs);
    if (format == 0) {
      if (!Bounds(bytes, cursor, glyphs))
        return std::nullopt;
      for (unsigned glyph = 0; glyph < glyphs; ++glyph)
        result.selectors[glyph] = std::to_integer<unsigned>(bytes[cursor + glyph]);
    } else if (format == 3) {
      if (!Bounds(bytes, cursor, 2))
        return std::nullopt;
      const auto ranges = U16(bytes, cursor);
      cursor += 2;
      if (!ranges || !Bounds(bytes, cursor, std::size_t(ranges) * 3 + 2))
        return std::nullopt;
      unsigned first = U16(bytes, cursor);
      if (first != 0)
        return std::nullopt;
      for (unsigned range = 0; range < ranges; ++range) {
        const auto selector = std::to_integer<unsigned>(bytes[cursor + 2]);
        const auto last = U16(bytes, cursor + 3);
        if (last <= first || last > glyphs)
          return std::nullopt;
        std::fill(result.selectors.begin() + first, result.selectors.begin() + last, selector);
        first = last;
        cursor += 3;
      }
      if (first != glyphs)
        return std::nullopt;
    } else
      return std::nullopt;
    if (std::ranges::any_of(result.selectors,
                            [&](auto selector) { return selector >= result.private_subrs.size(); }))
      return std::nullopt;
  }
  return result;
}
// Validate the exact Type2 opcode/stack subset supported by pinned stb before it reads a shape.
// Byte/operation/coordinate budgets also bound subroutine expansion and temporary vertex counts.
bool ValidType2(const CffData &cff, unsigned glyph) {
  if (glyph >= cff.charstrings.objects.size())
    return false;
  Slice current = cff.charstrings.objects[glyph];
  struct Return {
    Slice slice;
    std::size_t cursor;
  };
  std::vector<Return> returns;
  std::vector<double> stack;
  const auto &local =
      cff.selectors.empty() ? cff.local_subrs : cff.private_subrs[cff.selectors[glyph]];
  unsigned mask_bits = 0;
  bool header = true;
  std::size_t cursor = 0;
  double absolute_operands = 0;
  for (unsigned operations = 0; operations < 8192; ++operations) {
    const auto data = cff.bytes.subspan(current.offset, current.size);
    if (!Bounds(data, cursor, 1))
      return false;
    const auto op = std::to_integer<unsigned>(data[cursor++]);
    if (op == 28 || op == 255 || op >= 32) {
      double value = 0;
      if (op == 28 || op == 255) {
        const auto size = op == 28 ? 2 : 4;
        if (!Bounds(data, cursor, size))
          return false;
        value = op == 28
                    ? static_cast<std::int16_t>(U16(data, cursor))
                    : static_cast<float>(static_cast<std::int32_t>(U32(data, cursor))) / 65536.0F;
        cursor += size;
      } else if (op <= 246)
        value = static_cast<int>(op) - 139;
      else {
        if (!Bounds(data, cursor, 1))
          return false;
        value = static_cast<int>(op <= 250 ? op - 247 : op - 251) * 256 +
                std::to_integer<unsigned>(data[cursor++]) + 108;
        if (op > 250)
          value = -value;
      }
      absolute_operands += std::abs(value);
      if (stack.size() == 48 || absolute_operands > 10000000)
        return false;
      stack.push_back(value);
      continue;
    }
    const auto size = stack.size();
    if (op == 10 || op == 29) {
      if (stack.empty() || returns.size() >= 10)
        return false;
      const auto &subrs = op == 10 ? local : cff.global_subrs;
      const int bias = subrs.objects.size() >= 33900  ? 32768
                       : subrs.objects.size() >= 1240 ? 1131
                                                      : 107;
      const auto index = static_cast<std::int64_t>(stack.back()) + bias;
      stack.pop_back();
      if (index < 0 || static_cast<std::uint64_t>(index) >= subrs.objects.size())
        return false;
      returns.push_back({current, cursor});
      current = subrs.objects[static_cast<std::size_t>(index)];
      cursor = 0;
      continue;
    }
    if (op == 11) {
      if (returns.empty())
        return false;
      current = returns.back().slice;
      cursor = returns.back().cursor;
      returns.pop_back();
      continue;
    }
    if (op == 14)
      return true;
    if (op == 1 || op == 3 || op == 18 || op == 23)
      mask_bits += static_cast<unsigned>(size / 2);
    else if (op == 19 || op == 20) {
      if (header)
        mask_bits += static_cast<unsigned>(size / 2);
      header = false;
      const auto masks = (mask_bits + 7) / 8;
      if (!Bounds(data, cursor, masks))
        return false;
      cursor += masks;
    } else if (op == 4 || op == 22) {
      if (size < 1)
        return false;
      header = false;
    } else if (op == 21) {
      if (size < 2)
        return false;
      header = false;
    } else if (op == 5) {
      if (size < 2)
        return false;
    } else if (op == 6 || op == 7) {
      if (size < 1)
        return false;
    } else if (op == 8) {
      if (size < 6)
        return false;
    } else if (op == 24 || op == 25) {
      if (size < 8)
        return false;
    } else if (op == 26 || op == 27 || op == 30 || op == 31) {
      if (size < 4)
        return false;
    } else if (op == 12) {
      if (!Bounds(data, cursor, 1))
        return false;
      const auto escaped = std::to_integer<unsigned>(data[cursor++]);
      const auto minimum = escaped == 34   ? 7
                           : escaped == 35 ? 13
                           : escaped == 36 ? 9
                           : escaped == 37 ? 11
                                           : 0;
      if (!minimum || size < static_cast<unsigned>(minimum))
        return false;
    } else
      return false;
    stack.clear();
  }
  return false;
}
