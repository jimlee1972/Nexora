#pragma once
#include "Nexora/Runtime/ProjectPackage.h"
#include <algorithm>
#include <cstring>
#include <unordered_set>

namespace nexora::runtime::package_detail {
inline bool Nonzero(AssetUuid id) { return id.high != 0 || id.low != 0; }
inline bool Less(AssetUuid left, AssetUuid right) {
  return left.high < right.high || (left.high == right.high && left.low < right.low);
}
template <class T> std::optional<T> Fail(std::string *error, std::string message) {
  if (error)
    *error = std::move(message);
  return std::nullopt;
}
inline std::uint64_t Hash(std::span<const std::byte> bytes) {
  std::uint64_t value = 1469598103934665603ULL;
  for (const auto byte : bytes) {
    value ^= std::to_integer<std::uint8_t>(byte);
    value *= 1099511628211ULL;
  }
  return value;
}
inline void Append(ByteBuffer &bytes, std::uint64_t value, std::size_t width) {
  for (std::size_t index = 0; index < width; ++index)
    bytes.push_back(static_cast<std::byte>(value >> (8 * index)));
}
struct Reader {
  std::span<const std::byte> bytes;
  std::size_t offset{};
  bool Number(std::uint64_t &value, std::size_t width) {
    if (width > bytes.size() - offset)
      return false;
    value = 0;
    for (std::size_t index = 0; index < width; ++index)
      value |= std::uint64_t(std::to_integer<std::uint8_t>(bytes[offset++])) << (8 * index);
    return true;
  }
  bool Skip(std::uint64_t size) {
    if (size > bytes.size() - offset)
      return false;
    offset += static_cast<std::size_t>(size);
    return true;
  }
  bool String(std::size_t maximum) {
    std::uint64_t size{};
    return Number(size, 4) && size <= maximum && Skip(size);
  }
};
// NXAB uses native integer layout today. Reject foreign-endian hosts rather than claiming a
// cross-endian wire; preflight every count/length before the existing deserializer can allocate.
inline bool PreflightBlob(std::span<const std::byte> bytes) {
  if (bytes.size() > kMaximumProjectBlobBytes || bytes.size() < 4 ||
      std::memcmp(bytes.data(), "NXAB", 4) != 0)
    return false;
  Reader reader{bytes, 4};
  std::uint64_t version{}, ignored{}, dependencies{}, payload{};
  return reader.Number(version, 4) && version == 1 && reader.Number(ignored, 8) &&
         reader.Number(ignored, 8) && reader.String(64) && reader.Number(dependencies, 4) &&
         dependencies <= kMaximumProjectPackageAssets && reader.Skip(dependencies * 16) &&
         reader.String(16) && reader.Number(payload, 8) && payload == bytes.size() - reader.offset;
}
inline bool ExactDependencies(const RuntimeBlob &blob,
                              const std::unordered_set<AssetUuid, AssetUuidHash> &expected) {
  std::unordered_set<AssetUuid, AssetUuidHash> actual;
  for (const auto id : blob.dependencies)
    if (!Nonzero(id) || !actual.insert(id).second)
      return false;
  return actual == expected;
}
} // namespace nexora::runtime::package_detail
