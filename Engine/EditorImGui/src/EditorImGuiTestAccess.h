#pragma once

#include "Nexora/EditorImGui/EditorImGui.h"

#include <cstdint>

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
  std::uint32_t content_visible_items = 0;
  std::uint32_t content_visible_folders = 0;
  std::uint32_t content_selection = 0;
  std::uint32_t content_forward_dependencies = 0;
  std::uint32_t content_reverse_dependencies = 0;
  bool project_writable = false;
  bool project_upgrade_required = false;
  std::uint32_t recent_projects = 0;
  bool project_selector_visible = false;
  std::uint32_t selector_recent_projects = 0;
};

class NEXORA_EDITOR_IMGUI_API EditorImGuiTestAccess final {
public:
  [[nodiscard]] static EditorImGuiTestState Inspect(const EditorImGuiHost &host) noexcept;
  static void SetInputTrickle(EditorImGuiHost &host, bool enabled) noexcept;
  static void QueueProjectSelection(EditorImGuiHost &host, ProjectSelectorRequest request);
  [[nodiscard]] static std::uint32_t OverrideDrawTexture(EditorImGuiHost &host,
                                                         std::uint64_t texture_id) noexcept;
};

} // namespace nexora::editor::imgui
