#pragma once

#include "Nexora/Editor/EditorWorkspace.h"
#include "Nexora/EditorImGui/Api.h"
#include "Nexora/Window/Window.h"

#include <cstdint>
#include <memory>
#include <span>
#include <string_view>

namespace nexora::editor::imgui {

enum class RecoveryChoice : std::uint8_t { None, Recover, Discard };

struct FrameMetrics final {
  std::uint32_t vertices = 0;
  std::uint32_t indices = 0;
  std::uint32_t command_lists = 0;
};

// Owns exactly one Dear ImGui context. All methods are serialized and must be called from the
// window owner thread. Draw data is borrowed until BeginFrame() or destruction.
class NEXORA_EDITOR_IMGUI_API EditorImGuiHost final {
public:
  EditorImGuiHost();
  ~EditorImGuiHost();
  EditorImGuiHost(EditorImGuiHost &&) noexcept;
  EditorImGuiHost &operator=(EditorImGuiHost &&) noexcept;
  EditorImGuiHost(const EditorImGuiHost &) = delete;
  EditorImGuiHost &operator=(const EditorImGuiHost &) = delete;

  void SetDisplay(float width, float height, float dpi_scale);
  void ProcessEvents(std::span<const Nexora::Window::WindowEvent> events);
  void BeginFrame(float delta_seconds = 1.0F / 60.0F);
  void DrawProductShell(const ProductShell &shell, SceneDocument *scene = nullptr,
                        bool recovery_available = false);
  [[nodiscard]] FrameMetrics EndFrame();
  [[nodiscard]] RecoveryChoice TakeRecoveryChoice() noexcept;

private:
  struct State;
  std::unique_ptr<State> state_;
};

} // namespace nexora::editor::imgui
