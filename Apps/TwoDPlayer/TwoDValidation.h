#pragma once
// Explicit fixture support, used only by cooked service acceptance mode.
#include "Nexora/Core/Vfs.h"
#include "Nexora/Runtime/GameplayStorage.h"
#include "Nexora/TwoD/GameplayUi.h"
#include "Nexora/TwoD/Scene.h"
#include <array>
#include <bit>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <thread>

namespace twod_validation {
using namespace nexora;
inline void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
inline twod::Bytes Read(core::VirtualFileSystem &vfs, const std::filesystem::path &root,
                        const char *relative) {
  // Files are fixed fixture names. VFS still enforces content-root containment.
  std::error_code error;
  const auto size = std::filesystem::file_size(root / relative, error);
  Require(error || (size && size <= 64 * 1024 * 1024), "validation asset size");
  core::AsyncReadOptions options;
  // Missing files also traverse the actual asynchronous failure path.
  options.size = error ? 1 : size;
  options.deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
  const auto handle = vfs.ReadAsync(std::string("content://") + relative, options);
  for (;;) {
    auto result = handle.Get();
    if (result.status == core::ReadResult::Status::Completed)
      return std::move(result.bytes);
    Require(result.status == core::ReadResult::Status::Pending &&
                std::chrono::steady_clock::now() < options.deadline,
            "validation async asset read");
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
}
inline runtime::RuntimeBlob Cook(core::VirtualFileSystem &vfs, const std::filesystem::path &root,
                                 const char *relative, std::string_view type) {
  auto blob = runtime::AssetCooker::Deserialize(Read(vfs, root, relative));
  Require(blob && blob->type == type, "validation cooked hash/type admission");
  return std::move(*blob);
}
inline twod::GameplayUiFont Font() {
  // Readable 5x7 ASCII fixture, not a fallback for missing CJK/production glyphs.
  constexpr std::string_view characters = " 2DUI SAVEOPN";
  constexpr std::array<std::array<unsigned, 7>, 12> rows{{
      {0, 0, 0, 0, 0, 0, 0},
      {14, 17, 1, 2, 4, 8, 31},
      {30, 17, 17, 17, 17, 17, 30},
      {17, 17, 17, 17, 17, 17, 14},
      {14, 4, 4, 4, 4, 4, 14},
      {0, 0, 0, 0, 0, 0, 0},
      {15, 16, 16, 14, 1, 1, 30},
      {14, 17, 17, 31, 17, 17, 17},
      {17, 17, 17, 17, 17, 10, 4},
      {31, 16, 16, 30, 16, 16, 31},
      {14, 17, 17, 17, 17, 17, 14},
      {30, 17, 17, 30, 16, 16, 16},
  }};
  // N is separate so the fixture table contains no duplicated space glyph registration.
  constexpr std::array<unsigned, 7> n{17, 25, 25, 21, 19, 19, 17};
  twod::GameplayUiFont font;
  font.texture_id = std::uint64_t{1} << 60;
  font.width = 13 * 6;
  font.height = 8;
  font.pixel_size = 7;
  font.line_height = 8;
  font.rgba.resize(font.width * font.height * 4);
  for (std::size_t i = 0; i < characters.size(); ++i) {
    if (i == 5)
      continue; // Only one space metric.
    const auto &bits = i == 12 ? n : rows[i];
    for (unsigned y = 0; y < 7; ++y)
      for (unsigned x = 0; x < 5; ++x)
        if (bits[y] & (1U << (4 - x))) {
          const auto pixel = (y * font.width + i * 6 + x) * 4;
          for (unsigned channel = 0; channel < 4; ++channel)
            font.rgba[pixel + channel] = std::byte{255};
        }
    font.glyphs.push_back({static_cast<std::uint32_t>(characters[i]), 6, 0, 0, 5, 7,
                           static_cast<float>(i * 6) / static_cast<float>(font.width), 0,
                           static_cast<float>(i * 6 + 5) / static_cast<float>(font.width),
                           7.F / 8});
  }
  return font;
}
inline NexoraStorageApiV1 Storage(runtime::GameplayStorage &storage) {
  NexoraStorageApiV1 api{};
  api.struct_size = sizeof(api);
  api.abi_version = 1;
  Require(storage.GetApi(api) == 0, "validation storage negotiation");
  return api;
}
inline std::array<std::byte, 16> EncodePosition(double x, double y) {
  Require(std::isfinite(x) && std::isfinite(y) && std::abs(x) <= 1e6 && std::abs(y) <= 1e6,
          "validation position bounds");
  std::array<std::byte, 16> bytes{};
  const std::array<std::uint64_t, 2> words{std::bit_cast<std::uint64_t>(x),
                                           std::bit_cast<std::uint64_t>(y)};
  for (std::size_t axis = 0; axis < 2; ++axis)
    for (unsigned i = 0; i < 8; ++i)
      bytes[axis * 8 + i] = std::byte((words[axis] >> (8 * i)) & 255);
  return bytes;
}
inline void Save(const NexoraStorageApiV1 &api, double x, double y) {
  const auto bytes = EncodePosition(x, y);
  Require(api.save(api.context, "position", 8, 1, bytes.data(),
                   static_cast<std::uint32_t>(bytes.size())) == 0,
          "validation position save");
}
inline std::optional<std::array<double, 2>> Load(const NexoraStorageApiV1 &api) {
  NexoraStorageInfoV1 info{sizeof(info), 1, 0, 0};
  const auto probe = api.load(api.context, "position", 8, nullptr, 0, &info);
  if (probe == NEXORA_STORAGE_NOT_FOUND)
    return {};
  Require(probe == 0 && info.schema_version == 1 && info.payload_size == 16,
          "validation position schema");
  std::array<std::byte, 16> bytes{};
  Require(api.load(api.context, "position", 8, bytes.data(),
                   static_cast<std::uint32_t>(bytes.size()), &info) == 0 &&
              info.schema_version == 1 && info.payload_size == bytes.size(),
          "validation position load");
  std::array<double, 2> result{};
  for (std::size_t axis = 0; axis < 2; ++axis) {
    std::uint64_t word{};
    for (unsigned i = 0; i < 8; ++i)
      word |= std::uint64_t(std::to_integer<unsigned>(bytes[axis * 8 + i])) << (8 * i);
    result[axis] = std::bit_cast<double>(word);
  }
  (void)EncodePosition(result[0], result[1]); // Validate before publishing coordinates.
  return result;
}
inline void Restore(runtime::World &world, runtime::Id entity, const std::array<double, 2> &xy) {
  auto transform = world.WorldTransform(entity);
  Require(bool(transform), "validation restore entity");
  transform->x = xy[0];
  transform->y = xy[1];
  runtime::WorldCommandBuffer commands;
  commands.SetTransform(entity, *transform);
  Require(commands.Apply(world), "validation restore transform");
}
struct Buttons {
  bool save{}, open{};
};
inline Buttons Draw(twod::GameplayUi &ui, float width, float height) {
  NexoraGameplayUiApiV1 api{};
  api.struct_size = sizeof(api);
  api.abi_version = 1;
  Require(ui.GetApi(&api) == 0, "validation UI negotiation");
  std::uint64_t font{};
  Require(api.get_font(api.context, 0, &font) == 0, "validation supplied font");
  Require(api.begin_frame(api.context, width, height) == 0 &&
              api.panel(api.context, {0, 0, width, 44}, 0xff202020) == 0 &&
              api.text(api.context, font, 14, {12, 12, 120, 24}, "2D UI", 5, 0xffffffff) == 0,
          "validation UI text");
  std::uint32_t save{}, open{};
  Require(api.button(api.context, 1, {160, 8, 80, 28}, "SAVE", 4, font, 14, 1, &save) == 0 &&
              api.button(api.context, 2, {248, 8, 80, 28}, "OPEN", 4, font, 14, 1, &open) == 0 &&
              api.end_frame(api.context) == 0,
          "validation UI buttons");
  return {(save & NEXORA_UI_ACTIVATED) != 0, (open & NEXORA_UI_ACTIVATED) != 0};
}
} // namespace twod_validation
