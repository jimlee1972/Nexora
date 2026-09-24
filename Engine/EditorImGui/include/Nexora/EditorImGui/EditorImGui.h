#pragma once

#include "Nexora/Editor/EditorWorkspace.h"
#include "Nexora/EditorImGui/Api.h"
#include "Nexora/Presentation/RenderSurface.h"
#include "Nexora/RHI/Device.h"
#include "Nexora/Window/Window.h"

#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>

namespace nexora::editor::imgui {

enum class RecoveryChoice : std::uint8_t { None, Recover, Discard };

struct FrameMetrics final {
  std::uint32_t vertices = 0;
  std::uint32_t indices = 0;
  std::uint32_t command_lists = 0;
};

struct RendererMetrics final {
  std::uint64_t frames = 0;
  std::uint64_t draw_calls = 0;
  std::uint64_t buffer_reallocations = 0;
  std::uint64_t font_rebuilds = 0;
  std::uint64_t rejected_textures = 0;
  std::uint64_t completion_waits = 0;
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
  void DrawProductShell(ProductShell &shell, SceneDocument *scene = nullptr,
                        ProjectWorkspace *workspace = nullptr);
  [[nodiscard]] FrameMetrics EndFrame();
  // The validation/offscreen renderer retains its pipeline, font texture, and geometrically sized
  // upload buffers. ReleaseRenderer must be called before the supplied Device is destroyed.
  [[nodiscard]] std::uint32_t Render(nexora::rhi::Device &device, nexora::rhi::TextureHandle target,
                                     std::uint32_t width, std::uint32_t height,
                                     nexora::rhi::ResourceState before, bool prepare_for_present);
  void ReleaseRenderer(nexora::rhi::Device &device);
  [[nodiscard]] RendererMetrics GetRendererMetrics() const noexcept;
  [[nodiscard]] std::uint64_t RegisterTexture(nexora::rhi::Device &device,
                                              nexora::rhi::TextureHandle texture);
  [[nodiscard]] bool UnregisterTexture(std::uint64_t texture_id) noexcept;
  [[nodiscard]] std::string SaveLayout() const;
  [[nodiscard]] bool LoadLayout(std::string_view layout);
  // Flattens the current ImGui draw data into backend-neutral indexed geometry that RenderSurface
  // records directly into its acquired native GPU image. The RHI overload remains a headless
  // contract-test path.
  [[nodiscard]] Nexora::Presentation::SurfaceStatus
  Render(Nexora::Presentation::RenderSurface &surface, std::uint32_t width, std::uint32_t height);
  void UpdateImeCandidate(Nexora::Presentation::RenderSurface &surface);
  [[nodiscard]] bool ApplyRecoveryChoice(ProjectWorkspace &workspace, RecoveryChoice choice);
  [[nodiscard]] RecoveryChoice TakeRecoveryChoice() noexcept;
  [[nodiscard]] std::string_view RecoveryError() const noexcept;

private:
  struct State;
  std::unique_ptr<State> state_;
};

} // namespace nexora::editor::imgui
