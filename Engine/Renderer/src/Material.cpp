#include "Nexora/Renderer/Material.h"

#include <algorithm>
#include <limits>
#include <unordered_set>

namespace nexora::renderer {
namespace {
constexpr std::uint64_t kFnvOffset = 14695981039346656037ULL;
constexpr std::uint64_t kFnvPrime = 1099511628211ULL;

void HashBytes(std::uint64_t &hash, const void *data, std::size_t size) noexcept {
  const auto *bytes = static_cast<const unsigned char *>(data);
  for (std::size_t index = 0; index < size; ++index) {
    hash = (hash ^ bytes[index]) * kFnvPrime;
  }
}

void HashString(std::uint64_t &hash, std::string_view value) noexcept {
  HashBytes(hash, value.data(), value.size());
  const unsigned char terminator{};
  HashBytes(hash, &terminator, 1);
}

void HashInteger(std::uint64_t &hash, std::uint64_t value, std::uint32_t byte_count) noexcept {
  for (std::uint32_t index = 0; index < byte_count; ++index) {
    const auto byte = static_cast<unsigned char>((value >> (index * 8U)) & 0xffU);
    HashBytes(hash, &byte, 1U);
  }
}

MaterialParameterType ParameterType(const MaterialParameterValue &value) noexcept {
  return static_cast<MaterialParameterType>(value.index());
}

std::uint32_t ParameterSize(MaterialParameterType type) noexcept {
  return (static_cast<std::uint32_t>(type) + 1U) * sizeof(float);
}

constexpr MaterialFeatureMask kStaticFeatures =
    FeatureBit(MaterialFeature::NormalMap) | FeatureBit(MaterialFeature::Emission) |
    FeatureBit(MaterialFeature::IBL) | FeatureBit(MaterialFeature::ShadowReceiver) |
    FeatureBit(MaterialFeature::ShadowCaster) | FeatureBit(MaterialFeature::PostProcess) |
    FeatureBit(MaterialFeature::Instancing) | FeatureBit(MaterialFeature::Skinning) |
    FeatureBit(MaterialFeature::ForwardPlus) | FeatureBit(MaterialFeature::AlphaTest);
} // namespace

MaterialValidation ValidateMaterial(const MaterialSchema &material) {
  if (material.shader_id.empty()) {
    return {false, "shader_id must not be empty"};
  }
  if (material.shader_profile.empty()) {
    return {false, "shader_profile must not be empty"};
  }
  if (material.surface_mode == SurfaceMode::Masked &&
      (material.features & FeatureBit(MaterialFeature::AlphaTest)) == 0U) {
    return {false, "masked materials require AlphaTest"};
  }
  std::unordered_set<std::string_view> names;
  for (const auto &parameter : material.parameters) {
    if (parameter.name.empty() || !names.emplace(parameter.name).second) {
      return {false, "parameter names must be non-empty and unique"};
    }
  }
  names.clear();
  for (const auto &texture : material.textures) {
    if (texture.semantic.empty() || !names.emplace(texture.semantic).second) {
      return {false, "texture semantics must be non-empty and unique"};
    }
  }
  return {true, {}};
}

MaterialVariantKey SelectMaterialVariant(const MaterialSchema &material) noexcept {
  return {material.shading_model, material.surface_mode, material.features & kStaticFeatures,
          material.shader_profile};
}

std::uint64_t HashMaterialVariant(const MaterialVariantKey &key) noexcept {
  std::uint64_t hash = kFnvOffset;
  HashInteger(hash, static_cast<std::uint8_t>(key.shading_model), 1U);
  HashInteger(hash, static_cast<std::uint8_t>(key.surface_mode), 1U);
  HashInteger(hash, key.static_features, 8U);
  HashString(hash, key.shader_profile);
  return hash;
}

std::vector<MaterialVariantKey>
StripMaterialVariants(std::span<const MaterialSchema> used_materials) {
  std::vector<MaterialVariantKey> result;
  result.reserve(used_materials.size());
  for (const auto &material : used_materials) {
    if (!ValidateMaterial(material).valid) {
      continue;
    }
    const auto key = SelectMaterialVariant(material);
    if (std::find(result.begin(), result.end(), key) == result.end()) {
      result.push_back(key);
    }
  }
  std::sort(result.begin(), result.end(), [](const auto &left, const auto &right) {
    return HashMaterialVariant(left) < HashMaterialVariant(right);
  });
  return result;
}

MaterialBinding BindMaterialResources(const MaterialSchema &material,
                                      const MaterialResourceResolver &resolver) {
  MaterialBinding result;
  result.layout_hash = kFnvOffset;
  result.textures.reserve(material.textures.size());
  for (const auto &texture : material.textures) {
    const auto resolved = resolver.ResolveTexture(texture.resource_id);
    // Ask for a fallback only when resolution actually failed: value_or() would evaluate
    // FallbackTexture() eagerly for every texture, even ones that resolved.
    const auto index = resolved ? *resolved : resolver.FallbackTexture(texture.semantic);
    result.textures.push_back({texture.semantic, index, !resolved.has_value()});
    HashString(result.layout_hash, texture.semantic);
  }
  return result;
}

MaterialInspectorReflection ReflectMaterial(const MaterialSchema &material) {
  MaterialInspectorReflection result;
  result.shading_model = material.shading_model;
  result.surface_mode = material.surface_mode;
  result.layout_hash = kFnvOffset;
  std::uint32_t offset{};
  for (const auto &parameter : material.parameters) {
    const auto type = ParameterType(parameter.value);
    const auto size = ParameterSize(type);
    const auto alignment = size == 12U ? 16U : size;
    offset = (offset + alignment - 1U) / alignment * alignment;
    result.parameters.push_back({parameter.name, type, offset, size});
    HashString(result.layout_hash, parameter.name);
    HashInteger(result.layout_hash, static_cast<std::uint8_t>(type), 1U);
    HashInteger(result.layout_hash, offset, 4U);
    HashInteger(result.layout_hash, size, 4U);
    offset += size;
  }
  for (const auto &texture : material.textures) {
    result.texture_semantics.push_back(texture.semantic);
    HashString(result.layout_hash, texture.semantic);
  }
  return result;
}

std::optional<MaterialHandle> MaterialRegistry::Publish(std::string asset_id,
                                                        MaterialSchema material) {
  if (asset_id.empty() || indices_.contains(asset_id) || !ValidateMaterial(material).valid ||
      entries_.size() >= std::numeric_limits<std::uint32_t>::max()) {
    return std::nullopt;
  }
  const auto index = static_cast<std::uint32_t>(entries_.size());
  entries_.push_back({std::move(asset_id), std::move(material), 1U});
  indices_.emplace(entries_.back().asset_id, index);
  return MaterialHandle{index, 1U};
}

std::optional<MaterialHandle> MaterialRegistry::Reload(std::string_view asset_id,
                                                       MaterialSchema material) {
  const auto iterator = indices_.find(std::string(asset_id));
  if (iterator == indices_.end() || !ValidateMaterial(material).valid) {
    return std::nullopt;
  }
  auto &entry = entries_[iterator->second];
  entry.material = std::move(material);
  ++entry.generation;
  if (entry.generation == 0U) {
    entry.generation = 1U;
  }
  return MaterialHandle{iterator->second, entry.generation};
}

const MaterialSchema *MaterialRegistry::Get(MaterialHandle handle) const noexcept {
  if (handle.index >= entries_.size() || entries_[handle.index].generation != handle.generation) {
    return nullptr;
  }
  return &entries_[handle.index].material;
}

std::optional<MaterialHandle> MaterialRegistry::Find(std::string_view asset_id) const {
  const auto iterator = indices_.find(std::string(asset_id));
  if (iterator == indices_.end()) {
    return std::nullopt;
  }
  return MaterialHandle{iterator->second, entries_[iterator->second].generation};
}
} // namespace nexora::renderer
