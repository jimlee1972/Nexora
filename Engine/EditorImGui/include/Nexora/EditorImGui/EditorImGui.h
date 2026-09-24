#pragma once

#include "Nexora/Editor/EditorWorkspace.h"
#include "Nexora/EditorImGui/Api.h"
#include "Nexora/Presentation/RenderSurface.h"
#include "Nexora/RHI/Device.h"
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
                        ProjectWorkspace *workspace = nullptr);
  [[nodiscard]] FrameMetrics EndFrame();
  [[nodiscard]] std::uint32_t Render(nexora::rhi::Device &device, nexora::rhi::TextureHandle target,
                                     std::uint32_t width, std::uint32_t height,
                                     nexora::rhi::ResourceState before, bool prepare_for_present);
  // Rasterizes the current ImGui draw data and composites it into the RenderSurface's acquired
  // swapchain image. This is the native Editor path; the RHI overload remains a headless contract
  // test path.
  [[nodiscard]] Nexora::Presentation::SurfaceStatus
  Render(Nexora::Presentation::RenderSurface &surface, std::uint32_t width, std::uint32_t height);
  void UpdateImeCandidate(Nexora::Presentation::RenderSurface &surface);
  [[nodiscard]] RecoveryChoice TakeRecoveryChoice() noexcept;
  [[nodiscard]] bool RecoveryPromptVisible() const noexcept;

private:
  struct State;
  std::unique_ptr<State> state_;
};

} // namespace nexora::editor::imgui
