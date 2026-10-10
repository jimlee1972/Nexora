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

struct EditorHierarchyTestRow final {
  SceneDocument::NodeKey key;
  std::uint32_t depth{};
  bool has_children{};
};

class NEXORA_EDITOR_IMGUI_API EditorImGuiTestAccess final {
public:
  [[nodiscard]] static std::optional<std::array<float, 2>>
  DiagnosticPosition(const EditorImGuiHost &, std::size_t);
  [[nodiscard]] static std::array<std::size_t, 2> DiagnosticRows(const EditorImGuiHost &);
  [[nodiscard]] static std::optional<std::array<float, 2>> SceneTabPosition(const EditorImGuiHost &,
                                                                            std::uint64_t);
  [[nodiscard]] static std::optional<std::array<float, 2>> SceneTabControl(const EditorImGuiHost &,
                                                                           std::size_t);
  static void SetSceneTabPath(EditorImGuiHost &, std::string_view);
  [[nodiscard]] static std::vector<SceneTabItem> SceneTabs(const EditorImGuiHost &);
  [[nodiscard]] static std::optional<std::array<float, 2>>
  SceneComparisonPosition(const EditorImGuiHost &, std::size_t);
  [[nodiscard]] static SceneComparisonSnapshot SceneComparisonStatus(const EditorImGuiHost &);
  [[nodiscard]] static std::uint32_t SceneComparisonRenderedRows(const EditorImGuiHost &);
  [[nodiscard]] static std::optional<std::array<float, 2>>
  StaticExportPosition(const EditorImGuiHost &, std::size_t);
  [[nodiscard]] static StaticExportSnapshot StaticExportStatus(const EditorImGuiHost &);
  [[nodiscard]] static EditorImGuiTestState Inspect(const EditorImGuiHost &host) noexcept;
  [[nodiscard]] static std::optional<std::array<float, 2>>
  ProjectUpgradePosition(const EditorImGuiHost &host, std::size_t control) noexcept;
  [[nodiscard]] static std::string_view ProjectSelectorRoot(const EditorImGuiHost &host) noexcept;
  // Deterministic event batches default to portable Ctrl semantics; production OS policy is
  // untouched.
  static void ConfigureSyntheticInput(EditorImGuiHost &host, bool macos_behaviors = false) noexcept;
  static void InvokeImeCallback(EditorImGuiHost &host, float x, float y, bool visible) noexcept;
  [[nodiscard]] static std::vector<std::byte> NativeTexturePixels(const EditorImGuiHost &host,
                                                                  std::uint64_t texture_id);
  [[nodiscard]] static std::optional<std::array<float, 2>>
  HierarchyRenamePosition(const EditorImGuiHost &host) noexcept;
  [[nodiscard]] static bool HierarchyRenameOpen(const EditorImGuiHost &host) noexcept;
  [[nodiscard]] static std::string_view HierarchyRenameText(const EditorImGuiHost &host) noexcept;
  static void SetHierarchyFilter(EditorImGuiHost &host, std::string_view filter) noexcept;
  [[nodiscard]] static std::vector<OpaqueComponentInfo>
  InspectorOpaqueInfo(const EditorImGuiHost &host);
  [[nodiscard]] static std::optional<std::array<float, 2>>
  ReflectedPropertyPosition(const EditorImGuiHost &host, std::string_view path);
  [[nodiscard]] static std::optional<bool> ReflectedPropertyMixed(const EditorImGuiHost &host,
                                                                  std::string_view path);
  [[nodiscard]] static std::optional<std::array<float, 2>>
  ReflectedChoicePosition(const EditorImGuiHost &host, std::string_view path,
                          std::string_view label);
  [[nodiscard]] static std::array<float, 2> PointerPosition(const EditorImGuiHost &host) noexcept;
  [[nodiscard]] static std::optional<std::array<float, 2>>
  PlayApplyPosition(const EditorImGuiHost &host, bool confirm) noexcept;
  [[nodiscard]] static bool PlayApplyOpen(const EditorImGuiHost &host) noexcept;
  static void FocusContent(EditorImGuiHost &host) noexcept;
  [[nodiscard]] static std::optional<std::filesystem::path>
  ContentFocusedFolder(const EditorImGuiHost &host);
  static void FocusGame(EditorImGuiHost &host) noexcept;
  [[nodiscard]] static bool GameInputBindingsOpen(const EditorImGuiHost &host) noexcept;
  [[nodiscard]] static std::string_view
  GameInputBindingsError(const EditorImGuiHost &host) noexcept;
  [[nodiscard]] static std::optional<std::array<float, 2>>
  GameInputBindingPosition(const EditorImGuiHost &host, std::size_t control) noexcept;
  [[nodiscard]] static std::optional<std::array<float, 2>>
  GameInputChoicePosition(const EditorImGuiHost &host, PlayInputControl control) noexcept;
  [[nodiscard]] static std::optional<std::array<float, 2>>
  ContentSearchPosition(const EditorImGuiHost &host) noexcept;
  [[nodiscard]] static std::optional<std::array<float, 2>>
  ContentAddMeshPosition(const EditorImGuiHost &host) noexcept;
  [[nodiscard]] static std::optional<std::array<float, 2>>
  ContentOpenScenePosition(const EditorImGuiHost &host) noexcept;
  [[nodiscard]] static std::optional<std::array<float, 2>>
  ContentRenamePosition(const EditorImGuiHost &host, std::size_t control) noexcept;
  [[nodiscard]] static std::string_view ContentRenameText(const EditorImGuiHost &host) noexcept;
  [[nodiscard]] static std::optional<std::array<float, 2>>
  ContentAssetPosition(const EditorImGuiHost &host, runtime::AssetUuid asset) noexcept;
  [[nodiscard]] static bool ContentDragActive(const EditorImGuiHost &host) noexcept;
  static void FocusProfiler(EditorImGuiHost &host) noexcept;
  [[nodiscard]] static std::optional<std::array<float, 2>>
  ProfileCapturePosition(const EditorImGuiHost &host) noexcept;
  [[nodiscard]] static std::optional<std::array<float, 2>>
  ProfileClearPosition(const EditorImGuiHost &host) noexcept;
  [[nodiscard]] static ProcessMemoryObservation ProfileMemory(const EditorImGuiHost &host) noexcept;
  [[nodiscard]] static std::optional<std::array<float, 2>>
  MemoryControlPosition(const EditorImGuiHost &host, std::size_t control) noexcept;
  [[nodiscard]] static const ProcessMemoryCapture *
  ImportedMemoryCapture(const EditorImGuiHost &host) noexcept;

