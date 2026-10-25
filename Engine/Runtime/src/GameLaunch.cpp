#include "Nexora/Runtime/GameLaunch.h"
#include "ProjectPackageIO.h"
#include "nexora/nexora.h"
#include <array>

namespace nexora::runtime {
namespace {
constexpr std::array<std::byte, 8> kMagic{std::byte{'N'}, std::byte{'X'}, std::byte{'L'},
                                          std::byte{'A'}, std::byte{'U'}, std::byte{'N'},
                                          std::byte{'0'}, std::byte{'1'}};
constexpr std::size_t kLaunchBytes = 24;
void Put(ByteBuffer &out, std::uint32_t value) {
  for (unsigned shift = 0; shift < 32; shift += 8)
    out.push_back(static_cast<std::byte>((value >> shift) & 255U));
}
std::uint32_t Get(std::span<const std::byte> bytes, std::size_t offset) {
  std::uint32_t value{};
  for (unsigned index = 0; index < 4; ++index)
    value |= std::to_integer<std::uint32_t>(bytes[offset + index]) << (8U * index);
  return value;
}
} // namespace
std::optional<ByteBuffer> EncodeGameLaunch(GameLaunch launch, std::string *error) {
  if (error)
    error->clear();
  if (launch.maximum_frames > 1000000)
    return package_detail::Fail<ByteBuffer>(error, "Game launch frame limit exceeds 1000000.");
  ByteBuffer bytes(kMagic.begin(), kMagic.end());
  bytes.reserve(kLaunchBytes);
  Put(bytes, launch.gameplay ? 1U : 0U);
  Put(bytes, launch.maximum_frames);
  Put(bytes, NEXORA_GAMEPLAY_ABI_VERSION);
  Put(bytes, 0);
  return bytes;
}
std::optional<GameLaunch> DecodeGameLaunch(std::span<const std::byte> bytes, std::string *error) {
  if (error)
    error->clear();
  if (bytes.size() != kLaunchBytes || !std::equal(kMagic.begin(), kMagic.end(), bytes.begin()))
    return package_detail::Fail<GameLaunch>(error, "Invalid game launch version or byte count.");
  const auto flags = Get(bytes, 8), frames = Get(bytes, 12);
  if (flags > 1 || frames > 1000000 || Get(bytes, 16) != NEXORA_GAMEPLAY_ABI_VERSION ||
      Get(bytes, 20))
    return package_detail::Fail<GameLaunch>(error,
                                            "Unsupported game launch flags, ABI or frame limit.");
  return GameLaunch{flags != 0, frames};
}
std::optional<GameLaunch> ReadGameLaunch(const std::filesystem::path &path, std::string *error) {
  const auto bytes = package_detail::ReadPackageFile(path, error, kLaunchBytes);
  return bytes ? DecodeGameLaunch(*bytes, error) : std::nullopt;
}
} // namespace nexora::runtime
