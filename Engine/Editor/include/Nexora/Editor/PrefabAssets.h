#pragma once
#include "Nexora/Editor/EditorWorkspace.h"

namespace nexora::editor {
struct PrefabPropertyIdentity final {
  foundation::Uuid id;
  std::string field;
  friend bool operator==(const PrefabPropertyIdentity &, const PrefabPropertyIdentity &) = default;
};
struct PrefabNodeIdentity final {
  foundation::Uuid id;
  runtime::Id serialized_node{};
  std::vector<PrefabPropertyIdentity> properties;
  friend bool operator==(const PrefabNodeIdentity &, const PrefabNodeIdentity &) = default;
};
struct PrefabRevisionReference final {
  foundation::Uuid asset;
  std::uint64_t revision{};
  friend bool operator==(const PrefabRevisionReference &,
                         const PrefabRevisionReference &) = default;
};
struct NestedPrefabReference final {
  foundation::Uuid instance, attachment;
  PrefabRevisionReference source;
  friend bool operator==(const NestedPrefabReference &, const NestedPrefabReference &) = default;
};
struct PrefabAsset final {
  foundation::Uuid id;
  std::uint64_t revision{1};
  // Exact serialized SceneDocument bytes, including unknown opaque components.
  std::string scene_bytes;
  std::vector<PrefabNodeIdentity> nodes;
  std::optional<PrefabRevisionReference> base;
  std::vector<NestedPrefabReference> nested;
  friend bool operator==(const PrefabAsset &, const PrefabAsset &) = default;
};
struct ResolvedPrefabInstance final {
  std::vector<foundation::Uuid> scope;
  PrefabRevisionReference source;
  std::optional<foundation::Uuid> attachment;
};
struct ResolvedPrefabGraph final {
  // Owning unique revision archive; no caller asset, document or provider borrows survive.
  std::vector<PrefabAsset> assets;
  std::vector<ResolvedPrefabInstance> instances;
  std::size_t expanded_nodes{};
};
struct InstantiatedPrefabNode final {
  std::vector<foundation::Uuid> scope;
  foundation::Uuid node;
  SceneDocument::NodeKey target;
};
class NEXORA_EDITOR_API PrefabAssets final {
public:
  static constexpr std::size_t kMaximumSceneBytes = 8 * 1024 * 1024;
  static constexpr std::size_t kMaximumAssetBytes = 16 * 1024 * 1024;
  static constexpr std::size_t kMaximumNodes = 4096;
  static constexpr std::size_t kMaximumProperties = 32768;
  static constexpr std::size_t kMaximumSources = 64;
  static constexpr std::size_t kMaximumInstances = 128;
  static constexpr std::size_t kMaximumDepth = 32;
  static constexpr std::size_t kMaximumGraphBytes = 64 * 1024 * 1024;
  // Serialized authoring call. Identity factory is used only during this call; nil/duplicates
  // reject. Previous identities survive matching serialized node/field keys; new assets start
  // revision one.
  [[nodiscard]] static std::optional<PrefabAsset> Capture(foundation::Uuid asset,
                                                          const SceneDocument &,
                                                          const std::function<foundation::Uuid()> &,
                                                          const PrefabAsset *previous = nullptr);
  [[nodiscard]] static bool Validate(const PrefabAsset &);
  [[nodiscard]] static std::optional<std::vector<std::byte>> Encode(const PrefabAsset &);
  [[nodiscard]] static std::optional<PrefabAsset> Decode(std::span<const std::byte>);
  // Canonical project-owned UUID path. Reads retain no file/document borrows. Publish requires
  // the writer lease and either a missing destination or the exact expected previous revision.
  [[nodiscard]] static std::optional<PrefabAsset> Load(const ProjectWorkspace &, foundation::Uuid);
  // Reads an immutable retained revision, or the current exact revision if not yet superseded.
  [[nodiscard]] static std::optional<PrefabAsset> LoadRevision(const ProjectWorkspace &,
                                                               PrefabRevisionReference);
  static bool Publish(const ProjectWorkspace &, const PrefabAsset &,
                      const PrefabAsset *expected = nullptr, std::string *error = nullptr);
  // Confirms wrapped publication before advancing only the document's saved baseline. Existing
  // generation, selection, clipboard and Undo/Redo remain intact. Callers serialize all owners.
  [[nodiscard]] static std::optional<PrefabAsset>
  SaveDocument(const ProjectWorkspace &, foundation::Uuid, SceneDocument &,
               const std::function<foundation::Uuid()> &, const PrefabAsset *previous = nullptr,
               std::string *error = nullptr);
  // Validates exact revisions and the complete base/nested closure before returning any result.
  // This is resolution metadata, not document-write or native code-loading authority.
  [[nodiscard]] static std::optional<ResolvedPrefabGraph>
  Resolve(PrefabRevisionReference root, std::span<const PrefabAsset> sources);
  // Bounded exact-revision closure from project storage; returns owning resolution metadata.
  [[nodiscard]] static std::optional<ResolvedPrefabGraph>
  ResolveProject(const ProjectWorkspace &, PrefabRevisionReference root);
  // Stages the complete exact nested closure outside the live document, then imports once.
  // Caller supplies current authoring authority; returned scoped mappings own all their data.
  [[nodiscard]] static std::optional<std::vector<InstantiatedPrefabNode>>
  Instantiate(PrefabRevisionReference root, std::span<const PrefabAsset> sources,
              SceneDocument &target, const SceneDocument::PreparedSave &expected, bool authorized);
  // Adds persistent Editor placement bindings together with the complete one-step forest import.
  [[nodiscard]] static std::optional<std::vector<InstantiatedPrefabNode>>
  InstantiateBound(foundation::Uuid instance, PrefabRevisionReference root,
                   std::span<const PrefabAsset> sources, SceneDocument &target,
                   const SceneDocument::PreparedSave &expected, bool authorized);

private:
  [[nodiscard]] static std::optional<std::vector<InstantiatedPrefabNode>>
  InstantiateWithPlacement(std::optional<foundation::Uuid>, PrefabRevisionReference,
                           std::span<const PrefabAsset>, SceneDocument &,
                           const SceneDocument::PreparedSave &, bool);
};
} // namespace nexora::editor
