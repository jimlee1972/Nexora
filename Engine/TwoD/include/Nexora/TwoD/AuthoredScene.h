#pragma once
#include "Nexora/TwoD/Tilemap.h"

namespace nexora::twod {
// The existing 2D Studio document wire, shared by Editor and Shipping. Decoding owns all
// data and never mutates a caller's World. A .scene filename does not change the ND2D format.
struct NEXORA_2D_API AuthoredScene final {
  runtime::World world;
  std::optional<SpriteScene> scene;
  Atlas atlas;
  Clip clip;
  Tilemap tiles;
  AuthoredScene() = default;
  AuthoredScene(const AuthoredScene &) = delete;
  AuthoredScene &operator=(const AuthoredScene &) = delete;
  [[nodiscard]] static std::shared_ptr<const AuthoredScene> Decode(std::span<const std::byte>);
};
[[nodiscard]] NEXORA_2D_API bool ValidateSceneReferences(const SpriteScene &, const Atlas &,
                                                         const Clip &, const Tilemap &);
[[nodiscard]] NEXORA_2D_API Bytes EncodeAuthoredScene(const SpriteScene &, const Atlas &,
                                                      const Clip &, const Tilemap &);
} // namespace nexora::twod
