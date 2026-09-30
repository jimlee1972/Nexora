#pragma once

#include "Nexora/Renderer/Api.h"

#include <array>
#include <compare>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <vector>

namespace nexora::renderer {
enum class ShadingModel : std::uint8_t { PBR, StylizedPBR, Anime, Vegetation, Water, Unlit };
enum class SurfaceMode : std::uint8_t { Opaque, Masked, Transparent, Additive };

enum class MaterialFeature : std::uint64_t {
  NormalMap = 1ULL << 0U,
  Emission = 1ULL << 1U,
  IBL = 1ULL << 2U,
  ShadowReceiver = 1ULL << 3U,
  ShadowCaster = 1ULL << 4U,
  PostProcess = 1ULL << 5U,
  Instancing = 1ULL << 6U,
  Skinning = 1ULL << 7U,
  ForwardPlus = 1ULL << 8U,
  AlphaTest = 1ULL << 9U,
};
using MaterialFeatureMask = std::uint64_t;

[[nodiscard]] constexpr MaterialFeatureMask FeatureBit(MaterialFeature feature) noexcept {
  return static_cast<MaterialFeatureMask>(feature);
}

enum class MaterialParameterType : std::uint8_t { Float, Float2, Float3, Float4 };
using MaterialParameterValue =
    std::variant<float, std::array<float, 2>, std::array<float, 3>, std::array<float, 4>>;

struct MaterialParameter final {
  std::string name;
  MaterialParameterValue value{};
};

struct MaterialTexture final {
  std::string semantic;
  std::string resource_id;
};

struct MaterialSchema final {
  std::string shader_id;
  ShadingModel shading_model{ShadingModel::PBR};
  SurfaceMode surface_mode{SurfaceMode::Opaque};
  MaterialFeatureMask features{};
  std::string shader_profile{"default"};
  std::vector<MaterialParameter> parameters;
  std::vector<MaterialTexture> textures;
};

struct MaterialValidation final {
  bool valid{};
  std::string message;
};

[[nodiscard]] NEXORA_RENDERER_API MaterialValidation
ValidateMaterial(const MaterialSchema &material);

struct MaterialVariantKey final {
  ShadingModel shading_model{ShadingModel::PBR};
  SurfaceMode surface_mode{SurfaceMode::Opaque};
  MaterialFeatureMask static_features{};
  std::string shader_profile;
  auto operator<=>(const MaterialVariantKey &) const = default;
};

[[nodiscard]] NEXORA_RENDERER_API MaterialVariantKey
SelectMaterialVariant(const MaterialSchema &material) noexcept;
[[nodiscard]] NEXORA_RENDERER_API std::uint64_t
HashMaterialVariant(const MaterialVariantKey &key) noexcept;
[[nodiscard]] NEXORA_RENDERER_API std::vector<MaterialVariantKey>
StripMaterialVariants(std::span<const MaterialSchema> used_materials);

class NEXORA_RENDERER_API MaterialResourceResolver {
public:
  virtual ~MaterialResourceResolver() = default;
  [[nodiscard]] virtual std::optional<std::uint32_t>
  ResolveTexture(std::string_view resource_id) const = 0;
  [[nodiscard]] virtual std::uint32_t FallbackTexture(std::string_view semantic) const = 0;
};

struct BoundMaterialTexture final {
  std::string semantic;
  std::uint32_t resource_index{};
  bool used_fallback{};
};

struct MaterialBinding final {
  std::vector<BoundMaterialTexture> textures;
  std::uint64_t layout_hash{};
};

[[nodiscard]] NEXORA_RENDERER_API MaterialBinding
BindMaterialResources(const MaterialSchema &material, const MaterialResourceResolver &resolver);

struct MaterialReflectionField final {
  std::string name;
  MaterialParameterType type{MaterialParameterType::Float};
  std::uint32_t byte_offset{};
  std::uint32_t byte_size{};
};

struct MaterialInspectorReflection final {
  ShadingModel shading_model{ShadingModel::PBR};
  SurfaceMode surface_mode{SurfaceMode::Opaque};
  std::vector<MaterialReflectionField> parameters;
  std::vector<std::string> texture_semantics;
  std::uint64_t layout_hash{};
};

[[nodiscard]] NEXORA_RENDERER_API MaterialInspectorReflection
ReflectMaterial(const MaterialSchema &material);

struct MaterialHandle final {
  std::uint32_t index{};
  std::uint32_t generation{};
  auto operator<=>(const MaterialHandle &) const = default;
};

class NEXORA_RENDERER_API MaterialRegistry final {
public:
  [[nodiscard]] std::optional<MaterialHandle> Publish(std::string asset_id,
                                                      MaterialSchema material);
  [[nodiscard]] std::optional<MaterialHandle> Reload(std::string_view asset_id,
                                                     MaterialSchema material);
  [[nodiscard]] const MaterialSchema *Get(MaterialHandle handle) const noexcept;
  [[nodiscard]] std::optional<MaterialHandle> Find(std::string_view asset_id) const;

private:
  struct Entry final {
    std::string asset_id;
    MaterialSchema material;
    std::uint32_t generation{1};
  };
  std::vector<Entry> entries_;
  std::unordered_map<std::string, std::uint32_t> indices_;
};
} // namespace nexora::renderer
