#pragma once

#include "Nexora/Editor/EditorProduction.h"
#include "Nexora/Editor/EditorWorkspace.h"
#include "Nexora/Editor/MaterialAssetCatalog.h"
#include "Nexora/Editor/MeshAssetCatalog.h"
#include "Nexora/Editor/PlayApply.h"
#include "Nexora/Editor/PlayInputBindings.h"
#include "Nexora/Editor/ProjectContent.h"
#include "Nexora/Editor/SceneFiles.h"
#include "Nexora/Editor/StaticProjectExportJob.h"
#include "Nexora/Editor/ViewportMath.h"
#include "Nexora/EditorImGui/Api.h"
#include "Nexora/Presentation/RenderSurface.h"
#include "Nexora/RHI/Device.h"
#include "Nexora/Runtime/EditorSdk.h"
#include "Nexora/Window/Window.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace nexora::editor::imgui {

class EditorImGuiTestAccess;

// Built-in native preview reserves the remaining instance slots for ground and gizmos.
inline constexpr std::size_t kMaximumNativeSceneFrameCandidates = 3999;

enum class RecoveryChoice : std::uint8_t { None, Recover, Discard };
enum class CloseChoice : std::uint8_t { None, SaveAndExit, DiscardAndExit, Cancel };
enum class PlayCommand : std::uint8_t { None, Start, Pause, Resume, Step, Stop };
enum class ProjectSelectorAction : std::uint8_t { Open, Create };
enum class SceneFileAction : std::uint8_t { New, Open, SaveAs };
enum class SceneTabAction : std::uint8_t { Select, Close, New, OpenOwned, OpenReference, SaveAll };
struct SceneTabItem final {
  std::uint64_t id{};
  SceneFileToken token{};
  std::string label;
  std::optional<std::filesystem::path> path;
  bool owned{}, dirty{}, closeable{}, read_only{};
};
struct SceneTabRequest final {
  SceneTabAction action{};
  SceneFileToken source{};
  std::uint64_t target{};
  SceneFileToken target_token{};
  std::filesystem::path path{};
  bool discard_dirty{}, save_before_close{};
};
struct GameInputBindingsSaveRequest final {
  foundation::Uuid project;
  std::filesystem::path root;
  PlayInputBindings bindings;
};

struct SceneFileRequest final {
  SceneFileAction action{SceneFileAction::New};
  SceneFileToken token;
  std::filesystem::path path{};
  // Save before New/Open. An empty save_path uses the current managed destination.
  bool save_current{};
  std::optional<std::filesystem::path> save_path{};
  bool discard_unsaved{};
  bool replace_existing{};
  bool close_after_save{};
  std::optional<SceneOverwriteToken> overwrite_token{};
};

struct StaticExportRequest final {
  SceneFileToken token;
  bool cancel{};
};

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

struct NativeSceneOrbit final {
  double yaw{0.588};
  double pitch{0.585};
  double distance{17.55};
  double target_y{};
};

enum class NativeSceneTool { Move, Rotate, Scale, Select };

struct NativeScenePickRequest final {
  std::uint32_t x{}, y{}; // framebuffer pixels
  bool additive{};
};

struct NativeSceneDragRequest final {
  std::int32_t start_x{}, start_y{}, end_x{}, end_y{}; // framebuffer pixels
  double snap_step{};
  bool vertical{};
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
// Move between frames; moved-from hosts support only destruction or assignment. Release the
// destination's public-RHI renderer before move assignment while its device is still alive.
class NEXORA_EDITOR_IMGUI_API EditorImGuiHost final {
public:
  EditorImGuiHost();
  ~EditorImGuiHost();
  EditorImGuiHost(EditorImGuiHost &&) noexcept;
  EditorImGuiHost &operator=(EditorImGuiHost &&) noexcept;
  EditorImGuiHost(const EditorImGuiHost &) = delete;
  EditorImGuiHost &operator=(const EditorImGuiHost &) = delete;