  [[nodiscard]] static GpuProfileObservation ProfileGpu(const EditorImGuiHost &host) noexcept;
  [[nodiscard]] static std::optional<std::array<float, 2>>
  GpuControlPosition(const EditorImGuiHost &host, std::size_t control) noexcept;
  [[nodiscard]] static const GpuTimingCapture *
  ImportedGpuCapture(const EditorImGuiHost &host) noexcept;
  static void FocusConsole(EditorImGuiHost &host) noexcept;
  [[nodiscard]] static std::optional<std::array<float, 2>>
  ConsoleControlPosition(const EditorImGuiHost &host, std::size_t control) noexcept;
  [[nodiscard]] static std::size_t ConsoleVisibleCount(const EditorImGuiHost &host) noexcept;
  [[nodiscard]] static std::optional<std::uint64_t>
  ConsoleFirstVisibleSequence(const EditorImGuiHost &host) noexcept;
  static void SetConsoleFilter(EditorImGuiHost &host, std::string_view text, int severity);
  [[nodiscard]] static std::optional<std::array<float, 2>>
  ProfileExportPosition(const EditorImGuiHost &host) noexcept;
  [[nodiscard]] static std::optional<std::array<float, 2>>
  ProfileJsonExportPosition(const EditorImGuiHost &host) noexcept;
  [[nodiscard]] static std::optional<std::array<float, 2>>
  ProfileCsvImportPosition(const EditorImGuiHost &host) noexcept;
  [[nodiscard]] static std::optional<std::array<float, 2>>
  ProfileJsonImportPosition(const EditorImGuiHost &host) noexcept;
  [[nodiscard]] static std::optional<std::array<float, 2>>
  ProfileImportClearPosition(const EditorImGuiHost &host) noexcept;
  [[nodiscard]] static const FrameProcessingCapture *
  ImportedProfileCapture(const EditorImGuiHost &host) noexcept;
  [[nodiscard]] static std::optional<std::array<float, 2>>
  HierarchyCutPosition(const EditorImGuiHost &host) noexcept;
  static void FocusHierarchy(EditorImGuiHost &host) noexcept;
  [[nodiscard]] static bool HierarchyDragActive(const EditorImGuiHost &host) noexcept;
  [[nodiscard]] static std::optional<std::array<float, 2>>
  HierarchyRowPosition(const EditorImGuiHost &host, SceneDocument::NodeKey key) noexcept;
  static void FocusScene(EditorImGuiHost &host) noexcept;
  [[nodiscard]] static std::optional<std::array<float, 2>>
  NativeSceneToolPosition(const EditorImGuiHost &host, NativeSceneTool tool) noexcept;
  [[nodiscard]] static std::optional<std::array<float, 2>>
  SceneFramePosition(const EditorImGuiHost &host) noexcept;
  [[nodiscard]] static std::optional<std::array<float, 2>>
  SceneFrameAllPosition(const EditorImGuiHost &host) noexcept;
  [[nodiscard]] static std::optional<std::array<float, 2>>
  SceneSelectAllPosition(const EditorImGuiHost &host) noexcept;
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
  [[nodiscard]] static std::string_view SceneFileText(const EditorImGuiHost &host) noexcept;
  // File menu/New/Open/Save As/path/submit/Cancel/Save/Discard/Replace/menu Save (0..10).
  [[nodiscard]] static std::optional<std::array<float, 2>>
  SceneFilePosition(const EditorImGuiHost &host, std::size_t control) noexcept;
  // Controls: kind combo, Empty/Camera/Light entries, Create root, Create child, name (0..6).
  [[nodiscard]] static std::optional<std::array<float, 2>>
  HierarchyCreatePosition(const EditorImGuiHost &host, std::size_t control) noexcept;
  static void QueueHierarchyCreate(EditorImGuiHost &host, std::string name,
                                   std::optional<SceneDocument::NodeKey> parent = std::nullopt);
  static void QueueHierarchyReorder(EditorImGuiHost &host, int direction) noexcept;
  static void QueueHierarchyExpansion(EditorImGuiHost &host, SceneDocument::NodeKey entity,
                                      bool expanded) noexcept;
  static void SetHierarchyExpanded(EditorImGuiHost &host,
                                   std::span<const SceneDocument::NodeKey> keys);
  [[nodiscard]] static std::vector<EditorHierarchyTestRow>
  HierarchyRows(const EditorImGuiHost &host, const SceneDocument &document);
  static void QueueHierarchyRename(EditorImGuiHost &host, SceneDocument::NodeKey entity,
                                   std::string name);
  static void QueueInspectorTransform(EditorImGuiHost &host, SceneDocument::NodeKey entity,
                                      runtime::Transform transform) noexcept;
  static void QueueInspectorMesh(EditorImGuiHost &host, SceneDocument::NodeKey entity,
                                 std::optional<runtime::AssetUuid> asset,
                                 std::uint64_t generation) noexcept;
  static void QueueInspectorMeshes(EditorImGuiHost &host,
                                   std::span<const SceneDocument::NodeKey> entities,
                                   std::optional<runtime::AssetUuid> asset,
                                   std::uint64_t generation);
  [[nodiscard]] static std::string_view InspectorMeshLabel(const EditorImGuiHost &host) noexcept;
  static void QueueInspectorMaterial(EditorImGuiHost &host, SceneDocument::NodeKey entity,
                                     runtime::AssetUuid asset, std::uint64_t generation) noexcept;
  [[nodiscard]] static std::string_view
  InspectorMaterialLabel(const EditorImGuiHost &host) noexcept;
  // Control 0: combo; 1: first available typed material in its popup.
  [[nodiscard]] static std::optional<std::array<float, 2>>
  InspectorMaterialPosition(const EditorImGuiHost &host, std::size_t control) noexcept;
  static void FocusInspector(EditorImGuiHost &host) noexcept;
  // Transform, Camera, Light (0, 1, 2); reports actual UI widget positions for pointer tests.
  [[nodiscard]] static std::optional<std::array<float, 2>>
  InspectorResetPosition(const EditorImGuiHost &host, std::size_t component) noexcept;
  // Component 0/1/2: Transform/Camera/Light; control 0/1: Copy/Paste values.
  [[nodiscard]] static std::optional<std::array<float, 2>>
  InspectorClipboardPosition(const EditorImGuiHost &host, std::size_t component,
                             std::size_t control) noexcept;
  static void CollapseInspector(EditorImGuiHost &host, bool collapsed) noexcept;
  [[nodiscard]] static std::optional<std::array<float, 2>>
  // Control 0: combo; 1: first available asset in its popup; 2: removal button.
  InspectorMeshPosition(const EditorImGuiHost &host, std::size_t control) noexcept;
  // nullopt: combo; zero: Automatic; nonzero: visible candidate in the open popup.
  [[nodiscard]] static std::optional<std::array<float, 2>>
  GameCameraPosition(const EditorImGuiHost &host, std::optional<runtime::Id> camera) noexcept;
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
  [[nodiscard]] static std::optional<std::array<float, 2>>
  CameraAlignPosition(const EditorImGuiHost &host) noexcept;
  static void FocusInspectorLightField(EditorImGuiHost &host) noexcept;
  static void FocusInspectorCameraField(EditorImGuiHost &host, std::size_t axis) noexcept;
  [[nodiscard]] static std::string_view InspectorComponentText(const EditorImGuiHost &host,
                                                               std::size_t field) noexcept;
  static void QueueInspectorCamera(EditorImGuiHost &host, SceneDocument::NodeKey entity,
                                   std::optional<runtime::CameraComponent> camera) noexcept;
  static void QueueInspectorLight(EditorImGuiHost &host, SceneDocument::NodeKey entity,
                                  std::optional<runtime::LightComponent> light) noexcept;
  static void FocusInspectorTransformField(EditorImGuiHost &host, std::size_t axis) noexcept;
  [[nodiscard]] static std::array<bool, 6>
  InspectorTransformMixed(const EditorImGuiHost &host) noexcept;
  [[nodiscard]] static std::string_view InspectorTransformText(const EditorImGuiHost &host,
                                                               std::size_t axis) noexcept;
  static void QueueInspectorTransforms(EditorImGuiHost &host,
                                       std::span<const SceneDocument::NodeKey> entities,
                                       std::span<const runtime::Transform> transforms);
  static void QueueInspectorEulerField(EditorImGuiHost &host,
                                       std::span<const SceneDocument::NodeKey> entities,
                                       std::size_t axis, double degrees);
  static void FocusInspectorEulerField(EditorImGuiHost &host, std::size_t axis) noexcept;
  [[nodiscard]] static std::optional<std::array<double, 3>>
  InspectorEulerAngles(const EditorImGuiHost &host, SceneDocument::NodeKey entity) noexcept;
  // Actual build panel controls: executable, cwd, add/remove argument, run, cancel, first argv.
  [[nodiscard]] static std::optional<std::array<float, 2>>
  BuildControlPosition(const EditorImGuiHost &host, std::size_t control) noexcept;
  static void SetBuildCommand(EditorImGuiHost &host, std::string_view executable,
                              std::string_view cwd, std::span<const std::string> arguments);
  [[nodiscard]] static BuildProcessSnapshot BuildStatus(const EditorImGuiHost &host);
  [[nodiscard]] static std::string BuildOutput(const EditorImGuiHost &host);
  static void QueueProjectSelection(EditorImGuiHost &host, ProjectSelectorRequest request);
  static void QueueProjectImportCancellation(EditorImGuiHost &host) noexcept;
  static void QueueContentConflictChoice(EditorImGuiHost &host, runtime::AssetUuid asset,
                                         DirtyConflictChoice choice) noexcept;
  [[nodiscard]] static std::uint32_t OverrideDrawTexture(EditorImGuiHost &host,
                                                         std::uint64_t texture_id) noexcept;
};

} // namespace nexora::editor::imgui
