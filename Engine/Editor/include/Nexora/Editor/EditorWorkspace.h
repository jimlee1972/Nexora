#pragma once

#include "Nexora/Editor/Api.h"
#include "Nexora/Editor/EditorProduction.h"
#include "Nexora/Editor/InspectorRotation.h"
#include "Nexora/Editor/MaterialImport.h"
#include "Nexora/Editor/MeshImport.h"
#include "Nexora/Editor/PlayInputBindings.h"
#include "Nexora/Editor/SceneAuthoring.h"
#include "Nexora/Foundation/Types.h"
#include "Nexora/Runtime/AssetPipeline.h"
#include "Nexora/Runtime/EditorSdk.h"

#include <compare>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace nexora::editor {

struct GizmoOperation;
enum class GizmoPivot;

struct PanelDescriptor final {
  std::string_view id;
  std::string_view title;
};

class NEXORA_EDITOR_API ProductShell final {
public:
  [[nodiscard]] static std::span<const PanelDescriptor> Panels() noexcept;
  [[nodiscard]] static bool IsStablePanelId(std::string_view id) noexcept;
  bool RouteCommand(std::string command);
  [[nodiscard]] std::string_view LastCommand() const noexcept { return last_command_; }

private:
  std::string last_command_;
};

struct ProjectDescriptor final {
  static constexpr std::uint32_t kSchemaVersion = 2;
  foundation::Uuid id;
  std::string name;
  std::uint32_t schema_version{kSchemaVersion};
};

enum class ProjectAccess : std::uint8_t { ReadWrite, ReadOnly };
enum class ProjectUpgradeState : std::uint8_t { Current, Applied, Required };

// Owning read-only inspection. The proposed descriptor is a plan, not publication authority.
struct ProjectUpgradePreview final {
  std::filesystem::path root;
  ProjectDescriptor project;
  ProjectUpgradeState state{ProjectUpgradeState::Current};
  std::vector<std::string> documents;
  std::string original_descriptor, proposed_descriptor;
};

class NEXORA_EDITOR_API ProjectWorkspace final {
public:
  static constexpr std::size_t kMaximumDocuments = 4096;
  static constexpr std::size_t kMaximumDocumentPathBytes = 1024;
  static constexpr std::size_t kMaximumEditorLayoutBytes = 1024 * 1024;
  static constexpr std::size_t kMaximumFrameProcessingCsvBytes = 128 * 1024;
  static constexpr std::size_t kMaximumFrameProcessingJsonBytes = 128 * 1024;
  static constexpr std::size_t kMaximumProcessMemoryJsonBytes = 128 * 1024;
  static constexpr std::size_t kMaximumGpuTimingJsonBytes = 128 * 1024;
  static constexpr std::size_t kMaximumPlayInputSettingsBytes = 1024;
  static constexpr std::size_t kMaximumProjectDescriptorBytes = 4096;
  // No writer lease, directory creation or file writes. A read-write Open always reinspects.
  [[nodiscard]] static std::optional<ProjectUpgradePreview>
  PreviewUpgrade(const std::filesystem::path &root, std::string *error = nullptr);
  ProjectWorkspace();
  ~ProjectWorkspace();
  ProjectWorkspace(ProjectWorkspace &&) noexcept;
  ProjectWorkspace &operator=(ProjectWorkspace &&) noexcept;
  ProjectWorkspace(const ProjectWorkspace &) = delete;
  ProjectWorkspace &operator=(const ProjectWorkspace &) = delete;

