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
};

class NEXORA_EDITOR_IMGUI_API EditorImGuiTestAccess final {
public:
  [[nodiscard]] static EditorImGuiTestState Inspect(const EditorImGuiHost &host) noexcept;
  static void SetInputTrickle(EditorImGuiHost &host, bool enabled) noexcept;
  [[nodiscard]] static std::uint32_t OverrideDrawTexture(EditorImGuiHost &host,
                                                         std::uint64_t texture_id) noexcept;
};

} // namespace nexora::editor::imgui
