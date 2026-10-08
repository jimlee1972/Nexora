#pragma once

#include "Nexora/Editor/ContentBrowser.h"
#include "Nexora/Editor/EditorWorkspace.h"
#include "Nexora/Editor/MaterialImport.h"

namespace nexora::editor {
class ProjectContentSession;

struct MaterialAssetSnapshot final {
  runtime::AssetUuid asset;
  std::shared_ptr<const MaterialAsset> material;
};

// Authoring-thread owning catalog; no file IO or native GPU objects. Lookups check generation.
class NEXORA_EDITOR_API MaterialAssetCatalog final {
public:
  bool PublishContent(const ContentBrowserModel &, std::string *error = nullptr);
  void Clear() noexcept;
  [[nodiscard]] std::uint64_t Generation() const noexcept { return generation_; }
  [[nodiscard]] std::optional<MaterialAssetSnapshot> ResolveAsset(runtime::AssetUuid,
                                                                  std::uint64_t generation) const;

private:
  std::uint64_t generation_{};
  std::unordered_map<runtime::AssetUuid, std::shared_ptr<const MaterialAsset>,
                     runtime::AssetUuidHash>
      materials_;
};

// UUID references live in a versioned owning opaque component; legacy MaterialComponent::shader
// is never reinterpreted. Unknown/invalid versions remain untouched and unresolved.
inline constexpr runtime::TypeId kMaterialAssetReferenceType = 0x45444d41544c0001ULL;
inline constexpr std::string_view kMaterialAssetReferenceName = "editor.material.asset";
[[nodiscard]] NEXORA_EDITOR_API OpaqueComponent MaterialAssetReference(runtime::AssetUuid);
[[nodiscard]] NEXORA_EDITOR_API std::optional<runtime::AssetUuid>
ReadMaterialAssetReference(const SceneDocument &, SceneDocument::NodeKey);
// Requires an exact 17-byte owning preview and byte_count; oversized opaque data is unresolved.
[[nodiscard]] NEXORA_EDITOR_API std::optional<runtime::AssetUuid>
ReadMaterialAssetReference(const OpaqueComponentInfo &);
// Validate access, selection, live component and current typed content/catalog before one Undo.
NEXORA_EDITOR_API bool AssignMaterialAsset(SceneDocument &, SceneDocument::NodeKey,
                                           runtime::AssetUuid, std::uint64_t expected_generation,
                                           const ProjectContentSession &,
                                           const MaterialAssetCatalog &, bool editable);

} // namespace nexora::editor