  bool Create(const std::filesystem::path &root, std::string name, std::string *error = nullptr);
  bool Open(const std::filesystem::path &root, std::string *error = nullptr);
  bool Open(const std::filesystem::path &root, ProjectAccess access, std::string *error = nullptr);
  bool SaveWorkspace(std::span<const std::string> open_documents, std::string *error = nullptr);
  bool RecoverWorkspace(std::string *error = nullptr);
  bool DiscardRecovery(std::string *error = nullptr);
  // Editor Play setting persisted independently from scene/workspace recovery. Empty means
  // inspection-only Play; reading never loads a module or grants project write access.
  bool SaveGameplayLibrary(std::string_view relative_path, std::string *error = nullptr);
  [[nodiscard]] std::optional<std::string> LoadGameplayLibrary(std::string *error = nullptr) const;
  // Explicit project input settings, independent from scene/workspace recovery. Missing legacy
  // settings return no value/error; unsafe, corrupt or recovery-pending reads/writes reject.
  bool SavePlayInputBindings(const PlayInputBindings &bindings, std::string *error = nullptr);
  [[nodiscard]] std::optional<PlayInputBindings>
  LoadPlayInputBindings(std::string *error = nullptr) const;
  // Export completed Editor-frame wall timing (FrameSample::cpu_ms) to a project-owned CSV.
  // Samples are borrowed only for this synchronous call; GPU/memory cells stay empty.
  bool ExportEditorFrameProcessing(std::span<const FrameSample> samples,
                                   std::uint64_t dropped_frames, std::string *error = nullptr);
  // Schema-1 JSON companion. Frame IDs/drop counts are decimal strings for lossless uint64
  // interchange; GPU/memory are explicitly unavailable and samples remain synchronous borrows.
  bool ExportEditorFrameProcessingJson(std::span<const FrameSample> samples,
                                       std::uint64_t dropped_frames, std::string *error = nullptr);
  // Reads the project CSV synchronously into a bounded owning snapshot. Read-only observers
  // may import; recovery, unsafe files and unsupported/corrupt data reject without mutation.
  [[nodiscard]] std::optional<FrameProcessingCapture>
  ImportEditorFrameProcessingCsv(std::string *error = nullptr) const;
  // Schema-1 JSON import also validates metric/scope, unavailable measurements and project UUID.
  [[nodiscard]] std::optional<FrameProcessingCapture>
  ImportEditorFrameProcessingJson(std::string *error = nullptr) const;
  // Separate schema-1 process-wide RSS trace, including unavailable sample attempts.
  bool ExportProcessMemoryJson(std::span<const ProcessMemorySample> samples,
                               std::uint64_t dropped_samples, std::string *error = nullptr);
  [[nodiscard]] std::optional<ProcessMemoryCapture>
  ImportProcessMemoryJson(std::string *error = nullptr) const;
  // Separate native completed command-buffer intervals; unavailable samples remain JSON null.
  // Destination project UUID is provenance, not ownership of the process-wide surface history.
  bool ExportGpuTimingJson(GpuProfileSource source, bool software_rasterizer,
                           std::span<const GpuProfileSample> samples, std::uint64_t dropped_samples,
                           std::string *error = nullptr);
  [[nodiscard]] std::optional<GpuTimingCapture>
  ImportGpuTimingJson(std::string *error = nullptr) const;
  bool SaveEditorLayout(std::string_view layout, std::string *error = nullptr);
  [[nodiscard]] std::optional<std::string> LoadEditorLayout(std::string *error = nullptr) const;
  [[nodiscard]] bool HasRecoveryJournal() const;
  [[nodiscard]] bool HasExternalChange() const;
  [[nodiscard]] const ProjectDescriptor &Project() const noexcept { return project_; }
  [[nodiscard]] const std::filesystem::path &Root() const noexcept { return root_; }
  [[nodiscard]] std::span<const std::string> OpenDocuments() const noexcept { return documents_; }
  [[nodiscard]] ProjectAccess Access() const noexcept { return access_; }
  [[nodiscard]] bool Writable() const noexcept { return access_ == ProjectAccess::ReadWrite; }
  [[nodiscard]] ProjectUpgradeState UpgradeState() const noexcept { return upgrade_state_; }

private:
  struct LockState;
  bool WriteWorkspace(std::span<const std::string> documents, std::string *error);
  std::filesystem::path root_;
  ProjectDescriptor project_;
  std::vector<std::string> documents_;
  std::filesystem::file_time_type workspace_write_time_{};
  std::unique_ptr<LockState> lock_;
  ProjectAccess access_{ProjectAccess::ReadOnly};
  ProjectUpgradeState upgrade_state_{ProjectUpgradeState::Current};
};

struct RecentProject final {
  foundation::Uuid id;
  std::filesystem::path root;
  std::string name;
};

