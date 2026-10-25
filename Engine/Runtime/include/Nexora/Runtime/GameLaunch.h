#pragma once
#include "Nexora/Runtime/AssetPipeline.h"
#include <filesystem>

namespace nexora::runtime {
// Engine-generated, fixed-name launcher metadata. Zero frames means a normal close-driven game.
struct GameLaunch final {
  bool gameplay{};
  std::uint32_t maximum_frames{};
  bool operator==(const GameLaunch &) const = default;
};
inline constexpr std::string_view kGameLaunchFilename = "Game.launch";
inline constexpr std::string_view kGamePackageFilename = "Game.nxgame";
#if defined(_WIN32)
inline constexpr std::string_view kGameModuleFilename = "Game.dll";
#elif defined(__APPLE__)
inline constexpr std::string_view kGameModuleFilename = "libGame.dylib";
#else
inline constexpr std::string_view kGameModuleFilename = "libGame.so";
#endif
[[nodiscard]] NEXORA_RUNTIME_API std::optional<ByteBuffer>
EncodeGameLaunch(GameLaunch, std::string *error = nullptr);
[[nodiscard]] NEXORA_RUNTIME_API std::optional<GameLaunch>
DecodeGameLaunch(std::span<const std::byte>, std::string *error = nullptr);
[[nodiscard]] NEXORA_RUNTIME_API std::optional<GameLaunch>
ReadGameLaunch(const std::filesystem::path &, std::string *error = nullptr);
} // namespace nexora::runtime