  void SetDisplay(float width, float height, float dpi_scale);
  // Pointer events use native client pixels. SetDisplay supplies logical dimensions/frame DPI
  // before forwarding the current native batch; the host converts pointer positions exactly once.
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
  void DrawProductShell(
      ProductShell &shell, SceneDocument *scene = nullptr, ProjectWorkspace *workspace = nullptr,
      ProjectContentSession *content = nullptr, RecentProjectStore *recent_projects = nullptr,
      AssetImportQueue *imports = nullptr, runtime::RuntimeConsole *console = nullptr,
      runtime::PlaySession *play = nullptr, ProfileSession *profile = nullptr,
      const MeshAssetCatalog *meshes = nullptr, const MaterialAssetCatalog *materials = nullptr);
  [[nodiscard]] PlayCommand TakePlayCommand() noexcept;
  [[nodiscard]] std::optional<PlayTransformReview> TakePlayApplyRequest();
  [[nodiscard]] std::string_view GameplayLibrary() const noexcept;
  void SetGameplayStatus(std::string message);
  void SetGameplayLibrary(std::string_view library, std::uint64_t project_generation = 0);
  [[nodiscard]] bool GameInputFocused() const noexcept;
  [[nodiscard]] PlayInputBindings GameInputBindings() const noexcept;
  // Owner loads validated settings during project activation; widgets never perform IO.
  bool SetGameInputBindings(const PlayInputBindings &bindings, const ProjectWorkspace &workspace);
  [[nodiscard]] std::optional<GameInputBindingsSaveRequest> TakeGameInputBindingsSaveRequest();
  void SetGameInputBindingsStatus(std::string message);
  [[nodiscard]] bool TakeProfileExportRequest() noexcept;
  [[nodiscard]] bool TakeProfileJsonExportRequest() noexcept;
  [[nodiscard]] bool TakeProfileCsvImportRequest() noexcept;
  [[nodiscard]] bool TakeProfileJsonImportRequest() noexcept;
  // Transfers a bounded owning wall-time snapshot; rejection preserves the previous import.
  // Bind to the last drawn project; project changes clear it. Live ProfileSession is untouched.
  bool SetImportedProfileCapture(FrameProcessingCapture capture);
  [[nodiscard]] bool TakeMemoryExportRequest() noexcept;
  [[nodiscard]] bool TakeMemoryImportRequest() noexcept;
  bool SetImportedMemoryCapture(ProcessMemoryCapture capture);
  [[nodiscard]] bool TakeGpuExportRequest() noexcept;
  [[nodiscard]] bool TakeGpuImportRequest() noexcept;
  bool SetImportedGpuCapture(GpuTimingCapture capture);
  void SetProfileExportStatus(std::string message);
  [[nodiscard]] std::optional<StaticExportRequest> TakeStaticExportRequest();
  // Owning bounded observations only. Application revalidates requests and owns job publication.
  void SetStaticExportStatus(StaticExportSnapshot snapshot, bool busy);
  [[nodiscard]] bool TakeSceneSaveRequest() noexcept;
  void SetSceneSaveResult(std::string message, bool success);
  // Owning context only; the application performs all file IO and rechecks token/access.
  // Owns a token/path copy, never a workspace/document borrow. Changing the token cancels
  // pending dialogs and output. Consume requests once on the application authoring thread;
  // SceneFileSession rechecks token, path, write access and dirty state before filesystem I/O.
  void SetSceneFileContext(SceneFileToken token, std::optional<std::filesystem::path> path,
                           bool save_blocked = false);
  [[nodiscard]] std::optional<SceneFileRequest> TakeSceneFileRequest();
  // Owns at most sixteen copied rows. Admission validates the whole context before mutation;
  // scope/target changes cancel old dialogs and output. Widgets perform no source IO.
  bool SetSceneTabs(std::span<const SceneTabItem>, std::uint64_t active, bool busy = false);
  [[nodiscard]] std::optional<SceneTabRequest> TakeSceneTabRequest();
  void SetSceneTabStatus(std::string message, bool success);
  void RequestSceneSaveAs(bool close_after_save = false,
                          std::optional<std::filesystem::path> suggested_path = std::nullopt);
  void RequestSceneOverwrite(SceneFileRequest request);
  void RequestSceneUnsavedChoice(SceneFileRequest request);
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
  // Borrowed texture/device; IDs never resurrect within this host after renderer release.
  // Returns zero for invalid ownership or exhausted generation capacity.
  [[nodiscard]] std::uint64_t RegisterTexture(nexora::rhi::Device &device,
                                              nexora::rhi::TextureHandle texture);
  // Native RenderSurface images own a tight linear RGBA8 copy. Maximum 1024x1024,
  // 64 live registrations and 16 MiB combined pixels; zero means invalid/capacity exhausted.
  // Register/unregister between frames. IDs share the host's monotonic generation namespace.
  [[nodiscard]] std::uint64_t RegisterNativeTexture(std::uint32_t width, std::uint32_t height,
                                                    std::span<const std::byte> pixels);
  [[nodiscard]] bool UnregisterTexture(std::uint64_t texture_id) noexcept;
  [[nodiscard]] std::string SaveLayout() const;
  [[nodiscard]] bool LoadLayout(std::string_view layout);
  [[nodiscard]] SceneOverviewCamera GetSceneOverviewCamera() const noexcept;
  bool SetSceneOverviewCamera(SceneOverviewCamera camera) noexcept;
  // Current frame's visible Scene canvas in framebuffer pixels, after docking and DPI scaling.
  // Empty when the panel is not drawn; the value resets at BeginFrame().
  [[nodiscard]] std::optional<Nexora::Presentation::SceneViewport>
  SceneCanvasViewport() const noexcept;
  // Current frame's Game canvas. Only published during Play when the native Scene canvas is hidden.
  [[nodiscard]] std::optional<Nexora::Presentation::SceneViewport>
  NativeGameViewport() const noexcept;
  // Preview-only choice for the current Play session; zero requests the automatic active camera.
  // The application revalidates it against the live World after commands and fixed ticks.
  [[nodiscard]] runtime::Id GameCameraSelection() const noexcept;
  void SetNativeGameStatus(std::string message, bool available = true);
  void SetNativeScenePreview(bool enabled) noexcept;
  void SetNativeScenePreviewAvailable(bool available) noexcept;
  [[nodiscard]] NativeSceneOrbit GetNativeSceneOrbit() const noexcept;
  [[nodiscard]] NativeSceneTool GetNativeSceneTool() const noexcept;
  [[nodiscard]] bool NativeSceneLocalAxes() const noexcept;
  [[nodiscard]] bool NativeSceneCenterPivot() const noexcept;
  bool SetNativeSceneOrbit(NativeSceneOrbit orbit) noexcept;
  [[nodiscard]] std::optional<Nexora::Presentation::SceneViewport>
  NativeScenePreviewViewport() const noexcept;
  [[nodiscard]] std::optional<NativeScenePickRequest> NativeScenePick() const noexcept;
  // One-shot owning scope; the host revalidates it against the current project/document and
  // supplies the actual bounded submission candidates after widget/authoring commands.
  [[nodiscard]] std::optional<SceneFileToken> TakeNativeSceneFrameAllRequest() noexcept;
  // One-shot owning scope, valid only in its issuing GUI frame. The application revalidates
  // its current scene-file token and selects generation-checked actual submission candidates.
  [[nodiscard]] std::optional<SceneFileToken> TakeNativeSceneSelectAllRequest() noexcept;
  // Borrowed numeric bounds are consumed only in this call. Applies once in the issuing GUI
  // frame; stale scope, empty/oversized/malformed bounds and blocked input preserve the camera.
  bool ApplyNativeSceneFrameAll(SceneFileToken token,
                                std::span<const PickCandidate> candidates) noexcept;
  [[nodiscard]] std::optional<NativeSceneDragRequest> NativeSceneDrag() const noexcept;
  [[nodiscard]] std::optional<NativeSceneDragRequest> NativeSceneDragPreview() const noexcept;
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
