#pragma once

#include "Nexora/EditorImGui/EditorImGui.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace nexora::editor::imgui {

// Test-only inspection surface for validating the optional module across a shared-library
// boundary. It intentionally exposes no Dear ImGui types so tests cannot bind a second copy of
// Dear ImGui's process-global context state.
struct EditorImGuiTestState final {
  bool keyboard_navigation_enabled = false;
  bool platform_viewports_enabled = false;
  bool input_trickle_enabled = false;
  float display_width = 0.0F;
  float display_height = 0.0F;
  float framebuffer_scale = 0.0F;
  float font_global_scale = 0.0F;
  std::uint32_t hierarchy_visible_rows = 0;
  std::uint32_t hierarchy_rendered_rows = 0;
  std::uint32_t hierarchy_selection = 0;
  SceneDocument::NodeKey hierarchy_selection_anchor;
  std::uint32_t inspector_selection = 0;
  bool inspector_transform_visible = false;
  std::uint32_t content_visible_items = 0;
  std::uint32_t content_visible_folders = 0;
  std::uint32_t content_selection = 0;
  std::uint32_t content_forward_dependencies = 0;
  std::uint32_t content_reverse_dependencies = 0;
  std::uint32_t content_dependency_cycle = 0;
  bool content_import_active = false;
  ImportOperationState content_import_state = ImportOperationState::Succeeded;
  std::uint32_t content_import_diagnostics = 0;
  std::uint32_t content_conflicts = 0;
  bool content_conflict_visible = false;
  bool content_conflict_compare_visible = false;
  DirtyConflictChoice content_conflict_choice = DirtyConflictChoice::Pending;
  bool project_writable = false;
  bool project_upgrade_required = false;
  std::uint32_t recent_projects = 0;
  bool project_selector_visible = false;
  std::uint32_t selector_recent_projects = 0;
  bool app_focused = false;
  bool selector_root_focus_pending = false;
  bool selector_root_active = false;
};

class NEXORA_EDITOR_IMGUI_API EditorImGuiTestAccess final {
public:
  [[nodiscard]] static EditorImGuiTestState Inspect(const EditorImGuiHost &host) noexcept;
  [[nodiscard]] static std::string_view ProjectSelectorRoot(const EditorImGuiHost &host) noexcept;
  static void SetInputTrickle(EditorImGuiHost &host, bool enabled) noexcept;
  static void SetHierarchyFilter(EditorImGuiHost &host, std::string_view filter) noexcept;
  [[nodiscard]] static std::vector<OpaqueComponentInfo>
  InspectorOpaqueInfo(const EditorImGuiHost &host);
  [[nodiscard]] static std::array<float, 2> PointerPosition(const EditorImGuiHost &host) noexcept;
  [[nodiscard]] static std::optional<std::array<float, 2>>
  PlayApplyPosition(const EditorImGuiHost &host, bool confirm) noexcept;
  [[nodiscard]] static bool PlayApplyOpen(const EditorImGuiHost &host) noexcept;
  static void FocusProfiler(EditorImGuiHost &host) noexcept;
  [[nodiscard]] static std::optional<std::array<float, 2>>
  ProfileExportPosition(const EditorImGuiHost &host) noexcept;
  static void FocusHierarchy(EditorImGuiHost &host) noexcept;
  [[nodiscard]] static std::optional<std::array<float, 2>>
  SceneMarkerPosition(const EditorImGuiHost &host, SceneDocument::NodeKey entity) noexcept;
  [[nodiscard]] static std::array<float, 2>
  SceneOverviewCenter(const EditorImGuiHost &host) noexcept;
  static void SetSceneSnap(EditorImGuiHost &host, bool enabled, int step_index) noexcept;
  static void QueueHierarchySelection(EditorImGuiHost &host, SceneDocument::NodeKey entity,
                                      bool additive, bool range) noexcept;
  static void QueueHierarchyMove(EditorImGuiHost &host, SceneDocument::NodeKey entity,
                                 std::optional<SceneDocument::NodeKey> parent,
                                 std::size_t index) noexcept;
  static void QueueHierarchyCreate(EditorImGuiHost &host, std::string name,
                                   std::optional<SceneDocument::NodeKey> parent = std::nullopt);
  static void QueueHierarchyReorder(EditorImGuiHost &host, int direction) noexcept;
  static void QueueHierarchyExpansion(EditorImGuiHost &host, SceneDocument::NodeKey entity,
                                      bool expanded) noexcept;
  static void QueueHierarchyRename(EditorImGuiHost &host, SceneDocument::NodeKey entity,
                                   std::string name);
  static void QueueInspectorTransform(EditorImGuiHost &host, SceneDocument::NodeKey entity,
                                      runtime::Transform transform) noexcept;
  static void QueueInspectorMesh(EditorImGuiHost &host, SceneDocument::NodeKey entity,
                                 std::optional<runtime::AssetUuid> asset,
                                 std::uint64_t generation) noexcept;
  static void SelectPlayEntity(EditorImGuiHost &host, runtime::Id entity) noexcept;
  [[nodiscard]] static runtime::Id PlayInspectorEntity(const EditorImGuiHost &host) noexcept;
  static void
  QueueInspectorCameras(EditorImGuiHost &host, std::span<const SceneDocument::NodeKey> entities,
                        std::span<const std::optional<runtime::CameraComponent>> cameras);
  static void QueueInspectorLights(EditorImGuiHost &host,
                                   std::span<const SceneDocument::NodeKey> entities,
                                   std::span<const std::optional<runtime::LightComponent>> lights);
  [[nodiscard]] static std::array<bool, 6>
  InspectorComponentMixed(const EditorImGuiHost &host) noexcept;
  static void FocusInspectorLightField(EditorImGuiHost &host) noexcept;
  static void FocusInspectorCameraField(EditorImGuiHost &host, std::size_t axis) noexcept;
  static void QueueInspectorCamera(EditorImGuiHost &host, SceneDocument::NodeKey entity,
                                   std::optional<runtime::CameraComponent> camera) noexcept;
  static void QueueInspectorLight(EditorImGuiHost &host, SceneDocument::NodeKey entity,
                                  std::optional<runtime::LightComponent> light) noexcept;
  static void QueueInspectorTransforms(EditorImGuiHost &host,
                                       std::span<const SceneDocument::NodeKey> entities,
                                       std::span<const runtime::Transform> transforms);
  static void QueueInspectorEulerField(EditorImGuiHost &host,
                                       std::span<const SceneDocument::NodeKey> entities,
                                       std::size_t axis, double degrees);
  static void FocusInspectorEulerField(EditorImGuiHost &host, std::size_t axis) noexcept;
  [[nodiscard]] static std::optional<std::array<double, 3>>
  InspectorEulerAngles(const EditorImGuiHost &host, SceneDocument::NodeKey entity) noexcept;
  static void QueueProjectSelection(EditorImGuiHost &host, ProjectSelectorRequest request);
  static void QueueProjectImportCancellation(EditorImGuiHost &host) noexcept;
  static void QueueContentConflictChoice(EditorImGuiHost &host, runtime::AssetUuid asset,
                                         DirtyConflictChoice choice) noexcept;
  [[nodiscard]] static std::uint32_t OverrideDrawTexture(EditorImGuiHost &host,
                                                         std::uint64_t texture_id) noexcept;
};

} // namespace nexora::editor::imgui