// User-level, versioned recent-project state. The application owns this store separately from a
// project so opening a project read-only never grants write access to project-owned files.
class NEXORA_EDITOR_API RecentProjectStore final {
public:
  static constexpr std::size_t kMaximumEntries = 12;

  [[nodiscard]] static std::filesystem::path DefaultPath();
  bool Open(const std::filesystem::path &path, std::string *error = nullptr);
  bool Record(const ProjectWorkspace &workspace, std::string *error = nullptr);
  bool Remove(foundation::Uuid id, std::string *error = nullptr);
  [[nodiscard]] std::span<const RecentProject> Entries() const noexcept { return entries_; }
  [[nodiscard]] const std::filesystem::path &Path() const noexcept { return path_; }

private:
  bool Save(std::string *error);
  std::filesystem::path path_;
  std::vector<RecentProject> entries_;
};

// Admission bounds for one Content index. A tree beyond either bound is rejected as a whole, before
// any identity sidecar is written, so the previous index and project files stay untouched.
inline constexpr std::size_t kMaximumIndexedAssets = 262144;
inline constexpr std::size_t kMaximumIndexedPathBytes = 1024; // UTF-8, project-relative.
struct AssetIndexLimits final {
  std::size_t max_assets = kMaximumIndexedAssets;
  std::size_t max_path_bytes = kMaximumIndexedPathBytes;
};

enum class ImportState { Pending, Imported, Cancelled, Failed };
enum class AssetIdentityMode { DerivedFromPath, PersistentReadOnly, PersistentReadWrite };

struct AssetEntry final {
  runtime::AssetUuid id;
  std::string relative_path; // Owning UTF-8, with portable '/' separators.
  std::string type;
  std::string artifact_hash;
  ImportState state{ImportState::Pending};
  std::string error;
  // Immutable CPU geometry for successfully imported triangulated .obj assets; owning across
  // workspace copies. GPU residency and reimport publication are separate contracts.
  std::shared_ptr<const MeshGeometry> mesh{};
  // Owning immutable schema-1 scalar PBR data, retained across model/Undo copies.
  std::shared_ptr<const MaterialAsset> material{};
};

class NEXORA_EDITOR_API AssetWorkspace final {
public:
  using Cancelled = std::function<bool()>;
  using Progress = std::function<void(std::size_t, std::size_t)>;
  // `limits` bounds the number of indexed files (".meta" sidecars excluded) and each project-
  // relative UTF-8 path. Exceeding either fails before any sidecar is created and keeps the
  // existing index.
  // Ordinary asset sources use fixed-size binary read chunks and incremental hashes, with
  // cancellation checks between reads. Failed/cancelled entries never carry a partial artifact.
  // OBJ/material parsing retains its bounded source/payload policy; live publication is
  // caller-owned.
  bool ImportTree(const std::filesystem::path &content_root, Cancelled cancelled = {},
                  Progress progress = {},
                  AssetIdentityMode identity_mode = AssetIdentityMode::DerivedFromPath,
                  std::string *error = nullptr, AssetIndexLimits limits = {});
  // Reads only one already-saved .scene and its sidecar in this initialized Content index.
  // Bounds source bytes at 64 MiB, preserves other entries/geometry, and checks IDs against the
  // current in-memory index. No directory scan, OBJ parsing, GPU work, or project-wide refresh.
  // A moved scene UUID retargets its stale entry only when both old source and sidecar are absent.
  bool ImportSavedScene(const std::filesystem::path &relative_path, std::string *error = nullptr);
  [[nodiscard]] static std::filesystem::path
  IdentitySidecar(const std::filesystem::path &asset_path);
  [[nodiscard]] std::vector<const AssetEntry *> Search(std::string_view query,
                                                       std::string_view type = {}) const;
  [[nodiscard]] const AssetEntry *Find(runtime::AssetUuid id) const;
  [[nodiscard]] std::span<const AssetEntry> Entries() const noexcept { return entries_; }
  [[nodiscard]] bool PersistentIdentities() const noexcept {
    return identity_mode_ != AssetIdentityMode::DerivedFromPath;
  }
  [[nodiscard]] bool WritableIdentities() const noexcept {
    return identity_mode_ == AssetIdentityMode::PersistentReadWrite;
  }
  [[nodiscard]] const std::filesystem::path &ContentRoot() const noexcept { return content_root_; }

private:
  std::vector<AssetEntry> entries_;
  std::filesystem::path content_root_;
  AssetIdentityMode identity_mode_{AssetIdentityMode::DerivedFromPath};
};

