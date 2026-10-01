#pragma once

#include "Nexora/EditorImGui/EditorImGui.h"

#include <cstddef>
#include <cstdint>
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
  std::uint32_t hierarchy_selection = 0;
  runtime::Id hierarchy_selection_anchor = 0;
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
  static void QueueHierarchySelection(EditorImGuiHost &host, runtime::Id entity, bool additive,
                                      bool range) noexcept;
  static void QueueHierarchyMove(EditorImGuiHost &host, runtime::Id entity, runtime::Id parent,
                                 std::size_t index) noexcept;
  static void QueueHierarchyReorder(EditorImGuiHost &host, int direction) noexcept;
  static void QueueProjectSelection(EditorImGuiHost &host, ProjectSelectorRequest request);
  static void QueueProjectImportCancellation(EditorImGuiHost &host) noexcept;
  static void QueueContentConflictChoice(EditorImGuiHost &host, runtime::AssetUuid asset,
                                         DirtyConflictChoice choice) noexcept;
  [[nodiscard]] static std::uint32_t OverrideDrawTexture(EditorImGuiHost &host,
                                                         std::uint64_t texture_id) noexcept;
};

} // namespace nexora::editor::imgui
