#pragma once

#include "Nexora/Editor/EditorWorkspace.h"
#include "Nexora/Editor/ProjectContent.h"
#include "Nexora/EditorImGui/Api.h"
#include "Nexora/Presentation/RenderSurface.h"
#include "Nexora/RHI/Device.h"
#include "Nexora/Runtime/EditorSdk.h"
#include "Nexora/Window/Window.h"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace nexora::editor::imgui {

class EditorImGuiTestAccess;

enum class RecoveryChoice : std::uint8_t { None, Recover, Discard };
enum class CloseChoice : std::uint8_t { None, SaveAndExit, DiscardAndExit, Cancel };
enum class ProjectSelectorAction : std::uint8_t { Open, Create };

struct ProjectSelectorRequest final {
  ProjectSelectorAction action{ProjectSelectorAction::Open};
  std::filesystem::path root;
  std::string name;
  ProjectAccess access{ProjectAccess::ReadWrite};
};

struct FrameMetrics final {
  std::uint32_t vertices = 0;
  std::uint32_t indices = 0;
  std::uint32_t command_lists = 0;
};

struct SceneOverviewCamera final {
  double x{}, z{};
  double pixels_per_unit{32.0};
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
  // Draws the startup project browser. It only emits a one-shot request; the application owns
  // project creation/opening, indexing, lease acquisition, and activation on the authoring thread.
  void DrawProjectSelector(const RecentProjectStore *recent_projects = nullptr,
                           ProjectAccess default_access = ProjectAccess::ReadWrite);
  [[nodiscard]] std::optional<ProjectSelectorRequest> TakeProjectSelectorRequest();
  [[nodiscard]] bool TakeProjectSelectorCancel() noexcept;
  void SetProjectSelectorError(std::string error);
  void SetProjectSelectorStatus(std::string status, bool busy);
  [[nodiscard]] std::string_view ProjectSelectorError() const noexcept;
  void DrawProductShell(ProductShell &shell, SceneDocument *scene = nullptr,
                        ProjectWorkspace *workspace = nullptr,
                        ProjectContentSession *content = nullptr,
                        RecentProjectStore *recent_projects = nullptr,
                        AssetImportQueue *imports = nullptr,
                        runtime::RuntimeConsole *console = nullptr);
  [[nodiscard]] bool TakeSceneSaveRequest() noexcept;
  void SetSceneSaveResult(std::string message, bool success);
  void RequestCloseConfirmation() noexcept;
  [[nodiscard]] CloseChoice TakeCloseChoice() noexcept;
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
  [[nodiscard]] SceneOverviewCamera GetSceneOverviewCamera() const noexcept;
  bool SetSceneOverviewCamera(SceneOverviewCamera camera) noexcept;
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
  friend class EditorImGuiTestAccess;
  struct State;
  std::unique_ptr<State> state_;
};

} // namespace nexora::editor::imgui