// Editor view of one runtime scene: node names plus selection, clipboard, and persistence. The
// hierarchy itself lives in the runtime (Entity::parent); this class reads it from there.
class NEXORA_EDITOR_API SceneDocument final {
public:
  struct NodeKey final {
    runtime::Id id{};
    std::uint64_t entity_generation{};
    std::uint64_t document_generation{};
    auto operator<=>(const NodeKey &) const = default;
  };
  struct NodeView final {
    runtime::Id id{}, parent{};
    std::string_view name;
    std::uint64_t entity_generation{};
    std::uint64_t document_generation{};
    [[nodiscard]] NodeKey Key() const noexcept {
      return {id, entity_generation, document_generation};
    }
  };
  // Owning Runtime bytes and Editor-owned opaque metadata; names/Euler hints are authoring data
  // and are intentionally absent. Runtime entities without a tracked Editor node stay in the
  // snapshot without fabricated metadata. Nodes follow Runtime storage order, opaque types sort
  // ascending, and all keys retain the captured document identity (not an authoring revision).
  struct RuntimeSceneCapture final {
    struct NodeMetadata final {
      NodeKey key;
      std::vector<OpaqueComponent> opaque;
      bool operator==(const NodeMetadata &) const = default;
    };
    runtime::Id scene{};
    std::uint64_t document_generation{};
    std::string runtime_snapshot;
    std::vector<NodeMetadata> nodes;
    bool operator==(const RuntimeSceneCapture &) const = default;
  };
  static constexpr std::size_t kMaximumRuntimeCaptureEntities = 100'000;
  static constexpr std::size_t kMaximumRuntimeCaptureBytes = 64 * 1024 * 1024;
  // Owning, immutable preparation for deferred single-file publication.
  class PreparedSave final {
  public:
    [[nodiscard]] const std::string &Bytes() const noexcept { return bytes_; }
    [[nodiscard]] std::uint64_t Generation() const noexcept { return generation_; }

