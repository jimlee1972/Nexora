#pragma once
#include "Nexora/Runtime/AssetPipeline.h"
#include "Nexora/TwoD/Api.h"
#include <array>
#include <span>
#include <string>
#include <vector>

namespace nexora::twod {
using AssetId = runtime::AssetUuid;
using Bytes = runtime::ByteBuffer;
struct Image final {
  std::uint32_t id{}, width{}, height{};
  Bytes rgba;
};
struct Region final {
  std::uint32_t id{}, x{}, y{}, width{}, height{};
  friend bool operator==(const Region &, const Region &) = default;
};
struct NEXORA_2D_API Atlas final {
  AssetId id;
  std::uint32_t width{}, height{}, gutter{1};
  bool nearest{};
  std::vector<Region> regions;
  Bytes rgba;
  [[nodiscard]] const Region *Find(std::uint32_t region) const noexcept;
};
// Row packing is canonical by image ID, independent of input iteration order. One-pixel
// extruded gutters protect bilinear sampling. All logical input/output budgets are validated.
[[nodiscard]] NEXORA_2D_API std::optional<Atlas>
PackAtlas(AssetId id, std::span<const Image> images, std::uint32_t width = 1024,
          std::uint32_t maximumHeight = 4096, bool nearest = false);
[[nodiscard]] NEXORA_2D_API bool ValidateAtlas(const Atlas &atlas) noexcept;
[[nodiscard]] NEXORA_2D_API Bytes EncodeAtlas(const Atlas &atlas);
[[nodiscard]] NEXORA_2D_API std::optional<Atlas> DecodeAtlas(std::span<const std::byte> bytes);
// Authoring geometry is independent of packed Atlas coordinates. Trim uses top-left source
// pixels; Pivot uses bottom-left source pixels. Borders are left/right/top/bottom source pixels.
// The new metadata wire is separate from historical Atlas/Clip/scene and C/Zig formats.
struct SpriteAsset final {
  AssetId id, atlas;
  std::uint32_t region{}, sourceWidth{}, sourceHeight{};
  std::uint32_t trimX{}, trimY{}, trimWidth{}, trimHeight{};
  float pivotX{}, pivotY{}, pixelsPerUnit{100};
  std::array<std::uint32_t, 4> border{};
  friend bool operator==(const SpriteAsset &, const SpriteAsset &) = default;
};
[[nodiscard]] NEXORA_2D_API bool ValidateSpriteAsset(const SpriteAsset &) noexcept;
[[nodiscard]] NEXORA_2D_API Bytes EncodeSpriteAsset(const SpriteAsset &);
[[nodiscard]] NEXORA_2D_API std::optional<SpriteAsset>
DecodeSpriteAsset(std::span<const std::byte> bytes);
// Atomically register .nx2atlas/.nx2clip/.nx2sprite with the existing import/cook pipeline.
NEXORA_2D_API bool RegisterImporters(runtime::ImporterRegistry &registry);
struct AnimationFrame final {
  std::uint32_t region{};
  double seconds{0.1};
  std::string event;
};
struct Clip final {
  AssetId id, atlas;
  bool loop{true};
  std::vector<AnimationFrame> frames;
};
[[nodiscard]] NEXORA_2D_API bool ValidateClip(const Clip &clip) noexcept;
[[nodiscard]] NEXORA_2D_API Bytes EncodeClip(const Clip &clip);
[[nodiscard]] NEXORA_2D_API std::optional<Clip> DecodeClip(std::span<const std::byte> bytes);
struct AnimationUpdate final {
  std::uint32_t region{};
  std::vector<std::string> events;
  bool finished{};
};
class NEXORA_2D_API Animator final {
public:
  // Owns a validated copy. Switching resets the clock; failed switches leave it unchanged.
  bool Play(const Clip &clip, double speed = 1);
  void Pause(bool paused) noexcept { paused_ = paused; }
  [[nodiscard]] std::optional<AnimationUpdate> Advance(double seconds);
  [[nodiscard]] std::optional<std::uint32_t> RegionId() const noexcept;

private:
  Clip clip_;
  std::size_t frame_{};
  double elapsed_{}, speed_{1};
  bool ready_{}, paused_{}, finished_{};
};
} // namespace nexora::twod
