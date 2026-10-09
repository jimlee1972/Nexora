#include "Nexora/EditorImGui/EditorImGui.h"
#include "Nexora/Editor/InspectorRotation.h"
#include "Nexora/Editor/ViewportMath.h"
#include "Nexora/Runtime/RenderSync.h"
#if defined(NEXORA_EDITOR_IMGUI_TEST_ACCESS)
#include "EditorImGuiTestAccess.h"
#endif

#include <imgui.h>
#include <imgui_internal.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <exception>
#include <limits>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

namespace nexora::editor::imgui {
static_assert(sizeof(ImWchar) == 4, "Editor text input must preserve all Unicode scalar values");
struct EditorImGuiHost::State final {
  struct HierarchySelectionRequest final {
    SceneDocument::NodeKey entity;
    bool additive = false;
    bool range = false;
  };
  struct HierarchyMoveRequest final {
    SceneDocument::NodeKey entity;
    std::optional<SceneDocument::NodeKey> parent;
    std::size_t index{};
  };
  struct HierarchyCreateRequest final {
    std::string name;
    std::optional<SceneDocument::NodeKey> parent;
    int kind{}; // Empty, Camera, Light.
    std::uint64_t document_generation{};
  };
  struct Renderer final {
    struct UploadSlot final {
      nexora::rhi::BufferHandle vertices;
      nexora::rhi::BufferHandle indices;
      std::uint64_t vertex_capacity = 0;
      std::uint64_t index_capacity = 0;
      std::uint64_t completion = 0;
    };
    struct TextureSlot final {
      nexora::rhi::TextureHandle texture;
      std::uint32_t generation = 1;
      bool live = false;
    };
    struct RetiredTexture final {
      nexora::rhi::TextureHandle texture;
      std::uint64_t completion = 0;
    };
    nexora::rhi::Device *device = nullptr;
    nexora::rhi::PipelineHandle pipeline;
    nexora::rhi::TextureHandle font_texture;
    std::array<UploadSlot, 3> upload_slots;
    std::vector<TextureSlot> textures{{}};
    std::vector<RetiredTexture> retired_textures;
    std::size_t upload_slot = 0;
    std::uint32_t font_generation = 0;
  } renderer;
  ~State() {
    if (context == nullptr)
      return;
    auto *previous = ImGui::GetCurrentContext();
    ImGui::SetCurrentContext(context);
    surface = nullptr;
    ImGui::GetIO().BackendPlatformUserData = nullptr;
    ImGui::DestroyContext(context);
    if (previous != context)
      ImGui::SetCurrentContext(previous);
  }
  ImGuiContext *context = nullptr;
  Nexora::Presentation::RenderSurface *surface = nullptr;
  float dpi_scale = 1.0F;
  std::optional<std::array<std::int32_t, 2>> native_pointer;
  // Sentinel below every real DpiBucket() result ({1.0, 1.25, 1.5, 2.0}) so the first SetDisplay()
  // call always builds the font atlas, even when the initial DPI resolves to the 1.0 bucket; a
  // default of 1.0F here would make that common case a no-op and leave the atlas unbuilt, which
  // ImGui::NewFrame() asserts on.
  float dpi_bucket = 0.0F;
  std::uint32_t font_generation = 1;
  std::uint32_t surface_font_generation = 0;
  std::uint64_t surface_font_domain = 0;
  // Host-scoped IDs must not resurrect after ReleaseRenderer resets the device cache.
  std::uint64_t next_texture_generation = 1;
  struct NativeTextureSlot final {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::vector<std::byte> pixels;
    std::uint32_t generation = 0;
    std::uint32_t uploaded_generation = 0;
  };
  std::vector<NativeTextureSlot> native_textures{{}}; // Slot zero is the font atlas.
  std::size_t native_texture_bytes = 0;
  RendererMetrics renderer_metrics;
  RecoveryChoice recovery_choice = RecoveryChoice::None;
  CloseChoice close_choice = CloseChoice::None;
  PlayCommand play_command = PlayCommand::None;
  bool play_apply_open = false;
  bool play_apply_popup_pending = false;
  PlayTransformReview play_apply_review;
  std::optional<PlayTransformReview> play_apply_requested;
  std::optional<std::array<float, 2>> play_apply_position;
  std::optional<std::array<float, 2>> play_apply_confirm_position;
  bool memory_export_requested = false;
  bool memory_import_requested = false;
  std::optional<ProcessMemoryCapture> imported_memory;
  std::array<std::optional<std::array<float, 2>>, 3> memory_control_positions{};
  bool gpu_export_requested = false;
  bool gpu_import_requested = false;
  std::optional<GpuTimingCapture> imported_gpu;
  std::array<std::optional<std::array<float, 2>>, 3> gpu_control_positions{};
  bool profile_export_requested = false;
  std::optional<StaticExportRequest> static_export_request;
  StaticExportSnapshot static_export;
  bool static_export_busy{};
  std::array<std::optional<std::array<float, 2>>, 3> static_export_positions{};
  bool profile_json_export_requested = false;
  bool profile_csv_import_requested = false;
  bool profile_json_import_requested = false;
  std::optional<FrameProcessingCapture> imported_profile;
  std::filesystem::path profile_project_root;
  std::string profile_project_id;
  std::string profile_export_status;
  std::optional<std::array<float, 2>> profile_export_position;
  std::optional<std::array<float, 2>> profile_json_export_position;
  std::optional<std::array<float, 2>> profile_csv_import_position;
  std::optional<std::array<float, 2>> profile_json_import_position;
  std::optional<std::array<float, 2>> profile_import_clear_position;
  std::optional<std::array<float, 2>> profile_capture_position;
  std::optional<std::array<float, 2>> profile_clear_position;
  ProcessMemoryObservation profile_memory;
  GpuProfileObservation profile_gpu;
  std::array<char, 1024> gameplay_library{};
  std::string gameplay_status;
  std::uint64_t gameplay_project_generation{};
  bool close_prompt_requested = false;
  bool recovery_prompt_opened = false;
  bool initial_dock_layout_built = false;
  bool focus_initial_content = false;
  bool focus_initial_scene = false;
  ImGuiTextFilter console_filter;
  int console_min_severity = 0;
  runtime::RuntimeConsole *console_scope = nullptr;
  bool console_display_paused = false;
  std::uint64_t console_clear_sequence = 0;
  std::vector<runtime::RuntimeLogRecord> console_frozen_records;
  std::array<std::optional<std::array<float, 2>>, 2> console_control_positions{};
  std::size_t console_visible_count = 0;
  std::optional<std::uint64_t> console_first_visible_sequence;
  std::string recovery_error;
  std::array<char, 128> hierarchy_filter{};
  std::array<char, 128> hierarchy_create_name{'E', 'n', 't', 'i', 't', 'y'};
  int hierarchy_create_kind{};
  bool hierarchy_create_custom_name{};
  std::array<std::optional<std::array<float, 2>>, 7> hierarchy_create_positions{};
  std::array<char, 256> hierarchy_rename{};
  std::uint32_t hierarchy_visible_rows = 0;
  std::uint32_t hierarchy_rendered_rows = 0;
  std::uint32_t hierarchy_selection = 0;
  std::optional<SceneDocument::NodeKey> hierarchy_selection_anchor;
  std::optional<SceneDocument::NodeKey> hierarchy_navigation_cursor;
  std::uint64_t hierarchy_navigation_generation{};
  std::string hierarchy_navigation_filter;
  std::vector<std::pair<SceneDocument::NodeKey, std::array<float, 2>>> hierarchy_row_positions;
  std::vector<SceneDocument::NodeKey> hierarchy_expanded;
  std::optional<HierarchySelectionRequest> hierarchy_selection_request;
  std::optional<HierarchyMoveRequest> hierarchy_move_request;
  std::optional<HierarchyCreateRequest> hierarchy_create_request;
  std::optional<int> hierarchy_reorder_request;
  std::optional<std::pair<SceneDocument::NodeKey, bool>> hierarchy_expansion_request;
  std::optional<SceneDocument::NodeKey> hierarchy_rename_target;
  bool hierarchy_rename_focus_pending = false;
  std::optional<std::array<float, 2>> hierarchy_rename_position;
  std::optional<std::pair<SceneDocument::NodeKey, std::string>> hierarchy_rename_request;
  std::string hierarchy_error;
  std::string hierarchy_status;
  struct SceneMarker final {
    SceneDocument::NodeKey entity;
    ImVec2 position;
  };
  std::vector<SceneMarker> scene_markers;
  std::optional<Nexora::Presentation::SceneViewport> scene_canvas_viewport;
  std::optional<std::array<float, 2>> scene_frame_position;
  std::optional<std::array<float, 2>> scene_frame_all_position, scene_select_all_position;
  std::optional<SceneFileToken> native_scene_select_all_request;
  std::array<std::optional<std::array<float, 2>>, 4> native_scene_tool_positions;
  std::optional<SceneFileToken> scene_frame_token;
  std::optional<SceneFileToken> native_scene_frame_all_request, native_scene_frame_all_apply;
  std::optional<Nexora::Presentation::SceneViewport> native_game_viewport;
  bool native_game_available = true;
  bool game_was_running = false;
  std::optional<std::array<float, 2>> hierarchy_cut_position;
  std::optional<std::array<float, 2>> content_add_mesh_position;
  std::optional<std::array<float, 2>> content_open_scene_position;
  std::string content_scene_error;
  std::vector<std::pair<runtime::AssetUuid, std::array<float, 2>>> content_asset_positions;
  std::optional<std::array<float, 2>> camera_align_position;
  runtime::Id game_camera_selection{};
  std::uint64_t game_camera_generation{};
  std::optional<std::array<float, 2>> game_camera_combo_position;
  std::vector<std::pair<runtime::Id, std::array<float, 2>>> game_camera_positions;
  bool game_input_focused = false;
  PlayInputBindings game_input_bindings{}, game_input_draft{};
  std::filesystem::path game_input_root;
  foundation::Uuid game_input_project{};
  bool game_input_binding_open{}, game_input_binding_pending{};
  std::string game_input_binding_error;
  std::string game_input_binding_status;
  std::optional<GameInputBindingsSaveRequest> game_input_save_requested;
  std::array<std::optional<std::array<float, 2>>, 23> game_input_binding_positions;
  std::vector<std::pair<PlayInputControl, std::array<float, 2>>> game_input_choice_positions;
  runtime::Id play_inspection_entity{};
  runtime::Id play_inspector_rendered{};
  bool inspect_play_selection{};
  std::string native_game_status;
  bool native_scene_preview = false;
  bool native_scene_preview_available = true;
  NativeSceneOrbit native_scene_orbit{};
  NativeSceneTool native_scene_tool{NativeSceneTool::Move};
  bool native_scene_center_pivot{};
  bool native_scene_local_axes{};
  std::optional<NativeScenePickRequest> native_scene_pick;
  std::optional<std::array<std::int32_t, 2>> native_scene_drag_origin;
  double native_scene_drag_snap_step{};
  bool native_scene_drag_vertical{};
  std::optional<NativeSceneDragRequest> native_scene_drag;
  std::optional<NativeSceneDragRequest> native_scene_drag_preview;
  ImVec2 scene_center_world{};
  float scene_pixels_per_unit = 32.0F;
  bool scene_snap_to_grid = false;
  int scene_snap_step_index = 2;
  struct SceneDrag final {
    enum class Axis { Free, X, Z };
    std::vector<SceneDocument::NodeKey> entities;
    ImVec2 start_mouse;
    float pixels_per_unit{};
    Axis axis{Axis::Free};
    float snap_step{};
  };
  std::optional<SceneDrag> scene_drag;
  std::uint64_t scene_gesture_document_generation{};
  struct InspectorTransformRequest final {
    std::vector<SceneDocument::NodeKey> entities;
    std::vector<runtime::Transform> transforms;
  };
  struct InspectorEulerHint final {
    SceneDocument::NodeKey entity;
    runtime::Transform transform;
    EulerDegrees degrees;
  };
  struct InspectorEulerRequest final {
    std::vector<SceneDocument::NodeKey> entities;
    std::size_t axis;
    double degrees;
  };
  std::unordered_map<runtime::Id, InspectorEulerHint> inspector_euler_hints;
  std::vector<SceneDocument::NodeKey> inspector_euler_selection;
  std::array<std::array<char, 64>, 3> inspector_euler_text{};
  std::array<bool, 3> inspector_euler_active{};
  std::optional<std::size_t> inspector_euler_focus_request;
  std::optional<InspectorEulerRequest> inspector_euler_request;
  std::optional<InspectorTransformRequest> inspector_transform_request;
  std::vector<SceneDocument::NodeKey> inspector_transform_selection;
  std::array<std::array<char, 64>, 6> inspector_transform_text{};
  std::array<bool, 6> inspector_transform_active{}, inspector_transform_mixed{};
  std::optional<std::size_t> inspector_transform_focus_request;
  std::uint32_t inspector_draft_generation{};
  template <typename Component> struct InspectorComponentRequest final {
    std::vector<SceneDocument::NodeKey> entities;
    std::vector<std::optional<Component>> values;
  };
  using InspectorCameraRequest = InspectorComponentRequest<runtime::CameraComponent>;
  using InspectorLightRequest = InspectorComponentRequest<runtime::LightComponent>;
  std::optional<InspectorCameraRequest> inspector_camera_request;
  std::optional<InspectorLightRequest> inspector_light_request;
  std::array<std::optional<std::array<float, 2>>, 3> inspector_reset_positions{};
  struct TransformValues final {
    runtime::Transform transform;
    EulerDegrees degrees;
  };
  using ComponentValues =
      std::variant<TransformValues, runtime::CameraComponent, runtime::LightComponent>;
  std::optional<ComponentValues> inspector_component_clipboard;
  std::array<std::array<std::optional<std::array<float, 2>>, 2>, 3> inspector_clipboard_positions{};
  std::optional<std::size_t> inspector_camera_focus_request;
  std::vector<SceneDocument::NodeKey> inspector_component_selection;
  std::array<std::array<char, 64>, 3> inspector_camera_text{};
  std::array<bool, 3> inspector_camera_active{};
  std::array<char, 64> inspector_light_text{};
  bool inspector_light_active{}, inspector_light_focus_request{};
  std::array<bool, 3> inspector_camera_mixed{};
  bool inspector_camera_presence_mixed{}, inspector_light_presence_mixed{}, inspector_light_mixed{};
  struct InspectorMeshRequest final {
    std::vector<SceneDocument::NodeKey> entities;
    std::optional<runtime::AssetUuid> asset;
    std::uint64_t generation{};
  };
  std::optional<InspectorMeshRequest> inspector_mesh_request;
  struct InspectorMaterialRequest final {
    SceneDocument::NodeKey entity;
    runtime::AssetUuid asset;
    std::uint64_t generation{};
  };
  std::optional<InspectorMaterialRequest> inspector_material_request;
  std::string inspector_material_label;
  std::array<std::optional<std::array<float, 2>>, 2> inspector_material_positions{};
  std::string inspector_mesh_label;
  std::array<std::optional<std::array<float, 2>>, 3> inspector_mesh_positions{};
  std::uint32_t inspector_selection = 0;
  std::vector<OpaqueComponentInfo> inspector_opaque_info;
  bool inspector_transform_visible = false;
  std::string inspector_error;
  enum class FileDialog { None, OpenPath, SavePath, Unsaved, Overwrite };
  FileDialog scene_file_dialog{FileDialog::None};
  bool scene_file_context{}, scene_file_save_blocked{}, scene_file_popup_pending{};
  bool scene_file_focus_path = false;
  bool scene_file_save_before_switch{}, scene_file_close_popup{};
  SceneFileToken scene_file_token;
  std::optional<std::filesystem::path> scene_file_path;
  std::optional<SceneFileRequest> scene_file_intent, scene_file_output;
  std::array<char, 1024> scene_file_text{};
  std::array<std::optional<std::array<float, 2>>, 11> scene_file_positions{};
  bool scene_save_requested = false;
  std::string scene_save_message;
  bool scene_save_success = false;
  std::vector<SceneTabItem> scene_tabs;
  std::uint64_t scene_tab_active{};
  SceneFileToken scene_tab_source{};
  bool scene_tabs_busy{}, scene_tab_popup_pending{};
  int scene_tab_dialog{}; // 1: owned path, 2: reference path, 3: close choice.
  std::optional<SceneTabRequest> scene_tab_close, scene_tab_output;
  std::array<char, 1024> scene_tab_path{};
  std::string scene_tab_status;
  std::array<std::optional<std::array<float, 2>>, 11> scene_tab_controls{};
  std::unordered_map<std::uint64_t, std::array<float, 2>> scene_tab_positions;
  std::array<char, 128> content_query{};
  std::array<char, 64> content_type{};
  std::array<char, 1024> content_rename{};
  std::optional<std::array<float, 2>> content_search_position;
  std::optional<runtime::AssetUuid> content_navigation_cursor, content_navigation_anchor;
  std::uint64_t content_navigation_generation{}, content_navigation_revision{};
  std::filesystem::path content_navigation_root, content_navigation_folder;
  std::optional<std::filesystem::path> content_focused_folder;
  std::optional<runtime::AssetUuid> content_rename_target;
  std::uint64_t content_rename_generation{};
  std::filesystem::path content_rename_root, content_rename_path;
  bool content_rename_focus{};
  std::array<std::optional<std::array<float, 2>>, 3> content_rename_positions;
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
  std::optional<std::pair<runtime::AssetUuid, DirtyConflictChoice>> content_conflict_choice_request;
  bool project_writable = false;
  bool project_upgrade_required = false;
  std::uint32_t recent_projects = 0;
  std::array<char, 1024> selector_root{};
  std::array<char, 256> selector_name{};
  std::optional<ProjectSelectorRequest> selector_request;
  bool selector_cancel_requested = false;
  std::string selector_error;
  std::string selector_status;
  bool selector_busy = false;
  bool selector_initialized = false;
  bool selector_focus_root = true;
  bool selector_read_only = false;
  bool selector_visible = false;
  std::uint32_t selector_recent_projects = 0;
  bool selector_root_active = false;
  bool app_focused = false;

  static void SetImeData(ImGuiContext *context, ImGuiViewport *, ImGuiPlatformImeData *data) {
    ImGui::SetCurrentContext(context);
    auto *state = static_cast<State *>(ImGui::GetIO().BackendPlatformUserData);
    if (state == nullptr || state->surface == nullptr || !data->WantVisible)
      return;
    static_cast<void>(state->surface->SetImeCandidatePosition(
        static_cast<std::int32_t>(std::lround(data->InputPos.x * state->dpi_scale)),
        static_cast<std::int32_t>(std::lround(data->InputPos.y * state->dpi_scale))));
  }
};

namespace {
void Activate(ImGuiContext *context) { ImGui::SetCurrentContext(context); }

template <typename StateT> void CancelNativeSceneGesture(StateT &state) {
  state.native_scene_drag_origin.reset();
  state.native_scene_drag_preview.reset();
  state.native_scene_drag.reset();
  state.native_scene_pick.reset();
  state.native_scene_select_all_request.reset();
  state.native_scene_frame_all_request.reset();
  state.native_scene_frame_all_apply.reset();
}

template <typename StateT> void CancelSceneGestures(StateT &state) {
  CancelNativeSceneGesture(state);
  state.scene_drag.reset();
}

template <typename StateT> void CancelInspectorDrafts(StateT &state) {
  if (std::ranges::any_of(state.inspector_transform_active, [](bool active) { return active; }) ||
      std::ranges::any_of(state.inspector_euler_active, [](bool active) { return active; }) ||
      std::ranges::any_of(state.inspector_camera_active, [](bool active) { return active; }) ||
      state.inspector_light_active || state.inspector_transform_request ||
      state.inspector_euler_request || state.inspector_camera_request ||
      state.inspector_light_request || state.inspector_material_request)
    ++state.inspector_draft_generation;
  state.inspector_transform_selection.clear();
  state.inspector_euler_selection.clear();
  state.inspector_transform_active = {};
  state.inspector_euler_active = {};
  state.inspector_transform_request.reset();
  state.inspector_euler_request.reset();
  state.inspector_component_selection.clear();
  state.inspector_camera_active = {};
  state.inspector_light_active = false;
  state.inspector_camera_request.reset();
  state.inspector_light_request.reset();
  state.inspector_material_request.reset();
}

void ApplyTheme() {
  ImGui::StyleColorsDark();
  auto &style = ImGui::GetStyle();
  style.WindowRounding = 4.0F;
  style.FrameRounding = 3.0F;
}

float DpiBucket(float scale) {
  constexpr std::array buckets{1.0F, 1.25F, 1.5F, 2.0F};
  return *std::ranges::min_element(buckets, {},
                                   [scale](float bucket) { return std::abs(bucket - scale); });
}

std::uint64_t GrownCapacity(std::uint64_t required) {
  std::uint64_t capacity = 4096;
  while (capacity < required)
    capacity *= 2;
  return capacity;
}

std::uint64_t TextureId(std::uint32_t index, std::uint32_t generation) {
  return static_cast<std::uint64_t>(generation) << 32U | index;
}

std::string PanelWindowName(std::string_view id) {
  const auto panels = ProductShell::Panels();
  const auto panel = std::ranges::find(panels, id, &PanelDescriptor::id);
  if (panel == panels.end())
    return std::string(id);
  return std::string(panel->title) + "###" + std::string(panel->id);
}

std::string PathLabel(const std::filesystem::path &path) {
  const auto encoded = path.generic_u8string();
  std::string result;
  result.reserve(encoded.size());
  for (const char8_t byte : encoded)
    result.push_back(static_cast<char>(byte));
  return result;
}

std::optional<std::filesystem::path> PathFromLabel(std::string_view label) {
  if (label.empty())
    return std::nullopt;
  try {
    std::u8string encoded;
    encoded.reserve(label.size());
    for (const char byte : label)
      encoded.push_back(static_cast<char8_t>(static_cast<unsigned char>(byte)));
    return std::filesystem::path(encoded);
  } catch (const std::exception &) {
    return std::nullopt;
  }
}

template <typename StateT>
void QueueProjectSelection(StateT &state, ProjectSelectorAction action,
                           const std::filesystem::path &root, std::string name,
                           ProjectAccess access) {
  if (root.empty()) {
    state.selector_error = "Choose a project root before continuing.";
    return;
  }
  if (action == ProjectSelectorAction::Create && name.empty()) {
    state.selector_error = "Enter a project name before creating it.";
    return;
  }
  state.selector_error.clear();
  state.selector_request = {action, root, std::move(name), access};
}

void BuildInitialDockLayout(ImGuiID dockspace, const ImGuiViewport &viewport) {
  ImGui::DockBuilderRemoveNode(dockspace);
  // ImGuiDockNodeFlags_DockSpace is ImGuiDockNodeFlagsPrivate_, a different enum type from the
  // public ImGuiDockNodeFlags_ that ImGuiDockNodeFlags_PassthruCentralNode belongs to; OR-ing them
  // directly triggers -Wdeprecated-enum-enum-conversion, so combine them as plain ints first.
  ImGui::DockBuilderAddNode(
      dockspace,
      static_cast<ImGuiDockNodeFlags>(static_cast<int>(ImGuiDockNodeFlags_DockSpace) |
                                      static_cast<int>(ImGuiDockNodeFlags_PassthruCentralNode)));
  ImGui::DockBuilderSetNodeSize(dockspace, viewport.Size);

  ImGuiID center = dockspace;
  const ImGuiID hierarchy =
      ImGui::DockBuilderSplitNode(center, ImGuiDir_Left, 0.22F, nullptr, &center);
  const ImGuiID inspector =
      ImGui::DockBuilderSplitNode(center, ImGuiDir_Right, 0.28F, nullptr, &center);
  const ImGuiID console =
      ImGui::DockBuilderSplitNode(center, ImGuiDir_Down, 0.25F, nullptr, &center);
  const auto project_window = PanelWindowName("nexora.project");
  const auto hierarchy_window = PanelWindowName("nexora.hierarchy");
  const auto inspector_window = PanelWindowName("nexora.inspector");
  const auto console_window = PanelWindowName("nexora.console");
  const auto profiler_window = PanelWindowName("nexora.profiler");
  const auto content_window = PanelWindowName("nexora.content");
  const auto scene_window = PanelWindowName("nexora.scene");
  const auto game_window = PanelWindowName("nexora.game");
  ImGui::DockBuilderDockWindow(project_window.c_str(), hierarchy);
  ImGui::DockBuilderDockWindow(hierarchy_window.c_str(), hierarchy);
  ImGui::DockBuilderDockWindow(inspector_window.c_str(), inspector);
  ImGui::DockBuilderDockWindow(console_window.c_str(), console);
  ImGui::DockBuilderDockWindow(profiler_window.c_str(), console);
  ImGui::DockBuilderDockWindow(content_window.c_str(), console);
  ImGui::DockBuilderDockWindow(scene_window.c_str(), center);
  ImGui::DockBuilderDockWindow(game_window.c_str(), center);
  ImGui::DockBuilderFinish(dockspace);
  // DockBuilderFinish binds existing windows and may replace the pre-finish selection. Set the
  // selected tabs after that bind so first-frame submission order cannot hide authoring views.
  if (auto *node = ImGui::DockBuilderGetNode(hierarchy))
    node->SelectedTabId = ImHashStr(hierarchy_window.c_str());
  if (auto *node = ImGui::DockBuilderGetNode(console))
    node->SelectedTabId = ImHashStr(content_window.c_str());
  if (auto *node = ImGui::DockBuilderGetNode(center))
    node->SelectedTabId = ImHashStr(scene_window.c_str());
}

struct AssetDragData final {
  std::uint64_t project_generation{};
  runtime::AssetUuid asset;
};
static_assert(std::is_trivially_copyable_v<AssetDragData>);

struct HierarchyDragData final {
  runtime::Id entity{};
  std::uint64_t entity_generation{};
  std::uint64_t document_generation{};
};
static_assert(std::is_trivially_copyable_v<HierarchyDragData>);

constexpr std::string_view kHierarchyDragType = "NEXORA_HIERARCHY_ENTITY";

bool ContainsAsciiInsensitive(std::string_view value, std::string_view query) {
  if (query.empty())
    return true;
  return std::ranges::search(value, query, [](char left, char right) {
           return std::tolower(static_cast<unsigned char>(left)) ==
                  std::tolower(static_cast<unsigned char>(right));
         }).begin() != value.end();
}

struct HierarchyRow final {
  const SceneDocument::NodeView *node = nullptr;
  std::uint32_t depth = 0;
  bool has_children = false;
};

template <typename StateT>
std::vector<HierarchyRow> BuildHierarchyRows(const StateT &state,
                                             std::span<const SceneDocument::NodeView> nodes,
                                             std::string_view filter) {
  std::vector<HierarchyRow> rows;
  rows.reserve(nodes.size());
  if (!filter.empty()) {
    for (const auto &node : nodes)
      if (ContainsAsciiInsensitive(node.name, filter))
        rows.push_back({&node, 0, false});
    return rows;
  }

  std::unordered_map<runtime::Id, const SceneDocument::NodeView *> by_id;
  std::unordered_map<runtime::Id, std::vector<const SceneDocument::NodeView *>> children;
  by_id.reserve(nodes.size());
  children.reserve(nodes.size());
  for (const auto &node : nodes)
    by_id.emplace(node.id, &node);
  std::unordered_set<runtime::Id> expanded;
  expanded.reserve(state.hierarchy_expanded.size());
  for (const auto &key : state.hierarchy_expanded)
    expanded.insert(key.id);
  for (const auto &node : nodes) {
    const auto parent = node.parent != 0 && by_id.contains(node.parent) ? node.parent : 0;
    children[parent].push_back(&node);
  }

  // Borrowed nodes live for this synchronous call. Reverse pushes preserve exact sibling order;
  // explicit work storage grows with input, so a valid deep tree cannot exhaust the native stack.
  struct Pending final {
    const SceneDocument::NodeView *node;
    std::uint32_t depth;
  };
  std::vector<Pending> pending;
  pending.reserve(nodes.size());
  if (const auto roots = children.find(0); roots != children.end())
    for (const auto *node : std::views::reverse(roots->second))
      pending.push_back({node, 0});
  while (!pending.empty()) {
    const auto current = pending.back();
    pending.pop_back();
    const auto group = children.find(current.node->id);
    const bool has_children = group != children.end() && !group->second.empty();
    rows.push_back({current.node, current.depth, has_children});
    if (has_children && expanded.contains(current.node->id))
      for (const auto *node : std::views::reverse(group->second))
        pending.push_back({node, current.depth + 1});
  }
  return rows;
}

template <typename StateT>
bool ApplyHierarchySelection(StateT &state, SceneDocument &scene,
                             std::span<const SceneDocument::NodeKey> visible,
                             SceneDocument::NodeKey entity, bool additive, bool range) {
  const auto target = std::ranges::find(visible, entity);
  if (target == visible.end())
    return false;

  std::vector<SceneDocument::NodeKey> selection;
  if (range && state.hierarchy_selection_anchor) {
    const auto anchor = std::ranges::find(visible, *state.hierarchy_selection_anchor);
    if (anchor != visible.end()) {
      const auto first = std::min(anchor, target);
      const auto last = std::max(anchor, target);
      selection.assign(first, std::next(last));
    }
  }
  if (selection.empty() && additive) {
    for (const auto selected : scene.Selection()) {
      const auto key = scene.Key(selected);
      if (!key)
        return false;
      selection.push_back(*key);
    }
    if (const auto selected = std::ranges::find(selection, entity); selected != selection.end())
      selection.erase(selected);
    else
      selection.push_back(entity);
  } else if (selection.empty()) {
    selection.push_back(entity);
  }
  if (!scene.Select(selection))
    return false;
  if (!range || !state.hierarchy_selection_anchor)
    state.hierarchy_selection_anchor = entity;
  return true;
}

std::size_t SiblingIndex(std::span<const SceneDocument::NodeView> nodes, runtime::Id entity) {
  const auto target = std::ranges::find(nodes, entity, &SceneDocument::NodeView::id);
  if (target == nodes.end())
    return 0;
  return static_cast<std::size_t>(std::ranges::count_if(
      nodes.begin(), target, [parent = target->parent](const SceneDocument::NodeView &node) {
        return node.parent == parent;
      }));
}

bool MoveHierarchySelection(SceneDocument &scene, std::span<const SceneDocument::NodeView> nodes,
                            int direction) {
  if (scene.Selection().size() != 1 || direction == 0)
    return false;
  const auto entity = scene.Selection().front();
  const auto target = std::ranges::find(nodes, entity, &SceneDocument::NodeView::id);
  if (target == nodes.end())
    return false;
  const auto sibling_count = static_cast<std::size_t>(
      std::ranges::count(nodes, target->parent, &SceneDocument::NodeView::parent));
  const auto current = SiblingIndex(nodes, entity);
  if ((direction < 0 && current == 0) || (direction > 0 && current + 1 >= sibling_count))
    return false;
  const auto parent = target->parent == 0 ? std::nullopt : scene.Key(target->parent);
  if (target->parent != 0 && !parent)
    return false;
  return scene.Move(target->Key(), parent, direction < 0 ? current - 1 : current + 1);
}

const char *ThumbnailLabel(ThumbnailState state) {
  switch (state) {
  case ThumbnailState::Loading:
    return "[Loading]";
  case ThumbnailState::Ready:
    return "[Ready]";
  case ThumbnailState::Failed:
    return "[Failed]";
  }
  return "[Unknown]";
}

bool ActionableConflict(const DirtyConflict &conflict) noexcept {
  return conflict.choice == DirtyConflictChoice::Pending ||
         conflict.choice == DirtyConflictChoice::Compare;
}

bool AcceptAssetDrop(ProjectContentSession &content, const std::filesystem::path &folder) {
  if (!ImGui::BeginDragDropTarget())
    return false;
  bool moved = false;
  if (const auto *payload = ImGui::AcceptDragDropPayload(AssetDragPayload::kType.data());
      payload != nullptr && payload->DataSize == sizeof(AssetDragData)) {
    const auto &data = *static_cast<const AssetDragData *>(payload->Data);
    moved = content.Move(
        {std::string(AssetDragPayload::kType), data.project_generation, data.asset}, folder);
  }
  ImGui::EndDragDropTarget();
  return moved;
}

template <typename StateT>
void ApplyPendingHierarchyRequests(StateT &state, SceneDocument *scene, bool editable,
                                   bool retain_rename) {
  state.hierarchy_visible_rows = 0;
  state.hierarchy_rendered_rows = 0;
  state.hierarchy_row_positions.clear();
  state.hierarchy_selection = 0;
  if (scene == nullptr) {
    state.hierarchy_selection_anchor.reset();
    state.hierarchy_navigation_cursor.reset();
    state.hierarchy_navigation_generation = 0;
    state.hierarchy_expanded.clear();
    state.hierarchy_selection_request.reset();
    state.hierarchy_create_request.reset();
    state.hierarchy_move_request.reset();
    state.hierarchy_reorder_request.reset();
    state.hierarchy_expansion_request.reset();
    state.hierarchy_rename_target.reset();
    state.hierarchy_rename_request.reset();
    state.hierarchy_error.clear();
    return;
  }
  if (!editable) {
    state.hierarchy_create_request.reset();
    state.hierarchy_move_request.reset();
    state.hierarchy_reorder_request.reset();
    state.hierarchy_rename_request.reset();
    if (!retain_rename)
      state.hierarchy_rename_target.reset();
  }
  const auto nodes = scene->Nodes();
  if (!state.hierarchy_expanded.empty()) {
    std::unordered_map<runtime::Id, SceneDocument::NodeKey> current_keys;
    current_keys.reserve(nodes.size());
    for (const auto &node : nodes)
      current_keys.emplace(node.id, node.Key());
    std::erase_if(state.hierarchy_expanded, [&](const auto key) {
      const auto current = current_keys.find(key.id);
      return current == current_keys.end() || current->second != key;
    });
  }
  if (state.hierarchy_selection_anchor &&
      scene->Key(state.hierarchy_selection_anchor->id) != state.hierarchy_selection_anchor)
    state.hierarchy_selection_anchor.reset();
  if (state.hierarchy_rename_target &&
      scene->Key(state.hierarchy_rename_target->id) != state.hierarchy_rename_target)
    state.hierarchy_rename_target.reset();

  if (state.hierarchy_expansion_request) {
    const auto request = std::exchange(state.hierarchy_expansion_request, std::nullopt);
    if (scene->Key(request->first.id) == request->first) {
      const auto expanded = std::ranges::find(state.hierarchy_expanded, request->first);
      if (request->second && expanded == state.hierarchy_expanded.end())
        state.hierarchy_expanded.push_back(request->first);
      else if (!request->second && expanded != state.hierarchy_expanded.end())
        state.hierarchy_expanded.erase(expanded);
    }
  }

  const auto rows =
      BuildHierarchyRows(state, nodes, std::string_view(state.hierarchy_filter.data()));
  std::vector<SceneDocument::NodeKey> visible;
  visible.reserve(rows.size());
  for (const auto &row : rows)
    visible.push_back(row.node->Key());
  state.hierarchy_visible_rows = static_cast<std::uint32_t>(
      std::min<std::size_t>(rows.size(), std::numeric_limits<std::uint32_t>::max()));

  if (state.hierarchy_selection_request) {
    const auto request = std::exchange(state.hierarchy_selection_request, std::nullopt);
    static_cast<void>(ApplyHierarchySelection(state, *scene, visible, request->entity,
                                              request->additive, request->range));
  }
  if (state.hierarchy_move_request) {
    const auto request = std::exchange(state.hierarchy_move_request, std::nullopt);
    static_cast<void>(scene->Move(request->entity, request->parent, request->index));
  }
  if (state.hierarchy_reorder_request) {
    const auto direction = std::exchange(state.hierarchy_reorder_request, std::nullopt);
    static_cast<void>(MoveHierarchySelection(*scene, nodes, *direction));
  }
  if (state.hierarchy_rename_request) {
    auto request = std::exchange(state.hierarchy_rename_request, std::nullopt);
    if (!scene->Rename(request->first, std::move(request->second)))
      state.hierarchy_error = "Rename rejected because the entity or document generation is stale.";
    else
      state.hierarchy_error.clear();
  }
  if (state.hierarchy_create_request) {
    auto request = std::exchange(state.hierarchy_create_request, std::nullopt);
    const bool stale_parent = request->parent && scene->Key(request->parent->id) != request->parent;
    const bool stale_document =
        request->document_generation != 0 && request->document_generation != scene->Generation();
    const auto parent = request->parent ? request->parent->id : 0;
    const auto created = stale_parent || stale_document ? runtime::Id{}
                         : request->kind == 1
                             ? scene->CreateCamera(std::move(request->name), parent)
                         : request->kind == 2 ? scene->CreateLight(std::move(request->name), parent)
                         : request->kind == 0 ? scene->Create(std::move(request->name), parent)
                                              : runtime::Id{};
    if (created == 0) {
      state.hierarchy_error =
          "Create rejected. Check the single-line name and current scene/parent.";
    } else {
      const std::array selection{created};
      static_cast<void>(scene->Select(selection));
      state.hierarchy_selection_anchor = scene->Key(created);
      if (request->parent && std::ranges::find(state.hierarchy_expanded, *request->parent) ==
                                 state.hierarchy_expanded.end())
        state.hierarchy_expanded.push_back(*request->parent);
      state.hierarchy_filter.fill({});
      state.hierarchy_error.clear();
    }
  }
  state.hierarchy_selection = static_cast<std::uint32_t>(
      std::min<std::size_t>(scene->Selection().size(), std::numeric_limits<std::uint32_t>::max()));
}

template <typename StateT> void CopyHierarchySelection(StateT &state, SceneDocument &scene) {
  if (!scene.CopySelection()) {
    state.hierarchy_error = "Copy rejected because the selection is empty or stale.";
    state.hierarchy_status.clear();
    return;
  }
  state.hierarchy_status = "Copied selected subtrees.";
  state.hierarchy_error.clear();
}

template <typename StateT> void CutHierarchySelection(StateT &state, SceneDocument &scene) {
  CancelSceneGestures(state);
  CancelInspectorDrafts(state);
  if (!scene.CutSelection()) {
    state.hierarchy_error = "Cut failed. Select existing Scene entities.";
    state.hierarchy_status.clear();
    return;
  }
  state.hierarchy_selection_anchor.reset();
  state.inspect_play_selection = false;
  state.hierarchy_status = "Cut selected subtrees. Paste or Undo.";
  state.hierarchy_error.clear();
}

template <typename StateT> void PasteHierarchySelection(StateT &state, SceneDocument &scene) {
  CancelSceneGestures(state);
  CancelInspectorDrafts(state);
  if (!scene.Paste()) {
    state.hierarchy_error = "Paste failed. Copy a valid scene selection first.";
    state.hierarchy_status.clear();
    return;
  }
  state.hierarchy_selection_anchor =
      scene.Selection().size() == 1 ? scene.Key(scene.Selection().front()) : std::nullopt;
  state.hierarchy_filter.fill({});
  state.inspect_play_selection = false;
  state.hierarchy_status = "Pasted " + std::to_string(scene.Selection().size()) + " roots.";
  state.hierarchy_error.clear();
}

template <typename StateT> void DeleteHierarchySelection(StateT &state, SceneDocument &scene) {
  if (!scene.DeleteSelection()) {
    state.hierarchy_error = "Delete failed because the selection is empty or stale.";
    state.hierarchy_status.clear();
    return;
  }
  state.hierarchy_selection_anchor.reset();
  state.hierarchy_status = "Deleted selected entities. Undo restores them.";
  state.hierarchy_error.clear();
}

template <typename StateT> void DuplicateHierarchySelection(StateT &state, SceneDocument &scene) {
  CancelSceneGestures(state);
  CancelInspectorDrafts(state);
  if (!scene.DuplicateSelection()) {
    state.hierarchy_error = "Duplicate failed because the selection is empty or stale.";
    state.hierarchy_status.clear();
    return;
  }
  state.hierarchy_selection_anchor =
      scene.Selection().size() == 1 ? scene.Key(scene.Selection().front()) : std::nullopt;
  state.hierarchy_filter.fill({});
  state.inspect_play_selection = false;
  state.hierarchy_status = "Duplicated " + std::to_string(scene.Selection().size()) + " roots.";
  state.hierarchy_error.clear();
}

template <typename StateT>
void DrawHierarchy(StateT &state, SceneDocument *scene, ProductShell &shell,
                   bool interaction_blocked, bool editable, bool rename_editable) {
  ImGui::SetNextItemWidth(-1.0F);
  ImGui::InputTextWithHint("##hierarchy-filter", "Filter entities...",
                           state.hierarchy_filter.data(), state.hierarchy_filter.size());
  if (ImGui::IsItemHovered())
    ImGui::SetTooltip("Filter entities. Ctrl+A selects all visible Hierarchy rows.");
  if (scene == nullptr) {
    ImGui::TextUnformatted("No scene is open.");
    return;
  }

  const auto capture_create = [&](std::size_t control) {
    const auto minimum = ImGui::GetItemRectMin(), maximum = ImGui::GetItemRectMax();
    state.hierarchy_create_positions[control] =
        std::array{(minimum.x + maximum.x) * 0.5F, (minimum.y + maximum.y) * 0.5F};
  };
  ImGui::SetNextItemWidth(-92.0F);
  if (ImGui::InputTextWithHint("##hierarchy-create-name", "New entity name...",
                               state.hierarchy_create_name.data(),
                               state.hierarchy_create_name.size()))
    state.hierarchy_create_custom_name = true;
  capture_create(6);
  ImGui::SameLine();
  ImGui::SetNextItemWidth(84.0F);
  constexpr std::array kinds{"Empty", "Camera", "Light"};
  ImGui::BeginDisabled(!editable);
  const bool kind_open =
      ImGui::BeginCombo("##hierarchy-create-kind", kinds[state.hierarchy_create_kind]);
  capture_create(0);
  if (kind_open) {
    for (int kind = 0; kind < static_cast<int>(kinds.size()); ++kind) {
      if (ImGui::Selectable(kinds[kind], state.hierarchy_create_kind == kind)) {
        if (!state.hierarchy_create_custom_name)
          std::snprintf(state.hierarchy_create_name.data(), state.hierarchy_create_name.size(),
                        "%s", kind == 0 ? "Entity" : kinds[kind]);
        state.hierarchy_create_kind = kind;
      }
      capture_create(static_cast<std::size_t>(kind + 1));
    }
    ImGui::EndCombo();
  }
  ImGui::EndDisabled();
  ImGui::BeginDisabled(!editable);
  if (ImGui::SmallButton("Create root"))
    state.hierarchy_create_request = typename StateT::HierarchyCreateRequest{
        std::string(state.hierarchy_create_name.data()), std::nullopt, state.hierarchy_create_kind,
        scene->Generation()};
  capture_create(4);
  ImGui::SameLine();
  const bool one_selected = scene->Selection().size() == 1;
  ImGui::BeginDisabled(!one_selected);
  if (ImGui::SmallButton("Create child") && one_selected) {
    if (const auto parent = scene->Key(scene->Selection().front()))
      state.hierarchy_create_request = typename StateT::HierarchyCreateRequest{
          std::string(state.hierarchy_create_name.data()), parent, state.hierarchy_create_kind,
          scene->Generation()};
    else
      state.hierarchy_error = "Create rejected because the selected parent is stale.";
  }
  capture_create(5);
  ImGui::EndDisabled();
  ImGui::SameLine();
  ImGui::BeginDisabled(scene->Selection().empty());
  if (ImGui::SmallButton("Delete selected")) {
    static_cast<void>(shell.RouteCommand("editor.scene.delete"));
    DeleteHierarchySelection(state, *scene);
  }
  ImGui::EndDisabled();
  ImGui::EndDisabled();
  if (editable && ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
      !ImGui::GetIO().WantTextInput && ImGui::IsKeyPressed(ImGuiKey_Delete, false)) {
    static_cast<void>(shell.RouteCommand("editor.scene.delete"));
    DeleteHierarchySelection(state, *scene);
  }
  ImGui::Separator();
  ImGui::BeginDisabled(scene->Selection().empty() || interaction_blocked);
  if (ImGui::SmallButton("Copy"))
    CopyHierarchySelection(state, *scene);
  ImGui::EndDisabled();
  ImGui::SameLine();
  ImGui::BeginDisabled(scene->Selection().empty() || !editable);
  if (ImGui::SmallButton("Cut")) {
    static_cast<void>(shell.RouteCommand("editor.scene.cut"));
    CutHierarchySelection(state, *scene);
  }
  const auto cut_min = ImGui::GetItemRectMin(), cut_max = ImGui::GetItemRectMax();
  state.hierarchy_cut_position =
      std::array{(cut_min.x + cut_max.x) * 0.5F, (cut_min.y + cut_max.y) * 0.5F};
  ImGui::EndDisabled();
  ImGui::SameLine();
  ImGui::BeginDisabled(!editable);
  if (ImGui::SmallButton("Paste"))
    PasteHierarchySelection(state, *scene);
  ImGui::EndDisabled();
  ImGui::SameLine();
  ImGui::BeginDisabled(scene->Selection().empty() || !editable);
  if (ImGui::SmallButton("Duplicate")) {
    static_cast<void>(shell.RouteCommand("editor.scene.duplicate"));
    DuplicateHierarchySelection(state, *scene);
  }
  ImGui::EndDisabled();
  if (!state.hierarchy_error.empty())
    ImGui::TextWrapped("%s", state.hierarchy_error.c_str());
  if (!state.hierarchy_status.empty())
    ImGui::TextUnformatted(state.hierarchy_status.c_str());

  const auto nodes = scene->Nodes();
  const std::string_view filter(state.hierarchy_filter.data());
  if (state.hierarchy_navigation_generation != scene->Generation() ||
      state.hierarchy_navigation_filter != filter) {
    state.hierarchy_navigation_cursor.reset();
    state.hierarchy_selection_anchor.reset();
    state.hierarchy_navigation_generation = scene->Generation();
    state.hierarchy_navigation_filter = filter;
  }
  auto rows = BuildHierarchyRows(state, nodes, filter);
  std::vector<SceneDocument::NodeKey> visible;
  visible.reserve(rows.size());
  for (const auto &row : rows)
    visible.push_back(row.node->Key());
  std::optional<SceneDocument::NodeKey> reveal;
  const bool keyboard =
      state.app_focused && !state.game_input_focused && !interaction_blocked &&
      !state.hierarchy_rename_target && !state.scene_drag && !state.native_scene_drag_origin &&
      !state.native_scene_drag && ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
      !ImGui::GetIO().WantTextInput && !ImGui::IsAnyItemActive() && !ImGui::GetDragDropPayload() &&
      !ImGui::IsMouseDown(ImGuiMouseButton_Left) &&
      !ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel);
  if (keyboard && !ImGui::GetIO().KeyCtrl && !ImGui::GetIO().KeyAlt && !ImGui::GetIO().KeySuper) {
    const auto pressed = [&](ImGuiKey key) {
      constexpr auto flags = ImGuiInputFlags_RouteFocused | ImGuiInputFlags_Repeat;
      const bool plain = ImGui::Shortcut(key, flags);
      const bool range = ImGui::Shortcut(ImGuiMod_Shift | key, flags);
      return plain || range;
    };
    const bool first = pressed(ImGuiKey_Home), last = pressed(ImGuiKey_End);
    const bool previous = pressed(ImGuiKey_UpArrow), next = pressed(ImGuiKey_DownArrow);
    constexpr auto flags = ImGuiInputFlags_RouteFocused | ImGuiInputFlags_Repeat;
    const bool left = ImGui::Shortcut(ImGuiKey_LeftArrow, flags);
    const bool right = ImGui::Shortcut(ImGuiKey_RightArrow, flags);
    if (!rows.empty() && (first || last || previous || next ||
                          ((left || right) && !ImGui::GetIO().KeyShift && filter.empty()))) {
      auto cursor = visible.end();
      // A collapsed/filtered multi-selection may contain many hidden entities. Membership
      // must not scan that entire selection for every visible fallback candidate.
      const std::unordered_set<runtime::Id> selected_ids(scene->Selection().begin(),
                                                         scene->Selection().end());
      const auto selected = [&](SceneDocument::NodeKey key) {
        return selected_ids.contains(key.id);
      };
      if (state.hierarchy_navigation_cursor && selected(*state.hierarchy_navigation_cursor))
        cursor = std::ranges::find(visible, *state.hierarchy_navigation_cursor);
      if (cursor == visible.end())
        cursor = std::ranges::find_if(visible, selected);
      auto index = cursor == visible.end() ? (previous || last ? rows.size() - 1 : 0)
                                           : static_cast<std::size_t>(cursor - visible.begin());
      const auto initial = visible[index];
      bool expansion_changed = false;
      if (first)
        index = 0;
      else if (last)
        index = rows.size() - 1;
      else if (previous || next) {
        if (cursor != visible.end())
          index = previous ? (index == 0 ? 0 : index - 1) : std::min(index + 1, rows.size() - 1);
      } else if (!ImGui::GetIO().KeyShift && filter.empty()) {
        const auto expanded = std::ranges::find(state.hierarchy_expanded, initial);
        if (right && rows[index].has_children && expanded == state.hierarchy_expanded.end()) {
          state.hierarchy_expanded.push_back(initial);
          expansion_changed = true;
        } else if (left && rows[index].has_children && expanded != state.hierarchy_expanded.end()) {
          state.hierarchy_expanded.erase(expanded);
          expansion_changed = true;
        } else if (right && rows[index].has_children && index + 1 < rows.size()) {
          ++index;
        } else if (left && rows[index].node->parent) {
          const auto parent = std::ranges::find_if(
              visible, [&](auto key) { return key.id == rows[index].node->parent; });
          if (parent != visible.end())
            index = static_cast<std::size_t>(parent - visible.begin());
        }
      }
      const auto target = visible[index];
      if (first || last || previous || next || !ImGui::GetIO().KeyShift) {
        const bool range = ImGui::GetIO().KeyShift;
        if (range &&
            (!state.hierarchy_selection_anchor ||
             std::ranges::find(visible, *state.hierarchy_selection_anchor) == visible.end()))
          state.hierarchy_selection_anchor = initial;
        CancelSceneGestures(state);
        CancelInspectorDrafts(state);
        if (ApplyHierarchySelection(state, *scene, visible, target, false, range)) {
          state.hierarchy_navigation_cursor = target;
          reveal = target;
        }
      }
      // Expansion changes must be reflected by the same frame's clipper and range snapshot.
      if (expansion_changed) {
        rows = BuildHierarchyRows(state, nodes, filter);
        visible.clear();
        for (const auto &row : rows)
          visible.push_back(row.node->Key());
      }
    }
  }
  state.hierarchy_visible_rows = static_cast<std::uint32_t>(
      std::min<std::size_t>(rows.size(), std::numeric_limits<std::uint32_t>::max()));
  state.hierarchy_rendered_rows = 0;
  if (!interaction_blocked && ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
      !ImGui::GetIO().WantTextInput &&
      ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_A, ImGuiInputFlags_RouteFocused)) {
    CancelSceneGestures(state);
    if (scene->Select(visible)) {
      static_cast<void>(shell.RouteCommand("editor.scene.select-all"));
      state.hierarchy_navigation_cursor.reset();
      state.hierarchy_selection_anchor =
          visible.empty() ? std::nullopt : std::optional{visible.front()};
      state.hierarchy_status = "Selected " + std::to_string(visible.size()) + " visible rows.";
      state.hierarchy_error.clear();
    } else {
      state.hierarchy_error = "Selection rejected because entity generations changed.";
      state.hierarchy_status.clear();
    }
  }

  const auto begin_rename = [&](const SceneDocument::NodeView &node) {
    auto count = std::min(node.name.size(), state.hierarchy_rename.size() - 1);
    // Never cut a UTF-8 sequence in half: back up to a code point boundary.
    while (count > 0 && count < node.name.size() &&
           (static_cast<unsigned char>(node.name[count]) & 0xC0U) == 0x80U)
      --count;
    std::memcpy(state.hierarchy_rename.data(), node.name.data(), count);
    state.hierarchy_rename[count] = {};
    CancelSceneGestures(state);
    CancelInspectorDrafts(state);
    state.hierarchy_rename_target = node.Key();
    state.hierarchy_rename_focus_pending = true;
    static_cast<void>(shell.RouteCommand("editor.scene.rename"));
    state.hierarchy_error.clear();
  };
  const SceneDocument::NodeView *selected_node = nullptr;
  if (scene->Selection().size() == 1) {
    const auto selected =
        std::ranges::find(nodes, scene->Selection().front(), &SceneDocument::NodeView::id);
    if (selected != nodes.end())
      selected_node = &*selected;
  }

  if (selected_node && editable && ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
      !ImGui::GetIO().WantTextInput && ImGui::Shortcut(ImGuiKey_F2, ImGuiInputFlags_RouteFocused))
    begin_rename(*selected_node);
  ImGui::BeginDisabled(selected_node == nullptr || !editable);
  if (ImGui::SmallButton("Rename") && selected_node != nullptr)
    begin_rename(*selected_node);
  ImGui::SameLine();
  if (ImGui::SmallButton("Move up"))
    static_cast<void>(MoveHierarchySelection(*scene, nodes, -1));
  ImGui::SameLine();
  if (ImGui::SmallButton("Move down"))
    static_cast<void>(MoveHierarchySelection(*scene, nodes, 1));
  ImGui::EndDisabled();
  ImGui::SameLine();
  if (ImGui::SmallButton("Clear selection")) {
    static_cast<void>(scene->Select(std::span<const runtime::Id>{}));
    state.hierarchy_selection_anchor.reset();
    state.hierarchy_navigation_cursor.reset();
  }
  ImGui::Separator();

  const auto handle_selection = [&](SceneDocument::NodeKey entity) {
    const auto &io = ImGui::GetIO();
    static_cast<void>(
        ApplyHierarchySelection(state, *scene, visible, entity, io.KeyCtrl, io.KeyShift));
    state.hierarchy_navigation_cursor = entity;
  };
  const auto handle_drag = [&](const SceneDocument::NodeView &node) {
    if (!editable)
      return;
    if (ImGui::BeginDragDropSource()) {
      const HierarchyDragData drag{node.id, node.entity_generation, node.document_generation};
      ImGui::SetDragDropPayload(kHierarchyDragType.data(), &drag, sizeof(drag));
      ImGui::Text("Move %.*s", static_cast<int>(node.name.size()), node.name.data());
      ImGui::EndDragDropSource();
    }
    if (!ImGui::BeginDragDropTarget())
      return;
    if (const auto *payload = ImGui::AcceptDragDropPayload(kHierarchyDragType.data());
        payload != nullptr && payload->DataSize == sizeof(HierarchyDragData)) {
      const auto &drag = *static_cast<const HierarchyDragData *>(payload->Data);
      const SceneDocument::NodeKey dragged{drag.entity, drag.entity_generation,
                                           drag.document_generation};
      const auto child_count = static_cast<std::size_t>(
          std::ranges::count(nodes, node.id, &SceneDocument::NodeView::parent));
      static_cast<void>(scene->Move(dragged, node.Key(), child_count));
    }
    ImGui::EndDragDropTarget();
  };

  ImGuiListClipper clipper;
  clipper.Begin(static_cast<int>(rows.size()));
  if (reveal) {
    const auto target = std::ranges::find(visible, *reveal);
    if (target != visible.end())
      clipper.IncludeItemByIndex(static_cast<int>(target - visible.begin()));
  }
  while (clipper.Step()) {
    for (int row_index = clipper.DisplayStart; row_index < clipper.DisplayEnd; ++row_index) {
      const auto &row = rows[static_cast<std::size_t>(row_index)];
      const auto &node = *row.node;
      const auto key = node.Key();
      const float indent = static_cast<float>(row.depth) * ImGui::GetTreeNodeToLabelSpacing();
      if (indent > 0.0F)
        ImGui::Indent(indent);

      const auto expanded = std::ranges::find(state.hierarchy_expanded, key);
      const bool is_expanded = expanded != state.hierarchy_expanded.end();
      ImGuiTreeNodeFlags flags =
          ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_NoTreePushOnOpen;
      if (filter.empty() && row.has_children) {
        flags |= ImGuiTreeNodeFlags_OpenOnArrow;
        ImGui::SetNextItemOpen(is_expanded, ImGuiCond_Always);
      } else {
        flags |= ImGuiTreeNodeFlags_Leaf;
      }
      if (std::ranges::find(scene->Selection(), node.id) != scene->Selection().end())
        flags |= ImGuiTreeNodeFlags_Selected;

      const auto tree_id = "##hierarchy-" + std::to_string(node.id) + "-" +
                           std::to_string(node.entity_generation) + "-" +
                           std::to_string(node.document_generation);
      const bool open = ImGui::TreeNodeEx(tree_id.c_str(), flags, "%.*s",
                                          static_cast<int>(node.name.size()), node.name.data());
      const auto minimum = ImGui::GetItemRectMin(), maximum = ImGui::GetItemRectMax();
      state.hierarchy_row_positions.push_back(
          {key, {(minimum.x + maximum.x) * 0.5F, (minimum.y + maximum.y) * 0.5F}});
      if (reveal == key)
        ImGui::SetScrollHereY();
      const bool toggled = ImGui::IsItemToggledOpen();
      if (ImGui::IsItemClicked() && !toggled)
        handle_selection(key);
      if (editable && ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
        begin_rename(node);
      if (filter.empty() && row.has_children && toggled) {
        if (open && !is_expanded)
          state.hierarchy_expanded.push_back(key);
        else if (!open && is_expanded)
          state.hierarchy_expanded.erase(expanded);
      }
      handle_drag(node);
      if (indent > 0.0F)
        ImGui::Unindent(indent);
      ++state.hierarchy_rendered_rows;
    }
  }
  clipper.End();

  ImGui::Selectable("Drop here to move to scene root", false, ImGuiSelectableFlags_AllowOverlap);
  if (editable && ImGui::BeginDragDropTarget()) {
    if (const auto *payload = ImGui::AcceptDragDropPayload(kHierarchyDragType.data());
        payload != nullptr && payload->DataSize == sizeof(HierarchyDragData)) {
      const auto &drag = *static_cast<const HierarchyDragData *>(payload->Data);
      const SceneDocument::NodeKey dragged{drag.entity, drag.entity_generation,
                                           drag.document_generation};
      const auto root_count = static_cast<std::size_t>(
          std::ranges::count(nodes, runtime::Id{}, &SceneDocument::NodeView::parent));
      static_cast<void>(scene->Move(dragged, std::nullopt, root_count));
    }
    ImGui::EndDragDropTarget();
  }
  if (visible.empty())
    ImGui::TextDisabled("No matching entities.");

  if (state.hierarchy_rename_target)
    ImGui::OpenPopup("Rename entity###editor.hierarchy.rename");
  if (ImGui::BeginPopupModal("Rename entity###editor.hierarchy.rename", nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::BeginDisabled(!rename_editable);
    ImGui::SetNextItemWidth(320.0F);
    if (state.hierarchy_rename_focus_pending) {
      ImGui::SetKeyboardFocusHere();
      state.hierarchy_rename_focus_pending = false;
    }
    const bool entered =
        ImGui::InputText("Name", state.hierarchy_rename.data(), state.hierarchy_rename.size(),
                         ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue);
    const bool clicked = ImGui::Button("Rename");
    const auto rename_min = ImGui::GetItemRectMin(), rename_max = ImGui::GetItemRectMax();
    state.hierarchy_rename_position =
        std::array{(rename_min.x + rename_max.x) * 0.5F, (rename_min.y + rename_max.y) * 0.5F};
    const bool submit = entered || clicked || ImGui::IsKeyPressed(ImGuiKey_Enter, false);
    ImGui::EndDisabled();
    ImGui::SameLine();
    const bool cancel = ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape, false);
    if (!state.hierarchy_rename_target) {
      // The target went stale (entity or document replaced) while the modal was open.
      state.hierarchy_error.clear();
      ImGui::CloseCurrentPopup();
    } else if (submit && rename_editable && !cancel) {
      if (scene->Rename(*state.hierarchy_rename_target,
                        std::string(state.hierarchy_rename.data()))) {
        state.hierarchy_rename_target.reset();
        state.hierarchy_error.clear();
        ImGui::CloseCurrentPopup();
      } else {
        state.hierarchy_error =
            "Rename rejected. Use a non-empty single-line name on a current entity.";
      }
    } else if (cancel) {
      state.hierarchy_rename_target.reset();
      state.hierarchy_error.clear();
      ImGui::CloseCurrentPopup();
    }
    if (!state.hierarchy_error.empty())
      ImGui::TextWrapped("%s", state.hierarchy_error.c_str());
    ImGui::EndPopup();
  }
  state.hierarchy_selection = static_cast<std::uint32_t>(
      std::min<std::size_t>(scene->Selection().size(), std::numeric_limits<std::uint32_t>::max()));
}

template <typename StateT>
bool FrameSceneSelection(StateT &state, const SceneDocument &scene, bool all = false,
                         double width = 0, double height = 0) {
  double min_x = std::numeric_limits<double>::infinity();
  double max_x = -min_x;
  double min_z = min_x;
  double max_z = -min_x;
  std::size_t included = 0;
  const auto include = [&](const runtime::Transform &pose) {
    ++included;
    min_x = std::min(min_x, pose.x);
    max_x = std::max(max_x, pose.x);
    min_z = std::min(min_z, pose.z);
    max_z = std::max(max_z, pose.z);
  };
  if (all || scene.Selection().size() > 1) {
    const auto poses = scene.WorldPoses();
    if (!poses)
      return false;
    const std::unordered_set<runtime::Id> selected(scene.Selection().begin(),
                                                   scene.Selection().end());
    for (const auto &pose : *poses)
      if (all || selected.contains(pose.id))
        include(pose.transform);
  } else {
    for (const auto id : scene.Selection()) {
      const auto pose = scene.WorldTransform(id);
      if (!pose)
        return false;
      include(*pose);
    }
  }
  if ((!all && included != scene.Selection().size()) ||
      min_x == std::numeric_limits<double>::infinity())
    return false;
  const double x = min_x * 0.5 + max_x * 0.5;
  const double z = min_z * 0.5 + max_z * 0.5;
  if (!std::isfinite(x) || !std::isfinite(z) || std::abs(x) > std::numeric_limits<float>::max() ||
      std::abs(z) > std::numeric_limits<float>::max())
    return false;
  state.scene_center_world = {static_cast<float>(x), static_cast<float>(z)};
  if (all)
    state.scene_pixels_per_unit = static_cast<float>(
        std::clamp(std::min(std::max(1.0, width - 48.0) / std::max(1.0, max_x - min_x),
                            std::max(1.0, height - 48.0) / std::max(1.0, max_z - min_z)),
                   4.0, 256.0));
  return true;
}

template <typename StateT>
bool FrameNativeSceneBounds(StateT &state, const std::array<double, 3> &minimum,
                            const std::array<double, 3> &maximum, double aspect) {
  if (!std::isfinite(aspect) || aspect <= 0)
    return false;
  for (std::size_t axis = 0; axis < 3; ++axis)
    if (!std::isfinite(minimum[axis]) || !std::isfinite(maximum[axis]) ||
        minimum[axis] > maximum[axis])
      return false;
  std::array<double, 3> center{}, extent{};
  for (std::size_t axis = 0; axis < 3; ++axis) {
    center[axis] = minimum[axis] * 0.5 + maximum[axis] * 0.5;
    extent[axis] = maximum[axis] * 0.5 - minimum[axis] * 0.5;
    if (!std::isfinite(center[axis]) || std::abs(center[axis]) > 100000.0)
      return false;
  }
  const double radius = std::hypot(extent[0], extent[1], extent[2]);
  if (!std::isfinite(radius))
    return false;
  constexpr double kHalfVerticalFov = 0.425;
  const double half_fov =
      std::min(kHalfVerticalFov, std::atan(std::tan(kHalfVerticalFov) * aspect));
  const double distance = std::clamp(1.5 * radius / std::sin(half_fov), 2.0, 100.0);
  state.native_scene_orbit.target_y = center[1];
  state.native_scene_orbit.distance = distance;
  state.scene_center_world = {static_cast<float>(center[0]), static_cast<float>(center[2])};
  return true;
}

template <typename StateT>
bool FrameNativeSceneSelection(StateT &state, const SceneDocument &scene,
                               const ProjectContentSession *content, const MeshAssetCatalog *meshes,
                               double aspect) {
  if (scene.Selection().empty() || !std::isfinite(aspect) || aspect <= 0)
    return false;
  // Frame selected forests once, including children of an empty parent. Geometry
  // is CPU-owned; framing does not perform IO, publish assets, or consume the native upload budget.
  std::unordered_map<runtime::Id, std::vector<runtime::Id>> children;
  for (const auto &node : scene.Nodes())
    children[node.parent].push_back(node.id);
  std::vector<runtime::Id> pending(scene.Selection().begin(), scene.Selection().end());
  std::unordered_set<runtime::Id> visited;
  const auto infinity = std::numeric_limits<double>::infinity();
  std::array<double, 3> minimum{infinity, infinity, infinity};
  std::array<double, 3> maximum{-infinity, -infinity, -infinity};
  for (std::size_t index = 0; index < pending.size(); ++index) {
    const auto id = pending[index];
    if (!visited.insert(id).second)
      continue;
    if (const auto found = children.find(id); found != children.end())
      pending.insert(pending.end(), found->second.begin(), found->second.end());
    const auto key = scene.Key(id);
    const auto pose = scene.WorldTransform(id);
    if (!key || !pose || !runtime::IsValidTransform(*pose))
      return false;
    std::array<double, 3> low{-0.45, -0.45, -0.45}, high{0.45, 0.45, 0.45};
    auto matrix = runtime::ToMatrix(*pose);
    double offset_y = 0.5;
    if (content && meshes) {
      const auto component = scene.MeshRenderer(*key);
      const auto asset = component ? meshes->ResolveResource(component->mesh,
                                                             content->Browser().ProjectGeneration())
                                   : std::nullopt;
      if (asset && asset->geometry && content->Browser().Find(asset->asset)) {
        const auto exact = scene.WorldMatrix(id);
        bool valid = exact.has_value();
        for (std::size_t axis = 0; axis < 3; ++axis)
          valid = valid && std::isfinite(asset->geometry->minimum[axis]) &&
                  std::isfinite(asset->geometry->maximum[axis]) &&
                  asset->geometry->minimum[axis] <= asset->geometry->maximum[axis];
        if (valid) {
          matrix = *exact;
          std::ranges::copy(asset->geometry->minimum, low.begin());
          std::ranges::copy(asset->geometry->maximum, high.begin());
          offset_y = 0;
        }
      }
    }
    for (const double x : {low[0], high[0]})
      for (const double y : {low[1], high[1]})
        for (const double z : {low[2], high[2]}) {
          const std::array point{matrix[0] * x + matrix[4] * y + matrix[8] * z + matrix[12],
                                 matrix[1] * x + matrix[5] * y + matrix[9] * z + matrix[13] +
                                     offset_y,
                                 matrix[2] * x + matrix[6] * y + matrix[10] * z + matrix[14]};
          for (std::size_t axis = 0; axis < 3; ++axis) {
            if (!std::isfinite(point[axis]))
              return false;
            minimum[axis] = std::min(minimum[axis], point[axis]);
            maximum[axis] = std::max(maximum[axis], point[axis]);
          }
        }
  }
  return FrameNativeSceneBounds(state, minimum, maximum, aspect);
}

const char *PauseReasonLabel(runtime::PauseReason reason) {
  switch (reason) {
  case runtime::PauseReason::None:
    return "None";
  case runtime::PauseReason::User:
    return "User pause";
  case runtime::PauseReason::StepComplete:
    return "Step complete";
  case runtime::PauseReason::DebuggerBreak:
    return "Debugger break";
  case runtime::PauseReason::RuntimeFailure:
    return "Runtime callback failed";
  }
  return "Unknown";
}
void DrawPlayEntityInspector(const runtime::RuntimeEntitySnapshot &entity) {
  ImGui::TextUnformatted("Play World (read-only)");
  ImGui::Text("Entity #%llu | Scene #%llu | %s", static_cast<unsigned long long>(entity.id),
              static_cast<unsigned long long>(entity.scene),
              entity.scene_state == runtime::SceneState::Active ? "Active" : "Inactive");
  ImGui::Text("Parent: #%llu", static_cast<unsigned long long>(entity.parent));
  const auto transform = [](const char *label, const runtime::Transform &pose) {
    ImGui::SeparatorText(label);
    ImGui::Text("Position: %.3f, %.3f, %.3f", pose.x, pose.y, pose.z);
    ImGui::Text("Rotation (quaternion): %.3f, %.3f, %.3f, %.3f", pose.qx, pose.qy, pose.qz,
                pose.qw);
    ImGui::Text("Scale: %.3f, %.3f, %.3f", pose.sx, pose.sy, pose.sz);
  };
  transform("Local Transform", entity.transform);
  transform("World Transform", entity.world_transform);
  if (entity.camera_data) {
    const auto &camera = *entity.camera_data;
    ImGui::SeparatorText("Camera");
    ImGui::Text("Vertical FOV: %.3f", camera.vertical_field_of_view);
    ImGui::Text("Near / Far: %.3f / %.3f", camera.near_plane, camera.far_plane);
  }
  if (entity.light_data) {
    ImGui::SeparatorText("Light");
    ImGui::Text("Intensity: %.3f", entity.light_data->intensity);
  }
  if (entity.mesh_data) {
    ImGui::SeparatorText("Mesh Renderer");
    ImGui::Text("Mesh: #%llu", static_cast<unsigned long long>(entity.mesh_data->mesh));
    ImGui::Text("Material shader: #%llu",
                static_cast<unsigned long long>(entity.mesh_data->material.shader));
  }
}

void DrawPlayOverview(const runtime::RuntimeInspectionSnapshot &snapshot) {
  ImGui::TextDisabled("Play World top-down X/Z (inspection snapshot)");
  const ImVec2 available = ImGui::GetContentRegionAvail();
  const ImVec2 size{std::max(available.x, 1.0F), 180.0F};
  ImGui::InvisibleButton("##play-overview", size);
  const ImVec2 min = ImGui::GetItemRectMin();
  const ImVec2 max = ImGui::GetItemRectMax();
  auto *draw = ImGui::GetWindowDrawList();
  draw->AddRectFilled(min, max, IM_COL32(22, 26, 33, 255));
  draw->PushClipRect(min, max, true);
  constexpr std::size_t max_markers = 4096;
  double min_x = 0.0, max_x = 0.0, min_z = 0.0, max_z = 0.0;
  std::size_t valid = 0;
  for (const auto &entity : snapshot.entities) {
    const auto &pose = entity.world_transform;
    if (!std::isfinite(pose.x) || !std::isfinite(pose.z) || std::abs(pose.x) > 1.0e9 ||
        std::abs(pose.z) > 1.0e9)
      continue;
    min_x = std::min(min_x, pose.x);
    max_x = std::max(max_x, pose.x);
    min_z = std::min(min_z, pose.z);
    max_z = std::max(max_z, pose.z);
    ++valid;
  }
  const double center_x = (min_x + max_x) * 0.5;
  const double center_z = (min_z + max_z) * 0.5;
  const double units_x = std::max(max_x - min_x, 1.0);
  const double units_z = std::max(max_z - min_z, 1.0);
  const float scale = static_cast<float>(
      std::clamp(std::min((size.x - 32.0F) / units_x, (size.y - 32.0F) / units_z), 0.001, 64.0));
  const ImVec2 center{(min.x + max.x) * 0.5F, (min.y + max.y) * 0.5F};
  const float axis_x = center.x + static_cast<float>(-center_x * scale);
  const float axis_z = center.y + static_cast<float>(-center_z * scale);
  if (axis_x >= min.x && axis_x <= max.x)
    draw->AddLine({axis_x, min.y}, {axis_x, max.y}, IM_COL32(80, 123, 185, 255));
  if (axis_z >= min.y && axis_z <= max.y)
    draw->AddLine({min.x, axis_z}, {max.x, axis_z}, IM_COL32(170, 79, 79, 255));
  std::size_t drawn = 0;
  for (const auto &entity : snapshot.entities) {
    if (drawn >= max_markers)
      break;
    const auto &pose = entity.world_transform;
    if (!std::isfinite(pose.x) || !std::isfinite(pose.z) || std::abs(pose.x) > 1.0e9 ||
        std::abs(pose.z) > 1.0e9)
      continue;
    const ImVec2 point{center.x + static_cast<float>((pose.x - center_x) * scale),
                       center.y + static_cast<float>((pose.z - center_z) * scale)};
    if (point.x < min.x || point.x > max.x || point.y < min.y || point.y > max.y)
      continue;
    const ImU32 color = entity.camera          ? IM_COL32(110, 170, 255, 255)
                        : entity.light         ? IM_COL32(255, 222, 135, 255)
                        : entity.mesh_renderer ? IM_COL32(139, 202, 185, 255)
                                               : IM_COL32(180, 181, 191, 255);
    draw->AddCircleFilled(point, 5.0F, color);
    ++drawn;
  }
  draw->PopClipRect();
  if (valid > max_markers)
    ImGui::TextDisabled("Showing at most %zu of %zu valid entities", max_markers, valid);
  ImGui::TextDisabled("Blue: camera  Yellow: light  Green: mesh  Gray: entity");
}

template <typename DragT>
std::array<double, 2> SceneDragWorldDelta(const DragT &drag, ImVec2 mouse) {
  const double dx =
      drag.axis == DragT::Axis::Z ? 0.0 : (mouse.x - drag.start_mouse.x) / drag.pixels_per_unit;
  const double dz =
      drag.axis == DragT::Axis::X ? 0.0 : (mouse.y - drag.start_mouse.y) / drag.pixels_per_unit;
  if (drag.snap_step <= 0.0F)
    return {dx, dz};
  return {SnapToStep(dx, drag.snap_step), SnapToStep(dz, drag.snap_step)};
}

void CaptureCanvasViewport(std::optional<Nexora::Presentation::SceneViewport> &rectangle,
                           ImVec2 min, ImVec2 max) {
  const auto clip_min = ImGui::GetWindowDrawList()->GetClipRectMin();
  const auto clip_max = ImGui::GetWindowDrawList()->GetClipRectMax();
  const auto &framebuffer = ImGui::GetIO();
  const auto pixel_width = framebuffer.DisplaySize.x * framebuffer.DisplayFramebufferScale.x;
  const auto pixel_height = framebuffer.DisplaySize.y * framebuffer.DisplayFramebufferScale.y;
  const auto left =
      std::clamp(std::floor(std::max(min.x, clip_min.x) * framebuffer.DisplayFramebufferScale.x),
                 0.0F, pixel_width);
  const auto top =
      std::clamp(std::floor(std::max(min.y, clip_min.y) * framebuffer.DisplayFramebufferScale.y),
                 0.0F, pixel_height);
  const auto right =
      std::clamp(std::ceil(std::min(max.x, clip_max.x) * framebuffer.DisplayFramebufferScale.x),
                 0.0F, pixel_width);
  const auto bottom =
      std::clamp(std::ceil(std::min(max.y, clip_max.y) * framebuffer.DisplayFramebufferScale.y),
                 0.0F, pixel_height);
  if (right > left && bottom > top)
    rectangle = {static_cast<std::uint32_t>(left), static_cast<std::uint32_t>(top),
                 static_cast<std::uint32_t>(right - left),
                 static_cast<std::uint32_t>(bottom - top)};
}

template <typename StateT>
bool PlaceContentMesh(StateT &state, SceneDocument &scene, ProjectContentSession *content,
                      const MeshAssetCatalog *meshes, runtime::AssetUuid asset,
                      std::uint64_t generation, runtime::Transform pose, bool editable) {
  const auto *item = content ? content->Browser().Find(asset) : nullptr;
  const auto resolved =
      item && meshes ? meshes->ResolveAsset(asset, generation) : std::optional<MeshAssetSnapshot>{};
  if (!editable || !content || !content->Writable() ||
      content->Browser().ProjectGeneration() != generation || !resolved)
    return false;
  CancelSceneGestures(state);
  CancelInspectorDrafts(state);
  const auto created = scene.CreateMesh(PathLabel(item->path.stem()),
                                        runtime::MeshComponent{resolved->resource, {}}, pose);
  if (!created) {
    state.content_scene_error =
        "Mesh placement rejected because the name, scene or pose is invalid.";
    return false;
  }
  static_cast<void>(scene.Select(std::array{created}));
  state.inspect_play_selection = false;
  state.content_scene_error.clear();
  return true;
}

template <typename StateT>
std::optional<runtime::Transform> NativeMeshDropPose(const StateT &state) {
  if (!state.scene_canvas_viewport)
    return std::nullopt;
  const auto &viewport = *state.scene_canvas_viewport;
  const auto &io = ImGui::GetIO();
  const auto &orbit = state.native_scene_orbit;
  const float x = std::clamp(state.scene_center_world.x, -100000.0F, 100000.0F);
  const float z = std::clamp(state.scene_center_world.y, -100000.0F, 100000.0F);
  const float y = static_cast<float>(orbit.target_y);
  ViewportCamera camera;
  camera.target = {x, y, z};
  camera.position = {
      x + static_cast<float>(orbit.distance * std::sin(orbit.yaw) * std::cos(orbit.pitch)),
      y + static_cast<float>(orbit.distance * std::sin(orbit.pitch)),
      z + static_cast<float>(orbit.distance * std::cos(orbit.yaw) * std::cos(orbit.pitch))};
  camera.vertical_fov_degrees = static_cast<double>(0.85F) * 180.0 / std::numbers::pi;
  const auto ray = ViewportPickRay(camera, viewport.width, viewport.height,
                                   io.MousePos.x * io.DisplayFramebufferScale.x - viewport.x,
                                   io.MousePos.y * io.DisplayFramebufferScale.y - viewport.y);
  if (!ray || std::abs(ray->direction.y) < 1e-6)
    return std::nullopt;
  const double distance = -ray->origin.y / ray->direction.y;
  if (!std::isfinite(distance) || distance < 0)
    return std::nullopt;
  runtime::Transform pose{ray->origin.x + distance * ray->direction.x, 0,
                          ray->origin.z + distance * ray->direction.z};
  if (!runtime::IsValidTransform(pose) || std::abs(pose.x) > 100000 || std::abs(pose.z) > 100000)
    return std::nullopt;
  const auto dx = camera.target.x - camera.position.x;
  const auto dy = camera.target.y - camera.position.y;
  const auto dz = camera.target.z - camera.position.z;
  const auto depth = ((pose.x - camera.position.x) * dx - camera.position.y * dy +
                      (pose.z - camera.position.z) * dz) /
                     std::hypot(dx, dy, dz);
  // Use the native Scene projection's near/far planes, rather than placing outside its view.
  if (!std::isfinite(depth) || depth < 0.1 || depth > 500)
    return std::nullopt;
  return pose;
}

template <typename StateT>
void AcceptSceneMeshDrop(StateT &state, SceneDocument &scene, ProjectContentSession *content,
                         const MeshAssetCatalog *meshes, bool editable,
                         std::optional<runtime::Transform> pose) {
  if (!ImGui::BeginDragDropTarget())
    return;
  if (const auto *payload = ImGui::AcceptDragDropPayload(AssetDragPayload::kType.data(),
                                                         ImGuiDragDropFlags_AcceptBeforeDelivery);
      payload && payload->DataSize == sizeof(AssetDragData)) {
    AssetDragData copied;
    std::memcpy(&copied, payload->Data, sizeof(copied));
    const auto *item = content ? content->Browser().Find(copied.asset) : nullptr;
    const bool resolved = item && meshes &&
                          content->Browser().ProjectGeneration() == copied.project_generation &&
                          meshes->ResolveAsset(copied.asset, copied.project_generation).has_value();
    const bool allowed = editable && state.app_focused && content && content->Writable();
    if (payload->IsPreview()) {
      if (!allowed)
        ImGui::SetTooltip("Scene editing is currently unavailable.");
      else if (!resolved)
        ImGui::SetTooltip("This mesh is no longer available.");
      else if (!pose)
        ImGui::SetTooltip("No supported ground placement at this point.");
      else
        ImGui::SetTooltip("Place %s at (%.2f, %.2f, %.2f)",
                          PathLabel(item->path.filename()).c_str(), pose->x, pose->y, pose->z);
    }
    if (payload->IsDelivery() && allowed && resolved && pose)
      static_cast<void>(PlaceContentMesh(state, scene, content, meshes, copied.asset,
                                         copied.project_generation, *pose, editable));
  }
  ImGui::EndDragDropTarget();
}

template <typename StateT>
void DrawSceneOverview(StateT &state, SceneDocument &scene, bool editable,
                       ProjectContentSession *content, const MeshAssetCatalog *meshes,
                       bool navigation_allowed) {
  ImGui::TextUnformatted("Top-down X/Z | Drag marker: free move | Drag red X/blue Z: axis move | "
                         "Middle: pan | Wheel: zoom");
  ImGui::BeginDisabled(scene.Selection().empty() || state.scene_drag.has_value());
  const bool frame_selected = ImGui::SmallButton("Frame selected");
  ImGui::EndDisabled();
  ImGui::SameLine();
  ImGui::BeginDisabled(state.scene_drag.has_value());
  const bool frame_all = ImGui::SmallButton("Frame all");
  const auto frame_min = ImGui::GetItemRectMin(), frame_max = ImGui::GetItemRectMax();
  state.scene_frame_all_position =
      std::array{(frame_min.x + frame_max.x) * 0.5F, (frame_min.y + frame_max.y) * 0.5F};
  ImGui::EndDisabled();
  ImGui::SameLine();
  ImGui::BeginDisabled(!navigation_allowed || state.scene_drag.has_value());
  const bool select_all = ImGui::SmallButton("Select all");
  const auto select_min = ImGui::GetItemRectMin(), select_max = ImGui::GetItemRectMax();
  state.scene_select_all_position =
      std::array{(select_min.x + select_max.x) * 0.5F, (select_min.y + select_max.y) * 0.5F};
  ImGui::EndDisabled();
  ImGui::TextDisabled("F: selected | Home: all | Ctrl+A: select all");
  constexpr std::array snap_steps{0.25F, 0.5F, 1.0F, 2.0F, 4.0F};
  ImGui::Checkbox("Snap movement", &state.scene_snap_to_grid);
  ImGui::SameLine();
  ImGui::BeginDisabled(!state.scene_snap_to_grid);
  ImGui::SetNextItemWidth(100.0F);
  ImGui::Combo("Step (world units)", &state.scene_snap_step_index,
               "0.25\0"
               "0.5\0"
               "1\0"
               "2\0"
               "4\0");
  ImGui::EndDisabled();
  const auto available = ImGui::GetContentRegionAvail();
  const ImVec2 size{std::max(available.x, 1.0F), std::max(available.y, 160.0F)};
  ImGui::InvisibleButton("##scene-overview", size,
                         ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonMiddle);
  const auto min = ImGui::GetItemRectMin();
  const auto max = ImGui::GetItemRectMax();
  CaptureCanvasViewport(state.scene_canvas_viewport, min, max);
  const ImVec2 center{(min.x + max.x) * 0.5F, (min.y + max.y) * 0.5F};
  const auto &io = ImGui::GetIO();
  if (navigation_allowed && !state.scene_drag) {
    const bool keyboard =
        !io.WantTextInput && ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
    if (!io.MouseDown[ImGuiMouseButton_Left] &&
        (select_all ||
         (keyboard && ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_A, ImGuiInputFlags_RouteFocused)))) {
      std::vector<SceneDocument::NodeKey> keys;
      for (const auto &node : scene.Nodes())
        keys.push_back(node.Key());
      if (scene.Select(keys)) {
        state.hierarchy_selection_anchor =
            keys.empty() ? std::nullopt : std::optional{keys.front()};
        state.inspect_play_selection = false;
      }
    }
    if (frame_all || (keyboard && ImGui::IsKeyPressed(ImGuiKey_Home, false)))
      static_cast<void>(FrameSceneSelection(state, scene, true, size.x, size.y));
    else if (frame_selected || (keyboard && ImGui::IsKeyPressed(ImGuiKey_F, false)))
      static_cast<void>(FrameSceneSelection(state, scene));
  }
  if (state.scene_drag) {
    if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
      state.scene_drag.reset();
    } else if (!io.MouseDown[ImGuiMouseButton_Left]) {
      if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
        const auto &drag = *state.scene_drag;
        const float raw_dx =
            drag.axis == StateT::SceneDrag::Axis::Z ? 0.0F : io.MousePos.x - drag.start_mouse.x;
        const float raw_dz =
            drag.axis == StateT::SceneDrag::Axis::X ? 0.0F : io.MousePos.y - drag.start_mouse.y;
        if (raw_dx * raw_dx + raw_dz * raw_dz >= 36.0F) {
          const auto [dx, dz] = SceneDragWorldDelta(drag, io.MousePos);
          if ((dx != 0.0 || dz != 0.0) && !scene.TranslateSelectionXZ(drag.entities, dx, dz))
            state.hierarchy_error =
                "Scene drag rejected because an entity changed or the pose is invalid.";
        }
      }
      state.scene_drag.reset();
    }
  }
  if (ImGui::IsItemHovered()) {
    if (!state.scene_drag && ImGui::IsMouseDragging(ImGuiMouseButton_Middle, 0.0F)) {
      state.scene_center_world.x -= io.MouseDelta.x / state.scene_pixels_per_unit;
      state.scene_center_world.y -= io.MouseDelta.y / state.scene_pixels_per_unit;
    }
    if (!state.scene_drag && io.MouseWheel != 0.0F) {
      const auto old_scale = state.scene_pixels_per_unit;
      const auto new_scale = std::clamp(old_scale * std::pow(1.15F, io.MouseWheel), 4.0F, 256.0F);
      state.scene_center_world.x +=
          (io.MousePos.x - center.x) * (1.0F / old_scale - 1.0F / new_scale);
      state.scene_center_world.y +=
          (io.MousePos.y - center.y) * (1.0F / old_scale - 1.0F / new_scale);
      state.scene_pixels_per_unit = new_scale;
    }
  }

  auto *draw = ImGui::GetWindowDrawList();
  draw->AddRectFilled(min, max, IM_COL32(22, 26, 33, 255));
  draw->PushClipRect(min, max, true);
  const ImVec2 origin{center.x - state.scene_center_world.x * state.scene_pixels_per_unit,
                      center.y - state.scene_center_world.y * state.scene_pixels_per_unit};
  float grid_units = 1.0F;
  while (grid_units * state.scene_pixels_per_unit < 24.0F)
    grid_units *= 2.0F;
  const float spacing = grid_units * state.scene_pixels_per_unit;
  const auto first_line = [spacing](float offset) {
    const auto remainder = std::fmod(offset, spacing);
    return remainder < 0.0F ? remainder + spacing : remainder;
  };
  for (float x = min.x + first_line(origin.x - min.x); x < max.x; x += spacing)
    draw->AddLine({x, min.y}, {x, max.y}, IM_COL32(49, 56, 67, 255));
  for (float y = min.y + first_line(origin.y - min.y); y < max.y; y += spacing)
    draw->AddLine({min.x, y}, {max.x, y}, IM_COL32(49, 56, 67, 255));
  if (origin.y >= min.y && origin.y <= max.y)
    draw->AddLine({min.x, origin.y}, {max.x, origin.y}, IM_COL32(170, 79, 79, 255), 1.5F);
  if (origin.x >= min.x && origin.x <= max.x)
    draw->AddLine({origin.x, min.y}, {origin.x, max.y}, IM_COL32(80, 123, 185, 255), 1.5F);

  ImVec2 preview_pixels{};
  if (state.scene_drag && io.MouseDown[ImGuiMouseButton_Left]) {
    const auto &drag = *state.scene_drag;
    const auto raw_dx =
        drag.axis == StateT::SceneDrag::Axis::Z ? 0.0F : io.MousePos.x - drag.start_mouse.x;
    const auto raw_dz =
        drag.axis == StateT::SceneDrag::Axis::X ? 0.0F : io.MousePos.y - drag.start_mouse.y;
    if (raw_dx * raw_dx + raw_dz * raw_dz >= 36.0F) {
      const auto [dx, dz] = SceneDragWorldDelta(drag, io.MousePos);
      preview_pixels = {static_cast<float>(dx * drag.pixels_per_unit),
                        static_cast<float>(dz * drag.pixels_per_unit)};
      if (dx != 0.0 || dz != 0.0)
        draw->AddLine(
            drag.start_mouse,
            {drag.start_mouse.x + preview_pixels.x, drag.start_mouse.y + preview_pixels.y},
            IM_COL32(255, 199, 87, 255), 2.0F);
    }
  }

  state.scene_markers.clear();
  const auto nodes = scene.Nodes();
  const auto poses = scene.WorldPoses();
  std::unordered_map<runtime::Id, const runtime::Transform *> world_poses;
  if (poses) {
    world_poses.reserve(poses->size());
    for (const auto &pose : *poses)
      world_poses.emplace(pose.id, &pose.transform);
  }
  std::unordered_map<runtime::Id, bool> dragged;
  if (poses && state.scene_drag && (preview_pixels.x != 0 || preview_pixels.y != 0)) {
    std::unordered_map<runtime::Id, runtime::Id> parents;
    parents.reserve(nodes.size());
    for (const auto &node : nodes)
      parents.emplace(node.id, node.parent);
    std::unordered_set<runtime::Id> selected;
    for (const auto &key : state.scene_drag->entities)
      selected.insert(key.id);
    dragged.reserve(nodes.size());
    std::vector<runtime::Id> path;
    for (const auto &node : nodes) {
      path.clear();
      auto ancestor = node.id;
      while (ancestor && !dragged.contains(ancestor)) {
        if (selected.contains(ancestor)) {
          dragged.emplace(ancestor, true);
          break;
        }
        path.push_back(ancestor);
        const auto parent = parents.find(ancestor);
        ancestor = parent == parents.end() ? 0 : parent->second;
      }
      const bool moves = ancestor && dragged.at(ancestor);
      for (const auto id : path)
        dragged.emplace(id, moves);
    }
  }
  for (const auto &node : nodes) {
    const auto found = world_poses.find(node.id);
    if (found == world_poses.end())
      continue;
    const auto *pose = found->second;
    ImVec2 position{origin.x + static_cast<float>(pose->x) * state.scene_pixels_per_unit,
                    origin.y + static_cast<float>(pose->z) * state.scene_pixels_per_unit};
    if (const auto moving = dragged.find(node.id); moving != dragged.end() && moving->second) {
      position.x += preview_pixels.x;
      position.y += preview_pixels.y;
    }
    if (!std::isfinite(position.x) || !std::isfinite(position.y) || position.x < min.x - 10.0F ||
        position.x > max.x + 10.0F || position.y < min.y - 10.0F || position.y > max.y + 10.0F)
      continue;
    state.scene_markers.push_back({node.Key(), position});
    const bool selected = std::ranges::find(scene.Selection(), node.id) != scene.Selection().end();
    draw->AddCircleFilled(position, selected ? 7.0F : 5.0F,
                          selected ? IM_COL32(255, 199, 87, 255) : IM_COL32(139, 202, 185, 255));
    if (selected)
      draw->AddCircle(position, 9.0F, IM_COL32(255, 223, 148, 255));
    if (state.scene_pixels_per_unit >= 12.0F)
      draw->AddText({position.x + 11.0F, position.y - 8.0F}, IM_COL32(224, 230, 239, 255),
                    node.name.data(), node.name.data() + node.name.size());
  }
  const auto handle = std::ranges::find_if(state.scene_markers, [&](const auto &marker) {
    return std::ranges::find(scene.Selection(), marker.entity.id) != scene.Selection().end();
  });
  if (handle != state.scene_markers.end()) {
    const auto position = handle->position;
    draw->AddLine({position.x + 12.0F, position.y}, {position.x + 42.0F, position.y},
                  IM_COL32(225, 99, 99, 255), 3.0F);
    draw->AddTriangleFilled({position.x + 48.0F, position.y},
                            {position.x + 39.0F, position.y - 5.0F},
                            {position.x + 39.0F, position.y + 5.0F}, IM_COL32(225, 99, 99, 255));
    draw->AddLine({position.x, position.y + 12.0F}, {position.x, position.y + 42.0F},
                  IM_COL32(99, 150, 225, 255), 3.0F);
    draw->AddTriangleFilled({position.x, position.y + 48.0F},
                            {position.x - 5.0F, position.y + 39.0F},
                            {position.x + 5.0F, position.y + 39.0F}, IM_COL32(99, 150, 225, 255));
  }
  draw->PopClipRect();
  if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
    std::optional<SceneDocument::NodeKey> picked;
    auto axis = StateT::SceneDrag::Axis::Free;
    if (handle != state.scene_markers.end()) {
      const auto position = handle->position;
      const float x = io.MousePos.x - position.x;
      const float z = io.MousePos.y - position.y;
      if (x >= 12.0F && x <= 48.0F && std::abs(z) <= 6.0F) {
        picked = handle->entity;
        axis = StateT::SceneDrag::Axis::X;
      } else if (z >= 12.0F && z <= 48.0F && std::abs(x) <= 6.0F) {
        picked = handle->entity;
        axis = StateT::SceneDrag::Axis::Z;
      }
    }
    float nearest = 100.0F;
    for (const auto &marker : state.scene_markers) {
      if (picked)
        break;
      const float dx = marker.position.x - io.MousePos.x;
      const float dy = marker.position.y - io.MousePos.y;
      const float distance = dx * dx + dy * dy;
      if (distance <= nearest) {
        picked = marker.entity;
        nearest = distance;
      }
    }
    if (picked) {
      const bool already_selected =
          std::ranges::find(scene.Selection(), picked->id) != scene.Selection().end();
      std::vector<SceneDocument::NodeKey> visible;
      visible.reserve(state.scene_markers.size());
      for (const auto &marker : state.scene_markers)
        visible.push_back(marker.entity);
      if (!already_selected || io.KeyCtrl || io.KeyShift)
        static_cast<void>(
            ApplyHierarchySelection(state, scene, visible, *picked, io.KeyCtrl, io.KeyShift));
      if (editable && !io.KeyCtrl && !io.KeyShift) {
        std::vector<SceneDocument::NodeKey> keys;
        for (const auto id : scene.Selection()) {
          const auto key = scene.Key(id);
          if (key)
            keys.push_back(*key);
        }
        if (!keys.empty())
          state.scene_drag = typename StateT::SceneDrag{
              std::move(keys), io.MousePos, state.scene_pixels_per_unit, axis,
              state.scene_snap_to_grid ? snap_steps[state.scene_snap_step_index] : 0.0F};
      }
    } else if (!io.KeyCtrl && !io.KeyShift) {
      static_cast<void>(scene.Select(std::span<const runtime::Id>{}));
      state.hierarchy_selection_anchor.reset();
    }
  }
  const runtime::Transform drop_pose{
      state.scene_center_world.x + (io.MousePos.x - center.x) / state.scene_pixels_per_unit, 0,
      state.scene_center_world.y + (io.MousePos.y - center.y) / state.scene_pixels_per_unit};
  AcceptSceneMeshDrop(state, scene, content, meshes, editable,
                      runtime::IsValidTransform(drop_pose) ? std::optional{drop_pose}
                                                           : std::nullopt);
}

template <typename StateT>
EulerDegrees InspectorAngles(StateT &state, const SceneDocument &scene, SceneDocument::NodeKey key,
                             const runtime::Transform &transform) {
  const auto degrees = scene.EulerAngles(key.id).value_or(EulerDegrees{});
  state.inspector_euler_hints.insert_or_assign(
      key.id, typename StateT::InspectorEulerHint{key, transform, degrees});
  return degrees;
}

template <typename StateT>
void ApplyInspectorEuler(StateT &state, SceneDocument &scene,
                         std::span<const SceneDocument::NodeKey> selection, bool editable) {
  if (!state.inspector_euler_request)
    return;
  const auto request = std::exchange(state.inspector_euler_request, std::nullopt);
  if (!editable || !std::ranges::equal(request->entities, selection)) {
    state.inspector_error = "Rotation edit rejected because access or selection changed.";
    return;
  }
  CancelSceneGestures(state);
  if (!scene.SetEulerField(request->entities, request->axis, request->degrees)) {
    state.inspector_error =
        "Rotation edit rejected because its values or entity generation are stale.";
    return;
  }
  for (const auto key : request->entities)
    static_cast<void>(InspectorAngles(state, scene, key, *scene.Transform(key.id)));
  state.inspector_error.clear();
}

template <typename StateT>
bool ResetInspectorComponent(StateT &state, SceneDocument &scene,
                             std::span<const SceneDocument::NodeKey> keys, std::size_t component) {
  CancelSceneGestures(state);
  CancelInspectorDrafts(state);
  ImGui::ClearActiveID();
  const bool reset = component == 0   ? scene.ResetTransforms(keys)
                     : component == 1 ? scene.ResetCameras(keys)
                                      : scene.ResetLights(keys);
  if (reset)
    state.inspector_error.clear();
  else
    state.inspector_error = "Reset rejected because entity generations are stale.";
  return reset;
}

template <typename StateT> void CaptureSceneTabControl(StateT &state, std::size_t control) {
  const auto low = ImGui::GetItemRectMin(), high = ImGui::GetItemRectMax();
  state.scene_tab_controls[control] = std::array{(low.x + high.x) * .5F, (low.y + high.y) * .5F};
}
template <typename StateT> void EmitSceneTab(StateT &state, SceneTabRequest request) {
  CancelSceneGestures(state);
  CancelInspectorDrafts(state);
  state.scene_tab_status.clear();
  state.scene_tab_output = std::move(request);
}
template <typename StateT> void DrawSceneTabs(StateT &state, bool allowed, bool writable) {
  state.scene_tab_controls = {};
  state.scene_tab_positions.clear();
  if (state.scene_tabs.empty())
    return;
  const auto active =
      std::ranges::find(state.scene_tabs, state.scene_tab_active, &SceneTabItem::id);
  if (active == state.scene_tabs.end())
    return;
  const bool idle =
      allowed && !state.scene_tabs_busy && !state.scene_tab_output && !state.scene_tab_dialog;
  const bool named = std::ranges::all_of(
      state.scene_tabs, [](const auto &item) { return !item.owned || item.path.has_value(); });
  const auto begin_close = [&] {
    SceneTabRequest request{SceneTabAction::Close, state.scene_tab_source, active->id,
                            active->token};
    if (active->dirty) {
      CancelSceneGestures(state);
      CancelInspectorDrafts(state);
      state.scene_tab_close = request;
      state.scene_tab_dialog = 3;
      state.scene_tab_popup_pending = true;
      state.scene_tab_status.clear();
    } else
      EmitSceneTab(state, std::move(request));
  };
  if (idle && !ImGui::GetIO().WantTextInput) {
    const auto route = ImGuiInputFlags_RouteGlobal;
    if (writable && state.scene_tabs.size() < 16 &&
        ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiMod_Alt | ImGuiKey_N, route))
      EmitSceneTab(state, {SceneTabAction::New, state.scene_tab_source});
    else if (writable && named && ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiMod_Alt | ImGuiKey_S, route))
      EmitSceneTab(state, {SceneTabAction::SaveAll, state.scene_tab_source});
    else if (state.scene_tabs.size() < 16 &&
             (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiMod_Alt | ImGuiMod_Shift | ImGuiKey_O, route) ||
              ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiMod_Alt | ImGuiKey_O, route))) {
      CancelSceneGestures(state);
      CancelInspectorDrafts(state);
      state.scene_tab_path = {};
      state.scene_tab_dialog = ImGui::GetIO().KeyShift ? 2 : 1;
      state.scene_tab_popup_pending = true;
      state.scene_tab_status.clear();
    } else if (active->closeable &&
               ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiMod_Alt | ImGuiKey_W, route)) {
      begin_close();
    } else {
      const bool previous = ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiMod_Alt | ImGuiKey_PageUp, route);
      const bool next = ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiMod_Alt | ImGuiKey_PageDown, route);
      if ((previous || next) && state.scene_tabs.size() > 1) {
        const auto index = static_cast<std::size_t>(active - state.scene_tabs.begin());
        const auto target_index =
            previous ? (index + state.scene_tabs.size() - 1) % state.scene_tabs.size()
                     : (index + 1) % state.scene_tabs.size();
        const auto &target = state.scene_tabs[target_index];
        EmitSceneTab(state,
                     {SceneTabAction::Select, state.scene_tab_source, target.id, target.token});
      }
    }
  }
  ImGui::BeginDisabled(!idle || state.scene_tab_output || state.scene_tab_dialog);
  if (ImGui::BeginTabBar("Scene documents")) {
    for (const auto &item : state.scene_tabs) {
      const auto label = item.label + (item.dirty ? " *" : "") +
                         (item.owned ? "" : " [reference]") + "###scene-document-" +
                         std::to_string(item.id);
      const bool visible = ImGui::BeginTabItem(
          label.c_str(), nullptr,
          item.id == state.scene_tab_active ? ImGuiTabItemFlags_SetSelected : 0);
      const auto low = ImGui::GetItemRectMin(), high = ImGui::GetItemRectMax();
      state.scene_tab_positions[item.id] =
          std::array{(low.x + high.x) * .5F, (low.y + high.y) * .5F};
      if (ImGui::IsItemClicked() && item.id != state.scene_tab_active)
        EmitSceneTab(state, {SceneTabAction::Select, state.scene_tab_source, item.id, item.token});
      if (visible)
        ImGui::EndTabItem();
    }
    ImGui::EndTabBar();
  }
  ImGui::BeginDisabled(!writable || state.scene_tabs.size() >= 16);
  if (ImGui::Button("New additive"))
    EmitSceneTab(state, {SceneTabAction::New, state.scene_tab_source});
  CaptureSceneTabControl(state, 0);
  ImGui::EndDisabled();
  const auto continue_row = [](const char *label) {
    const auto &style = ImGui::GetStyle();
    const auto right = ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x;
    if (ImGui::GetItemRectMax().x + style.ItemSpacing.x + ImGui::CalcTextSize(label).x +
            2 * style.FramePadding.x <=
        right)
      ImGui::SameLine();
  };
  for (int reference = 0; reference != 2; ++reference) {
    continue_row(reference ? "Open reference..." : "Open additive...");
    ImGui::BeginDisabled(state.scene_tabs.size() >= 16);
    if (ImGui::Button(reference ? "Open reference..." : "Open additive...")) {
      CancelSceneGestures(state);
      CancelInspectorDrafts(state);
      state.scene_tab_path = {};
      state.scene_tab_dialog = reference ? 2 : 1;
      state.scene_tab_popup_pending = true;
      state.scene_tab_status.clear();
    }
    CaptureSceneTabControl(state, static_cast<std::size_t>(reference + 1));
    ImGui::EndDisabled();
  }
  continue_row("Save All");
  ImGui::BeginDisabled(!writable || !named);
  if (ImGui::Button("Save All"))
    EmitSceneTab(state, {SceneTabAction::SaveAll, state.scene_tab_source});
  CaptureSceneTabControl(state, 3);
  ImGui::EndDisabled();
  continue_row("Close scene");
  ImGui::BeginDisabled(!active->closeable);
  if (ImGui::Button("Close scene"))
    begin_close();
  CaptureSceneTabControl(state, 4);
  ImGui::EndDisabled();
  ImGui::EndDisabled();
  if (active->read_only)
    ImGui::TextDisabled("Read-only scene: resolve the saved scene set before editing.");
  else if (!active->owned)
    ImGui::TextDisabled("Reference scene: editing and file writes disabled.");
  if (!state.scene_tab_status.empty())
    ImGui::TextWrapped("%s", state.scene_tab_status.c_str());
  ImGui::Separator();
}

template <typename StateT> void DrawSceneTabDialog(StateT &state, bool writable, bool cancel) {
  if (cancel) {
    state.scene_tab_dialog = 0;
    state.scene_tab_close.reset();
    state.scene_tab_output.reset();
    state.scene_tab_popup_pending = false;
  }
  const bool opening = state.scene_tab_popup_pending;
  if (opening) {
    ImGui::OpenPopup("Scene documents###editor.scene-tabs");
    state.scene_tab_popup_pending = false;
  }
  const auto size = ImGui::GetMainViewport()->WorkSize;
  ImGui::SetNextWindowSize({std::max(1.0F, std::min(600.0F, size.x - 24)), 0}, ImGuiCond_Always);
  if (!ImGui::BeginPopupModal("Scene documents###editor.scene-tabs", nullptr,
                              ImGuiWindowFlags_AlwaysAutoResize))
    return;
  if (!state.scene_tab_dialog) {
    ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
    return;
  }
  const bool closing = state.scene_tab_dialog == 3;
  ImGui::BeginDisabled(state.scene_tabs_busy || state.scene_tab_output.has_value());
  if (closing && state.scene_tab_close) {
    ImGui::TextWrapped("This scene has unsaved changes. Save the owned scenes before closing, "
                       "discard this scene's changes, or keep it open.");
    const auto target =
        std::ranges::find(state.scene_tabs, state.scene_tab_close->target, &SceneTabItem::id);
    ImGui::BeginDisabled(!writable || target == state.scene_tabs.end() || !target->owned);
    if (ImGui::Button("Save all and close")) {
      auto request = *state.scene_tab_close;
      request.save_before_close = true;
      EmitSceneTab(state, std::move(request));
    }
    CaptureSceneTabControl(state, 8);
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("Discard and close")) {
      auto request = *state.scene_tab_close;
      request.discard_dirty = true;
      EmitSceneTab(state, std::move(request));
    }
    CaptureSceneTabControl(state, 9);
  } else {
    ImGui::TextUnformatted(state.scene_tab_dialog == 2 ? "Open an inspection-only reference scene."
                                                       : "Open another scene in this project.");
    if (opening)
      ImGui::SetKeyboardFocusHere();
    const bool submitted =
        ImGui::InputText("Relative .scene path", state.scene_tab_path.data(),
                         state.scene_tab_path.size(), ImGuiInputTextFlags_EnterReturnsTrue);
    CaptureSceneTabControl(state, 5);
    if (ImGui::Button("Open") || submitted) {
      const std::string text(state.scene_tab_path.data());
      if (text.empty() || !foundation::IsValidUtf8(text))
        state.scene_tab_status = "Choose a valid project-relative scene path.";
      else {
        SceneTabRequest request{state.scene_tab_dialog == 2 ? SceneTabAction::OpenReference
                                                            : SceneTabAction::OpenOwned,
                                state.scene_tab_source};
        request.path = std::filesystem::path(std::u8string(text.begin(), text.end()));
        EmitSceneTab(state, std::move(request));
      }
    }
    CaptureSceneTabControl(state, 6);
  }
  ImGui::SameLine();
  if (ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
    state.scene_tab_dialog = 0;
    state.scene_tab_close.reset();
    ImGui::CloseCurrentPopup();
  }
  CaptureSceneTabControl(state, closing ? 10 : 7);
  ImGui::EndDisabled();
  if (!state.scene_tab_status.empty())
    ImGui::TextWrapped("%s", state.scene_tab_status.c_str());
  ImGui::EndPopup();
}

template <typename StateT> void CaptureSceneFileControl(StateT &state, std::size_t control) {
  const auto minimum = ImGui::GetItemRectMin(), maximum = ImGui::GetItemRectMax();
  state.scene_file_positions[control] =
      std::array{(minimum.x + maximum.x) * 0.5F, (minimum.y + maximum.y) * 0.5F};
}

template <typename StateT> void EmitSceneFile(StateT &state) {
  state.scene_file_output = std::exchange(state.scene_file_intent, std::nullopt);
  state.scene_file_dialog = StateT::FileDialog::None;
}

template <typename StateT> void SetSceneFileDraft(StateT &state) {
  const auto path = state.scene_file_path.value_or(std::filesystem::path("Content/Untitled.scene"));
  const auto text = PathLabel(path);
  std::snprintf(state.scene_file_text.data(), state.scene_file_text.size(), "%s", text.c_str());
}

template <typename StateT>
void BeginSceneFile(StateT &state, SceneDocument &scene, SceneFileAction action) {
  CancelSceneGestures(state);
  CancelInspectorDrafts(state);
  ImGui::ClearActiveID();
  state.hierarchy_rename_target.reset();
  state.scene_file_intent = SceneFileRequest{action, state.scene_file_token};
  state.scene_file_save_before_switch = false;
  SetSceneFileDraft(state);
  if (action == SceneFileAction::New) {
    if (!scene.Dirty()) {
      EmitSceneFile(state);
      return;
    }
    state.scene_file_dialog = StateT::FileDialog::Unsaved;
  } else
    state.scene_file_dialog = action == SceneFileAction::Open ? StateT::FileDialog::OpenPath
                                                              : StateT::FileDialog::SavePath;
  state.scene_file_focus_path = true;
  state.scene_file_popup_pending = true;
}

template <typename StateT>
void BeginContentScene(StateT &state, SceneDocument &scene, const std::filesystem::path &path) {
  if (state.scene_file_dialog != StateT::FileDialog::None || state.scene_file_output)
    return;
  CancelSceneGestures(state);
  CancelInspectorDrafts(state);
  ImGui::ClearActiveID();
  state.hierarchy_rename_target.reset();
  state.scene_file_intent = SceneFileRequest{SceneFileAction::Open, state.scene_file_token, path};
  state.scene_file_save_before_switch = false;
  if (!scene.Dirty()) {
    EmitSceneFile(state);
    return;
  }
  state.scene_file_dialog = StateT::FileDialog::Unsaved;
  state.scene_file_popup_pending = true;
}

template <typename StateT>
void DrawSceneFileDialog(StateT &state, SceneDocument *scene, bool writable, bool cancel) {
  if (cancel || !scene || !state.scene_file_context ||
      state.scene_file_token.document_generation != scene->Generation() ||
      (state.scene_file_intent && state.scene_file_intent->token != state.scene_file_token)) {
    state.scene_file_intent.reset();
    state.scene_file_output.reset();
    state.scene_file_dialog = StateT::FileDialog::None;
    state.scene_file_popup_pending = false;
  }
  if (state.scene_file_popup_pending) {
    ImGui::OpenPopup("Scene file###editor.scene-file");
    state.scene_file_popup_pending = false;
  }
  const auto size = ImGui::GetMainViewport()->WorkSize;
  // A fixed available width prevents wrapped diagnostics from measuring at a transient tiny
  // auto-fit width and putting the path field outside the viewport when reopening for retry.
  ImGui::SetNextWindowSize({std::max(1.0F, std::min(640.0F, size.x - 24)), 0}, ImGuiCond_Always);
  ImGui::SetNextWindowSizeConstraints({0, 0},
                                      {std::max(1.0F, size.x - 24), std::max(1.0F, size.y - 24)});
  if (!ImGui::BeginPopupModal("Scene file###editor.scene-file", nullptr,
                              ImGuiWindowFlags_AlwaysAutoResize))
    return;
  const auto cancel_dialog = [&] {
    state.scene_file_dialog = StateT::FileDialog::None;
    state.scene_file_intent.reset();
    state.scene_file_close_popup = false;
    ImGui::CloseCurrentPopup();
  };
  if (state.scene_file_dialog == StateT::FileDialog::None || !state.scene_file_intent) {
    cancel_dialog();
    ImGui::EndPopup();
    return;
  }
  if (!state.scene_save_success && !state.scene_save_message.empty())
    ImGui::TextWrapped("%s", state.scene_save_message.c_str());
  bool emit = false;
  auto &request = *state.scene_file_intent;
  if (state.scene_file_dialog == StateT::FileDialog::Unsaved) {
    ImGui::TextUnformatted("Save scene changes before continuing?");
    ImGui::BeginDisabled(!writable || state.scene_file_save_blocked);
    if (ImGui::Button("Save")) {
      request.save_current = true;
      if (state.scene_file_path) {
        emit = true;
      } else {
        state.scene_file_save_before_switch = true;
        state.scene_file_dialog = StateT::FileDialog::SavePath;
        state.scene_file_focus_path = true;
        SetSceneFileDraft(state);
      }
    }
    CaptureSceneFileControl(state, 7);
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("Discard changes")) {
      request.discard_unsaved = true;
      emit = true;
    }
    CaptureSceneFileControl(state, 8);
  } else if (state.scene_file_dialog == StateT::FileDialog::Overwrite) {
    const auto &path = request.save_path ? *request.save_path : request.path;
    ImGui::TextWrapped("Replace the existing scene at %s?", PathLabel(path).c_str());
    ImGui::BeginDisabled(!writable);
    if (ImGui::Button("Replace")) {
      request.replace_existing = true;
      emit = true;
    }
    CaptureSceneFileControl(state, 9);
    ImGui::EndDisabled();
  } else {
    const bool opening = state.scene_file_dialog == StateT::FileDialog::OpenPath;
    ImGui::TextUnformatted(opening ? "Open scene" : "Save scene as");
    ImGui::TextWrapped("Choose a .scene path inside this project, such as Content/Level.scene.");
    ImGui::SetNextItemWidth(std::max(1.0F, std::min(520.0F, size.x - 80)));
    if (std::exchange(state.scene_file_focus_path, false))
      ImGui::SetKeyboardFocusHere();
    const bool submit =
        ImGui::InputText("Scene path", state.scene_file_text.data(), state.scene_file_text.size(),
                         ImGuiInputTextFlags_EnterReturnsTrue);
    CaptureSceneFileControl(state, 4);
    ImGui::BeginDisabled(!opening && !writable);
    const bool apply = ImGui::Button(opening ? "Open" : "Save As");
    CaptureSceneFileControl(state, 5);
    if (apply || (submit && (opening || writable))) {
      const std::string_view text(state.scene_file_text.data());
      if (!text.empty() && foundation::IsValidUtf8(text)) {
        const std::filesystem::path path(std::u8string(text.begin(), text.end()));
        if (state.scene_file_save_before_switch) {
          request.save_path = path;
          emit = true;
        } else {
          request.path = path;
          if (opening && scene->Dirty())
            state.scene_file_dialog = StateT::FileDialog::Unsaved;
          else {
            emit = true;
          }
        }
      }
    }
    ImGui::EndDisabled();
  }
  ImGui::SameLine();
  if (ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape, false))
    cancel_dialog();
  CaptureSceneFileControl(state, 6);
  // Transfer only after widgets finish using the intent's fields in this frame.
  if (emit && state.scene_file_intent) {
    EmitSceneFile(state);
    ImGui::CloseCurrentPopup();
  }
  ImGui::EndPopup();
}

template <typename StateT> void CaptureInspectorReset(StateT &state, std::size_t component) {
  const auto minimum = ImGui::GetItemRectMin(), maximum = ImGui::GetItemRectMax();
  state.inspector_reset_positions[component] =
      std::array{(minimum.x + maximum.x) * 0.5F, (minimum.y + maximum.y) * 0.5F};
}

// The clipboard owns only committed numeric values. It never borrows a source entity/document.
template <typename StateT>
bool DrawInspectorClipboard(StateT &state, SceneDocument &scene,
                            std::span<const SceneDocument::NodeKey> keys, std::size_t component,
                            bool editable, bool copy_allowed) {
  const bool present =
      component == 0 || std::ranges::any_of(keys, [&](const auto key) {
        return component == 1 ? scene.Camera(key).has_value() : scene.Light(key).has_value();
      });
  const bool can_copy = copy_allowed && state.app_focused && keys.size() == 1 && present;
  ImGui::PushID(static_cast<int>(component));
  const auto capture = [&](std::size_t control) {
    const auto minimum = ImGui::GetItemRectMin(), maximum = ImGui::GetItemRectMax();
    state.inspector_clipboard_positions[component][control] =
        std::array{(minimum.x + maximum.x) * 0.5F, (minimum.y + maximum.y) * 0.5F};
  };
  ImGui::BeginDisabled(!can_copy);
  if (ImGui::SmallButton("Copy values")) {
    CancelSceneGestures(state);
    CancelInspectorDrafts(state);
    ImGui::ClearActiveID();
    if (component == 0)
      state.inspector_component_clipboard = typename StateT::TransformValues{
          *scene.Transform(keys.front().id), *scene.EulerAngles(keys.front().id)};
    else if (component == 1)
      state.inspector_component_clipboard = *scene.Camera(keys.front());
    else
      state.inspector_component_clipboard = *scene.Light(keys.front());
    state.inspector_error.clear();
  }
  capture(0);
  ImGui::EndDisabled();
  if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
    ImGui::SetTooltip(
        "Copy committed values from one selected entity. Unsubmitted input is discarded.");
  ImGui::SameLine();
  const bool can_paste = editable && state.app_focused && present &&
                         state.inspector_component_clipboard &&
                         state.inspector_component_clipboard->index() == component;
  ImGui::BeginDisabled(!can_paste);
  bool pasted = false;
  if (ImGui::SmallButton("Paste values")) {
    CancelSceneGestures(state);
    CancelInspectorDrafts(state);
    ImGui::ClearActiveID();
    const auto &copied = *state.inspector_component_clipboard;
    if (component == 0) {
      const auto &value = std::get<0>(copied);
      pasted = scene.SetTransformValues(keys, value.transform, value.degrees);
    } else if (component == 1) {
      std::vector<std::optional<runtime::CameraComponent>> values;
      for (const auto key : keys)
        values.push_back(scene.Camera(key) ? std::optional{std::get<1>(copied)} : std::nullopt);
      pasted = scene.SetCameras(keys, values);
    } else {
      std::vector<std::optional<runtime::LightComponent>> values;
      for (const auto key : keys)
        values.push_back(scene.Light(key) ? std::optional{std::get<2>(copied)} : std::nullopt);
      pasted = scene.SetLights(keys, values);
    }
    if (pasted)
      state.inspector_error.clear();
    else
      state.inspector_error =
          "Paste rejected because the selected entities are no longer available.";
  }
  capture(1);
  ImGui::EndDisabled();
  if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
    ImGui::SetTooltip(
        component == 0 ? "Paste copied Transform values, including authored Euler turns."
                       : "Paste matching copied values to existing components in the selection.");
  ImGui::PopID();
  return pasted;
}

template <typename StateT>
void AcceptInspectorMeshDrop(StateT &state, ProjectContentSession *content,
                             const MeshAssetCatalog *meshes, bool editable,
                             const std::vector<SceneDocument::NodeKey> &keys) {
  if (!ImGui::BeginDragDropTarget())
    return;
  if (const auto *payload = ImGui::AcceptDragDropPayload(AssetDragPayload::kType.data(),
                                                         ImGuiDragDropFlags_AcceptBeforeDelivery);
      payload && payload->DataSize == sizeof(AssetDragData)) {
    AssetDragData copied;
    std::memcpy(&copied, payload->Data, sizeof(copied));
    const auto *item = content ? content->Browser().Find(copied.asset) : nullptr;
    const bool resolved = item && meshes &&
                          content->Browser().ProjectGeneration() == copied.project_generation &&
                          meshes->ResolveAsset(copied.asset, copied.project_generation).has_value();
    const bool allowed = editable && state.app_focused && content && content->Writable();
    if (payload->IsPreview()) {
      if (!allowed)
        ImGui::SetTooltip("Mesh editing is currently unavailable.");
      else if (!resolved)
        ImGui::SetTooltip("This mesh is no longer available.");
      else
        ImGui::SetTooltip("Assign %s to %zu selected entities",
                          PathLabel(item->path.filename()).c_str(), keys.size());
    }
    if (payload->IsDelivery() && allowed && resolved) {
      CancelInspectorDrafts(state);
      state.inspector_mesh_request =
          typename StateT::InspectorMeshRequest{keys, copied.asset, copied.project_generation};
    }
  }
  ImGui::EndDragDropTarget();
}

template <typename StateT>
void DrawInspector(StateT &state, SceneDocument *scene, ProjectContentSession *content,
                   const MeshAssetCatalog *meshes, const MaterialAssetCatalog *materials,
                   bool editable, bool copy_allowed) {
  state.inspector_selection =
      scene == nullptr ? 0U : static_cast<std::uint32_t>(scene->Selection().size());
  state.inspector_transform_visible = false;
  state.inspector_camera_presence_mixed = state.inspector_light_presence_mixed = false;
  state.inspector_camera_mixed = {};
  state.inspector_light_mixed = false;
  state.inspector_material_label.clear();
  state.inspector_material_positions = {};
  state.inspector_mesh_label.clear();
  state.inspector_mesh_positions = {};
  state.inspector_reset_positions = {};
  state.inspector_clipboard_positions = {};
  state.inspector_opaque_info.clear();
  std::unordered_set<runtime::Id> selected_entities;
  if (scene != nullptr)
    selected_entities.insert(scene->Selection().begin(), scene->Selection().end());
  std::erase_if(state.inspector_euler_hints, [&](const auto &entry) {
    const auto &hint = entry.second;
    return scene == nullptr || scene->Key(hint.entity.id) != hint.entity ||
           !selected_entities.contains(hint.entity.id);
  });
  if (scene == nullptr || scene->Selection().empty()) {
    CancelInspectorDrafts(state);
    state.inspector_mesh_request.reset();
    state.inspector_material_request.reset();
    state.inspector_camera_request.reset();
    state.inspector_light_request.reset();
    ImGui::TextUnformatted("Select an entity to inspect it.");
    return;
  }
  std::vector<SceneDocument::NodeKey> keys;
  std::vector<runtime::Transform> transforms;
  keys.reserve(scene->Selection().size());
  transforms.reserve(scene->Selection().size());
  for (const auto entity : scene->Selection()) {
    const auto key = scene->Key(entity);
    const auto transform = scene->Transform(entity);
    if (!key || !transform) {
      CancelInspectorDrafts(state);
      ImGui::TextUnformatted("A selected entity is no longer available.");
      return;
    }
    keys.push_back(*key);
    transforms.push_back(*transform);
  }
  state.inspector_transform_visible = true;
  if (keys.size() == 1)
    ImGui::Text("%.*s", static_cast<int>(scene->Name(keys.front().id).size()),
                scene->Name(keys.front().id).data());
  else
    ImGui::Text("%zu entities selected", keys.size());
  if (state.inspector_component_clipboard) {
    constexpr std::array names{"Transform", "Camera", "Light"};
    ImGui::TextDisabled("Copied values: %s", names[state.inspector_component_clipboard->index()]);
  }
  std::optional<runtime::AssetUuid> material_reference;
  bool has_material_reference = false;
  for (const auto key : keys)
    if (auto info = scene->InspectOpaqueComponents(key)) {
      for (const auto &component : *info) {
        const auto reference = ReadMaterialAssetReference(component);
        if (keys.size() == 1 && (component.type == kMaterialAssetReferenceType ||
                                 component.type_name == kMaterialAssetReferenceName)) {
          has_material_reference = true;
          material_reference = reference;
        }
      }
      state.inspector_opaque_info.insert(state.inspector_opaque_info.end(),
                                         std::make_move_iterator(info->begin()),
                                         std::make_move_iterator(info->end()));
    }
  std::erase_if(state.inspector_opaque_info,
                [](const auto &info) { return ReadMaterialAssetReference(info).has_value(); });
  if (!state.inspector_opaque_info.empty() &&
      ImGui::CollapsingHeader("Unavailable component data", ImGuiTreeNodeFlags_DefaultOpen)) {
    ImGui::TextWrapped("Read-only: component data is preserved. Restore a compatible plugin or "
                       "Editor version to edit it.");
    ImGui::BeginChild("##opaque-components", {0, 140}, ImGuiChildFlags_Borders,
                      ImGuiWindowFlags_HorizontalScrollbar);
    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(state.inspector_opaque_info.size()),
                  3 * ImGui::GetTextLineHeightWithSpacing());
    while (clipper.Step())
      for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
        const auto &info = state.inspector_opaque_info[static_cast<std::size_t>(row)];
        ImGui::TextUnformatted(info.type_name.c_str());
        ImGui::Text("Entity %llu | Type %llu | %zu bytes",
                    static_cast<unsigned long long>(info.entity),
                    static_cast<unsigned long long>(info.type), info.byte_count);
        std::string hex;
        for (const auto byte : info.preview) {
          hex += "0123456789abcdef"[byte >> 4];
          hex += "0123456789abcdef"[byte & 15];
        }
        if (info.byte_count > info.preview.size())
          hex += "...";
        ImGui::TextDisabled("%s", hex.empty() ? "(empty payload)" : hex.c_str());
      }
    ImGui::EndChild();
  }
  if (!editable)
    CancelInspectorDrafts(state);
  if (state.inspector_transform_selection != keys) {
    state.inspector_transform_selection = keys;
    state.inspector_transform_active = {};
  }
  std::uint32_t selection_hash = 2166136261U;
  for (const auto key : keys) {
    for (const auto value :
         {static_cast<std::uint64_t>(key.id), static_cast<std::uint64_t>(key.entity_generation),
          static_cast<std::uint64_t>(key.document_generation)}) {
      selection_hash = (selection_hash ^ static_cast<std::uint32_t>(value)) * 16777619U;
      selection_hash = (selection_hash ^ static_cast<std::uint32_t>(value >> 32U)) * 16777619U;
    }
  }
  ImGui::PushID(static_cast<int>(selection_hash));
  ImGui::PushID(static_cast<int>(state.inspector_draft_generation));
  ImGui::SeparatorText("Transform");
  if (DrawInspectorClipboard(state, *scene, keys, 0, editable, copy_allowed))
    for (std::size_t i = 0; i < keys.size(); ++i)
      transforms[i] = *scene->Transform(keys[i].id);
  ImGui::BeginDisabled(!editable);
  if (ImGui::SmallButton("Reset Transform") && ResetInspectorComponent(state, *scene, keys, 0))
    std::ranges::fill(transforms, runtime::Transform{});
  CaptureInspectorReset(state, 0);
  ImGui::TextDisabled("Enter applies; Escape cancels.");
  struct Field final {
    const char *label;
    double runtime::Transform::*member;
  };
  constexpr std::array fields{
      Field{"Position X", &runtime::Transform::x}, Field{"Position Y", &runtime::Transform::y},
      Field{"Position Z", &runtime::Transform::z}, Field{"Scale X", &runtime::Transform::sx},
      Field{"Scale Y", &runtime::Transform::sy},   Field{"Scale Z", &runtime::Transform::sz}};
  for (std::size_t axis = 0; axis < fields.size(); ++axis) {
    const auto &field = fields[axis];
    double value = transforms.front().*(field.member);
    const bool mixed = std::ranges::any_of(
        transforms, [&](const auto &transform) { return transform.*(field.member) != value; });
    state.inspector_transform_mixed[axis] = mixed;
    auto &text = state.inspector_transform_text[axis];
    if (!state.inspector_transform_active[axis]) {
      if (mixed)
        text[0] = '\0';
      else {
        const auto formatted = std::to_chars(text.data(), text.data() + text.size() - 1, value,
                                             std::chars_format::general, 17);
        *formatted.ptr = '\0';
      }
    }
    if (state.inspector_transform_focus_request == axis) {
      ImGui::SetKeyboardFocusHere();
      state.inspector_transform_focus_request.reset();
    }
    const bool submit = ImGui::InputTextWithHint(
        field.label, mixed ? "Mixed" : "Value", text.data(), text.size(),
        ImGuiInputTextFlags_CharsScientific | ImGuiInputTextFlags_EnterReturnsTrue);
    state.inspector_transform_active[axis] = ImGui::IsItemActive();
    if (submit) {
      const std::string_view entered{text.data()};
      const auto number = entered.starts_with('+') ? entered.substr(1) : entered;
      double next{};
      const auto parsed = std::from_chars(number.data(), number.data() + number.size(), next);
      if (parsed.ec != std::errc{} || parsed.ptr != number.data() + number.size() ||
          !std::isfinite(next))
        state.inspector_error = "Enter a finite position or nonzero scale.";
      else if (mixed || next != value) {
        auto edits = transforms;
        for (auto &transform : edits)
          transform.*(field.member) = next;
        state.inspector_transform_request.emplace(
            typename StateT::InspectorTransformRequest{keys, std::move(edits)});
      } else {
        state.inspector_error.clear();
      }
    }
  }

  ImGui::SeparatorText("Local rotation (degrees, Z-X-Y)");
  std::vector<EulerDegrees> angles;
  angles.reserve(keys.size());
  for (std::size_t i = 0; i < keys.size(); ++i)
    angles.push_back(InspectorAngles(state, *scene, keys[i], transforms[i]));
  if (state.inspector_euler_selection != keys) {
    state.inspector_euler_selection = keys;
    state.inspector_euler_active = {};
  }
  constexpr std::array labels{"Rotation X", "Rotation Y", "Rotation Z"};
  for (std::size_t axis = 0; axis < labels.size(); ++axis) {
    double value = angles.front()[axis];
    const bool mixed = std::ranges::any_of(
        angles, [&](const auto &item) { return std::abs(item[axis] - value) > 1e-8; });
    auto &text = state.inspector_euler_text[axis];
    if (!state.inspector_euler_active[axis]) {
      if (mixed)
        text[0] = '\0';
      else
        std::snprintf(text.data(), text.size(), "%.6f", value);
    }
    if (state.inspector_euler_focus_request == axis) {
      ImGui::SetKeyboardFocusHere();
      state.inspector_euler_focus_request.reset();
    }
    const bool submit = ImGui::InputTextWithHint(
        labels[axis], mixed ? "Mixed" : "Degrees", text.data(), text.size(),
        ImGuiInputTextFlags_CharsScientific | ImGuiInputTextFlags_EnterReturnsTrue);
    state.inspector_euler_active[axis] = ImGui::IsItemActive();
    if (submit) {
      const std::string_view entered{text.data()};
      const auto number = entered.starts_with('+') ? entered.substr(1) : entered;
      const auto parsed = std::from_chars(number.data(), number.data() + number.size(), value);
      if (parsed.ec != std::errc{} || parsed.ptr != number.data() + number.size() ||
          !std::isfinite(value))
        state.inspector_error = "Enter a finite angle in degrees.";
      else
        state.inspector_euler_request.emplace(
            typename StateT::InspectorEulerRequest{keys, axis, value});
    }
  }
  ImGui::EndDisabled();
  ImGui::PopID();
  ImGui::PopID();

  if (state.inspector_transform_request) {
    const auto request = std::exchange(state.inspector_transform_request, std::nullopt);
    if (editable && request->entities == keys)
      CancelSceneGestures(state);
    if (!editable || request->entities != keys ||
        !scene->SetTransforms(request->entities, request->transforms))
      state.inspector_error =
          "Transform edit rejected because its values or entity generation are stale.";
    else
      state.inspector_error.clear();
  }
  ApplyInspectorEuler(state, *scene, keys, editable);
  if (state.inspector_component_selection != keys) {
    state.inspector_component_selection = keys;
    state.inspector_camera_active = {};
    state.inspector_light_active = false;
  }
  ImGui::PushID(static_cast<int>(selection_hash));
  ImGui::PushID(static_cast<int>(state.inspector_draft_generation));
  std::vector<std::optional<runtime::CameraComponent>> cameras;
  std::vector<std::optional<runtime::LightComponent>> lights;
  for (const auto key : keys) {
    cameras.push_back(scene->Camera(key));
    lights.push_back(scene->Light(key));
  }
  ImGui::SeparatorText("Camera");
  if (DrawInspectorClipboard(state, *scene, keys, 1, editable, copy_allowed))
    for (std::size_t i = 0; i < keys.size(); ++i)
      cameras[i] = scene->Camera(keys[i]);
  ImGui::BeginDisabled(!editable);
  bool camera_enabled = cameras.front().has_value();
  const bool camera_presence_mixed = std::ranges::any_of(
      cameras, [&](const auto &camera) { return camera.has_value() != camera_enabled; });
  state.inspector_camera_presence_mixed = camera_presence_mixed;
  state.inspector_camera_mixed = {};
  if (camera_presence_mixed)
    ImGui::PushItemFlag(ImGuiItemFlags_MixedValue, true);
  const bool camera_toggle =
      ImGui::Checkbox("Enabled###editor.inspector.camera.enabled", &camera_enabled);
  if (camera_presence_mixed)
    ImGui::PopItemFlag();
  if (camera_toggle) {
    state.inspector_camera_active = {};
    if (camera_presence_mixed)
      camera_enabled = true; // first click on mixed presence adds to the whole selection
    for (auto &camera : cameras)
      camera = camera_enabled ? camera.value_or(runtime::CameraComponent{})
                              : std::optional<runtime::CameraComponent>{};
    state.inspector_camera_request = typename StateT::InspectorCameraRequest{keys, cameras};
  }
  ImGui::SameLine();
  ImGui::BeginDisabled(
      std::ranges::none_of(cameras, [](const auto &camera) { return camera.has_value(); }));
  if (ImGui::SmallButton("Reset Camera") && ResetInspectorComponent(state, *scene, keys, 1))
    for (auto &camera : cameras)
      if (camera)
        camera = runtime::CameraComponent{};
  CaptureInspectorReset(state, 1);
  ImGui::EndDisabled();
  if (!camera_presence_mixed && camera_enabled) {
    ImGui::BeginDisabled(keys.size() != 1 || !state.native_scene_preview ||
                         !state.native_scene_preview_available);
    if (ImGui::SmallButton("Use Scene view pose")) {
      CancelSceneGestures(state);
      CancelInspectorDrafts(state);
      const auto &orbit = state.native_scene_orbit;
      // Mirror the native preview's float eye calculation. Scene view uses a right-handed -Z
      // camera: its back vector is (sin(yaw)*cos(pitch), sin(pitch), cos(yaw)*cos(pitch)).
      const float center_x = std::clamp(state.scene_center_world.x, -100000.0F, 100000.0F);
      const float center_z = std::clamp(state.scene_center_world.y, -100000.0F, 100000.0F);
      runtime::Transform world_pose;
      world_pose.x = center_x + static_cast<float>(orbit.distance * std::sin(orbit.yaw) *
                                                   std::cos(orbit.pitch));
      world_pose.y = static_cast<float>(orbit.target_y) +
                     static_cast<float>(orbit.distance * std::sin(orbit.pitch));
      world_pose.z = center_z + static_cast<float>(orbit.distance * std::cos(orbit.yaw) *
                                                   std::cos(orbit.pitch));
      constexpr double degrees = 180.0 / std::numbers::pi;
      const auto oriented =
          WithEulerDegrees(world_pose, {-orbit.pitch * degrees, orbit.yaw * degrees, 0});
      if (!oriented || !scene->AlignCameraToWorldPose(keys.front(), *oriented))
        state.inspector_error =
            "Camera alignment rejected because its pose or generation is stale.";
      else
        state.inspector_error.clear();
    }
    const auto align_min = ImGui::GetItemRectMin(), align_max = ImGui::GetItemRectMax();
    state.camera_align_position =
        std::array{(align_min.x + align_max.x) * 0.5F, (align_min.y + align_max.y) * 0.5F};
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
      ImGui::SetTooltip(
          "Select one Camera and enable Scene 3D. Aligns position/rotation; keeps lens and scale.");
    ImGui::EndDisabled();
    struct CameraField final {
      const char *label;
      double runtime::CameraComponent::*member;
    };
    constexpr std::array camera_fields{
        CameraField{"Vertical FOV", &runtime::CameraComponent::vertical_field_of_view},
        CameraField{"Near plane", &runtime::CameraComponent::near_plane},
        CameraField{"Far plane", &runtime::CameraComponent::far_plane}};
    for (std::size_t axis = 0; axis < camera_fields.size(); ++axis) {
      const auto &field = camera_fields[axis];
      double value = (*cameras.front()).*(field.member);
      const bool mixed = std::ranges::any_of(
          cameras, [&](const auto &camera) { return (*camera).*(field.member) != value; });
      state.inspector_camera_mixed[axis] = mixed;
      if (mixed)
        ImGui::PushItemFlag(ImGuiItemFlags_MixedValue, true);
      if (state.inspector_camera_focus_request == axis) {
        ImGui::SetKeyboardFocusHere();
        state.inspector_camera_focus_request.reset();
      }
      auto &text = state.inspector_camera_text[axis];
      if (!state.inspector_camera_active[axis]) {
        if (mixed)
          text[0] = '\0';
        else {
          const auto formatted = std::to_chars(text.data(), text.data() + text.size() - 1, value,
                                               std::chars_format::general, 17);
          *formatted.ptr = '\0';
        }
      }
      const bool submit = ImGui::InputTextWithHint(
          field.label, mixed ? "Mixed" : "Value", text.data(), text.size(),
          ImGuiInputTextFlags_CharsScientific | ImGuiInputTextFlags_EnterReturnsTrue);
      state.inspector_camera_active[axis] = ImGui::IsItemActive();
      if (mixed)
        ImGui::PopItemFlag();
      if (submit) {
        const std::string_view entered{text.data()};
        const auto number = entered.starts_with('+') ? entered.substr(1) : entered;
        const auto parsed = std::from_chars(number.data(), number.data() + number.size(), value);
        if (parsed.ec != std::errc{} || parsed.ptr != number.data() + number.size() ||
            !std::isfinite(value))
          state.inspector_error = "Enter a finite camera value.";
        else {
          for (auto &camera : cameras)
            (*camera).*(field.member) = value;
          state.inspector_camera_request = typename StateT::InspectorCameraRequest{keys, cameras};
        }
      }
    }
  } else {
    state.inspector_camera_active = {};
    if (camera_presence_mixed)
      ImGui::TextDisabled("Enable Camera for all selected entities to edit its fields.");
  }
  if (state.inspector_camera_request) {
    const auto request = std::exchange(state.inspector_camera_request, std::nullopt);
    if (editable && request->entities == keys)
      CancelSceneGestures(state);
    if (!editable || request->entities != keys ||
        !scene->SetCameras(request->entities, request->values))
      state.inspector_error =
          "Camera edit rejected because values, access or entity generations are invalid.";
    else
      state.inspector_error.clear();
  }
  ImGui::EndDisabled();
  ImGui::SeparatorText("Light");
  if (DrawInspectorClipboard(state, *scene, keys, 2, editable, copy_allowed))
    for (std::size_t i = 0; i < keys.size(); ++i)
      lights[i] = scene->Light(keys[i]);
  ImGui::BeginDisabled(!editable);
  bool light_enabled = lights.front().has_value();
  const bool light_presence_mixed = std::ranges::any_of(
      lights, [&](const auto &light) { return light.has_value() != light_enabled; });
  state.inspector_light_presence_mixed = light_presence_mixed;
  state.inspector_light_mixed = false;
  if (light_presence_mixed)
    ImGui::PushItemFlag(ImGuiItemFlags_MixedValue, true);
  const bool light_toggle =
      ImGui::Checkbox("Enabled###editor.inspector.light.enabled", &light_enabled);
  if (light_presence_mixed)
    ImGui::PopItemFlag();
  if (light_toggle) {
    state.inspector_light_active = false;
    if (light_presence_mixed)
      light_enabled = true;
    for (auto &light : lights)
      light = light_enabled ? light.value_or(runtime::LightComponent{})
                            : std::optional<runtime::LightComponent>{};
    state.inspector_light_request = typename StateT::InspectorLightRequest{keys, lights};
  }
  ImGui::SameLine();
  ImGui::BeginDisabled(
      std::ranges::none_of(lights, [](const auto &light) { return light.has_value(); }));
  if (ImGui::SmallButton("Reset Light") && ResetInspectorComponent(state, *scene, keys, 2))
    for (auto &light : lights)
      if (light)
        light = runtime::LightComponent{};
  CaptureInspectorReset(state, 2);
  ImGui::EndDisabled();
  if (!light_presence_mixed && light_enabled) {
    float intensity = lights.front()->intensity;
    const bool mixed = std::ranges::any_of(
        lights, [&](const auto &light) { return light->intensity != intensity; });
    state.inspector_light_mixed = mixed;
    if (mixed)
      ImGui::PushItemFlag(ImGuiItemFlags_MixedValue, true);
    auto &text = state.inspector_light_text;
    if (!state.inspector_light_active) {
      if (mixed)
        text[0] = '\0';
      else {
        const auto formatted = std::to_chars(text.data(), text.data() + text.size() - 1, intensity,
                                             std::chars_format::general, 9);
        *formatted.ptr = '\0';
      }
    }
    if (state.inspector_light_focus_request) {
      ImGui::SetKeyboardFocusHere();
      state.inspector_light_focus_request = false;
    }
    const bool submit = ImGui::InputTextWithHint(
        "Intensity", mixed ? "Mixed" : "Value", text.data(), text.size(),
        ImGuiInputTextFlags_CharsScientific | ImGuiInputTextFlags_EnterReturnsTrue);
    state.inspector_light_active = ImGui::IsItemActive();
    if (mixed)
      ImGui::PopItemFlag();
    if (submit) {
      const std::string_view entered{text.data()};
      const auto number = entered.starts_with('+') ? entered.substr(1) : entered;
      const auto parsed = std::from_chars(number.data(), number.data() + number.size(), intensity);
      if (parsed.ec != std::errc{} || parsed.ptr != number.data() + number.size() ||
          !std::isfinite(intensity))
        state.inspector_error = "Enter a finite light intensity.";
      else {
        for (auto &light : lights)
          light->intensity = intensity;
        state.inspector_light_request = typename StateT::InspectorLightRequest{keys, lights};
      }
    }
  } else {
    state.inspector_light_active = false;
    if (light_presence_mixed)
      ImGui::TextDisabled("Enable Light for all selected entities to edit intensity.");
  }
  if (state.inspector_light_request) {
    const auto request = std::exchange(state.inspector_light_request, std::nullopt);
    if (editable && request->entities == keys)
      CancelSceneGestures(state);
    if (!editable || request->entities != keys ||
        !scene->SetLights(request->entities, request->values))
      state.inspector_error =
          "Light edit rejected because intensity, access or entity generations are invalid.";
    else
      state.inspector_error.clear();
  }
  ImGui::EndDisabled();
  ImGui::PopID();
  ImGui::PopID();
  {
    const auto first = scene->MeshRenderer(keys.front());
    bool mixed = false;
    bool any_mesh = false;
    bool missing_mesh = false;
    const auto generation = content ? content->Browser().ProjectGeneration() : 0;
    for (const auto key : keys) {
      const auto mesh = scene->MeshRenderer(key);
      any_mesh |= mesh.has_value();
      mixed |=
          mesh.has_value() != first.has_value() || (mesh && first && mesh->mesh != first->mesh);
      const auto resolved =
          mesh && meshes ? meshes->ResolveResource(mesh->mesh, generation) : std::nullopt;
      missing_mesh |= mesh && (!resolved || !content || !content->Browser().Find(resolved->asset));
    }
    const auto resolved =
        first && meshes ? meshes->ResolveResource(first->mesh, generation) : std::nullopt;
    const auto *item = resolved && content ? content->Browser().Find(resolved->asset) : nullptr;
    state.inspector_mesh_label = mixed   ? "Mixed"
                                 : item  ? PathLabel(item->path)
                                 : first ? "Missing mesh"
                                         : "None";
    ImGui::SeparatorText("Mesh Renderer");
    ImGui::BeginDisabled(!editable || content == nullptr || !content->Writable() ||
                         meshes == nullptr);
    const bool mesh_combo_open =
        ImGui::BeginCombo("Mesh###editor.inspector.mesh.asset", state.inspector_mesh_label.c_str());
    const auto combo_min = ImGui::GetItemRectMin();
    const auto combo_max = ImGui::GetItemRectMax();
    state.inspector_mesh_positions[0] =
        std::array{(combo_min.x + combo_max.x) * 0.5F, (combo_min.y + combo_max.y) * 0.5F};
    if (!mesh_combo_open)
      AcceptInspectorMeshDrop(state, content, meshes, editable, keys);
    if (mesh_combo_open) {
      for (const auto &candidate : content->Browser().Items()) {
        if (!meshes->ResolveAsset(candidate.id, generation))
          continue;
        const auto identity = candidate.id.ToString();
        ImGui::PushID(identity.c_str());
        if (ImGui::Selectable(PathLabel(candidate.path).c_str(),
                              !mixed && resolved && resolved->asset == candidate.id))
          state.inspector_mesh_request =
              typename StateT::InspectorMeshRequest{keys, candidate.id, generation};
        if (!state.inspector_mesh_positions[1]) {
          const auto item_min = ImGui::GetItemRectMin();
          const auto item_max = ImGui::GetItemRectMax();
          state.inspector_mesh_positions[1] =
              std::array{(item_min.x + item_max.x) * 0.5F, (item_min.y + item_max.y) * 0.5F};
        }
        ImGui::PopID();
      }
      ImGui::EndCombo();
    }
    ImGui::EndDisabled();
    ImGui::BeginDisabled(!editable || !any_mesh || content == nullptr || !content->Writable());
    if (ImGui::Button("Remove Mesh Renderer###editor.inspector.mesh.remove"))
      state.inspector_mesh_request =
          typename StateT::InspectorMeshRequest{keys, std::nullopt, generation};
    const auto remove_min = ImGui::GetItemRectMin();
    const auto remove_max = ImGui::GetItemRectMax();
    state.inspector_mesh_positions[2] =
        std::array{(remove_min.x + remove_max.x) * 0.5F, (remove_min.y + remove_max.y) * 0.5F};
    ImGui::EndDisabled();
    if (missing_mesh)
      ImGui::TextWrapped("Some referenced meshes are unavailable. Their references are preserved.");
  }
  if (state.inspector_mesh_request) {
    const auto request = std::exchange(state.inspector_mesh_request, std::nullopt);
    const auto resolved = request->asset && meshes
                              ? meshes->ResolveAsset(*request->asset, request->generation)
                              : std::nullopt;
    const bool valid = editable && content && content->Writable() && request->entities == keys &&
                       content->Browser().ProjectGeneration() == request->generation &&
                       (!request->asset || (resolved && content->Browser().Find(*request->asset)));
    std::vector<std::optional<runtime::MeshComponent>> components;
    if (valid) {
      components.reserve(request->entities.size());
      for (const auto key : request->entities) {
        auto component = scene->MeshRenderer(key);
        if (request->asset) {
          if (!component)
            component.emplace();
          component->mesh = resolved->resource;
        } else {
          component.reset();
        }
        components.push_back(component);
      }
      CancelSceneGestures(state);
    }
    if (!valid || !scene->SetMeshRenderers(request->entities, components))
      state.inspector_error =
          "Mesh edit rejected because its selection, asset, project or entity is unavailable.";
    else
      state.inspector_error.clear();
  }
  ImGui::SeparatorText("Material asset");
  if (keys.size() != 1) {
    state.inspector_material_label = "Single selection required";
    ImGui::TextDisabled("Select one Mesh Renderer to assign a material asset.");
  } else if (!scene->MeshRenderer(keys.front())) {
    state.inspector_material_label = "No Mesh Renderer";
    ImGui::TextDisabled("Add a Mesh Renderer before assigning a material asset.");
  } else {
    const auto key = keys.front();
    const auto generation = content ? content->Browser().ProjectGeneration() : 0;
    const auto reference = material_reference;
    const auto resolved =
        reference && materials ? materials->ResolveAsset(*reference, generation) : std::nullopt;
    const auto *item = resolved && content ? content->Browser().Find(*reference) : nullptr;
    const bool has_reference = has_material_reference;
    state.inspector_material_label = item            ? PathLabel(item->path)
                                     : reference     ? "Missing material"
                                     : has_reference ? "Unsupported material reference"
                                                     : "Unassigned";
    ImGui::BeginDisabled(!editable || !content || !content->Writable() || !materials);
    const bool open = ImGui::BeginCombo("Material###editor.inspector.material.asset",
                                        state.inspector_material_label.c_str());
    const auto combo_min = ImGui::GetItemRectMin();
    const auto combo_max = ImGui::GetItemRectMax();
    state.inspector_material_positions[0] =
        std::array{(combo_min.x + combo_max.x) * 0.5F, (combo_min.y + combo_max.y) * 0.5F};
    if (open) {
      if (content && materials)
        for (const auto &candidate : content->Browser().Items()) {
          if (!candidate.material || !materials->ResolveAsset(candidate.id, generation))
            continue;
          const auto identity = candidate.id.ToString();
          ImGui::PushID(identity.c_str());
          if (ImGui::Selectable(PathLabel(candidate.path).c_str(),
                                reference && *reference == candidate.id))
            state.inspector_material_request =
                typename StateT::InspectorMaterialRequest{key, candidate.id, generation};
          if (!state.inspector_material_positions[1]) {
            const auto min = ImGui::GetItemRectMin();
            const auto max = ImGui::GetItemRectMax();
            state.inspector_material_positions[1] =
                std::array{(min.x + max.x) * 0.5F, (min.y + max.y) * 0.5F};
          }
          ImGui::PopID();
        }
      ImGui::EndCombo();
    }
    ImGui::EndDisabled();
    if (has_reference && !item)
      ImGui::TextWrapped(
          "The material reference is unavailable or unsupported. Its data is preserved.");
  }
  if (state.inspector_material_request) {
    const auto request = std::exchange(state.inspector_material_request, std::nullopt);
    const bool valid =
        editable && content && materials && keys.size() == 1 && request->entity == keys.front();
    if (valid)
      CancelSceneGestures(state);
    if (!valid || !AssignMaterialAsset(*scene, request->entity, request->asset, request->generation,
                                       *content, *materials, editable))
      state.inspector_error = "Material edit rejected because its selection, asset, access or "
                              "generation is unavailable.";
    else
      state.inspector_error.clear();
  }
  if (!state.inspector_error.empty())
    ImGui::TextWrapped("%s", state.inspector_error.c_str());
}

template <typename StateT>
void DrawProjectPanel(StateT &state, const ProjectWorkspace *workspace,
                      const RecentProjectStore *recent_projects) {
  state.project_writable = workspace != nullptr && workspace->Writable();
  state.project_upgrade_required =
      workspace != nullptr && workspace->UpgradeState() == ProjectUpgradeState::Required;
  state.recent_projects =
      recent_projects == nullptr
          ? 0
          : static_cast<std::uint32_t>(std::min<std::size_t>(
                recent_projects->Entries().size(), std::numeric_limits<std::uint32_t>::max()));

  const auto window = PanelWindowName("nexora.project");
  if (!ImGui::Begin(window.c_str())) {
    ImGui::End();
    return;
  }
  if (workspace == nullptr) {
    ImGui::TextUnformatted("No project is open.");
  } else {
    ImGui::Text("Name: %s", workspace->Project().name.c_str());
    ImGui::Text("UUID: %s", workspace->Project().id.ToString().c_str());
    ImGui::Text("Root: %s", PathLabel(workspace->Root()).c_str());
    ImGui::Text("Schema: %u / %u", workspace->Project().schema_version,
                ProjectDescriptor::kSchemaVersion);
    ImGui::TextColored(workspace->Writable() ? ImVec4(0.45F, 0.85F, 0.45F, 1.0F)
                                             : ImVec4(1.0F, 0.75F, 0.3F, 1.0F),
                       "Access: %s", workspace->Writable() ? "Read-write" : "Read-only");
    if (workspace->UpgradeState() == ProjectUpgradeState::Applied)
      ImGui::TextUnformatted("Project descriptor upgraded during this session.");
    else if (workspace->UpgradeState() == ProjectUpgradeState::Required)
      ImGui::TextWrapped(
          "This legacy project is open read-only. Reopen it for writing to upgrade safely.");
  }

  if (!state.static_export.message.empty()) {
    ImGui::SeparatorText("StaticView export");
    ImGui::TextWrapped("%s", state.static_export.message.c_str());
    if (state.static_export.phase == StaticExportPhase::Published) {
      ImGui::TextWrapped("%s", state.static_export.relative_path.c_str());
      ImGui::TextWrapped("Verify: %s", state.static_export.verify_command.c_str());
      ImGui::Text("Bytes: %llu / FNV-1a: %s",
                  static_cast<unsigned long long>(state.static_export.bytes),
                  state.static_export.checksum.c_str());
    }
  }
  ImGui::SeparatorText("Recent projects");
  if (recent_projects == nullptr || recent_projects->Entries().empty()) {
    ImGui::TextUnformatted("No recent projects.");
  } else {
    for (const auto &recent : recent_projects->Entries()) {
      ImGui::BulletText("%s", recent.name.c_str());
      ImGui::SameLine();
      ImGui::TextDisabled("%s", PathLabel(recent.root).c_str());
    }
  }
  ImGui::End();
}

template <typename StateT>
void DrawContentBrowser(StateT &state, ProjectContentSession &content, AssetImportQueue *imports,
                        SceneDocument *scene, const MeshAssetCatalog *meshes, bool scene_editable,
                        bool scene_openable, bool commands_allowed,
                        bool selection_commands_allowed) {
  static_cast<void>(content.PollReimport());
  auto &browser = content.Browser();
  state.content_visible_items = 0;
  state.content_visible_folders = 0;
  state.content_selection = static_cast<std::uint32_t>(browser.Selection().size());
  state.content_forward_dependencies = 0;
  state.content_reverse_dependencies = 0;
  state.content_dependency_cycle = 0;
  state.content_import_active = content.ReimportBusy();
  state.content_import_diagnostics = 0;
  state.content_conflicts = static_cast<std::uint32_t>(std::min<std::size_t>(
      std::ranges::count_if(content.Conflicts().Conflicts(), ActionableConflict),
      std::numeric_limits<std::uint32_t>::max()));
  state.content_conflict_visible = false;
  state.content_conflict_compare_visible = false;
  state.content_conflict_choice = DirtyConflictChoice::Pending;
  if (const auto status = content.ReimportStatus()) {
    state.content_import_state = status->state;
    state.content_import_diagnostics = static_cast<std::uint32_t>(std::min<std::size_t>(
        status->diagnostics.size(), std::numeric_limits<std::uint32_t>::max()));
  }

  const auto window = PanelWindowName("nexora.content");
  state.content_rename_positions = {};
  state.content_search_position.reset();
  state.content_focused_folder.reset();
  if (!ImGui::Begin(window.c_str())) {
    state.content_rename_target.reset();
    if (ImGui::BeginPopupModal("Rename asset###editor.content.rename", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
      ImGui::ClearActiveID();
      ImGui::CloseCurrentPopup();
      ImGui::EndPopup();
    }
    ImGui::End();
    return;
  }

  // SetFolder rebuilds the breadcrumb span being iterated, so defer navigation until the loop ends.
  std::optional<std::filesystem::path> navigate_to;
  int breadcrumb_index = 0;
  for (const auto &breadcrumb : browser.Breadcrumbs()) {
    ImGui::PushID(breadcrumb_index++);
    if (ImGui::Button(breadcrumb.label.c_str()))
      navigate_to = breadcrumb.path;
    static_cast<void>(AcceptAssetDrop(content, breadcrumb.path));
    ImGui::PopID();
    ImGui::SameLine();
    ImGui::TextUnformatted("/");
    ImGui::SameLine();
  }
  ImGui::NewLine();
  if (navigate_to) {
    static_cast<void>(browser.SetFolder(*navigate_to));
    navigate_to.reset();
  }

  bool filter_changed = ImGui::InputTextWithHint(
      "##content-search", "Search assets", state.content_query.data(), state.content_query.size());
  const auto search_min = ImGui::GetItemRectMin(), search_max = ImGui::GetItemRectMax();
  state.content_search_position =
      std::array{(search_min.x + search_max.x) * 0.5F, (search_min.y + search_max.y) * 0.5F};
  ImGui::SameLine();
  ImGui::SetNextItemWidth(120.0F);
  filter_changed |= ImGui::InputTextWithHint("##content-type", "Type", state.content_type.data(),
                                             state.content_type.size());
  if (filter_changed)
    browser.SetFilter(state.content_query.data(), state.content_type.data());
  ImGui::SameLine();
  ImGui::BeginDisabled(!commands_allowed || !content.CanUndo());
  if (ImGui::Button("Undo content"))
    static_cast<void>(content.Undo());
  ImGui::EndDisabled();

  const bool keyboard =
      selection_commands_allowed && !state.content_rename_target &&
      ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
      !ImGui::GetIO().WantTextInput && !ImGui::IsAnyItemActive() && !ImGui::GetDragDropPayload() &&
      !ImGui::IsMouseDown(ImGuiMouseButton_Left) &&
      !ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel);
  // Register the Alt route before its modifier transition. Parent navigation never escapes
  // the first breadcrumb, and uses the same inspection-only ownership gates as asset selection.
  if (keyboard && !ImGui::GetIO().KeyCtrl && !ImGui::GetIO().KeyShift && !ImGui::GetIO().KeySuper &&
      ImGui::Shortcut(ImGuiMod_Alt | ImGuiKey_UpArrow, ImGuiInputFlags_RouteFocused)) {
    const auto breadcrumbs = browser.Breadcrumbs();
    if (breadcrumbs.size() > 1) {
      const auto parent = breadcrumbs[breadcrumbs.size() - 2].path;
      static_cast<void>(browser.SetFolder(parent));
      ImGui::SetScrollY(0);
    }
  }
  const auto navigation_folder =
      browser.Breadcrumbs().empty() ? std::filesystem::path{} : browser.Breadcrumbs().back().path;
  if (filter_changed || state.content_navigation_generation != browser.ProjectGeneration() ||
      state.content_navigation_revision != browser.Revision() ||
      state.content_navigation_root != content.Root() ||
      state.content_navigation_folder != navigation_folder) {
    state.content_navigation_cursor.reset();
    state.content_navigation_anchor.reset();
    state.content_navigation_generation = browser.ProjectGeneration();
    state.content_navigation_revision = browser.Revision();
    state.content_navigation_root = content.Root();
    state.content_navigation_folder = navigation_folder;
  }
  std::optional<std::size_t> reveal_row;
  std::optional<runtime::AssetUuid> reveal_asset;
  if (keyboard && !ImGui::GetIO().KeyCtrl && !ImGui::GetIO().KeyAlt && !ImGui::GetIO().KeySuper) {
    const auto pressed = [&](ImGuiKey key) {
      constexpr auto flags = ImGuiInputFlags_RouteFocused | ImGuiInputFlags_Repeat;
      // Register both routes before a modifier transition, so the first Shift press is owned.
      const bool plain = ImGui::Shortcut(key, flags);
      const bool extend = ImGui::Shortcut(ImGuiMod_Shift | key, flags);
      return plain || extend;
    };
    const bool first = pressed(ImGuiKey_Home), last = pressed(ImGuiKey_End);
    const bool previous = pressed(ImGuiKey_UpArrow), next = pressed(ImGuiKey_DownArrow);
    if (first || last || previous || next) {
      const auto rows = browser.Visible(0, browser.Items().size());
      if (!rows.empty()) {
        auto cursor = rows.end();
        if (state.content_navigation_cursor && browser.IsSelected(*state.content_navigation_cursor))
          cursor = std::ranges::find(rows, *state.content_navigation_cursor,
                                     [](const ContentItem *item) { return item->id; });
        if (cursor == rows.end())
          cursor = std::ranges::find_if(
              rows, [&](const ContentItem *item) { return browser.IsSelected(item->id); });
        auto index = cursor == rows.end() ? (previous || last ? rows.size() - 1 : 0)
                                          : static_cast<std::size_t>(cursor - rows.begin());
        if (first)
          index = 0;
        else if (last)
          index = rows.size() - 1;
        else if (cursor != rows.end())
          index = previous ? (index == 0 ? 0 : index - 1) : std::min(index + 1, rows.size() - 1);
        const auto target = rows[index]->id;
        if (ImGui::GetIO().KeyShift) {
          auto anchor = state.content_navigation_anchor;
          if (!anchor || !browser.SelectVisibleRange(*anchor, target)) {
            anchor = cursor == rows.end() ? target : (*cursor)->id;
            static_cast<void>(browser.SelectVisibleRange(*anchor, target));
          }
          state.content_navigation_anchor = anchor;
        } else {
          static_cast<void>(browser.Select(target));
          state.content_navigation_anchor = target;
        }
        state.content_navigation_cursor = target;
        reveal_row = index;
        reveal_asset = target;
      }
    }
  }
  if (keyboard && ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_A, ImGuiInputFlags_RouteFocused)) {
    browser.SelectVisible();
    state.content_navigation_cursor.reset();
    state.content_navigation_anchor.reset();
  }
  std::vector<runtime::AssetUuid> delete_assets;
  if (keyboard && commands_allowed && content.Writable() && !ImGui::GetIO().KeyCtrl &&
      !ImGui::GetIO().KeyShift && !ImGui::GetIO().KeyAlt && !ImGui::GetIO().KeySuper &&
      ImGui::IsKeyPressed(ImGuiKey_Delete, false))
    delete_assets = browser.Selection();

  const auto folders = browser.ChildFolders();
  const bool folder_activation =
      keyboard && std::ranges::any_of(folders, [&](const Breadcrumb &entry) {
        const auto label = "[Folder] " + entry.label + "##" + PathLabel(entry.path);
        return ImGui::GetCurrentContext()->NavActivateId == ImGui::GetID(label.c_str());
      });
  const auto selected_assets = browser.Selection();
  const auto *selected_mesh =
      selected_assets.size() == 1 ? browser.Find(selected_assets.front()) : nullptr;
  const auto resolved_mesh =
      selected_mesh && meshes ? meshes->ResolveAsset(selected_mesh->id, browser.ProjectGeneration())
                              : std::optional<MeshAssetSnapshot>{};
  ImGui::BeginDisabled(!scene || !scene_editable || !content.Writable() || !resolved_mesh);
  if (ImGui::SmallButton("Add mesh to Scene")) {
    const bool native_placement =
        state.native_scene_preview && state.native_scene_preview_available;
    const runtime::Transform position{
        native_placement ? std::clamp(state.scene_center_world.x, -100000.0F, 100000.0F)
                         : state.scene_center_world.x,
        native_placement ? state.native_scene_orbit.target_y : 0,
        native_placement ? std::clamp(state.scene_center_world.y, -100000.0F, 100000.0F)
                         : state.scene_center_world.y};
    static_cast<void>(PlaceContentMesh(state, *scene, &content, meshes, selected_mesh->id,
                                       browser.ProjectGeneration(), position, scene_editable));
  }
  const auto add_min = ImGui::GetItemRectMin(), add_max = ImGui::GetItemRectMax();
  state.content_add_mesh_position =
      std::array{(add_min.x + add_max.x) * 0.5F, (add_min.y + add_max.y) * 0.5F};
  if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
    ImGui::SetTooltip("Select one resolved mesh in a writable project. One Scene Undo removes it.");
  ImGui::EndDisabled();
  if (!state.content_scene_error.empty())
    ImGui::TextWrapped("%s", state.content_scene_error.c_str());

  ImGui::SameLine();
  const auto *selected_scene =
      selected_assets.size() == 1 ? browser.Find(selected_assets.front()) : nullptr;
  const bool selected_scene_file = selected_scene && selected_scene->type == ".scene" &&
                                   selected_scene->path.extension() == ".scene";
  ImGui::BeginDisabled(!scene_openable || !selected_scene_file);
  if (ImGui::SmallButton("Open scene"))
    BeginContentScene(state, *scene, selected_scene->path);
  const auto open_min = ImGui::GetItemRectMin(), open_max = ImGui::GetItemRectMax();
  state.content_open_scene_position =
      std::array{(open_min.x + open_max.x) * 0.5F, (open_min.y + open_max.y) * 0.5F};
  ImGui::EndDisabled();
  if (!folder_activation && scene_openable && selected_scene_file &&
      !ImGui::GetIO().WantTextInput &&
      ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
      ImGui::IsKeyPressed(ImGuiKey_Enter, false))
    BeginContentScene(state, *scene, selected_scene->path);

  std::optional<runtime::AssetUuid> reimport_asset;
  bool open_rename = false;
  const auto begin_rename = [&](const ContentItem &item) {
    CancelSceneGestures(state);
    CancelInspectorDrafts(state);
    ImGui::ClearActiveID();
    state.content_rename.fill(0);
    const auto filename = PathLabel(item.path.filename());
    auto count = std::min(filename.size(), state.content_rename.size() - 1);
    while (count > 0 && count < filename.size() &&
           (static_cast<unsigned char>(filename[count]) & 0xC0U) == 0x80U)
      --count;
    std::memcpy(state.content_rename.data(), filename.data(), count);
    state.content_rename_target = item.id;
    state.content_rename_generation = browser.ProjectGeneration();
    state.content_rename_root = content.Root();
    state.content_rename_path = item.path;
    state.content_rename_focus = true;
    open_rename = true;
  };
  if (commands_allowed && content.Writable() && !state.content_rename_target &&
      selected_assets.size() == 1 && !ImGui::GetIO().WantTextInput &&
      ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
      ImGui::IsKeyPressed(ImGuiKey_F2, false)) {
    if (const auto *item = browser.Find(selected_assets.front()))
      begin_rename(*item);
  }
  state.content_visible_folders = static_cast<std::uint32_t>(folders.size());
  for (const auto &folder : folders) {
    const auto label = "[Folder] " + folder.label + "##" + PathLabel(folder.path);
    const bool activated =
        ImGui::Selectable(label.c_str(), false, ImGuiSelectableFlags_AllowDoubleClick);
    if (ImGui::IsItemFocused())
      state.content_focused_folder = folder.path;
    const bool keyboard_activation =
        keyboard && ImGui::GetCurrentContext()->NavActivateId == ImGui::GetItemID();
    if (activated && (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) || keyboard_activation))
      navigate_to = folder.path;
    static_cast<void>(AcceptAssetDrop(content, folder.path));
  }
  if (navigate_to && browser.Breadcrumbs().back().path != *navigate_to) {
    static_cast<void>(browser.SetFolder(*navigate_to));
    ImGui::SetScrollY(0);
  }

  const auto visible_count = browser.VisibleCount();
  state.content_visible_items = static_cast<std::uint32_t>(
      std::min<std::size_t>(visible_count, std::numeric_limits<std::uint32_t>::max()));
  ImGuiListClipper clipper;
  clipper.Begin(static_cast<int>(std::min<std::size_t>(
      visible_count, static_cast<std::size_t>(std::numeric_limits<int>::max()))));
  if (reveal_row && *reveal_row < static_cast<std::size_t>(std::numeric_limits<int>::max()))
    clipper.IncludeItemByIndex(static_cast<int>(*reveal_row));
  while (clipper.Step()) {
    const auto visible =
        browser.Visible(static_cast<std::size_t>(clipper.DisplayStart),
                        static_cast<std::size_t>(clipper.DisplayEnd - clipper.DisplayStart));
    for (const auto *item : visible) {
      const auto label = std::string(ThumbnailLabel(item->thumbnail)) + " " +
                         PathLabel(item->path.filename()) + "##" + item->id.ToString();
      if (ImGui::Selectable(label.c_str(), browser.IsSelected(item->id),
                            ImGuiSelectableFlags_AllowDoubleClick)) {
        if (ImGui::GetIO().KeyShift && state.content_navigation_anchor &&
            browser.SelectVisibleRange(*state.content_navigation_anchor, item->id)) {
          // Keep the original anchor when extending or shrinking the visible interval.
        } else if (ImGui::GetIO().KeyCtrl) {
          static_cast<void>(browser.Toggle(item->id));
          state.content_navigation_anchor = item->id;
        } else {
          static_cast<void>(browser.Select(item->id));
          state.content_navigation_anchor = item->id;
        }
        state.content_navigation_cursor = item->id;
        if (scene_openable && item->type == ".scene" && item->path.extension() == ".scene" &&
            ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
          BeginContentScene(state, *scene, item->path);
      }
      const auto asset_min = ImGui::GetItemRectMin(), asset_max = ImGui::GetItemRectMax();
      state.content_asset_positions.push_back(
          {item->id, {(asset_min.x + asset_max.x) * 0.5F, (asset_min.y + asset_max.y) * 0.5F}});
      if (reveal_asset == item->id)
        ImGui::SetScrollHereY(0.5F);
      if (ImGui::BeginDragDropSource()) {
        const AssetDragData payload{browser.ProjectGeneration(), item->id};
        ImGui::SetDragDropPayload(AssetDragPayload::kType.data(), &payload, sizeof(payload),
                                  ImGuiCond_Once);
        ImGui::TextUnformatted(PathLabel(item->path.filename()).c_str());
        ImGui::EndDragDropSource();
      }
      if (ImGui::BeginPopupContextItem()) {
        if (ImGui::MenuItem("Open scene", nullptr, false,
                            scene_openable && item->type == ".scene" &&
                                item->path.extension() == ".scene")) {
          BeginContentScene(state, *scene, item->path);
          ImGui::CloseCurrentPopup();
        }
        if (ImGui::MenuItem("Rename", "F2", false,
                            commands_allowed && content.Writable() && !state.content_rename_target))
          begin_rename(*item);
        if (ImGui::MenuItem("Reimport", nullptr, false,
                            commands_allowed && content.Writable() && imports != nullptr &&
                                !content.ReimportBusy()))
          reimport_asset = item->id;
        if (ImGui::MenuItem("Delete", "Delete", false, commands_allowed && content.Writable()))
          delete_assets = {item->id};
        ImGui::EndPopup();
      }
    }
  }

  if (reimport_asset && imports != nullptr)
    static_cast<void>(content.BeginReimport(*imports, *reimport_asset));
  if (!delete_assets.empty() && !state.content_rename_target) {
    CancelSceneGestures(state);
    CancelInspectorDrafts(state);
    static_cast<void>(content.Delete(delete_assets));
  }
  if (open_rename)
    ImGui::OpenPopup("Rename asset###editor.content.rename");
  if (ImGui::BeginPopupModal("Rename asset###editor.content.rename", nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    const auto *item =
        state.content_rename_target ? browser.Find(*state.content_rename_target) : nullptr;
    const bool valid = commands_allowed && content.Writable() && item &&
                       state.content_rename_generation == browser.ProjectGeneration() &&
                       state.content_rename_root == content.Root() &&
                       state.content_rename_path == item->path;
    const auto capture = [&](std::size_t control) {
      const auto minimum = ImGui::GetItemRectMin(), maximum = ImGui::GetItemRectMax();
      state.content_rename_positions[control] =
          std::array{(minimum.x + maximum.x) * 0.5F, (minimum.y + maximum.y) * 0.5F};
    };
    if (std::exchange(state.content_rename_focus, false))
      ImGui::SetKeyboardFocusHere();
    const bool submit =
        ImGui::InputText("Filename", state.content_rename.data(), state.content_rename.size(),
                         ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue);
    capture(0);
    const bool apply = ImGui::Button("Apply");
    capture(1);
    if (valid && (apply || submit)) {
      if (content.Rename(*state.content_rename_target, state.content_rename.data())) {
        state.content_rename_target.reset();
        ImGui::ClearActiveID();
        ImGui::CloseCurrentPopup();
      } else {
        state.content_rename_focus = true;
      }
    }
    ImGui::SameLine();
    const bool cancel = ImGui::Button("Cancel");
    capture(2);
    if (!valid || cancel || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
      state.content_rename_target.reset();
      ImGui::ClearActiveID();
      ImGui::CloseCurrentPopup();
    }
    if (valid && !content.LastError().empty())
      ImGui::TextWrapped("%s", std::string(content.LastError()).c_str());
    ImGui::EndPopup();
  }

  const auto selection = browser.Selection();
  state.content_selection = static_cast<std::uint32_t>(selection.size());
  ImGui::SeparatorText("Asset details");
  if (selection.size() == 1) {
    if (const auto *item = browser.Find(selection.front())) {
      ImGui::Text("Path: %s", PathLabel(item->path).c_str());
      ImGui::Text("UUID: %s", item->id.ToString().c_str());
      ImGui::Text("Type: %s", item->type.c_str());
      ImGui::Text("Artifact: %s", item->artifact_hash.c_str());
      const auto forward = content.Dependencies().Forward(item->id);
      const auto reverse = content.Dependencies().Reverse(item->id);
      state.content_forward_dependencies = static_cast<std::uint32_t>(forward.size());
      state.content_reverse_dependencies = static_cast<std::uint32_t>(reverse.size());
      if (ImGui::TreeNode("Dependencies")) {
        if (forward.empty())
          ImGui::TextUnformatted("None");
        for (const auto dependency : forward) {
          const auto *target = browser.Find(dependency);
          ImGui::BulletText("%s", target ? PathLabel(target->path).c_str()
                                         : dependency.ToString().c_str());
        }
        ImGui::TreePop();
      }
      if (ImGui::TreeNode("Referenced by")) {
        if (reverse.empty())
          ImGui::TextUnformatted("None");
        for (const auto dependency : reverse) {
          const auto *target = browser.Find(dependency);
          ImGui::BulletText("%s", target ? PathLabel(target->path).c_str()
                                         : dependency.ToString().c_str());
        }
        ImGui::TreePop();
      }
    }
  } else {
    ImGui::Text("%zu assets selected", selection.size());
  }
  const auto dependency_cycle = content.Dependencies().FindCycle();
  state.content_dependency_cycle = static_cast<std::uint32_t>(
      std::min<std::size_t>(dependency_cycle.size(), std::numeric_limits<std::uint32_t>::max()));
  if (!dependency_cycle.empty()) {
    ImGui::SeparatorText("Dependency cycle");
    ImGui::TextColored(ImVec4(1.0F, 0.45F, 0.35F, 1.0F),
                       "Cyclic dependencies block artifact publication.");
    for (const auto asset : dependency_cycle) {
      const auto *item = browser.Find(asset);
      const auto asset_label = item != nullptr ? PathLabel(item->path) : asset.ToString();
      ImGui::BulletText("%s", asset_label.c_str());
    }
  }
  const auto conflicts = content.Conflicts().Conflicts();
  const auto active_conflict = std::ranges::find_if(conflicts, ActionableConflict);
  if (active_conflict != conflicts.end()) {
    const auto conflict = *active_conflict;
    state.content_conflict_visible = true;
    state.content_conflict_compare_visible = conflict.choice == DirtyConflictChoice::Compare;
    state.content_conflict_choice = conflict.choice;
    ImGui::OpenPopup("External asset change###editor.content.dirty-conflict");

    std::optional<DirtyConflictChoice> requested_choice;
    if (state.content_conflict_choice_request) {
      const auto request = std::exchange(state.content_conflict_choice_request, std::nullopt);
      if (request->first == conflict.asset && request->second != DirtyConflictChoice::Pending)
        requested_choice = request->second;
    }

    if (ImGui::BeginPopupModal("External asset change###editor.content.dirty-conflict", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
      const auto *item = browser.Find(conflict.asset);
      const auto asset_label = item != nullptr ? PathLabel(item->path) : conflict.asset.ToString();
      ImGui::TextUnformatted("This asset changed on disk while the Editor has unsaved changes.");
      ImGui::TextUnformatted("Automatic reload is blocked until you choose how to continue.");
      ImGui::Separator();
      ImGui::Text("Asset: %s", asset_label.c_str());

      if (conflict.choice == DirtyConflictChoice::Compare ||
          requested_choice == DirtyConflictChoice::Compare) {
        state.content_conflict_compare_visible = true;
        if (ImGui::BeginTable("##dirty-conflict-compare", 2,
                              ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
          ImGui::TableSetupColumn("Editor version");
          ImGui::TableSetupColumn("Disk version");
          ImGui::TableHeadersRow();
          ImGui::TableNextRow();
          ImGui::TableSetColumnIndex(0);
          ImGui::TextWrapped("%s", conflict.editor_hash.c_str());
          ImGui::TableSetColumnIndex(1);
          ImGui::TextWrapped("%s", conflict.disk_hash.c_str());
          ImGui::EndTable();
        }
      } else {
        ImGui::TextDisabled("Choose Compare to inspect the editor and disk hashes first.");
      }

      if (ImGui::Button("Reload from disk"))
        requested_choice = DirtyConflictChoice::Reload;
      ImGui::SameLine();
      if (ImGui::Button("Keep editor version"))
        requested_choice = DirtyConflictChoice::Keep;
      ImGui::SameLine();
      if (ImGui::Button("Compare"))
        requested_choice = DirtyConflictChoice::Compare;

      if (requested_choice && content.Conflicts().Resolve(conflict.asset, *requested_choice)) {
        state.content_conflict_choice = *requested_choice;
        state.content_conflict_compare_visible = *requested_choice == DirtyConflictChoice::Compare;
        if (*requested_choice != DirtyConflictChoice::Compare) {
          state.content_conflict_visible = false;
          if (state.content_conflicts != 0)
            --state.content_conflicts;
          ImGui::CloseCurrentPopup();
        }
      }
      ImGui::EndPopup();
    }
  } else {
    state.content_conflict_choice_request.reset();
  }
  if (const auto status = content.ReimportStatus()) {
    ImGui::SeparatorText("Import operation");
    ImGui::Text("Operation: %llu", static_cast<unsigned long long>(status->operation));
    if (!status->progress.empty()) {
      const auto &progress = status->progress.back();
      const float fraction = progress.total == 0 ? 0.0F
                                                 : static_cast<float>(progress.completed) /
                                                       static_cast<float>(progress.total);
      ImGui::ProgressBar(std::clamp(fraction, 0.0F, 1.0F));
    }
    if (content.ReimportBusy() && ImGui::Button("Cancel import"))
      static_cast<void>(content.CancelReimport());
    for (const auto &diagnostic : status->diagnostics)
      ImGui::TextWrapped("[%s] %s", diagnostic.code.c_str(), diagnostic.message.c_str());
    if (status->dropped_progress != 0 || status->dropped_diagnostics != 0)
      ImGui::TextDisabled("Bounded history dropped %zu progress and %zu diagnostic events.",
                          status->dropped_progress, status->dropped_diagnostics);
  }
  if (!content.LastError().empty())
    ImGui::TextWrapped("Content error: %.*s", static_cast<int>(content.LastError().size()),
                       content.LastError().data());
  ImGui::End();
}

ImGuiKey ToImGuiKey(Nexora::Window::Key key) {
  using Key = Nexora::Window::Key;
  if (key >= Key::Digit0 && key <= Key::Digit9)
    return static_cast<ImGuiKey>(ImGuiKey_0 + static_cast<int>(key) -
                                 static_cast<int>(Key::Digit0));
  if (key >= Key::A && key <= Key::Z)
    return static_cast<ImGuiKey>(ImGuiKey_A + static_cast<int>(key) - static_cast<int>(Key::A));
  if (key >= Key::F1 && key <= Key::F12)
    return static_cast<ImGuiKey>(ImGuiKey_F1 + static_cast<int>(key) - static_cast<int>(Key::F1));
  if (key >= Key::Keypad0 && key <= Key::Keypad9)
    return static_cast<ImGuiKey>(ImGuiKey_Keypad0 + static_cast<int>(key) -
                                 static_cast<int>(Key::Keypad0));
#define NEXORA_KEY(native, imgui)                                                                  \
  case Key::native:                                                                                \
    return ImGuiKey_##imgui
  switch (key) {
    NEXORA_KEY(Tab, Tab);
    NEXORA_KEY(LeftArrow, LeftArrow);
    NEXORA_KEY(RightArrow, RightArrow);
    NEXORA_KEY(UpArrow, UpArrow);
    NEXORA_KEY(DownArrow, DownArrow);
    NEXORA_KEY(PageUp, PageUp);
    NEXORA_KEY(PageDown, PageDown);
    NEXORA_KEY(Home, Home);
    NEXORA_KEY(End, End);
    NEXORA_KEY(Insert, Insert);
    NEXORA_KEY(Delete, Delete);
    NEXORA_KEY(Backspace, Backspace);
    NEXORA_KEY(Space, Space);
    NEXORA_KEY(Enter, Enter);
    NEXORA_KEY(Escape, Escape);
    NEXORA_KEY(Apostrophe, Apostrophe);
    NEXORA_KEY(Comma, Comma);
    NEXORA_KEY(Minus, Minus);
    NEXORA_KEY(Period, Period);
    NEXORA_KEY(Slash, Slash);
    NEXORA_KEY(Semicolon, Semicolon);
    NEXORA_KEY(Equal, Equal);
    NEXORA_KEY(LeftBracket, LeftBracket);
    NEXORA_KEY(Backslash, Backslash);
    NEXORA_KEY(RightBracket, RightBracket);
    NEXORA_KEY(GraveAccent, GraveAccent);
    NEXORA_KEY(CapsLock, CapsLock);
    NEXORA_KEY(ScrollLock, ScrollLock);
    NEXORA_KEY(NumLock, NumLock);
    NEXORA_KEY(PrintScreen, PrintScreen);
    NEXORA_KEY(Pause, Pause);
    NEXORA_KEY(KeypadDecimal, KeypadDecimal);
    NEXORA_KEY(KeypadDivide, KeypadDivide);
    NEXORA_KEY(KeypadMultiply, KeypadMultiply);
    NEXORA_KEY(KeypadSubtract, KeypadSubtract);
    NEXORA_KEY(KeypadAdd, KeypadAdd);
    NEXORA_KEY(KeypadEnter, KeypadEnter);
    NEXORA_KEY(KeypadEqual, KeypadEqual);
    NEXORA_KEY(LeftShift, LeftShift);
    NEXORA_KEY(LeftControl, LeftCtrl);
    NEXORA_KEY(LeftAlt, LeftAlt);
    NEXORA_KEY(LeftSuper, LeftSuper);
    NEXORA_KEY(RightShift, RightShift);
    NEXORA_KEY(RightControl, RightCtrl);
    NEXORA_KEY(RightAlt, RightAlt);
    NEXORA_KEY(RightSuper, RightSuper);
    NEXORA_KEY(Menu, Menu);
  default:
    return ImGuiKey_None;
  }
#undef NEXORA_KEY
}
} // namespace

EditorImGuiHost::EditorImGuiHost() : state_(std::make_unique<State>()) {
  state_->context = ImGui::CreateContext();
  Activate(state_->context);
  auto &io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_DockingEnable | ImGuiConfigFlags_NavEnableKeyboard;
  io.IniFilename = nullptr;
  io.BackendPlatformUserData = state_.get();
  io.Fonts->SetTexID(static_cast<ImTextureID>(TextureId(0, 1)));
  ImGui::GetPlatformIO().Platform_SetImeDataFn = &State::SetImeData;
  constexpr std::string_view default_project_name = "New Project";
  std::ranges::copy(default_project_name, state_->selector_name.begin());
  ApplyTheme();
}

EditorImGuiHost::~EditorImGuiHost() = default;
EditorImGuiHost::EditorImGuiHost(EditorImGuiHost &&) noexcept = default;
EditorImGuiHost &EditorImGuiHost::operator=(EditorImGuiHost &&) noexcept = default;

void EditorImGuiHost::SetDisplay(float width, float height, float dpi_scale) {
  Activate(state_->context);
  ImGui::GetIO().DisplaySize = {std::max(width, 1.0F), std::max(height, 1.0F)};
  dpi_scale = std::isfinite(dpi_scale) ? std::max(dpi_scale, 0.25F) : 1.0F;
  if (dpi_scale != state_->dpi_scale)
    CancelSceneGestures(*state_);
  if (state_->native_pointer && dpi_scale != state_->dpi_scale)
    ImGui::GetIO().AddMousePosEvent(static_cast<float>((*state_->native_pointer)[0]) / dpi_scale,
                                    static_cast<float>((*state_->native_pointer)[1]) / dpi_scale);
  ImGui::GetIO().DisplayFramebufferScale = {dpi_scale, dpi_scale};
  const float bucket = DpiBucket(dpi_scale);
  if (bucket != state_->dpi_bucket) {
    state_->dpi_bucket = bucket;
    auto &fonts = *ImGui::GetIO().Fonts;
    fonts.Clear();
    ImFontConfig config;
    config.SizePixels = 13.0F * bucket;
    fonts.AddFontDefault(&config);
    fonts.Build();
    fonts.SetTexID(static_cast<ImTextureID>(TextureId(0, 1)));
    ImGui::GetIO().FontGlobalScale = 1.0F / bucket;
    ++state_->font_generation;
  }
  state_->dpi_scale = dpi_scale;
  ApplyTheme();
}

void EditorImGuiHost::ProcessEvents(std::span<const Nexora::Window::WindowEvent> events) {
  Activate(state_->context);
  auto &io = ImGui::GetIO();
  for (const auto &event : events) {
    switch (event.type) {
    case Nexora::Window::WindowEventType::Pointer:
      state_->native_pointer = std::array{event.value0, event.value1};
      io.AddMousePosEvent(static_cast<float>(event.value0) / state_->dpi_scale,
                          static_cast<float>(event.value1) / state_->dpi_scale);
      break;
    case Nexora::Window::WindowEventType::Wheel:
      io.AddMouseWheelEvent(static_cast<float>(event.value0) / 120.0F,
                            static_cast<float>(event.value1) / 120.0F);
      break;
    case Nexora::Window::WindowEventType::PointerButton:
      if (event.value0 >= 0 && event.value0 < ImGuiMouseButton_COUNT)
        io.AddMouseButtonEvent(event.value0, event.value1 != 0);
      break;
    case Nexora::Window::WindowEventType::Key: {
      const auto physical_key = static_cast<Nexora::Window::Key>(event.value0);
      if (state_->game_input_focused) {
        if (physical_key == Nexora::Window::Key::Escape) {
          state_->game_input_focused = false;
          break;
        }
        if (physical_key != Nexora::Window::Key::F5 && physical_key != Nexora::Window::Key::F6 &&
            physical_key != Nexora::Window::Key::F10)
          break;
      }
      const auto modifiers = static_cast<unsigned>(event.modifiers);
      io.AddKeyEvent(ImGuiMod_Ctrl, (modifiers & static_cast<unsigned>(
                                                     Nexora::Window::KeyModifiers::Control)) != 0);
      io.AddKeyEvent(ImGuiMod_Shift,
                     (modifiers & static_cast<unsigned>(Nexora::Window::KeyModifiers::Shift)) != 0);
      io.AddKeyEvent(ImGuiMod_Alt,
                     (modifiers & static_cast<unsigned>(Nexora::Window::KeyModifiers::Alt)) != 0);
      io.AddKeyEvent(ImGuiMod_Super,
                     (modifiers & static_cast<unsigned>(Nexora::Window::KeyModifiers::Super)) != 0);
      const auto key = ToImGuiKey(static_cast<Nexora::Window::Key>(event.value0));
      if (key != ImGuiKey_None)
        io.AddKeyEvent(key, event.value1 != 0);
      break;
    }
    case Nexora::Window::WindowEventType::Text:
      if (state_->game_input_focused)
        break;
      if (event.value0 > 0 && event.value0 <= 0x10ffff &&
          !(event.value0 >= 0xd800 && event.value0 <= 0xdfff))
        io.AddInputCharacter(static_cast<unsigned int>(event.value0));
      break;
    case Nexora::Window::WindowEventType::FocusChanged:
      state_->app_focused = event.value0 != 0;
      // Dear ImGui releases held inputs on focus loss. Cancel before that synthetic release
      // can be interpreted as a completed authoring gesture.
      if (!state_->app_focused) {
        state_->game_input_binding_open = false;
        state_->game_input_binding_pending = false;
        if (state_->game_input_save_requested)
          state_->game_input_binding_status.clear();
        state_->game_input_save_requested.reset();
        state_->native_pointer.reset();
        state_->game_input_focused = false;
        CancelSceneGestures(*state_);
        CancelInspectorDrafts(*state_);
        state_->hierarchy_rename_target.reset();
        state_->hierarchy_rename_focus_pending = false;
        state_->content_rename_target.reset();
        state_->content_rename_focus = false;
        state_->content_rename_positions = {};
      }
      io.AddFocusEvent(event.value0 != 0);
      break;
    case Nexora::Window::WindowEventType::DpiChanged:
      SetDisplay(io.DisplaySize.x, io.DisplaySize.y, event.scale);
      break;
    default:
      break;
    }
  }
}

void EditorImGuiHost::BeginFrame(float delta_seconds) {
  Activate(state_->context);
  state_->scene_file_positions = {};
  state_->hierarchy_create_positions = {};
  state_->scene_canvas_viewport.reset();
  state_->scene_frame_position.reset();
  state_->scene_frame_all_position.reset();
  state_->scene_select_all_position.reset();
  state_->native_scene_select_all_request.reset();
  state_->native_scene_tool_positions = {};
  state_->scene_frame_token.reset();
  state_->native_scene_frame_all_request.reset();
  state_->native_scene_frame_all_apply.reset();
  state_->inspector_reset_positions = {};
  state_->inspector_clipboard_positions = {};
  state_->native_game_viewport.reset();
  state_->native_scene_pick.reset();
  state_->native_scene_drag.reset();
  state_->native_scene_drag_preview.reset();
  ImGui::GetIO().DeltaTime = std::max(delta_seconds, 0.0001F);
  ImGui::NewFrame();
}

void EditorImGuiHost::DrawProjectSelector(const RecentProjectStore *recent_projects,
                                          ProjectAccess default_access) {
  Activate(state_->context);
  state_->selector_visible = true;
  state_->selector_recent_projects =
      recent_projects == nullptr
          ? 0
          : static_cast<std::uint32_t>(std::min<std::size_t>(
                recent_projects->Entries().size(), std::numeric_limits<std::uint32_t>::max()));
  if (!state_->selector_initialized) {
    state_->selector_read_only = default_access == ProjectAccess::ReadOnly;
    state_->selector_initialized = true;
  }

  const auto *viewport = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(viewport->WorkPos);
  ImGui::SetNextWindowSize(viewport->WorkSize);
  constexpr auto flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove |
                         ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings;
  if (!ImGui::Begin("Project Browser###nexora.project-selector", nullptr, flags)) {
    ImGui::End();
    return;
  }

  ImGui::TextUnformatted("Nexora Editor");
  ImGui::SeparatorText("Create or open a project");
  ImGui::BeginDisabled(state_->selector_busy);
  ImGui::SetNextItemWidth(std::clamp(viewport->WorkSize.x - 32.0F, 1.0F, 720.0F));
  const bool focus_root = state_->selector_focus_root && state_->app_focused;
  if (focus_root)
    ImGui::SetWindowFocus();
  ImGui::InputText("Project root", state_->selector_root.data(), state_->selector_root.size());
  if (focus_root) {
    ImGui::FocusItem();
    ImGui::ActivateItemByID(ImGui::GetItemID());
    state_->selector_focus_root = false;
  }
  state_->selector_root_active = ImGui::IsItemActive();
  ImGui::SetNextItemWidth(std::clamp(viewport->WorkSize.x - 32.0F, 1.0F, 420.0F));
  ImGui::InputText("Project name", state_->selector_name.data(), state_->selector_name.size());
  ImGui::Checkbox("Open read-only", &state_->selector_read_only);

  const auto typed_root = PathFromLabel(state_->selector_root.data());
  // BeginDisabled does not suppress key chords, so gate the shortcuts on the same conditions that
  // disable their buttons: no new request while an import runs, and no Create when read-only.
  const bool open_requested =
      ImGui::Button("Open project (Ctrl+O)") ||
      (!state_->selector_busy && ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_O));
  if (open_requested) {
    if (typed_root)
      QueueProjectSelection(*state_, ProjectSelectorAction::Open, *typed_root, {},
                            state_->selector_read_only ? ProjectAccess::ReadOnly
                                                       : ProjectAccess::ReadWrite);
    else
      state_->selector_error = "Project root must be non-empty valid UTF-8.";
  }
  ImGui::SameLine();
  ImGui::BeginDisabled(state_->selector_read_only);
  const bool create_requested = ImGui::Button("Create project (Ctrl+N)") ||
                                (!state_->selector_busy && !state_->selector_read_only &&
                                 ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_N));
  if (create_requested) {
    if (typed_root)
      QueueProjectSelection(*state_, ProjectSelectorAction::Create, *typed_root,
                            state_->selector_name.data(), ProjectAccess::ReadWrite);
    else
      state_->selector_error = "Project root must be non-empty valid UTF-8.";
  }
  ImGui::EndDisabled();
  if (state_->selector_read_only)
    ImGui::TextDisabled("Creating a project requires read-write access.");
  if (!state_->selector_error.empty())
    ImGui::TextWrapped("%s", state_->selector_error.c_str());

  ImGui::SeparatorText("Recent projects");
  if (recent_projects == nullptr || recent_projects->Entries().empty()) {
    ImGui::TextUnformatted("No recent projects.");
  } else {
    for (const auto &recent : recent_projects->Entries()) {
      ImGui::PushID(recent.id.ToString().c_str());
      if (ImGui::Button("Open"))
        QueueProjectSelection(*state_, ProjectSelectorAction::Open, recent.root, {},
                              state_->selector_read_only ? ProjectAccess::ReadOnly
                                                         : ProjectAccess::ReadWrite);
      ImGui::SameLine();
      ImGui::Text("%s", recent.name.c_str());
      ImGui::SameLine();
      ImGui::TextDisabled("%s", PathLabel(recent.root).c_str());
      ImGui::PopID();
    }
  }
  ImGui::EndDisabled();
  if (!state_->selector_status.empty())
    ImGui::TextWrapped("%s", state_->selector_status.c_str());
  if (state_->selector_busy && ImGui::Button("Cancel import"))
    state_->selector_cancel_requested = true;
  ImGui::End();
}

std::optional<ProjectSelectorRequest> EditorImGuiHost::TakeProjectSelectorRequest() {
  auto request = std::move(state_->selector_request);
  state_->selector_request.reset();
  return request;
}

bool EditorImGuiHost::TakeProjectSelectorCancel() noexcept {
  const bool requested = state_->selector_cancel_requested;
  state_->selector_cancel_requested = false;
  return requested;
}

void EditorImGuiHost::SetProjectSelectorError(std::string error) {
  state_->selector_error = std::move(error);
}

void EditorImGuiHost::SetProjectSelectorStatus(std::string status, bool busy) {
  state_->selector_status = std::move(status);
  state_->selector_busy = busy;
}

std::string_view EditorImGuiHost::ProjectSelectorError() const noexcept {
  return state_->selector_error;
}

void EditorImGuiHost::DrawProductShell(ProductShell &shell, SceneDocument *scene,
                                       ProjectWorkspace *workspace, ProjectContentSession *content,
                                       RecentProjectStore *recent_projects,
                                       AssetImportQueue *imports, runtime::RuntimeConsole *console,
                                       runtime::PlaySession *play, ProfileSession *profile,
                                       const MeshAssetCatalog *meshes,
                                       const MaterialAssetCatalog *materials) {
  Activate(state_->context);
  const bool game_running = play && play->State() != runtime::PlayState::Stopped;
  const auto play_snapshot = play ? play->Inspect() : runtime::RuntimeInspectionSnapshot{};
  state_->game_camera_combo_position.reset();
  state_->game_camera_positions.clear();
  state_->game_input_binding_positions = {};
  state_->game_input_choice_positions.clear();
  if (!game_running || state_->game_camera_generation != play->Generation())
    state_->game_camera_selection = 0;
  state_->game_camera_generation = play ? play->Generation() : 0;
  std::vector<runtime::Id> game_cameras;
  if (game_running) {
    for (const auto &entity : play_snapshot.entities)
      if (entity.camera && entity.scene_state == runtime::SceneState::Active &&
          runtime::CameraView(*play->PlayWorld(), entity.id, 1.0F))
        game_cameras.push_back(entity.id);
    if (state_->game_camera_selection &&
        std::ranges::find(game_cameras, state_->game_camera_selection) == game_cameras.end())
      state_->game_camera_selection = 0;
  }
  state_->hierarchy_cut_position.reset();
  state_->hierarchy_rename_position.reset();
  state_->content_add_mesh_position.reset();
  state_->content_open_scene_position.reset();
  state_->content_asset_positions.clear();
  state_->camera_align_position.reset();
  state_->play_inspector_rendered = 0;
  state_->inspector_opaque_info.clear();
  state_->memory_control_positions = {};
  state_->profile_export_position.reset();
  state_->profile_json_export_position.reset();
  state_->profile_csv_import_position.reset();
  state_->profile_json_import_position.reset();
  state_->profile_import_clear_position.reset();
  state_->profile_capture_position.reset();
  state_->profile_clear_position.reset();
  state_->profile_memory = profile ? profile->ProcessMemory() : ProcessMemoryObservation{};
  state_->profile_gpu = profile ? profile->GpuTiming() : GpuProfileObservation{};
  state_->gpu_control_positions = {};
  const auto profile_root = workspace ? workspace->Root() : std::filesystem::path{};
  const auto profile_id = workspace ? workspace->Project().id.ToString() : std::string{};
  if (profile_root != state_->profile_project_root || profile_id != state_->profile_project_id) {
    state_->profile_project_root = profile_root;
    state_->profile_project_id = profile_id;
    state_->imported_memory.reset();
    state_->imported_gpu.reset();
    state_->gpu_export_requested = false;
    state_->gpu_import_requested = false;
    state_->memory_export_requested = false;
    state_->memory_import_requested = false;
    state_->imported_profile.reset();
    state_->profile_csv_import_requested = false;
    state_->profile_json_import_requested = false;
    state_->profile_export_requested = false;
    state_->profile_json_export_requested = false;
    state_->profile_export_status.clear();
  }
  state_->console_control_positions = {};
  state_->console_visible_count = 0;
  state_->console_first_visible_sequence.reset();
  if (state_->console_scope != console) {
    state_->console_scope = console;
    state_->console_display_paused = false;
    state_->console_clear_sequence = 0;
    state_->console_frozen_records.clear();
  }
  state_->play_apply_position.reset();
  state_->play_apply_confirm_position.reset();
  if (!game_running) {
    state_->play_apply_open = false;
    state_->play_apply_popup_pending = false;
    state_->play_apply_review = {};
    state_->play_apply_requested.reset();
    state_->play_inspection_entity = 0;
    state_->inspect_play_selection = false;
  }
  const auto scene_generation = scene == nullptr ? 0 : scene->Generation();
  if (scene_generation != state_->scene_gesture_document_generation)
    CancelSceneGestures(*state_);
  state_->scene_gesture_document_generation = scene_generation;
  if (scene)
    state_->scene_frame_token =
        SceneFileToken{workspace ? workspace->Project().id : foundation::Uuid{}, scene_generation};
  state_->selector_visible = false;
  const bool recovery_available = workspace != nullptr && workspace->HasRecoveryJournal();
  const auto input_root = workspace ? workspace->Root() : std::filesystem::path{};
  const auto input_project = workspace ? workspace->Project().id : foundation::Uuid{};
  if (state_->game_input_root != input_root || state_->game_input_project != input_project) {
    state_->game_input_bindings = {};
    state_->game_input_binding_status.clear();
    state_->game_input_save_requested.reset();
    state_->game_input_root = input_root;
    state_->game_input_project = input_project;
    state_->game_input_binding_open = false;
    state_->game_input_binding_pending = false;
  }
  if (!state_->app_focused || !play || play->State() != runtime::PlayState::Stopped ||
      recovery_available || state_->close_prompt_requested || state_->play_apply_open ||
      state_->scene_file_dialog != State::FileDialog::None || state_->scene_file_output ||
      state_->hierarchy_rename_target || state_->content_rename_target) {
    state_->game_input_binding_open = false;
    state_->game_input_binding_pending = false;
    if (state_->game_input_save_requested)
      state_->game_input_binding_status.clear();
    state_->game_input_save_requested.reset();
  }
  // Query from the same root ID scope that opens the modal, before entering a panel window.
  const bool close_confirmation_open =
      state_->close_prompt_requested || ImGui::IsPopupOpen("Unsaved scene###editor.close");
  const bool file_context_valid =
      scene && workspace && state_->scene_file_context &&
      state_->scene_file_token.project == workspace->Project().id &&
      state_->scene_file_token.document_generation == scene->Generation();
  const bool file_external_block = recovery_available || state_->play_apply_open ||
                                   close_confirmation_open || state_->game_input_binding_open;
  const auto active_tab =
      std::ranges::find(state_->scene_tabs, state_->scene_tab_active, &SceneTabItem::id);
  const bool tab_context_valid = file_context_valid && active_tab != state_->scene_tabs.end() &&
                                 active_tab->token == state_->scene_file_token;
  const bool reference_scene = tab_context_valid && (!active_tab->owned || active_tab->read_only);
  const bool tab_modal = state_->scene_tab_dialog || state_->scene_tab_output;
  const bool writable = workspace && workspace->Writable() && !reference_scene;
  const bool file_busy = state_->scene_file_dialog != State::FileDialog::None ||
                         state_->scene_file_output || tab_modal;
  if (ImGui::BeginMainMenuBar()) {
    const bool menu = ImGui::BeginMenu("File", file_context_valid && !file_external_block &&
                                                   !file_busy && !state_->content_rename_target);
    CaptureSceneFileControl(*state_, 0);
    if (menu) {
      if (ImGui::MenuItem("New Scene", "Ctrl+N", false, writable && !game_running))
        BeginSceneFile(*state_, *scene, SceneFileAction::New);
      CaptureSceneFileControl(*state_, 1);
      if (ImGui::MenuItem("Open Scene...", "Ctrl+O", false, !game_running))
        BeginSceneFile(*state_, *scene, SceneFileAction::Open);
      CaptureSceneFileControl(*state_, 2);
      if (ImGui::MenuItem("Save", "Ctrl+S", false, writable && !state_->scene_file_save_blocked))
        state_->scene_save_requested = true;
      CaptureSceneFileControl(*state_, 10);
      if (ImGui::MenuItem("Save As...", "Ctrl+Shift+S", false, writable))
        BeginSceneFile(*state_, *scene, SceneFileAction::SaveAs);
      CaptureSceneFileControl(*state_, 3);
      ImGui::EndMenu();
    }
    state_->static_export_positions = {};
    if (ImGui::BeginMenu("Build", file_context_valid && !file_external_block && !file_busy)) {
      const auto capture_position = [&](std::size_t index) {
        const auto low = ImGui::GetItemRectMin(), high = ImGui::GetItemRectMax();
        state_->static_export_positions[index] =
            std::array{(low.x + high.x) * .5F, (low.y + high.y) * .5F};
      };
      if (ImGui::MenuItem("Export StaticView package", nullptr, false,
                          writable && !game_running && !state_->scene_file_save_blocked &&
                              state_->scene_file_path && content && content->Writable() &&
                              !content->ReimportBusy() && !state_->static_export_busy))
        state_->static_export_request = StaticExportRequest{state_->scene_file_token, false};
      capture_position(1);
      if (ImGui::MenuItem("Cancel StaticView export", nullptr, false, state_->static_export_busy))
        state_->static_export_request = StaticExportRequest{state_->scene_file_token, true};
      capture_position(2);
      ImGui::EndMenu();
    } else {
      const auto low = ImGui::GetItemRectMin(), high = ImGui::GetItemRectMax();
      state_->static_export_positions[0] =
          std::array{(low.x + high.x) * .5F, (low.y + high.y) * .5F};
    }
    if (file_context_valid)
      ImGui::TextDisabled("%s%s",
                          state_->scene_file_path
                              ? PathLabel(state_->scene_file_path->filename()).c_str()
                              : "Untitled",
                          scene->Dirty() ? " *" : "");
    ImGui::EndMainMenuBar();
  }
  if (file_context_valid && state_->app_focused && !file_external_block && !tab_modal &&
      state_->scene_file_dialog == State::FileDialog::None && !state_->scene_file_output &&
      !state_->content_rename_target && !ImGui::GetIO().WantTextInput) {
    if (writable && !game_running &&
        ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_N, ImGuiInputFlags_RouteGlobal))
      BeginSceneFile(*state_, *scene, SceneFileAction::New);
    else if (!game_running &&
             ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_O, ImGuiInputFlags_RouteGlobal))
      BeginSceneFile(*state_, *scene, SceneFileAction::Open);
    else if (writable && ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_S,
                                         ImGuiInputFlags_RouteGlobal))
      BeginSceneFile(*state_, *scene, SceneFileAction::SaveAs);
  }
  const bool external_modal_open =
      file_external_block || tab_modal || state_->scene_file_dialog != State::FileDialog::None ||
      state_->scene_file_output || ImGui::IsPopupOpen("Scene file###editor.scene-file");
  if (!content)
    state_->content_rename_target.reset();
  const bool rename_editable = !reference_scene && (!workspace || workspace->Writable()) &&
                               !external_modal_open && !state_->content_rename_target;
  const bool rename_open = state_->hierarchy_rename_target.has_value();
  const bool interaction_blocked =
      external_modal_open || rename_open || state_->content_rename_target;
  const bool scene_editable = rename_editable && !rename_open;
  if (const auto *payload = ImGui::GetDragDropPayload();
      payload && payload->IsDataType(AssetDragPayload::kType.data()) &&
      (!state_->app_focused || !scene_editable || (content && !content->Writable()) ||
       ImGui::IsKeyPressed(ImGuiKey_Escape, false))) {
    ImGui::ClearDragDrop();
    ImGui::ClearActiveID(); // Held mouse buttons cannot recreate the canceled source on later
                            // frames.
  }

  if (!scene_editable) {
    CancelSceneGestures(*state_);
    state_->scene_save_requested = false;
  }
  if (interaction_blocked) {
    state_->game_input_focused = false;
    CancelSceneGestures(*state_);
  }
  if (scene_editable && ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_S, ImGuiInputFlags_RouteGlobal)) {
    static_cast<void>(shell.RouteCommand("editor.scene.save"));
    state_->scene_save_requested = true;
  }
  if (play != nullptr && !interaction_blocked && !ImGui::GetIO().WantTextInput) {
    if (ImGui::Shortcut(ImGuiKey_F5, ImGuiInputFlags_RouteGlobal))
      state_->play_command =
          play->State() == runtime::PlayState::Stopped ? PlayCommand::Start : PlayCommand::Stop;
    else if (play->State() != runtime::PlayState::Stopped &&
             ImGui::Shortcut(ImGuiKey_F6, ImGuiInputFlags_RouteGlobal))
      state_->play_command =
          play->State() == runtime::PlayState::Playing ? PlayCommand::Pause : PlayCommand::Resume;
    else if (ImGui::Shortcut(ImGuiKey_F10, ImGuiInputFlags_RouteGlobal) &&
             play->State() == runtime::PlayState::Paused)
      state_->play_command = PlayCommand::Step;
  }
  if (scene != nullptr && scene_editable && !ImGui::GetIO().WantTextInput &&
      ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_N, ImGuiInputFlags_RouteGlobal)) {
    CancelSceneGestures(*state_);
    static_cast<void>(shell.RouteCommand("editor.scene.create"));
    state_->hierarchy_create_request = State::HierarchyCreateRequest{
        std::string(state_->hierarchy_create_name.data()), std::nullopt,
        state_->hierarchy_create_kind, scene->Generation()};
  }
  if (scene != nullptr && scene_editable && !ImGui::GetIO().WantTextInput) {
    if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_Z, ImGuiInputFlags_RouteGlobal) ||
        ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_Y, ImGuiInputFlags_RouteGlobal)) {
      CancelSceneGestures(*state_);
      static_cast<void>(shell.RouteCommand("editor.scene.redo"));
      state_->scene_save_message = scene->Redo() ? "Redo complete." : "Nothing to redo.";
      state_->scene_save_success = true;
      state_->hierarchy_selection_anchor.reset();
    } else if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_Z, ImGuiInputFlags_RouteGlobal)) {
      CancelSceneGestures(*state_);
      static_cast<void>(shell.RouteCommand("editor.scene.undo"));
      state_->scene_save_message = scene->Undo() ? "Undo complete." : "Nothing to undo.";
      state_->scene_save_success = true;
      state_->hierarchy_selection_anchor.reset();
    }
  }
  if (scene != nullptr && !interaction_blocked && !ImGui::GetIO().WantTextInput) {
    if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_C, ImGuiInputFlags_RouteGlobal)) {
      static_cast<void>(shell.RouteCommand("editor.scene.copy"));
      CopyHierarchySelection(*state_, *scene);
    }
    if (scene_editable &&
        ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_X, ImGuiInputFlags_RouteGlobal)) {
      static_cast<void>(shell.RouteCommand("editor.scene.cut"));
      CutHierarchySelection(*state_, *scene);
    }
    if (scene_editable &&
        ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_V, ImGuiInputFlags_RouteGlobal)) {
      CancelSceneGestures(*state_);
      static_cast<void>(shell.RouteCommand("editor.scene.paste"));
      PasteHierarchySelection(*state_, *scene);
    }
    if (scene_editable &&
        ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_D, ImGuiInputFlags_RouteGlobal)) {
      CancelSceneGestures(*state_);
      static_cast<void>(shell.RouteCommand("editor.scene.duplicate"));
      DuplicateHierarchySelection(*state_, *scene);
    }
  }
  const auto *viewport = ImGui::GetMainViewport();
  const ImGuiID dockspace =
      ImGui::DockSpaceOverViewport(0, viewport, ImGuiDockNodeFlags_PassthruCentralNode);
  if (!state_->initial_dock_layout_built) {
    BuildInitialDockLayout(dockspace, *viewport);
    state_->initial_dock_layout_built = true;
    state_->focus_initial_content = true;
    state_->focus_initial_scene = true;
  }
  const auto hierarchy_window = PanelWindowName("nexora.hierarchy");
  ApplyPendingHierarchyRequests(*state_, scene, scene_editable, rename_open && rename_editable);
  if (ImGui::Begin(hierarchy_window.c_str()))
    DrawHierarchy(*state_, scene, shell, interaction_blocked, scene_editable, rename_editable);
  ImGui::End();
  const auto inspector_window = PanelWindowName("nexora.inspector");
  if (ImGui::Begin(inspector_window.c_str())) {
    if (game_running) {
      if (ImGui::RadioButton("Editor", !state_->inspect_play_selection))
        state_->inspect_play_selection = false;
      ImGui::SameLine();
      if (ImGui::RadioButton("Play (read-only)", state_->inspect_play_selection))
        state_->inspect_play_selection = true;
    }
    if (state_->inspect_play_selection) {
      CancelInspectorDrafts(*state_);
      const auto selected =
          std::ranges::find(play_snapshot.entities, state_->play_inspection_entity,
                            &runtime::RuntimeEntitySnapshot::id);
      if (selected == play_snapshot.entities.end()) {
        state_->play_inspection_entity = 0;
        ImGui::TextDisabled("Select a Play entity in the Game panel.");
      } else {
        state_->play_inspector_rendered = selected->id;
        DrawPlayEntityInspector(*selected);
      }
    } else {
      DrawInspector(*state_, scene, content, meshes, materials, scene_editable,
                    !interaction_blocked);
    }
  } else {
    CancelInspectorDrafts(*state_);
  }
  ImGui::End();
  const auto scene_window = PanelWindowName("nexora.scene");
  if (ImGui::Begin(scene_window.c_str())) {
    DrawSceneTabs(*state_,
                  tab_context_valid && !file_external_block && !game_running &&
                      state_->scene_file_dialog == State::FileDialog::None &&
                      !state_->scene_file_output && !state_->content_rename_target,
                  workspace && workspace->Writable());
    if (workspace && !workspace->Writable())
      ImGui::TextDisabled("Read-only project: scene editing disabled.");
    ImGui::BeginDisabled(scene == nullptr || !scene_editable);
    if (ImGui::Button("Undo")) {
      CancelSceneGestures(*state_);
      static_cast<void>(shell.RouteCommand("editor.scene.undo"));
      state_->scene_save_message =
          scene != nullptr && scene->Undo() ? "Undo complete." : "Nothing to undo.";
      state_->scene_save_success = true;
      state_->hierarchy_selection_anchor.reset();
    }
    ImGui::SameLine();
    if (ImGui::Button("Redo")) {
      CancelSceneGestures(*state_);
      static_cast<void>(shell.RouteCommand("editor.scene.redo"));
      state_->scene_save_message =
          scene != nullptr && scene->Redo() ? "Redo complete." : "Nothing to redo.";
      state_->scene_save_success = true;
      state_->hierarchy_selection_anchor.reset();
    }
    ImGui::SameLine();
    if (ImGui::Button("Save Scene") && scene != nullptr) {
      static_cast<void>(shell.RouteCommand("editor.scene.save"));
      state_->scene_save_requested = true;
    }
    ImGui::EndDisabled();
    if (scene != nullptr) {
      if (scene->Dirty())
        ImGui::TextColored(ImVec4(1.0F, 0.72F, 0.28F, 1.0F), "Unsaved scene changes");
      else
        ImGui::TextDisabled("Scene saved");
    }
    if (!state_->scene_save_message.empty()) {
      if (!state_->scene_save_success)
        ImGui::TextColored(ImVec4(1.0F, 0.4F, 0.4F, 1.0F), "%s",
                           state_->scene_save_message.c_str());
      else
        ImGui::TextUnformatted(state_->scene_save_message.c_str());
    }
    if (ImGui::Checkbox("3D Preview", &state_->native_scene_preview))
      CancelSceneGestures(*state_);
    if (scene != nullptr) {
      if (state_->native_scene_preview) {
        state_->scene_markers.clear();
        ImGui::BeginDisabled(scene->Selection().empty() ||
                             state_->native_scene_drag_origin.has_value());
        const bool frame_clicked = ImGui::SmallButton("Frame selected");
        const auto frame_min = ImGui::GetItemRectMin(), frame_max = ImGui::GetItemRectMax();
        state_->scene_frame_position =
            std::array{(frame_min.x + frame_max.x) * 0.5F, (frame_min.y + frame_max.y) * 0.5F};
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::BeginDisabled(state_->native_scene_drag_origin.has_value());
        const bool frame_all_clicked = ImGui::SmallButton("Frame all");
        const auto all_min = ImGui::GetItemRectMin(), all_max = ImGui::GetItemRectMax();
        state_->scene_frame_all_position =
            std::array{(all_min.x + all_max.x) * 0.5F, (all_min.y + all_max.y) * 0.5F};
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::BeginDisabled(!state_->app_focused || interaction_blocked ||
                             !state_->native_scene_preview_available ||
                             state_->native_scene_drag_origin || state_->native_scene_drag);
        const bool select_all_clicked = ImGui::SmallButton("Select all");
        const auto select_min = ImGui::GetItemRectMin(), select_max = ImGui::GetItemRectMax();
        state_->scene_select_all_position =
            std::array{(select_min.x + select_max.x) * 0.5F, (select_min.y + select_max.y) * 0.5F};
        ImGui::EndDisabled();
        ImGui::TextDisabled(
            "F: selected | Home: all | Ctrl+A: select all | Right: orbit | Wheel: zoom");
        if (state_->native_scene_tool == NativeSceneTool::Select)
          ImGui::TextDisabled(
              "Click: select | Ctrl+click: toggle | W: move | E: rotate | R: scale");
        else if (state_->native_scene_tool == NativeSceneTool::Rotate)
          ImGui::TextDisabled(
              "Drag colored rings: rotate | Shift: 15 deg snap | W: move | R: scale");
        else if (state_->native_scene_tool == NativeSceneTool::Scale)
          ImGui::TextDisabled(
              "Drag cubes: local/uniform scale | Shift: 0.25 snap | W: move | E: rotate");
        else
          ImGui::TextDisabled(
              "Drag axes/planes: move | Free drag: X/Z | Shift+free drag: Y | E/R: rotate/scale");
        ImGui::TextDisabled("Middle: pan X/Z | Shift+middle: pan Y | Delete: selected");
        ImGui::BeginDisabled(!state_->app_focused || interaction_blocked ||
                             state_->native_scene_drag_origin || state_->native_scene_drag);
        constexpr std::array tools{NativeSceneTool::Select, NativeSceneTool::Move,
                                   NativeSceneTool::Rotate, NativeSceneTool::Scale};
        constexpr std::array tool_labels{"Select (Q)", "Move (W)", "Rotate (E)", "Scale (R)"};
        for (std::size_t index = 0; index < tools.size(); ++index) {
          if (index != 0)
            ImGui::SameLine();
          if (ImGui::RadioButton(tool_labels[index], state_->native_scene_tool == tools[index])) {
            state_->native_scene_tool = tools[index];
            if (tools[index] == NativeSceneTool::Scale)
              state_->native_scene_local_axes = true;
          }
          const auto tool_min = ImGui::GetItemRectMin(), tool_max = ImGui::GetItemRectMax();
          state_->native_scene_tool_positions[static_cast<std::size_t>(tools[index])] =
              std::array{(tool_min.x + tool_max.x) * 0.5F, (tool_min.y + tool_max.y) * 0.5F};
        }
        ImGui::BeginDisabled(state_->native_scene_tool == NativeSceneTool::Scale);
        ImGui::Checkbox("Local axes (X)", &state_->native_scene_local_axes);
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::Checkbox("Center pivot (P)", &state_->native_scene_center_pivot);
        ImGui::EndDisabled();
        ImGui::SameLine();
        constexpr std::array snap_steps{0.25, 0.5, 1.0, 2.0, 4.0};
        ImGui::Checkbox("Snap movement", &state_->scene_snap_to_grid);
        ImGui::SameLine();
        ImGui::BeginDisabled(!state_->scene_snap_to_grid);
        ImGui::SetNextItemWidth(140.0F);
        ImGui::Combo("Step (world units)", &state_->scene_snap_step_index,
                     "0.25\0"
                     "0.5\0"
                     "1\0"
                     "2\0"
                     "4\0");
        ImGui::EndDisabled();
        if (!state_->native_scene_preview_available)
          ImGui::TextUnformatted("Native 3D preview is unavailable on this backend.");
        const auto available = ImGui::GetContentRegionAvail();
        ImGui::InvisibleButton(
            "##scene-native-preview", {std::max(available.x, 1.0F), std::max(available.y, 160.0F)},
            ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight |
                ImGuiButtonFlags_MouseButtonMiddle);
        CaptureCanvasViewport(state_->scene_canvas_viewport, ImGui::GetItemRectMin(),
                              ImGui::GetItemRectMax());
        const auto &io = ImGui::GetIO();
        // Resolve tool input before picking/starting a drag in the same input frame.
        if (state_->app_focused && !interaction_blocked &&
            ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
            (ImGui::IsItemHovered(ImGuiHoveredFlags_NoNavOverride) || ImGui::IsItemActive()) &&
            !state_->native_scene_drag_origin && !state_->native_scene_drag && !io.WantTextInput) {
          if (ImGui::IsKeyPressed(ImGuiKey_Q, false))
            state_->native_scene_tool = NativeSceneTool::Select;
          else if (ImGui::IsKeyPressed(ImGuiKey_W, false))
            state_->native_scene_tool = NativeSceneTool::Move;
          else if (ImGui::IsKeyPressed(ImGuiKey_E, false))
            state_->native_scene_tool = NativeSceneTool::Rotate;
          else if (ImGui::IsKeyPressed(ImGuiKey_R, false)) {
            state_->native_scene_tool = NativeSceneTool::Scale;
            state_->native_scene_local_axes = true;
          }
          if (!io.KeyCtrl && !io.KeyAlt && !io.KeySuper) {
            if (state_->native_scene_tool != NativeSceneTool::Scale &&
                ImGui::IsKeyPressed(ImGuiKey_X, false))
              state_->native_scene_local_axes = !state_->native_scene_local_axes;
            if (ImGui::IsKeyPressed(ImGuiKey_P, false))
              state_->native_scene_center_pivot = !state_->native_scene_center_pivot;
          }
        }
        if (scene_editable && !scene->Selection().empty() && !io.WantTextInput &&
            (ImGui::IsItemHovered() || ImGui::IsItemActive()) &&
            !ImGui::IsMouseDown(ImGuiMouseButton_Left) &&
            ImGui::IsKeyPressed(ImGuiKey_Delete, false)) {
          static_cast<void>(shell.RouteCommand("editor.scene.delete"));
          DeleteHierarchySelection(*state_, *scene);
          state_->native_scene_drag_origin.reset();
          state_->native_scene_drag_preview.reset();
        }
        if (state_->app_focused && !interaction_blocked && state_->scene_canvas_viewport &&
            ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
          const auto x = static_cast<std::uint32_t>(io.MousePos.x * io.DisplayFramebufferScale.x);
          const auto y = static_cast<std::uint32_t>(io.MousePos.y * io.DisplayFramebufferScale.y);
          const auto &view = *state_->scene_canvas_viewport;
          if (x >= view.x && y >= view.y && x < view.x + view.width && y < view.y + view.height) {
            state_->native_scene_pick = NativeScenePickRequest{x, y, io.KeyCtrl};
            if (scene_editable && !io.KeyCtrl &&
                state_->native_scene_tool != NativeSceneTool::Select)
              state_->native_scene_drag_origin =
                  std::array{static_cast<std::int32_t>(x), static_cast<std::int32_t>(y)};
            state_->native_scene_drag_snap_step =
                state_->scene_snap_to_grid ? snap_steps[state_->scene_snap_step_index] : 0.0;
            state_->native_scene_drag_vertical = io.KeyShift;
          }
        }
        if (state_->native_scene_drag_origin && ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
          CancelSceneGestures(*state_);
        }
        if (ImGui::IsMouseReleased(ImGuiMouseButton_Left) && state_->native_scene_drag_origin) {
          const auto start = *state_->native_scene_drag_origin;
          const auto end_x =
              static_cast<std::int32_t>(io.MousePos.x * io.DisplayFramebufferScale.x);
          const auto end_y =
              static_cast<std::int32_t>(io.MousePos.y * io.DisplayFramebufferScale.y);
          if (std::abs(end_x - start[0]) >= 4 || std::abs(end_y - start[1]) >= 4)
            state_->native_scene_drag = {start[0],
                                         start[1],
                                         end_x,
                                         end_y,
                                         state_->native_scene_drag_snap_step,
                                         state_->native_scene_drag_vertical};
          state_->native_scene_drag_origin.reset();
        }
        if (state_->native_scene_drag_origin && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
          const auto start = *state_->native_scene_drag_origin;
          const auto end_x =
              static_cast<std::int32_t>(io.MousePos.x * io.DisplayFramebufferScale.x);
          const auto end_y =
              static_cast<std::int32_t>(io.MousePos.y * io.DisplayFramebufferScale.y);
          if (std::abs(end_x - start[0]) >= 4 || std::abs(end_y - start[1]) >= 4)
            state_->native_scene_drag_preview = {start[0],
                                                 start[1],
                                                 end_x,
                                                 end_y,
                                                 state_->native_scene_drag_snap_step,
                                                 state_->native_scene_drag_vertical};
        }
        if (state_->app_focused && !interaction_blocked &&
            (ImGui::IsItemHovered() || ImGui::IsItemActive()) &&
            !state_->native_scene_drag_origin && !state_->native_scene_drag && !io.WantTextInput) {
          if (io.MouseWheel != 0.0F)
            state_->native_scene_orbit.distance =
                std::clamp(state_->native_scene_orbit.distance *
                               std::pow(0.85, static_cast<double>(io.MouseWheel)),
                           2.0, 100.0);
          if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Right, 0.0F)) {
            state_->native_scene_orbit.yaw = std::remainder(
                state_->native_scene_orbit.yaw + io.MouseDelta.x * 0.01, 6.283185307179586);
            state_->native_scene_orbit.pitch =
                std::clamp(state_->native_scene_orbit.pitch - io.MouseDelta.y * 0.01, 0.1, 1.45);
          }
          if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Middle, 0.0F)) {
            const auto yaw = state_->native_scene_orbit.yaw;
            const auto speed = state_->native_scene_orbit.distance * 0.003;
            if (io.KeyShift) {
              state_->native_scene_orbit.target_y =
                  std::clamp(state_->native_scene_orbit.target_y +
                                 static_cast<double>(io.MouseDelta.y) * speed,
                             -100000.0, 100000.0);
            } else {
              const auto dx = static_cast<double>(io.MouseDelta.x) * speed;
              const auto dz = static_cast<double>(io.MouseDelta.y) * speed;
              state_->scene_center_world.x =
                  static_cast<float>(std::clamp(static_cast<double>(state_->scene_center_world.x) -
                                                    std::cos(yaw) * dx + std::sin(yaw) * dz,
                                                -100000.0, 100000.0));
              state_->scene_center_world.y =
                  static_cast<float>(std::clamp(static_cast<double>(state_->scene_center_world.y) +
                                                    std::sin(yaw) * dx + std::cos(yaw) * dz,
                                                -100000.0, 100000.0));
            }
          }
        }
        const bool frame_keyboard =
            !io.WantTextInput && ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
        const bool frame_all =
            frame_all_clicked || (frame_keyboard && ImGui::IsKeyPressed(ImGuiKey_Home, false));
        if (state_->app_focused && !interaction_blocked && !state_->native_scene_drag_origin &&
            !state_->native_scene_drag && state_->native_scene_preview_available &&
            !io.MouseDown[ImGuiMouseButton_Left] && state_->scene_canvas_viewport &&
            (select_all_clicked ||
             (frame_keyboard &&
              ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_A, ImGuiInputFlags_RouteFocused)))) {
          state_->native_scene_select_all_request = state_->scene_frame_token;
        }
        if (state_->app_focused && !interaction_blocked && !state_->native_scene_drag_origin &&
            !state_->native_scene_drag && !state_->native_scene_select_all_request &&
            state_->scene_canvas_viewport &&
            (frame_all || frame_clicked ||
             (frame_keyboard && ImGui::IsKeyPressed(ImGuiKey_F, false)))) {
          const auto &view = *state_->scene_canvas_viewport;
          if (frame_all) {
            state_->native_scene_frame_all_request = state_->scene_frame_token;
            state_->native_scene_frame_all_apply = state_->scene_frame_token;
          } else {
            static_cast<void>(FrameNativeSceneSelection(
                *state_, *scene, content, meshes, static_cast<double>(view.width) / view.height));
          }
        }
        AcceptSceneMeshDrop(*state_, *scene, content, meshes, scene_editable,
                            NativeMeshDropPose(*state_));
      } else {
        CancelNativeSceneGesture(*state_);
        DrawSceneOverview(*state_, *scene, scene_editable, content, meshes,
                          state_->app_focused && !interaction_blocked);
      }
    }
  }
  ImGui::End();
  if (!state_->scene_canvas_viewport)
    CancelSceneGestures(*state_);
  bool game_canvas_visible = false;
  if (!play || play->State() != runtime::PlayState::Playing || !state_->app_focused)
    state_->game_input_focused = false;
  const auto game_window = PanelWindowName("nexora.game");
  if (game_running && !state_->game_was_running)
    ImGui::SetNextWindowFocus();
  state_->game_was_running = game_running;
  if (ImGui::Begin(game_window.c_str())) {
    if (play == nullptr) {
      ImGui::TextDisabled("Play session unavailable.");
    } else {
      if (content &&
          state_->gameplay_project_generation != content->Browser().ProjectGeneration()) {
        if (state_->gameplay_project_generation != 0) {
          state_->gameplay_library.fill(0);
          state_->gameplay_status.clear();
        }
        state_->gameplay_project_generation = content->Browser().ProjectGeneration();
      }
      ImGui::BeginDisabled(game_running);
      ImGui::InputTextWithHint("Gameplay library", "Optional path within this project",
                               state_->gameplay_library.data(), state_->gameplay_library.size());
      ImGui::EndDisabled();
      if (!state_->gameplay_status.empty())
        ImGui::TextWrapped("%s", state_->gameplay_status.c_str());
      const auto state = play->State();
      ImGui::BeginDisabled(state != runtime::PlayState::Stopped || interaction_blocked ||
                           !state_->app_focused);
      if (ImGui::Button("Input bindings###game.input.bindings")) {
        state_->game_input_draft = state_->game_input_bindings;
        state_->game_input_binding_error.clear();
        state_->game_input_binding_open = state_->game_input_binding_pending = true;
        state_->play_command = PlayCommand::None;
        CancelSceneGestures(*state_);
        CancelInspectorDrafts(*state_);
      }
      const auto binding_min = ImGui::GetItemRectMin(), binding_max = ImGui::GetItemRectMax();
      state_->game_input_binding_positions[0] = std::array{(binding_min.x + binding_max.x) * 0.5F,
                                                           (binding_min.y + binding_max.y) * 0.5F};
      ImGui::EndDisabled();
      if (!state_->game_input_binding_status.empty())
        ImGui::TextWrapped("%s", state_->game_input_binding_status.c_str());
      if (state == runtime::PlayState::Stopped) {
        if (ImGui::Button("Play"))
          state_->play_command = PlayCommand::Start;
      } else {
        if (ImGui::Button("Stop"))
          state_->play_command = PlayCommand::Stop;
        ImGui::SameLine();
        if (state == runtime::PlayState::Playing) {
          if (ImGui::Button("Pause"))
            state_->play_command = PlayCommand::Pause;
        } else {
          if (ImGui::Button("Resume"))
            state_->play_command = PlayCommand::Resume;
          ImGui::SameLine();
          if (ImGui::Button("Step"))
            state_->play_command = PlayCommand::Step;
        }
        const float apply_width =
            ImGui::CalcTextSize("Apply Changes").x + ImGui::GetStyle().FramePadding.x * 2;
        const float same_line_end =
            ImGui::GetItemRectMax().x + ImGui::GetStyle().ItemSpacing.x + apply_width;
        if (same_line_end <= ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x)
          ImGui::SameLine();
        ImGui::BeginDisabled(scene == nullptr || !writable || interaction_blocked ||
                             state_->close_prompt_requested);
        if (ImGui::Button("Apply Changes")) {
          state_->play_apply_review = CapturePlayTransformReview(*scene, *play);
          state_->play_apply_open = true;
          state_->game_input_focused = false;
          CancelSceneGestures(*state_);
          state_->play_command =
              state == runtime::PlayState::Playing ? PlayCommand::Pause : PlayCommand::None;
          state_->play_apply_popup_pending = true;
        }
        const auto apply_min = ImGui::GetItemRectMin();
        const auto apply_max = ImGui::GetItemRectMax();
        state_->play_apply_position =
            std::array{(apply_min.x + apply_max.x) * 0.5F, (apply_min.y + apply_max.y) * 0.5F};
        ImGui::EndDisabled();
      }
      ImGui::Text("State: %s", state == runtime::PlayState::Stopped   ? "Stopped"
                               : state == runtime::PlayState::Playing ? "Playing"
                                                                      : "Paused");
      if (state == runtime::PlayState::Paused) {
        ImGui::SameLine();
        ImGui::Text("| %s", PauseReasonLabel(play->LastPauseReason()));
      }
      ImGui::Text("Fixed ticks: %llu | Steps: %llu | Callback failures: %llu",
                  static_cast<unsigned long long>(play->Stats().fixed_ticks),
                  static_cast<unsigned long long>(play->Stats().manual_steps),
                  static_cast<unsigned long long>(play->Stats().crashes));
      if (game_running) {
        const auto label = [](runtime::Id camera) {
          return camera ? "Camera #" + std::to_string(camera) : std::string("Automatic");
        };
        ImGui::BeginDisabled(interaction_blocked);
        ImGui::PushID(std::to_string(state_->game_camera_generation).c_str());
        ImGui::SetNextItemWidth(200.0F);
        const bool open = ImGui::BeginCombo("Preview camera###game.camera",
                                            label(state_->game_camera_selection).c_str());
        const auto minimum = ImGui::GetItemRectMin(), maximum = ImGui::GetItemRectMax();
        state_->game_camera_combo_position =
            std::array{(minimum.x + maximum.x) * 0.5F, (minimum.y + maximum.y) * 0.5F};
        if (open) {
          ImGuiListClipper cameras;
          cameras.Begin(static_cast<int>(game_cameras.size() + 1));
          while (cameras.Step())
            for (int index = cameras.DisplayStart; index < cameras.DisplayEnd; ++index) {
              const auto camera =
                  index == 0 ? 0 : game_cameras[static_cast<std::size_t>(index - 1)];
              ImGui::PushID(std::to_string(camera).c_str());
              if (ImGui::Selectable(label(camera).c_str(), state_->game_camera_selection == camera))
                state_->game_camera_selection = camera;
              const auto item_min = ImGui::GetItemRectMin(), item_max = ImGui::GetItemRectMax();
              state_->game_camera_positions.push_back(
                  {camera, {(item_min.x + item_max.x) * 0.5F, (item_min.y + item_max.y) * 0.5F}});
              ImGui::PopID();
            }
          ImGui::EndCombo();
        }
        ImGui::PopID();
        ImGui::EndDisabled();
      }
      const auto &snapshot = play_snapshot;
      ImGui::Text("Play World entities: %zu", snapshot.entities.size());
      ImGui::Separator();
      if (game_running)
        ImGui::TextDisabled(state_->game_input_focused ? "Game input captured | Escape releases"
                                                       : "Click Game canvas to capture input");
      if (game_running && state_->native_game_available && !NativeScenePreviewViewport()) {
        ImGui::TextDisabled("Play camera | Assets frozen at Play start");
        if (!state_->native_game_status.empty())
          ImGui::TextWrapped("%s", state_->native_game_status.c_str());
        const auto available = ImGui::GetContentRegionAvail();
        ImGui::InvisibleButton("##game-native-preview", {std::max(available.x, 1.0F), 180.0F});
        CaptureCanvasViewport(state_->native_game_viewport, ImGui::GetItemRectMin(),
                              ImGui::GetItemRectMax());
      } else {
        if (game_running)
          ImGui::TextWrapped("%s",
                             !state_->native_game_available
                                 ? "Native Game View is unavailable on this backend."
                                 : "Hide the Scene 3D canvas to render Game View in this window.");
        DrawPlayOverview(snapshot);
      }
      game_canvas_visible = game_running;
      if (play->State() == runtime::PlayState::Playing && state_->app_focused &&
          !interaction_blocked && !state_->close_prompt_requested) {
        if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
          state_->game_input_focused = true;
          ImGui::GetIO().ClearInputKeys();
        } else if (!ImGui::IsItemHovered()) {
          state_->game_input_focused = false;
        }
      }
      ImGuiListClipper clipper;
      clipper.Begin(static_cast<int>(snapshot.entities.size()));
      while (clipper.Step())
        for (int index = clipper.DisplayStart; index < clipper.DisplayEnd; ++index) {
          const auto &entity = snapshot.entities[static_cast<std::size_t>(index)];
          char text[192];
          std::snprintf(text, sizeof(text), "#%llu  world (%.2f, %.2f, %.2f)%s%s%s",
                        static_cast<unsigned long long>(entity.id), entity.world_transform.x,
                        entity.world_transform.y, entity.world_transform.z,
                        entity.camera ? " Camera" : "", entity.light ? " Light" : "",
                        entity.mesh_renderer ? " Mesh" : "");
          const auto label = std::string(text) + "###play.entity";
          ImGui::PushID(std::to_string(entity.id).c_str());
          if (ImGui::Selectable(label.c_str(), state_->play_inspection_entity == entity.id)) {
            state_->play_inspection_entity = entity.id;
            state_->inspect_play_selection = true;
          }
          ImGui::PopID();
        }
    }
  }
  ImGui::End();
  if (!game_canvas_visible)
    state_->game_input_focused = false;
  const auto console_window = PanelWindowName("nexora.console");
  if (ImGui::Begin(console_window.c_str())) {
    if (console == nullptr) {
      ImGui::Text("Last command: %.*s", static_cast<int>(shell.LastCommand().size()),
                  shell.LastCommand().data());
    } else {
      state_->console_filter.Draw("Filter###editor.console.filter", 220.0F);
      ImGui::SameLine();
      constexpr const char *levels[] = {"All", "Info+", "Warning+", "Error+"};
      ImGui::SetNextItemWidth(110.0F);
      ImGui::Combo("Severity###editor.console.severity", &state_->console_min_severity, levels, 4);
      auto live_records = state_->console_display_paused ? std::vector<runtime::RuntimeLogRecord>{}
                                                         : console->Snapshot();
      const auto control_position = [&](std::size_t control) {
        const auto min = ImGui::GetItemRectMin();
        const auto max = ImGui::GetItemRectMax();
        state_->console_control_positions[control] =
            std::array{(min.x + max.x) * 0.5F, (min.y + max.y) * 0.5F};
      };
      if (ImGui::Button(state_->console_display_paused ? "Resume display###editor.console.pause"
                                                       : "Pause display###editor.console.pause")) {
        state_->console_display_paused = !state_->console_display_paused;
        if (state_->console_display_paused)
          state_->console_frozen_records = std::move(live_records);
        else {
          state_->console_frozen_records.clear();
          live_records = console->Snapshot();
        }
      }
      control_position(0);
      ImGui::SameLine();
      if (ImGui::Button("Clear view###editor.console.clear")) {
        // Capture every record present at the click, including records hidden by filters/pause.
        const auto current = console->Snapshot();
        if (!current.empty())
          state_->console_clear_sequence = current.back().sequence;
        state_->console_frozen_records.clear();
      }
      control_position(1);
      const auto &records =
          state_->console_display_paused ? state_->console_frozen_records : live_records;
      std::vector<const runtime::RuntimeLogRecord *> visible;
      visible.reserve(records.size());
      const auto minimum = static_cast<runtime::RuntimeLogSeverity>(state_->console_min_severity);
      for (const auto &record : records) {
        if (record.sequence <= state_->console_clear_sequence || record.severity < minimum)
          continue;
        const std::string searchable = record.category + " " + record.source + " " + record.message;
        if (state_->console_filter.PassFilter(searchable.c_str()))
          visible.push_back(&record);
      }
      state_->console_visible_count = visible.size();
      if (!visible.empty())
        state_->console_first_visible_sequence = visible.front()->sequence;
      ImGui::Text("%zu visible | %zu %s | %llu dropped", visible.size(), records.size(),
                  state_->console_display_paused ? "captured (paused)" : "retained",
                  static_cast<unsigned long long>(console->DroppedCount()));
      ImGui::BeginChild("Records###editor.console.records", ImVec2(0, 0), ImGuiChildFlags_Borders);
      ImGuiListClipper clipper;
      clipper.Begin(static_cast<int>(visible.size()));
      while (clipper.Step()) {
        for (int index = clipper.DisplayStart; index < clipper.DisplayEnd; ++index) {
          const auto &record = *visible[static_cast<std::size_t>(index)];
          const char *label = "Info";
          if (record.severity == runtime::RuntimeLogSeverity::Trace)
            label = "Trace";
          else if (record.severity == runtime::RuntimeLogSeverity::Warning)
            label = "Warning";
          else if (record.severity == runtime::RuntimeLogSeverity::Error)
            label = "Error";
          else if (record.severity == runtime::RuntimeLogSeverity::Fatal)
            label = "Fatal";
          std::string row = "[" + std::to_string(record.timestamp_nanoseconds / 1000000) + " ms] " +
                            label + " " + record.category + " (" + record.source +
                            "): " + record.message;
          std::replace(row.begin(), row.end(), '\n', ' ');
          std::replace(row.begin(), row.end(), '\r', ' ');
          ImGui::PushID(static_cast<int>(record.sequence));
          if (record.severity >= runtime::RuntimeLogSeverity::Error)
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0F, 0.45F, 0.45F, 1.0F));
          else if (record.severity == runtime::RuntimeLogSeverity::Warning)
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0F, 0.75F, 0.35F, 1.0F));
          ImGui::Selectable(row.c_str());
          if (ImGui::IsItemHovered())
            ImGui::SetTooltip("%s", row.c_str());
          if (record.severity >= runtime::RuntimeLogSeverity::Warning)
            ImGui::PopStyleColor();
          ImGui::PopID();
        }
      }
      ImGui::EndChild();
    }
  }
  ImGui::End();
  const auto profiler_window = PanelWindowName("nexora.profiler");
  if (ImGui::Begin(profiler_window.c_str())) {
    if (profile == nullptr) {
      ImGui::TextDisabled("Frame capture unavailable.");
    } else {
      ImGui::SeparatorText("Live capture");
      bool capturing = profile->Capturing();
      if (ImGui::Checkbox("Capture", &capturing))
        profile->SetCapturing(capturing);
      const auto capture_min = ImGui::GetItemRectMin(), capture_max = ImGui::GetItemRectMax();
      state_->profile_capture_position = std::array{(capture_min.x + capture_max.x) * 0.5F,
                                                    (capture_min.y + capture_max.y) * 0.5F};
      ImGui::SameLine();
      if (ImGui::Button("Clear"))
        profile->Clear();
      const auto memory_clear_min = ImGui::GetItemRectMin(),
                 memory_clear_max = ImGui::GetItemRectMax();
      state_->profile_clear_position = std::array{(memory_clear_min.x + memory_clear_max.x) * 0.5F,
                                                  (memory_clear_min.y + memory_clear_max.y) * 0.5F};
      state_->profile_memory = profile->ProcessMemory();
      state_->profile_gpu = profile->GpuTiming();
      const auto samples = profile->Samples();
      ImGui::SameLine();
      ImGui::BeginDisabled(samples.empty() || !workspace || !workspace->Writable() ||
                           interaction_blocked || state_->close_prompt_requested);
      if (ImGui::Button("Export CSV"))
        state_->profile_export_requested = true;
      const auto export_min = ImGui::GetItemRectMin();
      const auto export_max = ImGui::GetItemRectMax();
      state_->profile_export_position =
          std::array{(export_min.x + export_max.x) * 0.5F, (export_min.y + export_max.y) * 0.5F};
      ImGui::SameLine();
      if (ImGui::Button("Export JSON###editor.profiler.export-json"))
        state_->profile_json_export_requested = true;
      const auto json_min = ImGui::GetItemRectMin();
      const auto json_max = ImGui::GetItemRectMax();
      state_->profile_json_export_position =
          std::array{(json_min.x + json_max.x) * 0.5F, (json_min.y + json_max.y) * 0.5F};
      ImGui::EndDisabled();
      ImGui::BeginDisabled(!workspace || interaction_blocked || state_->close_prompt_requested);
      if (ImGui::Button("Import CSV###editor.profiler.import-csv"))
        state_->profile_csv_import_requested = true;
      const auto import_min = ImGui::GetItemRectMin(), import_max = ImGui::GetItemRectMax();
      state_->profile_csv_import_position =
          std::array{(import_min.x + import_max.x) * 0.5F, (import_min.y + import_max.y) * 0.5F};
      ImGui::SameLine();
      if (ImGui::Button("Import JSON###editor.profiler.import-json"))
        state_->profile_json_import_requested = true;
      const auto json_import_min = ImGui::GetItemRectMin();
      const auto json_import_max = ImGui::GetItemRectMax();
      state_->profile_json_import_position =
          std::array{(json_import_min.x + json_import_max.x) * 0.5F,
                     (json_import_min.y + json_import_max.y) * 0.5F};
      ImGui::SameLine();
      ImGui::BeginDisabled(!state_->imported_profile);
      if (ImGui::Button("Clear imported###editor.profiler.clear-import"))
        state_->imported_profile.reset();
      const auto clear_min = ImGui::GetItemRectMin(), clear_max = ImGui::GetItemRectMax();
      state_->profile_import_clear_position =
          std::array{(clear_min.x + clear_max.x) * 0.5F, (clear_min.y + clear_max.y) * 0.5F};
      ImGui::EndDisabled();
      ImGui::EndDisabled();
      const auto memory_position = [&](std::size_t index) {
        const auto low = ImGui::GetItemRectMin(), high = ImGui::GetItemRectMax();
        state_->memory_control_positions[index] =
            std::array{(low.x + high.x) * 0.5F, (low.y + high.y) * 0.5F};
      };
      ImGui::BeginDisabled(!workspace || !workspace->Writable() || interaction_blocked ||
                           state_->close_prompt_requested || profile->MemorySamples().empty());
      if (ImGui::Button("Export memory"))
        state_->memory_export_requested = true;
      memory_position(0);
      ImGui::EndDisabled();
      ImGui::SameLine();
      ImGui::BeginDisabled(!workspace || interaction_blocked || state_->close_prompt_requested);
      if (ImGui::Button("Import memory"))
        state_->memory_import_requested = true;
      memory_position(1);
      ImGui::SameLine();
      ImGui::BeginDisabled(!state_->imported_memory);
      if (ImGui::Button("Clear memory import"))
        state_->imported_memory.reset();
      memory_position(2);
      ImGui::EndDisabled();
      ImGui::EndDisabled();
      if (!state_->profile_export_status.empty())
        ImGui::TextWrapped("%s", state_->profile_export_status.c_str());
      ImGui::Text("%zu frames retained | %llu older frames dropped", samples.size(),
                  static_cast<unsigned long long>(profile->DroppedCount()));
      ImGui::TextDisabled("Editor frame processing: wall time after BeginFrame, before Present.");
      ImGui::TextDisabled("Wall-time exports remain separate from native GPU measurements.");
      const auto plot = [](std::span<const FrameSample> values_to_plot, const char *label) {
        if (values_to_plot.empty())
          return;
        std::vector<float> values;
        values.reserve(values_to_plot.size());
        double average = 0.0;
        double peak = 0.0;
        float maximum = 1.0F;
        for (const auto &sample : values_to_plot) {
          values.push_back(static_cast<float>(
              std::min(sample.cpu_ms, static_cast<double>(std::numeric_limits<float>::max()))));
          average += (sample.cpu_ms - average) / static_cast<double>(values.size());
          peak = std::max(peak, sample.cpu_ms);
          maximum = std::max(maximum, values.back());
        }
        ImGui::Text("Latest %.2f ms | Average %.2f ms | Peak %.2f ms", values_to_plot.back().cpu_ms,
                    average, peak);
        ImGui::PlotLines(label, values.data(), static_cast<int>(values.size()), 0, nullptr, 0.0F,
                         maximum, ImVec2(-1.0F, 120.0F));
      };
      plot(samples, "Frame processing (ms)");
      ImGui::SeparatorText("Completed native GPU timing (live)");
      const auto gpu = state_->profile_gpu;
      const auto gpu_source_name = [](GpuProfileSource source) {
        if (source == GpuProfileSource::VulkanTimestamps)
          return "Vulkan timestamp queries";
        if (source == GpuProfileSource::Dx12Timestamps)
          return "DX12 timestamp queries";
        if (source == GpuProfileSource::MetalCommandBuffer)
          return "Metal command-buffer timings";
        return "Unavailable";
      };
      const auto gpu_position = [&](std::size_t index) {
        const auto low = ImGui::GetItemRectMin(), high = ImGui::GetItemRectMax();
        state_->gpu_control_positions[index] =
            std::array{(low.x + high.x) * 0.5F, (low.y + high.y) * 0.5F};
      };
      ImGui::BeginDisabled(!workspace || !workspace->Writable() || interaction_blocked ||
                           state_->close_prompt_requested || profile->GpuSamples().empty());
      if (ImGui::Button("Export GPU"))
        state_->gpu_export_requested = true;
      gpu_position(0);
      ImGui::EndDisabled();
      ImGui::SameLine();
      ImGui::BeginDisabled(!workspace || interaction_blocked || state_->close_prompt_requested);
      if (ImGui::Button("Import GPU"))
        state_->gpu_import_requested = true;
      gpu_position(1);
      ImGui::SameLine();
      ImGui::BeginDisabled(!state_->imported_gpu);
      if (ImGui::Button("Clear GPU import"))
        state_->imported_gpu.reset();
      gpu_position(2);
      ImGui::EndDisabled();
      ImGui::EndDisabled();
      ImGui::Text("Source: %s%s", gpu_source_name(gpu.source),
                  gpu.software_rasterizer ? " (software rasterizer)" : "");
      if (gpu.milliseconds)
        ImGui::Text("Completed submission %llu: %.3f ms",
                    static_cast<unsigned long long>(gpu.completed_submission), *gpu.milliseconds);
      else
        ImGui::TextDisabled("Latest completed timing unavailable.");
      if (gpu.observed_peak_ms)
        ImGui::Text("Observed peak since Clear: %.3f ms", *gpu.observed_peak_ms);
      ImGui::Text("%zu GPU intervals retained | %llu older intervals dropped",
                  profile->GpuSamples().size(),
                  static_cast<unsigned long long>(profile->GpuDroppedCount()));
      ImGui::TextDisabled(
          "Native command-buffer interval; delayed completion, not display latency or CPU time.");
      ImGui::TextDisabled("Capture pauses recording; Clear starts a fresh GPU history.");
      const auto gpu_plot = [](std::span<const GpuProfileSample> gpu_samples, const char *label) {
        if (gpu_samples.empty())
          return;
        double maximum = 1;
        for (const auto &sample : gpu_samples)
          if (sample.milliseconds)
            maximum = std::max(maximum, *sample.milliseconds);
        const auto low = ImGui::GetCursorScreenPos();
        const ImVec2 size(std::max(1.0F, ImGui::GetContentRegionAvail().x), 90);
        ImGui::InvisibleButton(label, size);
        auto *draw = ImGui::GetWindowDrawList();
        draw->AddRectFilled(low, ImVec2(low.x + size.x, low.y + size.y),
                            ImGui::GetColorU32(ImGuiCol_FrameBg));
        const auto first = gpu_samples.front().submission;
        const double range =
            static_cast<double>(std::max<std::uint64_t>(1, gpu_samples.back().submission - first));
        std::optional<ImVec2> previous;
        for (const auto &sample : gpu_samples) {
          if (!sample.milliseconds) {
            previous.reset();
            continue;
          }
          const ImVec2 point(
              low.x + static_cast<float>(static_cast<double>(sample.submission - first) / range) *
                          (size.x - 1),
              low.y + (1 - static_cast<float>(*sample.milliseconds / maximum)) * (size.y - 1));
          if (previous)
            draw->AddLine(*previous, point, ImGui::GetColorU32(ImGuiCol_PlotLines));
          draw->AddCircleFilled(point, 2, ImGui::GetColorU32(ImGuiCol_PlotLines));
          previous = point;
        }
      };
      gpu_plot(profile->GpuSamples(), "GPU intervals###editor.profiler.gpu-plot");
      if (state_->imported_gpu) {
        const auto &imported = *state_->imported_gpu;
        ImGui::SeparatorText("Imported GPU timing (static)");
        ImGui::Text("Source: %s%s", gpu_source_name(imported.source),
                    imported.software_rasterizer ? " (software rasterizer)" : "");
        ImGui::Text("%zu intervals retained | %llu older intervals dropped",
                    imported.samples.size(),
                    static_cast<unsigned long long>(imported.older_samples_dropped));
        std::optional<double> peak;
        for (const auto &sample : imported.samples)
          if (sample.milliseconds)
            peak = std::max(peak.value_or(0), *sample.milliseconds);
        if (peak)
          ImGui::Text("Retained peak: %.3f ms", *peak);
        else
          ImGui::TextDisabled("Retained GPU timing unavailable.");
        ImGui::TextDisabled("Native command-buffer interval; export project is a destination.");
        gpu_plot(imported.samples, "Imported GPU intervals###editor.profiler.imported-gpu-plot");
      }
      ImGui::SeparatorText("Process resident memory (live)");
      const auto memory = state_->profile_memory;
      if (memory.resident_bytes)
        ImGui::Text("Latest %llu bytes", static_cast<unsigned long long>(*memory.resident_bytes));
      else
        ImGui::TextDisabled("Latest unavailable: no successful current observation.");
      if (memory.observed_peak_bytes)
        ImGui::Text("Observed peak %llu bytes",
                    static_cast<unsigned long long>(*memory.observed_peak_bytes));
      ImGui::TextDisabled("Current process RSS / working set, including shared resident pages.");
      ImGui::TextDisabled(
          "250 ms sampling; Capture pauses observations; Clear resets observed peak.");
      ImGui::TextDisabled("Process-wide across projects; excludes GPU/allocator accounting.");
      const auto memory_plot = [](std::span<const ProcessMemorySample> history,
                                  std::uint64_t dropped, const char *label) {
        ImGui::Text("%zu memory attempts retained | %llu older attempts dropped", history.size(),
                    static_cast<unsigned long long>(dropped));
        if (history.empty())
          return;
        std::optional<double> retained_peak;
        std::size_t unavailable = 0;
        for (const auto &sample : history) {
          if (sample.resident_bytes)
            retained_peak =
                std::max(retained_peak.value_or(0), static_cast<double>(*sample.resident_bytes));
          else
            ++unavailable;
        }
        ImGui::Text("%.0f ms since first observation | %zu unavailable attempts",
                    history.back().elapsed_ms, unavailable);
        if (retained_peak)
          ImGui::Text("%s | peak in retained samples %.2f MiB", label,
                      *retained_peak / (1024 * 1024));
        else
          ImGui::TextDisabled("%s | retained peak unavailable", label);
        const double maximum = std::max(1.0, retained_peak.value_or(0));
        ImGui::TextDisabled(
            "Horizontal axis: elapsed time, including pause gaps. Missing reads break the line.");
        ImGui::PushID(label);
        const auto low = ImGui::GetCursorScreenPos();
        const ImVec2 size(std::max(1.0F, ImGui::GetContentRegionAvail().x), 90);
        ImGui::InvisibleButton("memory-plot", size);
        auto *draw = ImGui::GetWindowDrawList();
        draw->AddRectFilled(low, ImVec2(low.x + size.x, low.y + size.y),
                            ImGui::GetColorU32(ImGuiCol_FrameBg));
        const double first = history.front().elapsed_ms;
        const double range = std::max(1.0, history.back().elapsed_ms - first);
        std::optional<ImVec2> previous;
        for (const auto &sample : history) {
          if (!sample.resident_bytes) {
            previous.reset();
            continue;
          }
          const ImVec2 point(
              low.x + static_cast<float>((sample.elapsed_ms - first) / range) * (size.x - 1),
              low.y +
                  (1 - static_cast<float>(static_cast<double>(*sample.resident_bytes) / maximum)) *
                      (size.y - 1));
          if (previous)
            draw->AddLine(*previous, point, ImGui::GetColorU32(ImGuiCol_PlotLines));
          draw->AddCircleFilled(point, 2, ImGui::GetColorU32(ImGuiCol_PlotLines));
          previous = point;
        }
        ImGui::PopID();
      };
      memory_plot(profile->MemorySamples(), profile->MemoryDroppedCount(), "Resident memory (MiB)");
      if (state_->imported_memory) {
        ImGui::SeparatorText("Imported process memory (static)");
        ImGui::TextDisabled("Process-wide trace; project identity denotes export destination, not "
                            "allocation ownership.");
        memory_plot(state_->imported_memory->samples,
                    state_->imported_memory->older_samples_dropped,
                    "Imported resident memory (MiB)");
      }
      if (state_->imported_profile) {
        ImGui::SeparatorText("Imported capture (static)");
        ImGui::Text(
            "%zu saved frames | %llu older frames dropped",
            state_->imported_profile->samples.size(),
            static_cast<unsigned long long>(state_->imported_profile->older_frames_dropped));
        ImGui::TextDisabled(
            "Saved Editor wall timing; GPU/memory unavailable. CSV has no project provenance.");
        plot(state_->imported_profile->samples, "Imported frame processing (ms)");
      }
    }
  }
  ImGui::End();
  if (content != nullptr)
    DrawContentBrowser(*state_, *content, imports, scene, meshes, scene_editable,
                       file_context_valid && state_->app_focused && !game_running &&
                           !interaction_blocked && !file_busy && !state_->content_rename_target,
                       state_->app_focused && !external_modal_open && !rename_open &&
                           (!workspace || workspace->Writable()),
                       state_->app_focused && !interaction_blocked && !state_->game_input_focused);
  DrawProjectPanel(*state_, workspace, recent_projects);
  if (std::exchange(state_->game_input_binding_pending, false))
    ImGui::OpenPopup("Game input bindings###game.input.binding-dialog");
  ImGui::SetNextWindowSize({620.0F, 440.0F}, ImGuiCond_Always);
  if (ImGui::BeginPopupModal("Game input bindings###game.input.binding-dialog", nullptr,
                             ImGuiWindowFlags_NoResize)) {
    if (!state_->game_input_binding_open) {
      ImGui::ClearActiveID();
      ImGui::CloseCurrentPopup();
    } else {
      ImGui::TextWrapped("Apply uses this profile for the current session. Apply and save also "
                         "stores it in this project for the next open.");
      constexpr const char *actions[] = {"Left (-X)",     "Right (+X)", "Forward (+Y)",
                                         "Back (-Y)",     "Action (1)", "Primary (2)",
                                         "Secondary (4)", "Sprint (8)", "Modifier (16)"};
      const auto label = [](PlayInputControl control) -> std::string {
        if (control >= PlayInputControl::A && control <= PlayInputControl::Z)
          return std::string(1, static_cast<char>('A' + static_cast<int>(control) -
                                                  static_cast<int>(PlayInputControl::A)));
        constexpr const char *names[] = {"Left arrow", "Right arrow", "Up arrow",    "Down arrow",
                                         "Space",      "Left Shift",  "Right Shift", "Left Ctrl",
                                         "Right Ctrl", "Left mouse",  "Right mouse"};
        if (control >= PlayInputControl::Left && control < PlayInputControl::Count)
          return names[static_cast<std::size_t>(control) -
                       static_cast<std::size_t>(PlayInputControl::Left)];
        return "Unbound";
      };
      const auto capture = [&](std::size_t index) {
        const auto min = ImGui::GetItemRectMin(), max = ImGui::GetItemRectMax();
        state_->game_input_binding_positions[index] =
            std::array{(min.x + max.x) * 0.5F, (min.y + max.y) * 0.5F};
      };
      if (ImGui::BeginTable("##input-bindings", 3, ImGuiTableFlags_SizingStretchSame)) {
        ImGui::TableSetupColumn("Action");
        ImGui::TableSetupColumn("Primary");
        ImGui::TableSetupColumn("Alternate");
        ImGui::TableHeadersRow();
        for (std::size_t action = 0; action < PlayInputBindings::kActions; ++action) {
          ImGui::TableNextRow();
          ImGui::TableNextColumn();
          ImGui::TextUnformatted(actions[action]);
          for (std::size_t slot = 0; slot < 2; ++slot) {
            ImGui::TableNextColumn();
            ImGui::PushID(static_cast<int>(action * 2 + slot));
            auto &binding = state_->game_input_draft.controls[action][slot];
            ImGui::SetNextItemWidth(-1);
            const bool open = ImGui::BeginCombo("##control", label(binding).c_str());
            capture(1 + action * 2 + slot);
            if (open) {
              const int count = static_cast<int>(action < 4 ? PlayInputControl::MouseLeft
                                                            : PlayInputControl::Count);
              ImGuiListClipper controls;
              controls.Begin(count);
              while (controls.Step())
                for (int value = controls.DisplayStart; value < controls.DisplayEnd; ++value) {
                  const auto control = static_cast<PlayInputControl>(value);
                  if (ImGui::Selectable(label(control).c_str(), binding == control)) {
                    binding = control;
                    state_->game_input_binding_error.clear();
                  }
                  const auto min = ImGui::GetItemRectMin(), max = ImGui::GetItemRectMax();
                  state_->game_input_choice_positions.push_back(
                      {control, {(min.x + max.x) * 0.5F, (min.y + max.y) * 0.5F}});
                }
              ImGui::EndCombo();
            }
            ImGui::PopID();
          }
        }
        ImGui::EndTable();
      }
      if (!state_->game_input_binding_error.empty())
        ImGui::TextWrapped("%s", state_->game_input_binding_error.c_str());
      if (ImGui::Button("Apply")) {
        if (state_->game_input_draft.Valid()) {
          state_->game_input_bindings = state_->game_input_draft;
          state_->game_input_binding_status =
              "Session bindings applied; project settings unchanged.";
          state_->game_input_binding_open = false;
          ImGui::CloseCurrentPopup();
        } else
          state_->game_input_binding_error = "Each control can be bound only once. Choose Unbound "
                                             "to release a duplicate.";
      }
      capture(19);
      ImGui::SameLine();
      ImGui::BeginDisabled(!workspace || !workspace->Writable() || recovery_available);
      if (ImGui::Button("Apply and save")) {
        if (state_->game_input_draft.Valid()) {
          state_->game_input_bindings = state_->game_input_draft;
          state_->game_input_binding_status = "Saving project input bindings...";
          state_->game_input_save_requested = GameInputBindingsSaveRequest{
              workspace->Project().id, workspace->Root(), state_->game_input_draft};
          state_->game_input_binding_open = false;
          ImGui::CloseCurrentPopup();
        } else
          state_->game_input_binding_error = "Each control can be bound only once. Choose Unbound "
                                             "to release a duplicate.";
      }
      capture(22);
      ImGui::EndDisabled();
      ImGui::SameLine();
      if (ImGui::Button("Reset defaults")) {
        state_->game_input_draft = {};
        state_->game_input_binding_error.clear();
      }
      capture(20);
      ImGui::SameLine();
      if (ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
        state_->game_input_binding_open = false;
        ImGui::CloseCurrentPopup();
      }
      capture(21);
    }
    ImGui::EndPopup();
  }
  if (state_->focus_initial_scene) {
    auto *initial_scene_window = ImGui::FindWindowByName(
        PanelWindowName(game_running ? "nexora.game" : "nexora.scene").c_str());
    if (initial_scene_window != nullptr && initial_scene_window->DockNode != nullptr &&
        initial_scene_window->DockNode->TabBar != nullptr) {
      initial_scene_window->DockNode->SelectedTabId = initial_scene_window->TabId;
      initial_scene_window->DockNode->TabBar->SelectedTabId = initial_scene_window->TabId;
      initial_scene_window->DockNode->TabBar->NextSelectedTabId = initial_scene_window->TabId;
      state_->focus_initial_scene = false;
    }
  }
  if (state_->focus_initial_content) {
    const auto content_window_name = PanelWindowName("nexora.content");
    auto *content_window = ImGui::FindWindowByName(content_window_name.c_str());
    if (content_window != nullptr && content_window->DockNode != nullptr &&
        content_window->DockNode->TabBar != nullptr) {
      content_window->DockNode->SelectedTabId = content_window->TabId;
      content_window->DockNode->TabBar->SelectedTabId = content_window->TabId;
      content_window->DockNode->TabBar->NextSelectedTabId = content_window->TabId;
      state_->focus_initial_content = false;
    }
  }

  if (state_->play_apply_popup_pending) {
    ImGui::OpenPopup("Apply Play transforms###editor.play-apply");
    state_->play_apply_popup_pending = false;
  }
  const auto review_work_size = ImGui::GetMainViewport()->WorkSize;
  if (ImGui::IsPopupOpen("Apply Play transforms###editor.play-apply"))
    ImGui::SetNextWindowSizeConstraints({0, 0}, {std::max(review_work_size.x - 24.0F, 1.0F),
                                                 std::max(review_work_size.y - 24.0F, 1.0F)});
  if (ImGui::BeginPopupModal("Apply Play transforms###editor.play-apply", nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    if (!game_running || !state_->play_apply_open || recovery_available) {
      state_->play_apply_open = false;
      state_->play_apply_review = {};
      ImGui::CloseCurrentPopup();
    } else {
      const auto &review = state_->play_apply_review;
      const bool conflicts =
          std::ranges::any_of(review.diffs, &runtime::TransformApplyDiff::conflict);
      ImGui::Text("%zu changed transforms", review.diffs.size());
      ImGui::TextWrapped("%s", conflicts ? "Conflicts must be resolved before applying"
                                         : "Review before applying");
      ImGui::TextWrapped("Only transforms are copied. Apply stops Play and creates one Undo step.");
      ImGui::BeginChild("##play-transform-diffs",
                        {std::max(1.0F, std::min(700.0F, review_work_size.x - 64.0F)),
                         std::max(40.0F, std::min(300.0F, review_work_size.y - 170.0F))},
                        true, ImGuiWindowFlags_HorizontalScrollbar);
      ImGuiListClipper clipper;
      clipper.Begin(static_cast<int>(review.diffs.size()),
                    ImGui::GetTextLineHeightWithSpacing() * 10);
      while (clipper.Step())
        for (int index = clipper.DisplayStart; index < clipper.DisplayEnd; ++index) {
          const auto &diff = review.diffs[static_cast<std::size_t>(index)];
          ImGui::Text("Entity #%llu%s", static_cast<unsigned long long>(diff.entity),
                      diff.conflict ? " | CONFLICT" : "");
          const auto pose = [](const char *label, const runtime::Transform &value) {
            ImGui::Text("%s position: %.17g, %.17g, %.17g", label, value.x, value.y, value.z);
            ImGui::Text("%s rotation: %.17g, %.17g, %.17g, %.17g", label, value.qx, value.qy,
                        value.qz, value.qw);
            ImGui::Text("%s scale: %.17g, %.17g, %.17g", label, value.sx, value.sy, value.sz);
          };
          pose("Original", diff.original);
          pose("Editor", diff.editor);
          pose("Play", diff.runtime);
        }
      ImGui::EndChild();
      ImGui::BeginDisabled(conflicts || review.diffs.empty() ||
                           play->State() != runtime::PlayState::Paused || !workspace || !writable ||
                           state_->close_prompt_requested);
      if (ImGui::Button("Apply and Stop")) {
        state_->play_apply_requested = state_->play_apply_review;
        state_->play_apply_open = false;
        ImGui::CloseCurrentPopup();
      }
      const auto confirm_min = ImGui::GetItemRectMin();
      const auto confirm_max = ImGui::GetItemRectMax();
      state_->play_apply_confirm_position = std::array{(confirm_min.x + confirm_max.x) * 0.5F,
                                                       (confirm_min.y + confirm_max.y) * 0.5F};
      ImGui::EndDisabled();
      ImGui::SameLine();
      if (ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        state_->play_apply_open = false;
        state_->play_apply_review = {};
        ImGui::CloseCurrentPopup();
      }
    }
    ImGui::EndPopup();
  }

  if (recovery_available && !state_->recovery_prompt_opened) {
    ImGui::OpenPopup("Recover workspace###editor.recovery");
    state_->recovery_prompt_opened = true;
  }
  if (!recovery_available)
    state_->recovery_prompt_opened = false;
  if (ImGui::BeginPopupModal("Recover workspace###editor.recovery", nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::TextUnformatted("A recovery journal is available.");
    if (!state_->recovery_error.empty())
      ImGui::TextWrapped("%s", state_->recovery_error.c_str());
    if (ImGui::Button("Recover")) {
      if (ApplyRecoveryChoice(*workspace, RecoveryChoice::Recover))
        ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Discard recovery data")) {
      if (ApplyRecoveryChoice(*workspace, RecoveryChoice::Discard))
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }
  if (state_->close_prompt_requested) {
    ImGui::OpenPopup("Unsaved scene###editor.close");
    state_->close_prompt_requested = false;
  }
  if (ImGui::BeginPopupModal("Unsaved scene###editor.close", nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    if (state_->scene_file_close_popup) {
      state_->scene_file_close_popup = false;
      ImGui::CloseCurrentPopup();
    } else {
      ImGui::TextUnformatted(state_->scene_tabs.empty()
                                 ? "Save scene changes before closing?"
                                 : "Save all owned scene changes before closing?");
      if (ImGui::Button("Save and Exit"))
        state_->close_choice = CloseChoice::SaveAndExit;
      ImGui::SameLine();
      if (ImGui::Button("Discard and Exit"))
        state_->close_choice = CloseChoice::DiscardAndExit;
      ImGui::SameLine();
      if (ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
        state_->close_choice = CloseChoice::Cancel;
        ImGui::CloseCurrentPopup();
      }
      if (!state_->scene_save_success && !state_->scene_save_message.empty())
        ImGui::TextWrapped("%s", state_->scene_save_message.c_str());
    }
    ImGui::EndPopup();
  }
  DrawSceneFileDialog(
      *state_, scene, writable,
      recovery_available || state_->play_apply_open || !file_context_valid ||
          (close_confirmation_open && !state_->scene_file_close_popup &&
           !state_->scene_file_intent.value_or(SceneFileRequest{}).close_after_save));
  DrawSceneTabDialog(*state_, workspace && workspace->Writable(),
                     !tab_context_valid || recovery_available || state_->play_apply_open ||
                         close_confirmation_open || game_running ||
                         state_->scene_file_dialog != State::FileDialog::None ||
                         state_->scene_file_output || state_->game_input_binding_open);
}

bool EditorImGuiHost::SetSceneTabs(std::span<const SceneTabItem> items, std::uint64_t active,
                                   bool busy) {
  if (items.size() > 16 || (items.empty() && active))
    return false;
  const auto selected = std::ranges::find(items, active, &SceneTabItem::id);
  if (!items.empty() && selected == items.end())
    return false;
  std::unordered_set<std::uint64_t> ids, generations;
  for (const auto &item : items) {
    if (!item.id || !item.token.document_generation || item.token.project == foundation::Uuid{} ||
        item.token.project != selected->token.project || !ids.insert(item.id).second ||
        !generations.insert(item.token.document_generation).second || item.label.empty() ||
        item.label.size() > 256 || !foundation::IsValidUtf8(item.label) ||
        std::ranges::any_of(item.label, [](unsigned char c) { return c < 32 || c == 127; }))
      return false;
    if (item.path) {
      const auto bytes = item.path->generic_u8string();
      if (bytes.empty() || bytes.size() >= 1024 ||
          !foundation::IsValidUtf8(
              std::string_view(reinterpret_cast<const char *>(bytes.data()), bytes.size())))
        return false;
    }
  }
  std::vector<SceneTabItem> next(items.begin(), items.end());
  const auto source = items.empty() ? SceneFileToken{} : selected->token;
  const auto live_target = [&](const std::optional<SceneTabRequest> &request) {
    if (!request || !request->target)
      return true;
    const auto target = std::ranges::find(items, request->target, &SceneTabItem::id);
    return target != items.end() && target->token == request->target_token;
  };
  if (source != state_->scene_tab_source || !live_target(state_->scene_tab_close) ||
      !live_target(state_->scene_tab_output)) {
    CancelSceneGestures(*state_);
    CancelInspectorDrafts(*state_);
    state_->scene_tab_dialog = 0;
    state_->scene_tab_close.reset();
    state_->scene_tab_output.reset();
    state_->scene_tab_popup_pending = false;
    state_->scene_tab_path = {};
    state_->scene_tab_status.clear();
    state_->scene_save_requested = false;
  }
  state_->scene_tabs = std::move(next);
  state_->scene_tab_active = active;
  state_->scene_tab_source = source;
  state_->scene_tabs_busy = busy;
  return true;
}
std::optional<SceneTabRequest> EditorImGuiHost::TakeSceneTabRequest() {
  return std::exchange(state_->scene_tab_output, std::nullopt);
}
void EditorImGuiHost::SetSceneTabStatus(std::string message, bool success) {
  state_->scene_tab_status = message.size() <= 4096 && foundation::IsValidUtf8(message)
                                 ? std::move(message)
                                 : "The scene action returned an invalid diagnostic.";
  if (success) {
    state_->scene_tab_dialog = 0;
    state_->scene_tab_close.reset();
    state_->scene_tab_popup_pending = false;
  }
}

bool EditorImGuiHost::TakeSceneSaveRequest() noexcept {
  // ImGui can trickle a queued mouse release into a later frame than a shortcut.
  // Retain Save until the authoring gesture has committed or been cancelled.
  if (state_->native_scene_drag_origin || state_->scene_drag)
    return false;
  return std::exchange(state_->scene_save_requested, false);
}

void EditorImGuiHost::SetSceneSaveResult(std::string message, bool success) {
  state_->scene_save_message = std::move(message);
  state_->scene_save_success = success;
}

void EditorImGuiHost::SetSceneFileContext(SceneFileToken token,
                                          std::optional<std::filesystem::path> path,
                                          bool save_blocked) {
  if (state_->scene_file_context && state_->scene_file_token != token) {
    state_->static_export_request.reset();
    state_->static_export = {};
    state_->static_export_busy = false;
    state_->scene_file_intent.reset();
    state_->scene_file_output.reset();
    state_->scene_file_dialog = State::FileDialog::None;
    state_->scene_file_popup_pending = false;
    state_->scene_file_save_before_switch = false;
    state_->scene_file_close_popup = false;
  }
  state_->scene_file_context = true;
  state_->scene_file_token = token;
  state_->scene_file_path = std::move(path);
  state_->scene_file_save_blocked = save_blocked;
}
std::optional<SceneFileRequest> EditorImGuiHost::TakeSceneFileRequest() {
  return std::exchange(state_->scene_file_output, std::nullopt);
}
void EditorImGuiHost::RequestSceneSaveAs(bool close_after_save,
                                         std::optional<std::filesystem::path> suggested_path) {
  Activate(state_->context);
  CancelSceneGestures(*state_);
  CancelInspectorDrafts(*state_);
  ImGui::ClearActiveID();
  state_->scene_file_intent = SceneFileRequest{SceneFileAction::SaveAs, state_->scene_file_token};
  state_->scene_file_intent->close_after_save = close_after_save;
  state_->scene_file_close_popup = close_after_save;
  state_->scene_file_save_before_switch = false;
  SetSceneFileDraft(*state_);
  if (suggested_path) {
    const auto text = PathLabel(*suggested_path);
    std::snprintf(state_->scene_file_text.data(), state_->scene_file_text.size(), "%s",
                  text.c_str());
  }
  state_->scene_file_dialog = State::FileDialog::SavePath;
  state_->scene_file_focus_path = true;
  state_->scene_file_popup_pending = true;
}
void EditorImGuiHost::RequestSceneOverwrite(SceneFileRequest request) {
  Activate(state_->context);
  CancelSceneGestures(*state_);
  CancelInspectorDrafts(*state_);
  ImGui::ClearActiveID();
  state_->scene_file_close_popup = request.close_after_save;
  state_->scene_file_intent = std::move(request);
  state_->scene_file_dialog = State::FileDialog::Overwrite;
  state_->scene_file_popup_pending = true;
}

void EditorImGuiHost::RequestSceneUnsavedChoice(SceneFileRequest request) {
  state_->scene_file_intent = std::move(request);
  state_->scene_file_dialog = State::FileDialog::Unsaved;
  state_->scene_file_popup_pending = true;
}

void EditorImGuiHost::RequestCloseConfirmation() noexcept {
  if (state_->game_input_save_requested)
    state_->game_input_binding_status.clear();
  state_->game_input_save_requested.reset();
  state_->game_input_binding_open = state_->game_input_binding_pending = false;
  state_->scene_file_close_popup = false;
  state_->scene_file_intent.reset();
  state_->scene_file_output.reset();
  state_->scene_file_dialog = State::FileDialog::None;
  state_->scene_file_popup_pending = false;
  state_->play_apply_open = false;
  state_->play_apply_popup_pending = false;
  state_->game_input_focused = false;
  state_->close_prompt_requested = true;
}

CloseChoice EditorImGuiHost::TakeCloseChoice() noexcept {
  return std::exchange(state_->close_choice, CloseChoice::None);
}

bool EditorImGuiHost::GameInputFocused() const noexcept { return state_->game_input_focused; }
PlayInputBindings EditorImGuiHost::GameInputBindings() const noexcept {
  return state_->game_input_bindings;
}
bool EditorImGuiHost::SetGameInputBindings(const PlayInputBindings &bindings,
                                           const ProjectWorkspace &workspace) {
  if (!bindings.Valid() || workspace.Root().empty())
    return false;
  state_->game_input_bindings = bindings;
  state_->game_input_binding_status.clear();
  state_->game_input_root = workspace.Root();
  state_->game_input_project = workspace.Project().id;
  state_->game_input_binding_open = state_->game_input_binding_pending = false;
  state_->game_input_save_requested.reset();
  return true;
}
std::optional<GameInputBindingsSaveRequest> EditorImGuiHost::TakeGameInputBindingsSaveRequest() {
  return std::exchange(state_->game_input_save_requested, std::nullopt);
}
void EditorImGuiHost::SetGameInputBindingsStatus(std::string message) {
  state_->game_input_binding_status = std::move(message);
}
std::string_view EditorImGuiHost::GameplayLibrary() const noexcept {
  return state_->gameplay_library.data();
}
void EditorImGuiHost::SetGameplayLibrary(std::string_view library,
                                         std::uint64_t project_generation) {
  if (state_->game_was_running)
    return;
  if (project_generation != 0)
    state_->gameplay_project_generation = project_generation;
  const auto count = std::min(library.size(), state_->gameplay_library.size() - 1);
  if (count)
    std::memcpy(state_->gameplay_library.data(), library.data(), count);
  state_->gameplay_library[count] = 0;
}
void EditorImGuiHost::SetGameplayStatus(std::string message) {
  state_->gameplay_status = std::move(message);
}

std::optional<PlayTransformReview> EditorImGuiHost::TakePlayApplyRequest() {
  return std::exchange(state_->play_apply_requested, std::nullopt);
}

bool EditorImGuiHost::TakeProfileExportRequest() noexcept {
  return std::exchange(state_->profile_export_requested, false);
}
bool EditorImGuiHost::TakeProfileJsonExportRequest() noexcept {
  return std::exchange(state_->profile_json_export_requested, false);
}
bool EditorImGuiHost::TakeProfileCsvImportRequest() noexcept {
  return std::exchange(state_->profile_csv_import_requested, false);
}
bool EditorImGuiHost::TakeProfileJsonImportRequest() noexcept {
  return std::exchange(state_->profile_json_import_requested, false);
}
bool EditorImGuiHost::SetImportedProfileCapture(FrameProcessingCapture capture) {
  if (state_->profile_project_root.empty() || capture.samples.empty() ||
      capture.samples.size() > 600)
    return false;
  std::uint64_t previous = 0;
  for (const auto &sample : capture.samples) {
    if (sample.frame == 0 || sample.frame <= previous || !std::isfinite(sample.cpu_ms) ||
        sample.cpu_ms < 0 || sample.gpu_ms != 0 || sample.memory_bytes != 0)
      return false;
    previous = sample.frame;
  }
  state_->imported_profile = std::move(capture);
  return true;
}
bool EditorImGuiHost::TakeMemoryExportRequest() noexcept {
  return std::exchange(state_->memory_export_requested, false);
}
bool EditorImGuiHost::TakeMemoryImportRequest() noexcept {
  return std::exchange(state_->memory_import_requested, false);
}
bool EditorImGuiHost::SetImportedMemoryCapture(ProcessMemoryCapture capture) {
  if (state_->profile_project_root.empty() || !ValidateProcessMemorySamples(capture.samples))
    return false;
  state_->imported_memory = std::move(capture);
  return true;
}
bool EditorImGuiHost::TakeGpuExportRequest() noexcept {
  return std::exchange(state_->gpu_export_requested, false);
}
bool EditorImGuiHost::TakeGpuImportRequest() noexcept {
  return std::exchange(state_->gpu_import_requested, false);
}
bool EditorImGuiHost::SetImportedGpuCapture(GpuTimingCapture capture) {
  if (state_->profile_project_root.empty() ||
      !ValidateGpuTimingSamples(capture.source, capture.samples))
    return false;
  state_->imported_gpu = std::move(capture);
  return true;
}
void EditorImGuiHost::SetProfileExportStatus(std::string message) {
  state_->profile_export_status = std::move(message);
}

std::optional<StaticExportRequest> EditorImGuiHost::TakeStaticExportRequest() {
  return std::exchange(state_->static_export_request, std::nullopt);
}
void EditorImGuiHost::SetStaticExportStatus(StaticExportSnapshot snapshot, bool busy) {
  if (snapshot.operation &&
      (!state_->scene_file_context || snapshot.source_project != state_->scene_file_token.project ||
       snapshot.document_generation != state_->scene_file_token.document_generation)) {
    snapshot = {};
    if (busy)
      snapshot.message = "A previous-scene export is pending; cancel or wait for its state check.";
  }
  if (snapshot.message.size() > 1024 || snapshot.relative_path.size() > 1024 ||
      snapshot.verify_command.size() > 1024 || snapshot.checksum.size() > 64 ||
      !foundation::IsValidUtf8(snapshot.message) ||
      !foundation::IsValidUtf8(snapshot.relative_path) ||
      !foundation::IsValidUtf8(snapshot.verify_command) ||
      !foundation::IsValidUtf8(snapshot.checksum))
    return;
  state_->static_export = std::move(snapshot);
  state_->static_export_busy = busy;
}

PlayCommand EditorImGuiHost::TakePlayCommand() noexcept {
  return std::exchange(state_->play_command, PlayCommand::None);
}

std::uint32_t EditorImGuiHost::Render(nexora::rhi::Device &device,
                                      nexora::rhi::TextureHandle target, std::uint32_t width,
                                      std::uint32_t height, nexora::rhi::ResourceState before,
                                      bool prepare_for_present) {
  Activate(state_->context);
  const auto *draw = ImGui::GetDrawData();
  if (draw == nullptr || draw->CmdListsCount == 0 || width == 0 || height == 0)
    return 0;
  auto &renderer = state_->renderer;
  if (renderer.device != nullptr && renderer.device != &device)
    throw std::logic_error("Editor ImGui renderer is already bound to another device");
  renderer.device = &device;
  const auto completed = device.CompletedSubmissionValue();
  std::erase_if(renderer.retired_textures, [&](const auto &retired) {
    if (retired.completion > completed)
      return false;
    device.DestroyTexture(retired.texture);
    return true;
  });
  if (!renderer.pipeline.IsValid())
    renderer.pipeline =
        device.CreatePipeline({0x494d4755494c4159ULL, 0x494d475549534844ULL,
                               nexora::rhi::TextureFormat::Rgba8Unorm, "Editor ImGui"});
  if (!renderer.font_texture.IsValid() || renderer.font_generation != state_->font_generation) {
    if (renderer.font_texture.IsValid()) {
      std::uint64_t last_use = 0;
      for (const auto &slot : renderer.upload_slots)
        last_use = std::max(last_use, slot.completion);
      renderer.retired_textures.push_back({renderer.font_texture, last_use});
    }
    unsigned char *pixels = nullptr;
    int atlas_width = 0, atlas_height = 0;
    ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&pixels, &atlas_width, &atlas_height);
    renderer.font_texture = device.CreateTexture(
        {static_cast<std::uint32_t>(atlas_width), static_cast<std::uint32_t>(atlas_height),
         nexora::rhi::TextureFormat::Rgba8Unorm, nexora::rhi::ResourceState::ShaderRead,
         "Editor ImGui font atlas"});
    device.WriteTextureRgba8(renderer.font_texture,
                             std::span{reinterpret_cast<const std::byte *>(pixels),
                                       static_cast<std::size_t>(atlas_width) * atlas_height * 4U},
                             static_cast<std::uint32_t>(atlas_width) * 4U);
    renderer.textures[0].texture = renderer.font_texture;
    renderer.textures[0].live = true;
    ImGui::GetIO().Fonts->SetTexID(static_cast<ImTextureID>(TextureId(0, 1)));
    renderer.font_generation = state_->font_generation;
    ++state_->renderer_metrics.font_rebuilds;
  }
  auto &upload = renderer.upload_slots[renderer.upload_slot];
  renderer.upload_slot = (renderer.upload_slot + 1) % renderer.upload_slots.size();
  if (upload.completion > device.CompletedSubmissionValue()) {
    device.WaitForSubmission(upload.completion);
    ++state_->renderer_metrics.completion_waits;
  }
  std::size_t total_vertex_bytes = 0, total_index_bytes = 0;
  for (int index = 0; index < draw->CmdListsCount; ++index) {
    total_vertex_bytes +=
        static_cast<std::size_t>(draw->CmdLists[index]->VtxBuffer.Size) * sizeof(ImDrawVert);
    total_index_bytes +=
        static_cast<std::size_t>(draw->CmdLists[index]->IdxBuffer.Size) * sizeof(ImDrawIdx);
  }
  const auto ensure_buffer = [&](nexora::rhi::BufferHandle &buffer, std::uint64_t &capacity,
                                 std::uint64_t required, std::string_view name) {
    if (capacity >= required)
      return;
    if (buffer.IsValid())
      device.DestroyBuffer(buffer);
    capacity = GrownCapacity(required);
    buffer = device.CreateBuffer({capacity, std::string(name)});
    ++state_->renderer_metrics.buffer_reallocations;
  };
  ensure_buffer(upload.vertices, upload.vertex_capacity, total_vertex_bytes,
                "Editor ImGui vertex ring");
  ensure_buffer(upload.indices, upload.index_capacity, total_index_bytes,
                "Editor ImGui index ring");
  auto commands = device.CreateCommandList(nexora::rhi::QueueType::Graphics);
  commands->Transition({target, before, nexora::rhi::ResourceState::RenderTarget});
  commands->BeginRendering({target, width, height});
  commands->BindPipeline(renderer.pipeline);
  std::uint32_t submitted = 0;
  std::uint64_t vertex_offset_bytes = 0, index_offset_bytes = 0;
  for (int list_index = 0; list_index < draw->CmdListsCount; ++list_index) {
    const auto *list = draw->CmdLists[list_index];
    if (list->VtxBuffer.empty() || list->IdxBuffer.empty())
      continue;
    const auto vertex_bytes = std::as_bytes(
        std::span{list->VtxBuffer.Data, static_cast<std::size_t>(list->VtxBuffer.Size)});
    const auto index_bytes = std::as_bytes(
        std::span{list->IdxBuffer.Data, static_cast<std::size_t>(list->IdxBuffer.Size)});
    device.WriteBuffer(upload.vertices, vertex_offset_bytes, vertex_bytes);
    device.WriteBuffer(upload.indices, index_offset_bytes, index_bytes);
    commands->BindVertexBuffer(upload.vertices, vertex_offset_bytes);
    commands->BindIndexBuffer(upload.indices,
                              sizeof(ImDrawIdx) == 2 ? nexora::rhi::IndexFormat::Uint16
                                                     : nexora::rhi::IndexFormat::Uint32,
                              index_offset_bytes);
    for (const auto &draw_command : list->CmdBuffer) {
      if (draw_command.UserCallback == nullptr && draw_command.ElemCount > 0) {
        const auto texture_id = static_cast<std::uint64_t>(draw_command.GetTexID());
        const auto texture_index = static_cast<std::uint32_t>(texture_id);
        const auto texture_generation = static_cast<std::uint32_t>(texture_id >> 32U);
        nexora::rhi::TextureHandle texture = renderer.font_texture;
        if (texture_index < renderer.textures.size() && renderer.textures[texture_index].live &&
            renderer.textures[texture_index].generation == texture_generation) {
          texture = renderer.textures[texture_index].texture;
        } else {
          ++state_->renderer_metrics.rejected_textures;
        }
        commands->BindTexture(0, texture);
        const auto left = std::max(0.0F, (draw_command.ClipRect.x - draw->DisplayPos.x) *
                                             draw->FramebufferScale.x);
        const auto top = std::max(0.0F, (draw_command.ClipRect.y - draw->DisplayPos.y) *
                                            draw->FramebufferScale.y);
        const auto right =
            std::min(static_cast<float>(width),
                     (draw_command.ClipRect.z - draw->DisplayPos.x) * draw->FramebufferScale.x);
        const auto bottom =
            std::min(static_cast<float>(height),
                     (draw_command.ClipRect.w - draw->DisplayPos.y) * draw->FramebufferScale.y);
        if (right <= left || bottom <= top)
          continue;
        commands->SetScissor({static_cast<std::int32_t>(left), static_cast<std::int32_t>(top),
                              static_cast<std::uint32_t>(right - left),
                              static_cast<std::uint32_t>(bottom - top)});
        commands->DrawIndexed(draw_command.ElemCount, 1, draw_command.IdxOffset,
                              static_cast<std::int32_t>(draw_command.VtxOffset));
        ++submitted;
      }
    }
    vertex_offset_bytes += vertex_bytes.size();
    index_offset_bytes += index_bytes.size();
  }
  commands->EndRendering();
  commands->Transition({target, nexora::rhi::ResourceState::RenderTarget,
                        prepare_for_present ? nexora::rhi::ResourceState::Present
                                            : nexora::rhi::ResourceState::ShaderRead});
  upload.completion = device.Submit(*commands);
  ++state_->renderer_metrics.frames;
  state_->renderer_metrics.draw_calls += submitted;
  return submitted;
}

void EditorImGuiHost::ReleaseRenderer(nexora::rhi::Device &device) {
  auto &renderer = state_->renderer;
  if (renderer.device == nullptr)
    return;
  if (renderer.device != &device)
    throw std::logic_error("Editor ImGui renderer belongs to another device");
  device.WaitIdle();
  for (auto &upload : renderer.upload_slots) {
    if (upload.vertices.IsValid())
      device.DestroyBuffer(upload.vertices);
    if (upload.indices.IsValid())
      device.DestroyBuffer(upload.indices);
  }
  for (const auto &retired : renderer.retired_textures)
    device.DestroyTexture(retired.texture);
  if (renderer.font_texture.IsValid())
    device.DestroyTexture(renderer.font_texture);
  if (renderer.pipeline.IsValid())
    device.DestroyPipeline(renderer.pipeline);
  renderer = {};
}

RendererMetrics EditorImGuiHost::GetRendererMetrics() const noexcept {
  return state_->renderer_metrics;
}

std::uint64_t EditorImGuiHost::RegisterTexture(nexora::rhi::Device &device,
                                               nexora::rhi::TextureHandle texture) {
  auto &renderer = state_->renderer;
  if (!texture.IsValid() || (renderer.device != nullptr && renderer.device != &device) ||
      state_->next_texture_generation > std::numeric_limits<std::uint32_t>::max())
    return 0;
  const auto generation = static_cast<std::uint32_t>(state_->next_texture_generation++);
  renderer.device = &device;
  for (std::uint32_t index = 1; index < renderer.textures.size(); ++index) {
    auto &slot = renderer.textures[index];
    if (!slot.live) {
      slot.texture = texture;
      slot.generation = generation;
      slot.live = true;
      return TextureId(index, generation);
    }
  }
  if (renderer.textures.size() >= std::numeric_limits<std::uint32_t>::max())
    return 0;
  renderer.textures.push_back({texture, generation, true});
  return TextureId(static_cast<std::uint32_t>(renderer.textures.size() - 1), generation);
}

std::uint64_t EditorImGuiHost::RegisterNativeTexture(std::uint32_t width, std::uint32_t height,
                                                     std::span<const std::byte> pixels) {
  constexpr std::size_t maximum_bytes = 16U * 1024U * 1024U;
  if (width == 0 || height == 0 || width > 1024 || height > 1024 ||
      pixels.size() != static_cast<std::size_t>(width) * height * 4U ||
      pixels.size() > maximum_bytes - state_->native_texture_bytes ||
      state_->next_texture_generation > std::numeric_limits<std::uint32_t>::max())
    return 0;
  auto &textures = state_->native_textures;
  std::size_t index = 1;
  while (index < textures.size() && textures[index].generation != 0)
    ++index;
  if (index == 65)
    return 0;
  State::NativeTextureSlot slot{width,
                                height,
                                {pixels.begin(), pixels.end()},
                                static_cast<std::uint32_t>(state_->next_texture_generation),
                                0};
  if (index == textures.size())
    textures.push_back(std::move(slot));
  else
    textures[index] = std::move(slot);
  ++state_->next_texture_generation;
  state_->native_texture_bytes += pixels.size();
  return TextureId(static_cast<std::uint32_t>(index), textures[index].generation);
}

bool EditorImGuiHost::UnregisterTexture(std::uint64_t texture_id) noexcept {
  const auto index = static_cast<std::uint32_t>(texture_id);
  const auto generation = static_cast<std::uint32_t>(texture_id >> 32U);
  auto &native = state_->native_textures;
  if (index != 0 && index < native.size() && generation != 0 &&
      native[index].generation == generation) {
    state_->native_texture_bytes -= native[index].pixels.size();
    native[index] = {};
    return true;
  }
  auto &textures = state_->renderer.textures;
  if (index == 0 || index >= textures.size() || !textures[index].live ||
      textures[index].generation != generation)
    return false;
  textures[index].live = false;
  textures[index].texture = {};
  textures[index].generation = 0;
  return true;
}

std::string EditorImGuiHost::SaveLayout() const {
  Activate(state_->context);
  std::size_t size = 0;
  const char *contents = ImGui::SaveIniSettingsToMemory(&size);
  return {contents, size};
}

bool EditorImGuiHost::LoadLayout(std::string_view layout) {
  if (layout.empty() || layout.find("[Window]") == std::string_view::npos)
    return false;
  Activate(state_->context);
  ImGui::LoadIniSettingsFromMemory(layout.data(), layout.size());
  state_->initial_dock_layout_built = true;
  return true;
}

SceneOverviewCamera EditorImGuiHost::GetSceneOverviewCamera() const noexcept {
  return {state_->scene_center_world.x, state_->scene_center_world.y,
          state_->scene_pixels_per_unit};
}

bool EditorImGuiHost::SetSceneOverviewCamera(SceneOverviewCamera camera) noexcept {
  if (!std::isfinite(camera.x) || !std::isfinite(camera.z) ||
      !std::isfinite(camera.pixels_per_unit) ||
      std::abs(camera.x) > std::numeric_limits<float>::max() ||
      std::abs(camera.z) > std::numeric_limits<float>::max() || camera.pixels_per_unit < 4.0 ||
      camera.pixels_per_unit > 256.0)
    return false;
  state_->scene_center_world = {static_cast<float>(camera.x), static_cast<float>(camera.z)};
  state_->scene_pixels_per_unit = static_cast<float>(camera.pixels_per_unit);
  return true;
}

std::optional<Nexora::Presentation::SceneViewport>
EditorImGuiHost::NativeGameViewport() const noexcept {
  return state_->native_game_viewport;
}
runtime::Id EditorImGuiHost::GameCameraSelection() const noexcept {
  return state_->game_camera_selection;
}
void EditorImGuiHost::SetNativeGameStatus(std::string message, bool available) {
  state_->native_game_status = std::move(message);
  state_->native_game_available = available;
}

std::optional<Nexora::Presentation::SceneViewport>
EditorImGuiHost::SceneCanvasViewport() const noexcept {
  return state_->scene_canvas_viewport;
}

void EditorImGuiHost::SetNativeScenePreview(bool enabled) noexcept {
  if (state_->native_scene_preview != enabled)
    CancelSceneGestures(*state_);
  state_->native_scene_preview = enabled;
}

void EditorImGuiHost::SetNativeScenePreviewAvailable(bool available) noexcept {
  state_->native_scene_preview_available = available;
}

NativeSceneOrbit EditorImGuiHost::GetNativeSceneOrbit() const noexcept {
  return state_->native_scene_orbit;
}

NativeSceneTool EditorImGuiHost::GetNativeSceneTool() const noexcept {
  return state_->native_scene_tool;
}

bool EditorImGuiHost::NativeSceneLocalAxes() const noexcept {
  return state_->native_scene_local_axes;
}

bool EditorImGuiHost::NativeSceneCenterPivot() const noexcept {
  return state_ && state_->native_scene_center_pivot;
}

bool EditorImGuiHost::SetNativeSceneOrbit(NativeSceneOrbit orbit) noexcept {
  if (!std::isfinite(orbit.yaw) || !std::isfinite(orbit.pitch) || !std::isfinite(orbit.distance) ||
      !std::isfinite(orbit.target_y) || std::abs(orbit.target_y) > 100000.0 || orbit.pitch < 0.1 ||
      orbit.pitch > 1.45 || orbit.distance < 2.0 || orbit.distance > 100.0)
    return false;
  orbit.yaw = std::remainder(orbit.yaw, 6.283185307179586);
  state_->native_scene_orbit = orbit;
  return true;
}

std::optional<Nexora::Presentation::SceneViewport>
EditorImGuiHost::NativeScenePreviewViewport() const noexcept {
  return state_->native_scene_preview ? state_->scene_canvas_viewport : std::nullopt;
}

std::optional<NativeScenePickRequest> EditorImGuiHost::NativeScenePick() const noexcept {
  return state_->native_scene_pick;
}

std::optional<SceneFileToken> EditorImGuiHost::TakeNativeSceneFrameAllRequest() noexcept {
  return std::exchange(state_->native_scene_frame_all_request, std::nullopt);
}

std::optional<SceneFileToken> EditorImGuiHost::TakeNativeSceneSelectAllRequest() noexcept {
  const auto request = std::exchange(state_->native_scene_select_all_request, std::nullopt);
  if (!request || state_->scene_frame_token != request || !state_->app_focused ||
      !NativeScenePreviewViewport() || !state_->native_scene_preview_available ||
      state_->native_scene_drag_origin || state_->native_scene_drag ||
      state_->close_prompt_requested || state_->play_apply_open ||
      state_->hierarchy_rename_target || state_->content_rename_target ||
      state_->scene_file_dialog != State::FileDialog::None || state_->scene_file_output ||
      (state_->scene_file_context && state_->scene_file_token != request))
    return std::nullopt;
  return request;
}

bool EditorImGuiHost::ApplyNativeSceneFrameAll(SceneFileToken token,
                                               std::span<const PickCandidate> candidates) noexcept {
  const auto intent = std::exchange(state_->native_scene_frame_all_apply, std::nullopt);
  state_->native_scene_frame_all_request.reset();
  const auto viewport = NativeScenePreviewViewport();
  if (!intent || *intent != token || state_->scene_frame_token != token || !state_->app_focused ||
      !viewport || viewport->width == 0 || viewport->height == 0 ||
      state_->native_scene_drag_origin || state_->native_scene_drag ||
      state_->close_prompt_requested || state_->play_apply_open ||
      state_->hierarchy_rename_target || state_->content_rename_target ||
      state_->scene_file_dialog != State::FileDialog::None || state_->scene_file_output ||
      (state_->scene_file_context && state_->scene_file_token != token) ||
      candidates.size() > kMaximumNativeSceneFrameCandidates)
    return false;
  const auto infinity = std::numeric_limits<double>::infinity();
  std::array<double, 3> minimum{infinity, infinity, infinity},
      maximum{-infinity, -infinity, -infinity};
  for (const auto &candidate : candidates) {
    if (!candidate.visible)
      continue;
    const std::array low{candidate.min.x, candidate.min.y, candidate.min.z};
    const std::array high{candidate.max.x, candidate.max.y, candidate.max.z};
    for (std::size_t axis = 0; axis < 3; ++axis) {
      if (candidate.entity == 0 || !std::isfinite(low[axis]) || !std::isfinite(high[axis]) ||
          low[axis] > high[axis])
        return false;
      minimum[axis] = std::min(minimum[axis], low[axis]);
      maximum[axis] = std::max(maximum[axis], high[axis]);
    }
  }
  return FrameNativeSceneBounds(*state_, minimum, maximum,
                                static_cast<double>(viewport->width) / viewport->height);
}

std::optional<NativeSceneDragRequest> EditorImGuiHost::NativeSceneDrag() const noexcept {
  return state_->native_scene_drag;
}

std::optional<NativeSceneDragRequest> EditorImGuiHost::NativeSceneDragPreview() const noexcept {
  return state_->native_scene_drag_preview;
}

Nexora::Presentation::SurfaceStatus
EditorImGuiHost::Render(Nexora::Presentation::RenderSurface &surface, std::uint32_t width,
                        std::uint32_t height) {
  Activate(state_->context);
  const auto *draw = ImGui::GetDrawData();
  const auto domain = surface.UiResourceDomain();
  if (draw == nullptr || width == 0 || height == 0 || domain == 0)
    return Nexora::Presentation::SurfaceStatus::InvalidDescriptor;
  if (state_->surface_font_domain != domain) {
    state_->surface_font_domain = domain;
    state_->surface_font_generation = 0;
    for (auto &texture : state_->native_textures)
      texture.uploaded_generation = 0;
  }
  std::vector<Nexora::Presentation::UiVertex> vertices;
  std::vector<std::byte> indices;
  std::vector<Nexora::Presentation::UiDrawCommand> commands;
  vertices.reserve(static_cast<std::size_t>(draw->TotalVtxCount));
  indices.reserve(static_cast<std::size_t>(draw->TotalIdxCount) * sizeof(ImDrawIdx));
  std::uint32_t vertex_base = 0;
  std::uint32_t index_base = 0;
  for (int list_index = 0; list_index < draw->CmdListsCount; ++list_index) {
    const auto &list = *draw->CmdLists[list_index];
    for (const auto &vertex : list.VtxBuffer)
      vertices.push_back({{(vertex.pos.x - draw->DisplayPos.x) * draw->FramebufferScale.x,
                           (vertex.pos.y - draw->DisplayPos.y) * draw->FramebufferScale.y},
                          {vertex.uv.x, vertex.uv.y},
                          vertex.col});
    const auto index_bytes = std::as_bytes(
        std::span{list.IdxBuffer.Data, static_cast<std::size_t>(list.IdxBuffer.Size)});
    indices.insert(indices.end(), index_bytes.begin(), index_bytes.end());
    for (const auto &command : list.CmdBuffer) {
      if (command.UserCallback != nullptr || command.ElemCount == 0)
        continue;
      const auto left =
          std::max(0.0F, (command.ClipRect.x - draw->DisplayPos.x) * draw->FramebufferScale.x);
      const auto top =
          std::max(0.0F, (command.ClipRect.y - draw->DisplayPos.y) * draw->FramebufferScale.y);
      const auto right =
          std::min(static_cast<float>(width),
                   (command.ClipRect.z - draw->DisplayPos.x) * draw->FramebufferScale.x);
      const auto bottom =
          std::min(static_cast<float>(height),
                   (command.ClipRect.w - draw->DisplayPos.y) * draw->FramebufferScale.y);
      if (right <= left || bottom <= top)
        continue;
      const auto texture_id = static_cast<std::uint64_t>(command.GetTexID());
      const auto texture_index = static_cast<std::uint32_t>(texture_id);
      const auto texture_generation = static_cast<std::uint32_t>(texture_id >> 32U);
      auto native_id = TextureId(0, 1);
      if (texture_index != 0 && texture_index < state_->native_textures.size() &&
          texture_generation != 0 &&
          state_->native_textures[texture_index].generation == texture_generation) {
        // Backend cache keys are bounded slots; public generations are checked before binding.
        native_id = TextureId(texture_index, 1);
      } else if (texture_id != native_id) {
        ++state_->renderer_metrics.rejected_textures;
      }
      commands.push_back({static_cast<std::int32_t>(left), static_cast<std::int32_t>(top),
                          static_cast<std::uint32_t>(right - left),
                          static_cast<std::uint32_t>(bottom - top), native_id, command.ElemCount,
                          index_base + command.IdxOffset,
                          static_cast<std::int32_t>(vertex_base + command.VtxOffset)});
    }
    vertex_base += static_cast<std::uint32_t>(list.VtxBuffer.Size);
    index_base += static_cast<std::uint32_t>(list.IdxBuffer.Size);
  }
  // ImGui legitimately produces no geometry while windows are still sizing themselves (the first
  // frame), and the surface contract rejects empty draw data. Nothing to draw is not a failure.
  if (vertices.empty() || indices.empty() || commands.empty())
    return Nexora::Presentation::SurfaceStatus::Ready;
  std::vector<Nexora::Presentation::UiTextureUpload> uploads;
  if (state_->surface_font_generation != state_->font_generation) {
    unsigned char *atlas = nullptr;
    int atlas_width = 0, atlas_height = 0;
    ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&atlas, &atlas_width, &atlas_height);
    uploads.push_back({TextureId(0, 1), static_cast<std::uint32_t>(atlas_width),
                       static_cast<std::uint32_t>(atlas_height),
                       static_cast<std::uint32_t>(atlas_width) * 4U,
                       std::span{reinterpret_cast<const std::byte *>(atlas),
                                 static_cast<std::size_t>(atlas_width) * atlas_height * 4U}});
  }
  for (std::uint32_t index = 1; index < state_->native_textures.size(); ++index) {
    const auto &texture = state_->native_textures[index];
    if (texture.generation != 0 && texture.uploaded_generation != texture.generation)
      uploads.push_back(
          {TextureId(index, 1), texture.width, texture.height, texture.width * 4U, texture.pixels});
  }
  const auto status = surface.RenderUi(
      {vertices, indices, commands, uploads, sizeof(ImDrawIdx) == sizeof(std::uint32_t)});
  if (status == Nexora::Presentation::SurfaceStatus::Ready) {
    state_->surface_font_generation = state_->font_generation;
    for (auto &texture : state_->native_textures)
      texture.uploaded_generation = texture.generation;
  }
  return status;
}

void EditorImGuiHost::UpdateImeCandidate(Nexora::Presentation::RenderSurface &surface) {
  Activate(state_->context);
  state_->surface = &surface;
}

FrameMetrics EditorImGuiHost::EndFrame() {
  Activate(state_->context);
  ImGui::Render();
  state_->surface = nullptr;
  const auto *draw = ImGui::GetDrawData();
  if (draw == nullptr)
    return {};
  return {static_cast<std::uint32_t>(draw->TotalVtxCount),
          static_cast<std::uint32_t>(draw->TotalIdxCount),
          static_cast<std::uint32_t>(draw->CmdListsCount)};
}

RecoveryChoice EditorImGuiHost::TakeRecoveryChoice() noexcept {
  const auto choice = state_->recovery_choice;
  state_->recovery_choice = RecoveryChoice::None;
  return choice;
}

bool EditorImGuiHost::ApplyRecoveryChoice(ProjectWorkspace &workspace, RecoveryChoice choice) {
  if (choice == RecoveryChoice::None || state_->recovery_choice != RecoveryChoice::None)
    return false;
  std::string error;
  const bool succeeded = choice == RecoveryChoice::Recover ? workspace.RecoverWorkspace(&error)
                                                           : workspace.DiscardRecovery(&error);
  if (!succeeded) {
    state_->recovery_error = error.empty() ? "recovery operation failed" : std::move(error);
    return false;
  }
  state_->recovery_error.clear();
  state_->recovery_choice = choice;
  return true;
}

std::string_view EditorImGuiHost::RecoveryError() const noexcept { return state_->recovery_error; }

#if defined(NEXORA_EDITOR_IMGUI_TEST_ACCESS)
EditorImGuiTestState EditorImGuiTestAccess::Inspect(const EditorImGuiHost &host) noexcept {
  Activate(host.state_->context);
  const auto &io = ImGui::GetIO();
  return {(io.ConfigFlags & ImGuiConfigFlags_NavEnableKeyboard) != 0,
          (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) != 0,
          io.ConfigInputTrickleEventQueue,
          io.DisplaySize.x,
          io.DisplaySize.y,
          io.DisplayFramebufferScale.x,
          io.FontGlobalScale,
          host.state_->hierarchy_visible_rows,
          host.state_->hierarchy_rendered_rows,
          host.state_->hierarchy_selection,
          host.state_->hierarchy_selection_anchor.value_or(SceneDocument::NodeKey{}),
          host.state_->inspector_selection,
          host.state_->inspector_transform_visible,
          host.state_->content_visible_items,
          host.state_->content_visible_folders,
          host.state_->content_selection,
          host.state_->content_forward_dependencies,
          host.state_->content_reverse_dependencies,
          host.state_->content_dependency_cycle,
          host.state_->content_import_active,
          host.state_->content_import_state,
          host.state_->content_import_diagnostics,
          host.state_->content_conflicts,
          host.state_->content_conflict_visible,
          host.state_->content_conflict_compare_visible,
          host.state_->content_conflict_choice,
          host.state_->project_writable,
          host.state_->project_upgrade_required,
          host.state_->recent_projects,
          host.state_->selector_visible,
          host.state_->selector_recent_projects,
          host.state_->app_focused,
          host.state_->selector_focus_root,
          host.state_->selector_root_active};
}

std::string_view EditorImGuiTestAccess::ProjectSelectorRoot(const EditorImGuiHost &host) noexcept {
  return host.state_->selector_root.data();
}

void EditorImGuiTestAccess::ConfigureSyntheticInput(EditorImGuiHost &host,
                                                    bool macos_behaviors) noexcept {
  Activate(host.state_->context);
  auto &io = ImGui::GetIO();
  io.ConfigInputTrickleEventQueue = false;
  io.ConfigMacOSXBehaviors = macos_behaviors;
}

void EditorImGuiTestAccess::InvokeImeCallback(EditorImGuiHost &host, float x, float y,
                                              bool visible) noexcept {
  Activate(host.state_->context);
  ImGuiPlatformImeData data{};
  data.InputPos = {x, y};
  data.WantVisible = visible;
  const auto callback = ImGui::GetPlatformIO().Platform_SetImeDataFn;
  IM_ASSERT(callback != nullptr);
  callback(host.state_->context, ImGui::GetMainViewport(), &data);
}

std::optional<std::array<float, 2>>
EditorImGuiTestAccess::HierarchyRenamePosition(const EditorImGuiHost &host) noexcept {
  return host.state_->hierarchy_rename_position;
}
bool EditorImGuiTestAccess::HierarchyRenameOpen(const EditorImGuiHost &host) noexcept {
  return host.state_->hierarchy_rename_target.has_value();
}
std::string_view EditorImGuiTestAccess::HierarchyRenameText(const EditorImGuiHost &host) noexcept {
  return host.state_->hierarchy_rename.data();
}

void EditorImGuiTestAccess::SetHierarchyFilter(EditorImGuiHost &host,
                                               std::string_view filter) noexcept {
  const auto count = std::min(filter.size(), host.state_->hierarchy_filter.size() - 1);
  std::memcpy(host.state_->hierarchy_filter.data(), filter.data(), count);
  host.state_->hierarchy_filter[count] = '\0';
}

std::vector<OpaqueComponentInfo>
EditorImGuiTestAccess::InspectorOpaqueInfo(const EditorImGuiHost &host) {
  return host.state_->inspector_opaque_info;
}

std::array<float, 2> EditorImGuiTestAccess::PointerPosition(const EditorImGuiHost &host) noexcept {
  Activate(host.state_->context);
  const auto position = ImGui::GetIO().MousePos;
  return {position.x, position.y};
}

std::optional<std::array<float, 2>>
EditorImGuiTestAccess::PlayApplyPosition(const EditorImGuiHost &host, bool confirm) noexcept {
  return confirm ? host.state_->play_apply_confirm_position : host.state_->play_apply_position;
}
bool EditorImGuiTestAccess::PlayApplyOpen(const EditorImGuiHost &host) noexcept {
  return host.state_->play_apply_open;
}

void EditorImGuiTestAccess::FocusContent(EditorImGuiHost &host) noexcept {
  Activate(host.state_->context);
  const auto name = PanelWindowName("nexora.content");
  ImGui::SetWindowFocus(name.c_str());
}
std::optional<std::filesystem::path>
EditorImGuiTestAccess::ContentFocusedFolder(const EditorImGuiHost &host) {
  return host.state_->content_focused_folder;
}
void EditorImGuiTestAccess::FocusGame(EditorImGuiHost &host) noexcept {
  Activate(host.state_->context);
  const auto name = PanelWindowName("nexora.game");
  ImGui::SetWindowFocus(name.c_str());
}
bool EditorImGuiTestAccess::GameInputBindingsOpen(const EditorImGuiHost &host) noexcept {
  return host.state_->game_input_binding_open;
}
std::string_view
EditorImGuiTestAccess::GameInputBindingsError(const EditorImGuiHost &host) noexcept {
  return host.state_->game_input_binding_error;
}
std::optional<std::array<float, 2>>
EditorImGuiTestAccess::GameInputBindingPosition(const EditorImGuiHost &host,
                                                std::size_t control) noexcept {
  return control < host.state_->game_input_binding_positions.size()
             ? host.state_->game_input_binding_positions[control]
             : std::nullopt;
}
std::optional<std::array<float, 2>>
EditorImGuiTestAccess::GameInputChoicePosition(const EditorImGuiHost &host,
                                               PlayInputControl control) noexcept {
  const auto found =
      std::ranges::find(host.state_->game_input_choice_positions, control,
                        &decltype(host.state_->game_input_choice_positions)::value_type::first);
  return found == host.state_->game_input_choice_positions.end() ? std::nullopt
                                                                 : std::optional{found->second};
}
std::optional<std::array<float, 2>>
EditorImGuiTestAccess::ContentSearchPosition(const EditorImGuiHost &host) noexcept {
  return host.state_->content_search_position;
}
std::optional<std::array<float, 2>>
EditorImGuiTestAccess::ContentAddMeshPosition(const EditorImGuiHost &host) noexcept {
  return host.state_->content_add_mesh_position;
}
std::optional<std::array<float, 2>>
EditorImGuiTestAccess::ContentOpenScenePosition(const EditorImGuiHost &host) noexcept {
  return host.state_->content_open_scene_position;
}
std::optional<std::array<float, 2>>
EditorImGuiTestAccess::ContentRenamePosition(const EditorImGuiHost &host,
                                             std::size_t control) noexcept {
  return control < host.state_->content_rename_positions.size()
             ? host.state_->content_rename_positions[control]
             : std::nullopt;
}
std::string_view EditorImGuiTestAccess::ContentRenameText(const EditorImGuiHost &host) noexcept {
  return host.state_->content_rename.data();
}
std::optional<std::array<float, 2>>
EditorImGuiTestAccess::ContentAssetPosition(const EditorImGuiHost &host,
                                            runtime::AssetUuid asset) noexcept {
  const auto found =
      std::ranges::find(host.state_->content_asset_positions, asset,
                        &decltype(host.state_->content_asset_positions)::value_type::first);
  return found == host.state_->content_asset_positions.end() ? std::nullopt
                                                             : std::optional{found->second};
}
bool EditorImGuiTestAccess::ContentDragActive(const EditorImGuiHost &host) noexcept {
  Activate(host.state_->context);
  const auto *payload = ImGui::GetDragDropPayload();
  return payload && payload->IsDataType(AssetDragPayload::kType.data());
}
void EditorImGuiTestAccess::FocusProfiler(EditorImGuiHost &host) noexcept {
  Activate(host.state_->context);
  const auto name = PanelWindowName("nexora.profiler");
  ImGui::SetWindowFocus(name.c_str());
}
void EditorImGuiTestAccess::FocusConsole(EditorImGuiHost &host) noexcept {
  Activate(host.state_->context);
  const auto name = PanelWindowName("nexora.console");
  ImGui::SetWindowFocus(name.c_str());
}
std::optional<std::array<float, 2>>
EditorImGuiTestAccess::ConsoleControlPosition(const EditorImGuiHost &host,
                                              std::size_t control) noexcept {
  return control < host.state_->console_control_positions.size()
             ? host.state_->console_control_positions[control]
             : std::nullopt;
}
std::size_t EditorImGuiTestAccess::ConsoleVisibleCount(const EditorImGuiHost &host) noexcept {
  return host.state_->console_visible_count;
}
std::optional<std::uint64_t>
EditorImGuiTestAccess::ConsoleFirstVisibleSequence(const EditorImGuiHost &host) noexcept {
  return host.state_->console_first_visible_sequence;
}
void EditorImGuiTestAccess::SetConsoleFilter(EditorImGuiHost &host, std::string_view text,
                                             int severity) {
  auto &buffer = host.state_->console_filter.InputBuf;
  const auto size = std::min(text.size(), sizeof(buffer) - 1);
  if (size)
    std::memcpy(buffer, text.data(), size);
  buffer[size] = '\0';
  host.state_->console_filter.Build();
  host.state_->console_min_severity = std::clamp(severity, 0, 3);
}
std::optional<std::array<float, 2>>
EditorImGuiTestAccess::ProfileExportPosition(const EditorImGuiHost &host) noexcept {
  return host.state_->profile_export_position;
}
std::optional<std::array<float, 2>>
EditorImGuiTestAccess::ProfileCapturePosition(const EditorImGuiHost &host) noexcept {
  return host.state_->profile_capture_position;
}
std::optional<std::array<float, 2>>
EditorImGuiTestAccess::ProfileClearPosition(const EditorImGuiHost &host) noexcept {
  return host.state_->profile_clear_position;
}
std::optional<std::array<float, 2>>
EditorImGuiTestAccess::MemoryControlPosition(const EditorImGuiHost &host,
                                             std::size_t control) noexcept {
  return control < host.state_->memory_control_positions.size()
             ? host.state_->memory_control_positions[control]
             : std::nullopt;
}
const ProcessMemoryCapture *
EditorImGuiTestAccess::ImportedMemoryCapture(const EditorImGuiHost &host) noexcept {
  return host.state_->imported_memory ? &*host.state_->imported_memory : nullptr;
}
ProcessMemoryObservation
EditorImGuiTestAccess::ProfileMemory(const EditorImGuiHost &host) noexcept {
  return host.state_->profile_memory;
}
GpuProfileObservation EditorImGuiTestAccess::ProfileGpu(const EditorImGuiHost &host) noexcept {
  return host.state_->profile_gpu;
}
std::optional<std::array<float, 2>>
EditorImGuiTestAccess::GpuControlPosition(const EditorImGuiHost &host,
                                          std::size_t control) noexcept {
  return control < host.state_->gpu_control_positions.size()
             ? host.state_->gpu_control_positions[control]
             : std::nullopt;
}
const GpuTimingCapture *
EditorImGuiTestAccess::ImportedGpuCapture(const EditorImGuiHost &host) noexcept {
  return host.state_->imported_gpu ? &*host.state_->imported_gpu : nullptr;
}
std::optional<std::array<float, 2>>
EditorImGuiTestAccess::ProfileJsonExportPosition(const EditorImGuiHost &host) noexcept {
  return host.state_->profile_json_export_position;
}
std::optional<std::array<float, 2>>
EditorImGuiTestAccess::ProfileCsvImportPosition(const EditorImGuiHost &host) noexcept {
  return host.state_->profile_csv_import_position;
}
std::optional<std::array<float, 2>>
EditorImGuiTestAccess::ProfileJsonImportPosition(const EditorImGuiHost &host) noexcept {
  return host.state_->profile_json_import_position;
}
std::optional<std::array<float, 2>>
EditorImGuiTestAccess::ProfileImportClearPosition(const EditorImGuiHost &host) noexcept {
  return host.state_->profile_import_clear_position;
}
const FrameProcessingCapture *
EditorImGuiTestAccess::ImportedProfileCapture(const EditorImGuiHost &host) noexcept {
  return host.state_->imported_profile ? &*host.state_->imported_profile : nullptr;
}

std::optional<std::array<float, 2>>
EditorImGuiTestAccess::HierarchyCutPosition(const EditorImGuiHost &host) noexcept {
  return host.state_->hierarchy_cut_position;
}
bool EditorImGuiTestAccess::HierarchyDragActive(const EditorImGuiHost &host) noexcept {
  Activate(host.state_->context);
  const auto *payload = ImGui::GetDragDropPayload();
  return payload && payload->IsDataType(kHierarchyDragType.data());
}
std::optional<std::array<float, 2>>
EditorImGuiTestAccess::HierarchyRowPosition(const EditorImGuiHost &host,
                                            SceneDocument::NodeKey key) noexcept {
  const auto row =
      std::ranges::find(host.state_->hierarchy_row_positions, key,
                        &decltype(host.state_->hierarchy_row_positions)::value_type::first);
  return row == host.state_->hierarchy_row_positions.end() ? std::nullopt
                                                           : std::optional{row->second};
}
void EditorImGuiTestAccess::FocusHierarchy(EditorImGuiHost &host) noexcept {
  Activate(host.state_->context);
  const auto name = PanelWindowName("nexora.hierarchy");
  ImGui::SetWindowFocus(name.c_str());
}

void EditorImGuiTestAccess::FocusScene(EditorImGuiHost &host) noexcept {
  Activate(host.state_->context);
  const auto name = PanelWindowName("nexora.scene");
  ImGui::SetWindowFocus(name.c_str());
}
std::optional<std::array<float, 2>>
EditorImGuiTestAccess::SceneFramePosition(const EditorImGuiHost &host) noexcept {
  return host.state_->scene_frame_position;
}
std::optional<std::array<float, 2>>
EditorImGuiTestAccess::SceneFrameAllPosition(const EditorImGuiHost &host) noexcept {
  return host.state_->scene_frame_all_position;
}

std::optional<std::array<float, 2>>
EditorImGuiTestAccess::SceneSelectAllPosition(const EditorImGuiHost &host) noexcept {
  return host.state_->scene_select_all_position;
}

std::optional<std::array<float, 2>>
EditorImGuiTestAccess::NativeSceneToolPosition(const EditorImGuiHost &host,
                                               NativeSceneTool tool) noexcept {
  const auto index = static_cast<std::size_t>(tool);
  return index < host.state_->native_scene_tool_positions.size()
             ? host.state_->native_scene_tool_positions[index]
             : std::nullopt;
}

std::optional<std::array<float, 2>>
EditorImGuiTestAccess::SceneMarkerPosition(const EditorImGuiHost &host,
                                           SceneDocument::NodeKey entity) noexcept {
  const auto found = std::ranges::find(host.state_->scene_markers, entity,
                                       &EditorImGuiHost::State::SceneMarker::entity);
  if (found == host.state_->scene_markers.end())
    return std::nullopt;
  return std::array{found->position.x, found->position.y};
}

std::array<float, 2>
EditorImGuiTestAccess::SceneOverviewCenter(const EditorImGuiHost &host) noexcept {
  return {host.state_->scene_center_world.x, host.state_->scene_center_world.y};
}

void EditorImGuiTestAccess::SetSceneSnap(EditorImGuiHost &host, bool enabled,
                                         int step_index) noexcept {
  host.state_->scene_snap_to_grid = enabled;
  host.state_->scene_snap_step_index = std::clamp(step_index, 0, 4);
}

void EditorImGuiTestAccess::QueueHierarchySelection(EditorImGuiHost &host,
                                                    SceneDocument::NodeKey entity, bool additive,
                                                    bool range) noexcept {
  host.state_->hierarchy_selection_request = {entity, additive, range};
}

void EditorImGuiTestAccess::QueueHierarchyMove(EditorImGuiHost &host, SceneDocument::NodeKey entity,
                                               std::optional<SceneDocument::NodeKey> parent,
                                               std::size_t index) noexcept {
  host.state_->hierarchy_move_request = {entity, parent, index};
}

std::optional<std::array<float, 2>>
EditorImGuiTestAccess::SceneTabPosition(const EditorImGuiHost &host, std::uint64_t id) {
  const auto at = host.state_->scene_tab_positions.find(id);
  return at == host.state_->scene_tab_positions.end() ? std::nullopt : std::optional(at->second);
}
std::optional<std::array<float, 2>>
EditorImGuiTestAccess::SceneTabControl(const EditorImGuiHost &host, std::size_t control) {
  return control < host.state_->scene_tab_controls.size() ? host.state_->scene_tab_controls[control]
                                                          : std::nullopt;
}
void EditorImGuiTestAccess::SetSceneTabPath(EditorImGuiHost &host, std::string_view text) {
  Activate(host.state_->context);
  ImGui::ClearActiveID();
  host.state_->scene_tab_path = {};
  std::copy_n(text.begin(), std::min(text.size(), host.state_->scene_tab_path.size() - 1),
              host.state_->scene_tab_path.begin());
}
std::vector<SceneTabItem> EditorImGuiTestAccess::SceneTabs(const EditorImGuiHost &host) {
  return host.state_->scene_tabs;
}

std::string_view EditorImGuiTestAccess::SceneFileText(const EditorImGuiHost &host) noexcept {
  return host.state_->scene_file_text.data();
}
std::optional<std::array<float, 2>>
EditorImGuiTestAccess::SceneFilePosition(const EditorImGuiHost &host,
                                         std::size_t control) noexcept {
  return control < host.state_->scene_file_positions.size()
             ? host.state_->scene_file_positions[control]
             : std::nullopt;
}

std::optional<std::array<float, 2>>
EditorImGuiTestAccess::HierarchyCreatePosition(const EditorImGuiHost &host,
                                               std::size_t control) noexcept {
  return control < host.state_->hierarchy_create_positions.size()
             ? host.state_->hierarchy_create_positions[control]
             : std::nullopt;
}

void EditorImGuiTestAccess::QueueHierarchyCreate(EditorImGuiHost &host, std::string name,
                                                 std::optional<SceneDocument::NodeKey> parent) {
  host.state_->hierarchy_create_request.emplace(
      EditorImGuiHost::State::HierarchyCreateRequest{std::move(name), parent});
}

void EditorImGuiTestAccess::QueueHierarchyReorder(EditorImGuiHost &host, int direction) noexcept {
  host.state_->hierarchy_reorder_request = direction;
}

void EditorImGuiTestAccess::QueueHierarchyExpansion(EditorImGuiHost &host,
                                                    SceneDocument::NodeKey entity,
                                                    bool expanded) noexcept {
  host.state_->hierarchy_expansion_request = std::pair{entity, expanded};
}

void EditorImGuiTestAccess::SetHierarchyExpanded(EditorImGuiHost &host,
                                                 std::span<const SceneDocument::NodeKey> keys) {
  host.state_->hierarchy_expanded.assign(keys.begin(), keys.end());
}

std::vector<EditorHierarchyTestRow>
EditorImGuiTestAccess::HierarchyRows(const EditorImGuiHost &host, const SceneDocument &document) {
  const auto nodes = document.Nodes();
  const auto rows = BuildHierarchyRows(*host.state_, nodes,
                                       std::string_view(host.state_->hierarchy_filter.data()));
  std::vector<EditorHierarchyTestRow> result;
  result.reserve(rows.size());
  for (const auto &row : rows)
    result.push_back({row.node->Key(), row.depth, row.has_children});
  return result;
}

void EditorImGuiTestAccess::QueueHierarchyRename(EditorImGuiHost &host,
                                                 SceneDocument::NodeKey entity, std::string name) {
  host.state_->hierarchy_rename_request = std::pair{entity, std::move(name)};
}

void EditorImGuiTestAccess::QueueInspectorTransform(EditorImGuiHost &host,
                                                    SceneDocument::NodeKey entity,
                                                    runtime::Transform transform) noexcept {
  host.state_->inspector_transform_request.emplace(
      EditorImGuiHost::State::InspectorTransformRequest{{entity}, {transform}});
}

void EditorImGuiTestAccess::SelectPlayEntity(EditorImGuiHost &host, runtime::Id entity) noexcept {
  host.state_->play_inspection_entity = entity;
  host.state_->inspect_play_selection = true;
}
std::optional<std::array<float, 2>>
EditorImGuiTestAccess::GameCameraPosition(const EditorImGuiHost &host,
                                          std::optional<runtime::Id> camera) noexcept {
  if (!camera)
    return host.state_->game_camera_combo_position;
  const auto found =
      std::ranges::find(host.state_->game_camera_positions, *camera,
                        &decltype(host.state_->game_camera_positions)::value_type::first);
  return found == host.state_->game_camera_positions.end() ? std::nullopt
                                                           : std::optional{found->second};
}
runtime::Id EditorImGuiTestAccess::PlayInspectorEntity(const EditorImGuiHost &host) noexcept {
  return host.state_->play_inspector_rendered;
}

void EditorImGuiTestAccess::QueueInspectorCamera(
    EditorImGuiHost &host, SceneDocument::NodeKey entity,
    std::optional<runtime::CameraComponent> camera) noexcept {
  host.state_->inspector_camera_request =
      EditorImGuiHost::State::InspectorCameraRequest{{entity}, {camera}};
}

void EditorImGuiTestAccess::QueueInspectorCameras(
    EditorImGuiHost &host, std::span<const SceneDocument::NodeKey> entities,
    std::span<const std::optional<runtime::CameraComponent>> cameras) {
  host.state_->inspector_camera_request = EditorImGuiHost::State::InspectorCameraRequest{
      {entities.begin(), entities.end()}, {cameras.begin(), cameras.end()}};
}
void EditorImGuiTestAccess::QueueInspectorLights(
    EditorImGuiHost &host, std::span<const SceneDocument::NodeKey> entities,
    std::span<const std::optional<runtime::LightComponent>> lights) {
  host.state_->inspector_light_request = EditorImGuiHost::State::InspectorLightRequest{
      {entities.begin(), entities.end()}, {lights.begin(), lights.end()}};
}
std::array<bool, 6>
EditorImGuiTestAccess::InspectorComponentMixed(const EditorImGuiHost &host) noexcept {
  return {host.state_->inspector_camera_presence_mixed, host.state_->inspector_camera_mixed[0],
          host.state_->inspector_camera_mixed[1],       host.state_->inspector_camera_mixed[2],
          host.state_->inspector_light_presence_mixed,  host.state_->inspector_light_mixed};
}
void EditorImGuiTestAccess::FocusInspectorCameraField(EditorImGuiHost &host,
                                                      std::size_t axis) noexcept {
  host.state_->inspector_camera_focus_request = axis;
}

void EditorImGuiTestAccess::FocusInspectorLightField(EditorImGuiHost &host) noexcept {
  host.state_->inspector_light_focus_request = true;
}

void EditorImGuiTestAccess::QueueInspectorMesh(EditorImGuiHost &host, SceneDocument::NodeKey entity,
                                               std::optional<runtime::AssetUuid> asset,
                                               std::uint64_t generation) noexcept {
  host.state_->inspector_mesh_request =
      EditorImGuiHost::State::InspectorMeshRequest{{entity}, asset, generation};
}

void EditorImGuiTestAccess::QueueInspectorMeshes(EditorImGuiHost &host,
                                                 std::span<const SceneDocument::NodeKey> entities,
                                                 std::optional<runtime::AssetUuid> asset,
                                                 std::uint64_t generation) {
  host.state_->inspector_mesh_request = EditorImGuiHost::State::InspectorMeshRequest{
      {entities.begin(), entities.end()}, asset, generation};
}
std::string_view EditorImGuiTestAccess::InspectorMeshLabel(const EditorImGuiHost &host) noexcept {
  return host.state_->inspector_mesh_label;
}

void EditorImGuiTestAccess::QueueInspectorMaterial(EditorImGuiHost &host,
                                                   SceneDocument::NodeKey entity,
                                                   runtime::AssetUuid asset,
                                                   std::uint64_t generation) noexcept {
  host.state_->inspector_material_request =
      EditorImGuiHost::State::InspectorMaterialRequest{entity, asset, generation};
}
std::string_view
EditorImGuiTestAccess::InspectorMaterialLabel(const EditorImGuiHost &host) noexcept {
  return host.state_->inspector_material_label;
}
std::optional<std::array<float, 2>>
EditorImGuiTestAccess::InspectorMaterialPosition(const EditorImGuiHost &host,
                                                 std::size_t control) noexcept {
  return control < host.state_->inspector_material_positions.size()
             ? host.state_->inspector_material_positions[control]
             : std::nullopt;
}

void EditorImGuiTestAccess::FocusInspector(EditorImGuiHost &host) noexcept {
  Activate(host.state_->context);
  const auto name = PanelWindowName("nexora.inspector");
  ImGui::SetWindowFocus(name.c_str());
}
std::optional<std::array<float, 2>>
EditorImGuiTestAccess::InspectorResetPosition(const EditorImGuiHost &host,
                                              std::size_t component) noexcept {
  return component < host.state_->inspector_reset_positions.size()
             ? host.state_->inspector_reset_positions[component]
             : std::nullopt;
}

std::optional<std::array<float, 2>> EditorImGuiTestAccess::InspectorClipboardPosition(
    const EditorImGuiHost &host, std::size_t component, std::size_t control) noexcept {
  return component < host.state_->inspector_clipboard_positions.size() && control < 2
             ? host.state_->inspector_clipboard_positions[component][control]
             : std::nullopt;
}

void EditorImGuiTestAccess::CollapseInspector(EditorImGuiHost &host, bool collapsed) noexcept {
  Activate(host.state_->context);
  const auto name = PanelWindowName("nexora.inspector");
  // Docked tabs cannot collapse; this hook exercises the supported floating-window lifecycle.
  if (collapsed)
    if (auto *window = ImGui::FindWindowByName(name.c_str()))
      ImGui::SetWindowDock(window, 0, ImGuiCond_Always);
  ImGui::SetWindowCollapsed(name.c_str(), collapsed);
}
std::optional<std::array<float, 2>>
EditorImGuiTestAccess::InspectorMeshPosition(const EditorImGuiHost &host,
                                             std::size_t control) noexcept {
  return control < host.state_->inspector_mesh_positions.size()
             ? host.state_->inspector_mesh_positions[control]
             : std::nullopt;
}

void EditorImGuiTestAccess::QueueInspectorLight(
    EditorImGuiHost &host, SceneDocument::NodeKey entity,
    std::optional<runtime::LightComponent> light) noexcept {
  host.state_->inspector_light_request =
      EditorImGuiHost::State::InspectorLightRequest{{entity}, {light}};
}

void EditorImGuiTestAccess::FocusInspectorTransformField(EditorImGuiHost &host,
                                                         std::size_t axis) noexcept {
  host.state_->inspector_transform_focus_request = axis;
}
std::string_view EditorImGuiTestAccess::InspectorComponentText(const EditorImGuiHost &host,
                                                               std::size_t field) noexcept {
  if (field < host.state_->inspector_camera_text.size())
    return host.state_->inspector_camera_text[field].data();
  return field == 3 ? host.state_->inspector_light_text.data() : "";
}
std::optional<std::array<float, 2>>
EditorImGuiTestAccess::CameraAlignPosition(const EditorImGuiHost &host) noexcept {
  return host.state_->camera_align_position;
}
std::array<bool, 6>
EditorImGuiTestAccess::InspectorTransformMixed(const EditorImGuiHost &host) noexcept {
  return host.state_->inspector_transform_mixed;
}
std::string_view EditorImGuiTestAccess::InspectorTransformText(const EditorImGuiHost &host,
                                                               std::size_t axis) noexcept {
  return axis < host.state_->inspector_transform_text.size()
             ? host.state_->inspector_transform_text[axis].data()
             : "";
}

void EditorImGuiTestAccess::QueueInspectorTransforms(
    EditorImGuiHost &host, std::span<const SceneDocument::NodeKey> entities,
    std::span<const runtime::Transform> transforms) {
  host.state_->inspector_transform_request.emplace(
      EditorImGuiHost::State::InspectorTransformRequest{{entities.begin(), entities.end()},
                                                        {transforms.begin(), transforms.end()}});
}

void EditorImGuiTestAccess::QueueInspectorEulerField(
    EditorImGuiHost &host, std::span<const SceneDocument::NodeKey> entities, std::size_t axis,
    double degrees) {
  host.state_->inspector_euler_request.emplace(EditorImGuiHost::State::InspectorEulerRequest{
      {entities.begin(), entities.end()}, axis, degrees});
}

void EditorImGuiTestAccess::FocusInspectorEulerField(EditorImGuiHost &host,
                                                     std::size_t axis) noexcept {
  host.state_->inspector_euler_focus_request = axis;
}

std::optional<std::array<double, 3>>
EditorImGuiTestAccess::InspectorEulerAngles(const EditorImGuiHost &host,
                                            SceneDocument::NodeKey entity) noexcept {
  const auto hint = host.state_->inspector_euler_hints.find(entity.id);
  if (hint == host.state_->inspector_euler_hints.end() || hint->second.entity != entity)
    return std::nullopt;
  return hint->second.degrees;
}

void EditorImGuiTestAccess::QueueProjectSelection(EditorImGuiHost &host,
                                                  ProjectSelectorRequest request) {
  host.state_->selector_request = std::move(request);
}

void EditorImGuiTestAccess::QueueProjectImportCancellation(EditorImGuiHost &host) noexcept {
  host.state_->selector_cancel_requested = true;
}

void EditorImGuiTestAccess::QueueContentConflictChoice(EditorImGuiHost &host,
                                                       runtime::AssetUuid asset,
                                                       DirtyConflictChoice choice) noexcept {
  host.state_->content_conflict_choice_request = std::pair{asset, choice};
}

std::vector<std::byte> EditorImGuiTestAccess::NativeTexturePixels(const EditorImGuiHost &host,
                                                                  std::uint64_t texture_id) {
  const auto index = static_cast<std::uint32_t>(texture_id);
  const auto generation = static_cast<std::uint32_t>(texture_id >> 32U);
  const auto &textures = host.state_->native_textures;
  if (index == 0 || index >= textures.size() || generation == 0 ||
      textures[index].generation != generation)
    return {};
  return textures[index].pixels;
}

std::optional<std::array<float, 2>>
EditorImGuiTestAccess::StaticExportPosition(const EditorImGuiHost &host, std::size_t index) {
  return index < host.state_->static_export_positions.size()
             ? host.state_->static_export_positions[index]
             : std::nullopt;
}
StaticExportSnapshot EditorImGuiTestAccess::StaticExportStatus(const EditorImGuiHost &host) {
  return host.state_->static_export;
}

std::uint32_t EditorImGuiTestAccess::OverrideDrawTexture(EditorImGuiHost &host,
                                                         std::uint64_t texture_id) noexcept {
  Activate(host.state_->context);
  auto *draw = ImGui::GetDrawData();
  if (draw == nullptr)
    return 0;
  std::uint32_t overridden = 0;
  for (int list = 0; list < draw->CmdListsCount; ++list) {
    for (auto &command : draw->CmdLists[list]->CmdBuffer) {
      if (command.UserCallback != nullptr || command.ElemCount == 0)
        continue;
      command.TextureId = static_cast<ImTextureID>(texture_id);
      ++overridden;
    }
  }
  return overridden;
}
#endif
} // namespace nexora::editor::imgui