  private:
    friend class SceneDocument;
    friend class SceneSaveBatch;
    friend class PrefabAssets;
    PreparedSave(std::uint64_t generation, std::string bytes, std::string signature,
                 std::string opaque_records)
        : generation_(generation), bytes_(std::move(bytes)), signature_(std::move(signature)),
          opaque_records_(std::move(opaque_records)) {}
    std::uint64_t generation_{};
    std::string bytes_, signature_, opaque_records_;
  };
  SceneDocument(runtime::World &world, runtime::Id scene);
  // Creates a node; with a parent the new entity starts at the parent's origin (identity local).
  runtime::Id Create(std::string name, runtime::Id parent = 0);
  // Creates one default Camera/Light node at identity local TRS, with a single Undo transaction.
  runtime::Id CreateCamera(std::string name, runtime::Id parent = 0);
  runtime::Id CreateLight(std::string name, runtime::Id parent = 0);
  // Creates one initialized mesh root with one Undo; caller owns asset generation/access checks.
  runtime::Id CreateMesh(std::string name, runtime::MeshComponent mesh,
                         runtime::Transform transform = {});
  bool Select(std::span<const runtime::Id> entities);
  bool Select(std::span<const NodeKey> entities);
  bool Rename(NodeKey entity, std::string name);
  // Undoable; keeps the entity's world pose like dragging in Unity's Hierarchy. Rejects cycles.
  bool Reparent(runtime::Id entity, runtime::Id parent);
  // A Hierarchy drag: reparent keeping the world pose and place the node at `index` among its new
  // siblings (clamped to the last position), as one undo step.
  bool Move(runtime::Id entity, runtime::Id parent, std::size_t index);
  bool Move(NodeKey entity, std::optional<NodeKey> parent, std::size_t index);
  bool SetTransform(runtime::Id entity, runtime::Transform transform);
  bool SetCamera(NodeKey entity, std::optional<runtime::CameraComponent> camera);
  bool SetCameras(std::span<const NodeKey> entities,
                  std::span<const std::optional<runtime::CameraComponent>> cameras);
  // Sets one Camera's world position/rotation as one Undo step, preserving its lens, local scale
  // and parent. Inverts each ancestor TRS, so positions remain exact under shear/mirrored parents.
  // Stale/missing/non-Camera targets and unrepresentable poses fail before mutation; no-ops keep
  // Redo.
  bool AlignCameraToWorldPose(NodeKey entity, runtime::Transform world_pose);
  bool SetLight(NodeKey entity, std::optional<runtime::LightComponent> light);
  bool SetLights(std::span<const NodeKey> entities,
                 std::span<const std::optional<runtime::LightComponent>> lights);
  // Atomic, generation-checked reset of existing components only. Missing components remain
  // absent; empty/stale/duplicate batches reject, and already-default batches retain Redo.
  bool ResetCameras(std::span<const NodeKey> entities);
  bool ResetLights(std::span<const NodeKey> entities);
  bool SetMeshRenderer(NodeKey entity, std::optional<runtime::MeshComponent> mesh);
  bool SetMeshRenderers(std::span<const NodeKey> entities,
                        std::span<const std::optional<runtime::MeshComponent>> meshes);
  // Editor-owned missing-plugin payloads. Writes are generation checked and undoable;
  // inspection returns an owning copy, never a pointer into node storage.
  bool SetOpaqueComponent(NodeKey entity, OpaqueComponent component);
  struct OpaqueComponentEdit final {
    NodeKey entity;
    OpaqueComponent expected;
    OpaqueComponent replacement;
  };
  // Existing-component compare-and-replace: all keys/bytes/budgets validate before publication.
  // One independent metadata Undo; rejected/no-op requests preserve history and every payload.
  // The serialized authoring owner enforces workspace access/recovery/Play authority.
  bool ApplyOpaqueComponents(std::span<const OpaqueComponentEdit> edits);
  [[nodiscard]] std::optional<std::vector<OpaqueComponent>> OpaqueComponents(NodeKey entity) const;
  [[nodiscard]] std::optional<std::vector<OpaqueComponentInfo>>
  InspectOpaqueComponents(NodeKey entity) const;
  bool SetTransforms(std::span<const NodeKey> entities,
                     std::span<const runtime::Transform> transforms);
  // Applies one local TRS and its matching finite authored Euler values to a nonempty,
  // generation-checked selection as one atomic Undo. Invalid/duplicate targets reject before
  // mutation; equal values preserve Redo. The caller owns workspace access policy.
  bool SetTransformValues(std::span<const NodeKey> entities, runtime::Transform transform,
                          const EulerDegrees &degrees);
  // Resets local TRS and authored Euler revolutions together as one Undo. Parent, selection and
  // unrelated components stay unchanged; already-default batches retain Redo.
  bool ResetTransforms(std::span<const NodeKey> entities);
  // Moves generation-checked selection roots by a world X/Z delta as one atomic undo step.
  bool TranslateSelectionXZ(std::span<const NodeKey> entities, double dx, double dz);
  bool TranslateSelection(std::span<const NodeKey> entities, double dx, double dy, double dz);
  // Applies one validated world-space gizmo operation to selection roots as one Undo step.
  bool ApplySelectionGizmo(std::span<const NodeKey> entities, const GizmoOperation &operation);
  // Owning world poses for a prospective gesture, including descendants. Uses the same root
  // validation and local edits as commit; does not change scene state, selection, or history.
  [[nodiscard]] std::optional<std::unordered_map<runtime::Id, runtime::Transform>>
  PreviewSelectionGizmo(std::span<const NodeKey> entities, const GizmoOperation &operation) const;
  // Owning exact affine matrices for the same prospective local edits, without mutation.
  [[nodiscard]] std::optional<std::unordered_map<runtime::Id, runtime::TransformMatrix>>
  PreviewSelectionGizmoMatrices(std::span<const NodeKey> entities,
                                const GizmoOperation &operation) const;
  // First selected root's rotation, at its origin (Pivot) or the mean selected-root origin
  // (Center). Selected descendants do not weight the center twice.
  [[nodiscard]] std::optional<runtime::Transform> SelectionGizmoFrame(GizmoPivot pivot) const;
  // Degrees use extrinsic Z-X-Y composition. One field edit is one atomic undo transaction.
  bool SetEulerField(std::span<const NodeKey> entities, std::size_t axis, double degrees);
  // Preserves authored revolutions while the local quaternion matches; otherwise canonical angles.
  [[nodiscard]] std::optional<EulerDegrees> EulerAngles(runtime::Id entity) const noexcept;
  [[nodiscard]] std::optional<runtime::Transform> Transform(runtime::Id entity) const noexcept;
  [[nodiscard]] std::optional<runtime::CameraComponent> Camera(NodeKey entity) const noexcept;
  [[nodiscard]] std::optional<runtime::LightComponent> Light(NodeKey entity) const noexcept;
  [[nodiscard]] std::optional<runtime::MeshComponent> MeshRenderer(NodeKey entity) const noexcept;
  [[nodiscard]] std::optional<runtime::Transform> WorldTransform(runtime::Id entity) const noexcept;
  // Owning current-document poses in World storage order, resolved together. Includes only
  // tracked nodes; untracked ancestors still participate in their exact world matrices.
  // Serialized owner-thread observation, not an authoring transaction or persistent cache.
  [[nodiscard]] std::optional<std::vector<runtime::SceneWorldPose>> WorldPoses() const;
  [[nodiscard]] std::optional<runtime::TransformMatrix>
  WorldMatrix(runtime::Id entity) const noexcept;
  // Copies selected-root forests, initialized components and authoring metadata into owned storage.
  // Paste/Duplicate create the complete forest as one Undo, preserving copy-time world root poses.
  bool CopySelection();
  // Copies then atomically deletes the selection; failure preserves the previous clipboard.
  // The next successful Paste preserves root names, then the retained snapshot becomes a copy.
  bool CutSelection();
  bool Paste();
  struct ImportedForestNode final {
    runtime::Id source{};
    NodeKey target;
  };
  static constexpr std::size_t kMaximumImportedForestNodes = 4096;
  static constexpr std::size_t kMaximumImportedForestBytes = 8 * 1024 * 1024;
  // Parses a complete owning source revision, then clones its ordered forest as one Undo.
  // Authority/current document observation are rechecked; no file IO or user clipboard changes.
  [[nodiscard]] std::optional<std::vector<ImportedForestNode>>
  ImportForestBytes(const PreparedSave &expected, std::string_view source, bool authorized);
  // Duplicates the current selection without replacing the user's copied clipboard.
  bool DuplicateSelection();
  // Deletes selected subtrees as one atomic Undo; selected descendants are not deleted twice.
  bool DeleteSelection();
  bool Undo();
  bool Redo();
  // Serialized authoring-thread calls. Prepare performs no IO or baseline/history mutation.
  [[nodiscard]] std::optional<PreparedSave> PrepareSave() const;
  // Current authoring content/generation check without IO or saved-baseline/history mutation.
  [[nodiscard]] bool MatchesPreparedSave(const PreparedSave &prepared) const;
  // Rejects changed document generation or content before IO; advances the baseline only after
  // successful single-file publication. Caller owns workspace access and destination policy.
  bool SavePrepared(const std::filesystem::path &path, const PreparedSave &prepared) const;
  bool Save(const std::filesystem::path &path) const;
  // Clears written_bytes before attempting IO; on success it owns exactly the bytes this Save
  // published, including authoring metadata, independent of subsequent external file changes.
  // Failure preserves document state/history and leaves written_bytes empty. Calls are serialized.
  bool Save(const std::filesystem::path &path, std::string *written_bytes) const;
  // Starts an unsaved empty document, preserving World scene ID/name/state/persistence.
  // Advances generations and clears selection/clipboard/history; rejected replacement is atomic.
  // This document boundary is not an Undo step. Caller owns workspace/dirty-content decisions.
  bool NewScene();
  bool Reload(const std::filesystem::path &path);
  // Reuses the versioned file parser for an already bounded owning/caller-held byte revision.
  // No filesystem IO; success/rejection has the same atomic replacement/history contract as Reload.
  bool ReloadBytes(std::string_view bytes);
  // Rejects excess authoring nodes during ingestion, before legacy hierarchy validation/migration.
  bool ReloadBytes(std::string_view bytes, std::size_t maximum_nodes);
  // Authoring-thread, synchronous capture of a live Editor World scene. Preflights identities,
  // opaque metadata and bounds before serialization/copy; serialization uses World authority
  // with the byte cap. No IO, World borrow in the result, or history/selection/baseline mutation.
  // A rejected capture returns no partial value and an actionable error when requested.
  [[nodiscard]] std::optional<RuntimeSceneCapture>
  CaptureRuntimeScene(std::string *error = nullptr) const;
  // Compares the live, serializable scene with the last successful Save or Reload.
  [[nodiscard]] bool Dirty() const;
  [[nodiscard]] std::span<const runtime::Id> Selection() const noexcept { return selection_; }
  [[nodiscard]] std::optional<NodeKey> Key(runtime::Id entity) const noexcept;
  [[nodiscard]] std::uint64_t Generation() const noexcept { return document_generation_; }
  [[nodiscard]] std::optional<runtime::Id> Parent(runtime::Id entity) const;
  [[nodiscard]] std::string_view Name(runtime::Id entity) const;
  // In runtime sibling order (scene storage order), so a Hierarchy view can list children as
  // ordered by Move and the runtime.
  [[nodiscard]] std::vector<NodeView> Nodes() const;

private:
  friend class AdditiveSceneSession;
  friend class PrefabAssets;
  enum class BuiltinEntity { Empty, Camera, Light };
  friend class SceneSaveBatch;
  bool ReloadOwnedBytes(std::string bytes, std::optional<std::size_t> maximum_nodes = std::nullopt);
  runtime::Id CreateBuiltin(std::string name, runtime::Id parent, BuiltinEntity kind);
  runtime::Id AdoptCreatedEntity(runtime::Id entity, std::string name);
  [[nodiscard]] std::optional<std::vector<std::pair<NodeKey, runtime::Transform>>>
  SelectionGizmoEdits(std::span<const NodeKey> entities, const GizmoOperation &operation) const;
  [[nodiscard]] std::optional<std::string> StateSignature() const;
  struct EulerHint final {
    runtime::Transform transform;
    EulerDegrees degrees;
  };
  struct Node final {
    runtime::Id id{};
    std::string name;
    std::uint64_t generation{};
    std::optional<EulerHint> euler_hint{};
    std::vector<OpaqueComponent> opaque{};
  };
  struct ClipboardNode final {
    std::string name;
    runtime::Entity entity;
    std::optional<EulerHint> euler_hint{};
    std::vector<OpaqueComponent> opaque{};
  };
  struct UndoEntry final {
    enum class Kind { Runtime, Rename, Opaque, OpaqueBatch } kind{Kind::Runtime};
    NodeKey entity;
    std::string previous_name;
    std::vector<std::pair<NodeKey, std::optional<EulerHint>>> previous_hints{};
    std::vector<Node> deleted_nodes{};
    std::vector<runtime::Id> previous_selection{};
    std::vector<Node> redo_nodes{};
    std::vector<runtime::Id> redo_selection{};
    std::vector<OpaqueComponent> previous_opaque{};
    bool restore_selection{};
    std::vector<std::pair<NodeKey, std::vector<OpaqueComponent>>> previous_opaque_batch{};
  };
  void PushUndo(UndoEntry entry);
  runtime::World &world_;
  runtime::Id scene_{};
  runtime::SceneEditor editor_;
  std::vector<Node> nodes_;
  std::vector<runtime::Id> selection_;
  std::vector<ClipboardNode> clipboard_;
  bool clipboard_cut_pending_{};
  std::vector<UndoEntry> undo_;
  std::vector<UndoEntry> redo_;
  std::uint64_t document_generation_{};
  std::uint64_t next_entity_generation_{1};
  mutable std::string saved_signature_;
  mutable std::string saved_opaque_records_;
  mutable std::optional<bool> opaque_dirty_{false};
};

} // namespace nexora::editor
