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
#include <vector>

namespace nexora::editor::imgui {
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
  bool profile_export_requested = false;
  std::string profile_export_status;
  std::optional<std::array<float, 2>> profile_export_position;
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
  std::string recovery_error;
  std::array<char, 128> hierarchy_filter{};
  std::array<char, 128> hierarchy_create_name{'E', 'n', 't', 'i', 't', 'y'};
  std::array<char, 256> hierarchy_rename{};
  std::uint32_t hierarchy_visible_rows = 0;
  std::uint32_t hierarchy_rendered_rows = 0;
  std::uint32_t hierarchy_selection = 0;
  std::optional<SceneDocument::NodeKey> hierarchy_selection_anchor;
  std::vector<SceneDocument::NodeKey> hierarchy_expanded;
  std::optional<HierarchySelectionRequest> hierarchy_selection_request;
  std::optional<HierarchyMoveRequest> hierarchy_move_request;
  std::optional<HierarchyCreateRequest> hierarchy_create_request;
  std::optional<int> hierarchy_reorder_request;
  std::optional<std::pair<SceneDocument::NodeKey, bool>> hierarchy_expansion_request;
  std::optional<SceneDocument::NodeKey> hierarchy_rename_target;
  std::optional<std::pair<SceneDocument::NodeKey, std::string>> hierarchy_rename_request;
  std::string hierarchy_error;
  std::string hierarchy_status;
  struct SceneMarker final {
    SceneDocument::NodeKey entity;
    ImVec2 position;
  };
  std::vector<SceneMarker> scene_markers;
  std::optional<Nexora::Presentation::SceneViewport> scene_canvas_viewport;
  std::optional<Nexora::Presentation::SceneViewport> native_game_viewport;
  bool native_game_available = true;
  bool game_was_running = false;
  std::optional<std::array<float, 2>> content_add_mesh_position;
  std::string content_scene_error;
  std::vector<std::pair<runtime::AssetUuid, std::array<float, 2>>> content_asset_positions;
  std::optional<std::array<float, 2>> camera_align_position;
  runtime::Id game_camera_selection{};
  std::uint64_t game_camera_generation{};
  std::optional<std::array<float, 2>> game_camera_combo_position;
  std::vector<std::pair<runtime::Id, std::array<float, 2>>> game_camera_positions;
  bool game_input_focused = false;
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
  std::string inspector_mesh_label;
  std::array<std::optional<std::array<float, 2>>, 3> inspector_mesh_positions{};
  std::uint32_t inspector_selection = 0;
  std::vector<OpaqueComponentInfo> inspector_opaque_info;
  bool inspector_transform_visible = false;
  std::string inspector_error;
  bool scene_save_requested = false;
  std::string scene_save_message;
  bool scene_save_success = false;
  std::array<char, 128> content_query{};
  std::array<char, 64> content_type{};
  std::array<char, 260> content_rename{};
  std::optional<runtime::AssetUuid> content_rename_target;
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
      state.inspector_light_request)
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
  const ImGuiID console =
      ImGui::DockBuilderSplitNode(center, ImGuiDir_Down, 0.25F, nullptr, &center);
  const auto project_window = PanelWindowName("nexora.project");
  const auto hierarchy_window = PanelWindowName("nexora.hierarchy");
  const auto console_window = PanelWindowName("nexora.console");
  const auto profiler_window = PanelWindowName("nexora.profiler");
  const auto content_window = PanelWindowName("nexora.content");
  const auto scene_window = PanelWindowName("nexora.scene");
  const auto game_window = PanelWindowName("nexora.game");
  ImGui::DockBuilderDockWindow(project_window.c_str(), hierarchy);
  ImGui::DockBuilderDockWindow(hierarchy_window.c_str(), hierarchy);
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

  const auto append = [&](auto &&self, runtime::Id parent, std::uint32_t depth) -> void {
    const auto group = children.find(parent);
    if (group == children.end())
      return;
    for (const auto *node : group->second) {
      const auto child_group = children.find(node->id);
      const bool has_children = child_group != children.end() && !child_group->second.empty();
      rows.push_back({node, depth, has_children});
      if (has_children && expanded.contains(node->id))
        self(self, node->id, depth + 1);
    }
  };
  append(append, 0, 0);
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
void ApplyPendingHierarchyRequests(StateT &state, SceneDocument *scene, bool editable) {
  state.hierarchy_visible_rows = 0;
  state.hierarchy_rendered_rows = 0;
  state.hierarchy_selection = 0;
  if (scene == nullptr) {
    state.hierarchy_selection_anchor.reset();
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
    state.hierarchy_rename_target.reset();
  }
  const auto nodes = scene->Nodes();
  std::erase_if(state.hierarchy_expanded, [scene](const auto key) {
    return key.document_generation != scene->Generation() || scene->Key(key.id) != key;
  });
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
    const auto created = stale_parent ? runtime::Id{}
                                      : scene->Create(std::move(request->name),
                                                      request->parent ? request->parent->id : 0);
    if (created == 0) {
      state.hierarchy_error =
          "Create rejected. Use a non-empty single-line name and a current parent.";
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
  state.hierarchy_status = "Copied " + std::to_string(scene.Selection().size()) + " entities.";
  state.hierarchy_error.clear();
}

template <typename StateT> void PasteHierarchySelection(StateT &state, SceneDocument &scene) {
  if (!scene.Paste()) {
    state.hierarchy_error = "Paste failed. Copy a valid scene selection first.";
    state.hierarchy_status.clear();
    return;
  }
  state.hierarchy_selection_anchor =
      scene.Selection().size() == 1 ? scene.Key(scene.Selection().front()) : std::nullopt;
  state.hierarchy_filter.fill({});
  state.hierarchy_status = "Pasted " + std::to_string(scene.Selection().size()) + " entities.";
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
  if (!scene.DuplicateSelection()) {
    state.hierarchy_error = "Duplicate failed because the selection is empty or stale.";
    state.hierarchy_status.clear();
    return;
  }
  state.hierarchy_selection_anchor =
      scene.Selection().size() == 1 ? scene.Key(scene.Selection().front()) : std::nullopt;
  state.hierarchy_filter.fill({});
  state.hierarchy_status = "Duplicated " + std::to_string(scene.Selection().size()) + " entities.";
  state.hierarchy_error.clear();
}

template <typename StateT>
void DrawHierarchy(StateT &state, SceneDocument *scene, ProductShell &shell,
                   bool interaction_blocked, bool editable) {
  ImGui::SetNextItemWidth(-1.0F);
  ImGui::InputTextWithHint("##hierarchy-filter", "Filter entities...",
                           state.hierarchy_filter.data(), state.hierarchy_filter.size());
  if (scene == nullptr) {
    ImGui::TextUnformatted("No scene is open.");
    return;
  }

  ImGui::SetNextItemWidth(-1.0F);
  ImGui::InputTextWithHint("##hierarchy-create-name", "New entity name...",
                           state.hierarchy_create_name.data(), state.hierarchy_create_name.size());
  ImGui::BeginDisabled(!editable);
  if (ImGui::SmallButton("Create root"))
    state.hierarchy_create_request = typename StateT::HierarchyCreateRequest{
        std::string(state.hierarchy_create_name.data()), std::nullopt};
  ImGui::SameLine();
  const bool one_selected = scene->Selection().size() == 1;
  ImGui::BeginDisabled(!one_selected);
  if (ImGui::SmallButton("Create child") && one_selected) {
    if (const auto parent = scene->Key(scene->Selection().front()))
      state.hierarchy_create_request = typename StateT::HierarchyCreateRequest{
          std::string(state.hierarchy_create_name.data()), parent};
    else
      state.hierarchy_error = "Create rejected because the selected parent is stale.";
  }
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
  const auto rows = BuildHierarchyRows(state, nodes, filter);
  std::vector<SceneDocument::NodeKey> visible;
  visible.reserve(rows.size());
  for (const auto &row : rows)
    visible.push_back(row.node->Key());
  state.hierarchy_visible_rows = static_cast<std::uint32_t>(
      std::min<std::size_t>(rows.size(), std::numeric_limits<std::uint32_t>::max()));
  state.hierarchy_rendered_rows = 0;

  const auto begin_rename = [&](const SceneDocument::NodeView &node) {
    auto count = std::min(node.name.size(), state.hierarchy_rename.size() - 1);
    // Never cut a UTF-8 sequence in half: back up to a code point boundary.
    while (count > 0 && count < node.name.size() &&
           (static_cast<unsigned char>(node.name[count]) & 0xC0U) == 0x80U)
      --count;
    std::memcpy(state.hierarchy_rename.data(), node.name.data(), count);
    state.hierarchy_rename[count] = {};
    state.hierarchy_rename_target = node.Key();
    state.hierarchy_error.clear();
  };
  const SceneDocument::NodeView *selected_node = nullptr;
  if (scene->Selection().size() == 1) {
    const auto selected =
        std::ranges::find(nodes, scene->Selection().front(), &SceneDocument::NodeView::id);
    if (selected != nodes.end())
      selected_node = &*selected;
  }

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
  }
  ImGui::Separator();

  const auto handle_selection = [&](SceneDocument::NodeKey entity) {
    const auto &io = ImGui::GetIO();
    static_cast<void>(
        ApplyHierarchySelection(state, *scene, visible, entity, io.KeyCtrl, io.KeyShift));
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
    ImGui::SetNextItemWidth(320.0F);
    ImGui::InputText("Name", state.hierarchy_rename.data(), state.hierarchy_rename.size());
    const bool submit = ImGui::Button("Rename") || ImGui::IsKeyPressed(ImGuiKey_Enter, false);
    ImGui::SameLine();
    const bool cancel = ImGui::Button("Cancel");
    if (!state.hierarchy_rename_target) {
      // The target went stale (entity or document replaced) while the modal was open.
      state.hierarchy_error.clear();
      ImGui::CloseCurrentPopup();
    } else if (submit) {
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

template <typename StateT> bool FrameSceneSelection(StateT &state, const SceneDocument &scene) {
  double min_x = std::numeric_limits<double>::infinity();
  double max_x = -min_x;
  double min_z = min_x;
  double max_z = -min_x;
  for (const auto id : scene.Selection()) {
    const auto pose = scene.WorldTransform(id);
    if (!pose)
      return false;
    min_x = std::min(min_x, pose->x);
    max_x = std::max(max_x, pose->x);
    min_z = std::min(min_z, pose->z);
    max_z = std::max(max_z, pose->z);
  }
  if (min_x == std::numeric_limits<double>::infinity())
    return false;
  const double x = min_x * 0.5 + max_x * 0.5;
  const double z = min_z * 0.5 + max_z * 0.5;
  if (!std::isfinite(x) || !std::isfinite(z) || std::abs(x) > std::numeric_limits<float>::max() ||
      std::abs(z) > std::numeric_limits<float>::max())
    return false;
  state.scene_center_world = {static_cast<float>(x), static_cast<float>(z)};
  return true;
}

template <typename StateT>
bool FrameNativeSceneSelection(StateT &state, const SceneDocument &scene) {
  double min_x = std::numeric_limits<double>::infinity();
  double max_x = -min_x;
  double min_y = std::numeric_limits<double>::infinity();
  double max_y = -min_y;
  double min_z = min_y;
  double max_z = -min_y;
  for (const auto id : scene.Selection()) {
    const auto pose = scene.WorldTransform(id);
    if (!pose)
      return false;
    min_x = std::min(min_x, pose->x);
    max_x = std::max(max_x, pose->x);
    min_y = std::min(min_y, pose->y);
    max_y = std::max(max_y, pose->y);
    min_z = std::min(min_z, pose->z);
    max_z = std::max(max_z, pose->z);
  }
  if (min_x == std::numeric_limits<double>::infinity())
    return false;
  const double x = min_x * 0.5 + max_x * 0.5;
  const double y = min_y * 0.5 + max_y * 0.5;
  const double z = min_z * 0.5 + max_z * 0.5;
  if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z) || std::abs(x) > 100000.0 ||
      std::abs(y) > 100000.0 || std::abs(z) > 100000.0)
    return false;
  double radius = 0.0;
  for (const auto id : scene.Selection()) {
    const auto pose = *scene.WorldTransform(id);
    const auto proxy_radius = 0.45 * std::hypot(pose.sx, pose.sy, pose.sz);
    radius = std::max(radius, std::hypot(pose.x - x, pose.y + 0.5 - y, pose.z - z) + proxy_radius);
  }
  if (!std::isfinite(radius))
    return false;
  constexpr double kHalfVerticalFov = 0.425;
  const double distance = std::clamp(1.5 * radius / std::sin(kHalfVerticalFov), 2.0, 100.0);
  state.native_scene_orbit.target_y = y;
  state.native_scene_orbit.distance = distance;
  state.scene_center_world = {static_cast<float>(x), static_cast<float>(z)};
  return true;
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
                       ProjectContentSession *content, const MeshAssetCatalog *meshes) {
  ImGui::TextUnformatted("Top-down X/Z | Drag marker: free move | Drag red X/blue Z: axis move | "
                         "Middle: pan | Wheel: zoom");
  ImGui::SameLine();
  ImGui::BeginDisabled(scene.Selection().empty());
  if (ImGui::SmallButton("Frame selected"))
    static_cast<void>(FrameSceneSelection(state, scene));
  ImGui::EndDisabled();
  if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
      !ImGui::GetIO().WantTextInput && ImGui::IsKeyPressed(ImGuiKey_F, false))
    static_cast<void>(FrameSceneSelection(state, scene));
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
  for (const auto &node : scene.Nodes()) {
    const auto pose = scene.WorldTransform(node.id);
    if (!pose)
      continue;
    ImVec2 position{origin.x + static_cast<float>(pose->x) * state.scene_pixels_per_unit,
                    origin.y + static_cast<float>(pose->z) * state.scene_pixels_per_unit};
    if (state.scene_drag) {
      auto ancestor = node.id;
      while (ancestor != 0) {
        if (std::ranges::find(state.scene_drag->entities, ancestor, &SceneDocument::NodeKey::id) !=
            state.scene_drag->entities.end()) {
          position.x += preview_pixels.x;
          position.y += preview_pixels.y;
          break;
        }
        ancestor = scene.Parent(ancestor).value_or(0);
      }
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
void DrawInspector(StateT &state, SceneDocument *scene, ProjectContentSession *content,
                   const MeshAssetCatalog *meshes, bool editable) {
  state.inspector_selection =
      scene == nullptr ? 0U : static_cast<std::uint32_t>(scene->Selection().size());
  state.inspector_transform_visible = false;
  state.inspector_camera_presence_mixed = state.inspector_light_presence_mixed = false;
  state.inspector_camera_mixed = {};
  state.inspector_light_mixed = false;
  state.inspector_mesh_label.clear();
  state.inspector_mesh_positions = {};
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
  for (const auto key : keys)
    if (auto info = scene->InspectOpaqueComponents(key))
      state.inspector_opaque_info.insert(state.inspector_opaque_info.end(),
                                         std::make_move_iterator(info->begin()),
                                         std::make_move_iterator(info->end()));
  if (!state.inspector_opaque_info.empty() &&
      ImGui::CollapsingHeader("Missing plugin components", ImGuiTreeNodeFlags_DefaultOpen)) {
    ImGui::TextWrapped("Read-only: component data is preserved. Install its plugin to edit it.");
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
  ImGui::TextDisabled("Enter applies; Escape cancels.");
  ImGui::BeginDisabled(!editable);
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
  ImGui::BeginDisabled(!editable);
  std::vector<std::optional<runtime::CameraComponent>> cameras;
  std::vector<std::optional<runtime::LightComponent>> lights;
  for (const auto key : keys) {
    cameras.push_back(scene->Camera(key));
    lights.push_back(scene->Light(key));
  }
  ImGui::SeparatorText("Camera");
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
  ImGui::SeparatorText("Light");
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
                        SceneDocument *scene, const MeshAssetCatalog *meshes, bool scene_editable) {
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
  if (!ImGui::Begin(window.c_str())) {
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
  if (navigate_to)
    static_cast<void>(browser.SetFolder(*navigate_to));

  bool filter_changed = ImGui::InputTextWithHint(
      "##content-search", "Search assets", state.content_query.data(), state.content_query.size());
  ImGui::SameLine();
  ImGui::SetNextItemWidth(120.0F);
  filter_changed |= ImGui::InputTextWithHint("##content-type", "Type", state.content_type.data(),
                                             state.content_type.size());
  if (filter_changed)
    browser.SetFilter(state.content_query.data(), state.content_type.data());
  ImGui::SameLine();
  ImGui::BeginDisabled(!content.CanUndo());
  if (ImGui::Button("Undo content"))
    static_cast<void>(content.Undo());
  ImGui::EndDisabled();

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

  std::optional<runtime::AssetUuid> delete_asset;
  std::optional<runtime::AssetUuid> reimport_asset;
  bool open_rename = false;
  const auto folders = browser.ChildFolders();
  state.content_visible_folders = static_cast<std::uint32_t>(folders.size());
  for (const auto &folder : folders) {
    const auto label = "[Folder] " + folder.label + "##" + folder.path.generic_string();
    if (ImGui::Selectable(label.c_str(), false, ImGuiSelectableFlags_AllowDoubleClick) &&
        ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
      static_cast<void>(browser.SetFolder(folder.path));
    static_cast<void>(AcceptAssetDrop(content, folder.path));
  }

  const auto visible_count = browser.VisibleCount();
  state.content_visible_items = static_cast<std::uint32_t>(
      std::min<std::size_t>(visible_count, std::numeric_limits<std::uint32_t>::max()));
  ImGuiListClipper clipper;
  clipper.Begin(static_cast<int>(std::min<std::size_t>(
      visible_count, static_cast<std::size_t>(std::numeric_limits<int>::max()))));
  while (clipper.Step()) {
    const auto visible =
        browser.Visible(static_cast<std::size_t>(clipper.DisplayStart),
                        static_cast<std::size_t>(clipper.DisplayEnd - clipper.DisplayStart));
    for (const auto *item : visible) {
      const auto label = std::string(ThumbnailLabel(item->thumbnail)) + " " +
                         item->path.filename().string() + "##" + item->id.ToString();
      if (ImGui::Selectable(label.c_str(), browser.IsSelected(item->id))) {
        if (ImGui::GetIO().KeyCtrl)
          static_cast<void>(browser.Toggle(item->id));
        else
          static_cast<void>(browser.Select(item->id));
      }
      const auto asset_min = ImGui::GetItemRectMin(), asset_max = ImGui::GetItemRectMax();
      state.content_asset_positions.push_back(
          {item->id, {(asset_min.x + asset_max.x) * 0.5F, (asset_min.y + asset_max.y) * 0.5F}});
      if (ImGui::BeginDragDropSource()) {
        const AssetDragData payload{browser.ProjectGeneration(), item->id};
        ImGui::SetDragDropPayload(AssetDragPayload::kType.data(), &payload, sizeof(payload),
                                  ImGuiCond_Once);
        ImGui::TextUnformatted(item->path.filename().string().c_str());
        ImGui::EndDragDropSource();
      }
      if (ImGui::BeginPopupContextItem()) {
        if (ImGui::MenuItem("Rename", nullptr, false, content.Writable())) {
          state.content_rename.fill(0);
          const auto filename = item->path.filename().string();
          auto count = std::min(filename.size(), state.content_rename.size() - 1);
          while (count > 0 && count < filename.size() &&
                 (static_cast<unsigned char>(filename[count]) & 0xC0U) == 0x80U)
            --count;
          std::memcpy(state.content_rename.data(), filename.data(), count);
          state.content_rename_target = item->id;
          open_rename = true;
        }
        if (ImGui::MenuItem("Reimport", nullptr, false,
                            content.Writable() && imports != nullptr && !content.ReimportBusy()))
          reimport_asset = item->id;
        if (ImGui::MenuItem("Delete", nullptr, false, content.Writable()))
          delete_asset = item->id;
        ImGui::EndPopup();
      }
    }
  }

  if (reimport_asset && imports != nullptr)
    static_cast<void>(content.BeginReimport(*imports, *reimport_asset));
  if (delete_asset) {
    const std::array assets{*delete_asset};
    static_cast<void>(content.Delete(assets));
  }
  if (open_rename)
    ImGui::OpenPopup("Rename asset###editor.content.rename");
  if (ImGui::BeginPopupModal("Rename asset###editor.content.rename", nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::InputText("Filename", state.content_rename.data(), state.content_rename.size());
    if (ImGui::Button("Apply") && state.content_rename_target &&
        content.Rename(*state.content_rename_target, state.content_rename.data())) {
      state.content_rename_target.reset();
      ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
      state.content_rename_target.reset();
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }

  const auto selection = browser.Selection();
  state.content_selection = static_cast<std::uint32_t>(selection.size());
  ImGui::SeparatorText("Asset details");
  if (selection.size() == 1) {
    if (const auto *item = browser.Find(selection.front())) {
      ImGui::Text("Path: %s", item->path.generic_string().c_str());
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
          ImGui::BulletText("%s", target ? target->path.generic_string().c_str()
                                         : dependency.ToString().c_str());
        }
        ImGui::TreePop();
      }
      if (ImGui::TreeNode("Referenced by")) {
        if (reverse.empty())
          ImGui::TextUnformatted("None");
        for (const auto dependency : reverse) {
          const auto *target = browser.Find(dependency);
          ImGui::BulletText("%s", target ? target->path.generic_string().c_str()
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
      const auto asset_label = item != nullptr ? item->path.generic_string() : asset.ToString();
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
      const auto asset_label =
          item != nullptr ? item->path.generic_string() : conflict.asset.ToString();
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

EditorImGuiHost::~EditorImGuiHost() {
  if (state_ && state_->context) {
    // ImGui::Shutdown() (invoked by DestroyContext) asserts BackendPlatformUserData is cleared,
    // treating a non-null value as a sign the platform backend never ran its own shutdown; clear it
    // here since EditorImGuiHost is the only "backend" this context has.
    Activate(state_->context);
    ImGui::GetIO().BackendPlatformUserData = nullptr;
    ImGui::DestroyContext(state_->context);
  }
}
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
        state_->native_pointer.reset();
        state_->game_input_focused = false;
        CancelSceneGestures(*state_);
        CancelInspectorDrafts(*state_);
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
  state_->scene_canvas_viewport.reset();
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
                                       const MeshAssetCatalog *meshes) {
  Activate(state_->context);
  const bool game_running = play && play->State() != runtime::PlayState::Stopped;
  const auto play_snapshot = play ? play->Inspect() : runtime::RuntimeInspectionSnapshot{};
  state_->game_camera_combo_position.reset();
  state_->game_camera_positions.clear();
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
  state_->content_add_mesh_position.reset();
  state_->content_asset_positions.clear();
  state_->camera_align_position.reset();
  state_->play_inspector_rendered = 0;
  state_->inspector_opaque_info.clear();
  state_->profile_export_position.reset();
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
  state_->selector_visible = false;
  const bool recovery_available = workspace != nullptr && workspace->HasRecoveryJournal();
  // Query from the same root ID scope that opens the modal, before entering a panel window.
  const bool close_confirmation_open =
      state_->close_prompt_requested || ImGui::IsPopupOpen("Unsaved scene###editor.close");
  const bool interaction_blocked =
      recovery_available || state_->play_apply_open || close_confirmation_open;
  const bool scene_editable = (!workspace || workspace->Writable()) && !interaction_blocked;
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
        std::string(state_->hierarchy_create_name.data()), std::nullopt};
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
  ApplyPendingHierarchyRequests(*state_, scene, scene_editable);
  if (ImGui::Begin(hierarchy_window.c_str()))
    DrawHierarchy(*state_, scene, shell, interaction_blocked, scene_editable);
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
      DrawInspector(*state_, scene, content, meshes, scene_editable);
    }
  } else {
    CancelInspectorDrafts(*state_);
  }
  ImGui::End();
  const auto scene_window = PanelWindowName("nexora.scene");
  if (ImGui::Begin(scene_window.c_str())) {
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
        if (ImGui::SmallButton("Frame selected"))
          static_cast<void>(FrameNativeSceneSelection(*state_, *scene));
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::TextDisabled("F: frame | Right: orbit | Wheel: zoom");
        if (state_->native_scene_tool == NativeSceneTool::Rotate)
          ImGui::TextDisabled(
              "Drag colored rings: rotate | Shift: 15 deg snap | W: move | R: scale");
        else if (state_->native_scene_tool == NativeSceneTool::Scale)
          ImGui::TextDisabled(
              "Drag cubes: local/uniform scale | Shift: 0.25 snap | W: move | E: rotate");
        else
          ImGui::TextDisabled("Left drag: move X/Z | Shift+drag: move Y | E: rotate | R: scale");
        ImGui::TextDisabled("Middle: pan X/Z | Shift+middle: pan Y | Delete: selected");
        ImGui::BeginDisabled(state_->native_scene_drag_origin.has_value());
        if (ImGui::RadioButton("Move", state_->native_scene_tool == NativeSceneTool::Move))
          state_->native_scene_tool = NativeSceneTool::Move;
        ImGui::SameLine();
        if (ImGui::RadioButton("Rotate", state_->native_scene_tool == NativeSceneTool::Rotate))
          state_->native_scene_tool = NativeSceneTool::Rotate;
        ImGui::SameLine();
        if (ImGui::RadioButton("Scale", state_->native_scene_tool == NativeSceneTool::Scale)) {
          state_->native_scene_tool = NativeSceneTool::Scale;
          state_->native_scene_local_axes = true;
        }
        ImGui::BeginDisabled(state_->native_scene_tool == NativeSceneTool::Scale);
        ImGui::Checkbox("Local axes", &state_->native_scene_local_axes);
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
        if (scene_editable && !scene->Selection().empty() && !io.WantTextInput &&
            (ImGui::IsItemHovered() || ImGui::IsItemActive()) &&
            !ImGui::IsMouseDown(ImGuiMouseButton_Left) &&
            ImGui::IsKeyPressed(ImGuiKey_Delete, false)) {
          static_cast<void>(shell.RouteCommand("editor.scene.delete"));
          DeleteHierarchySelection(*state_, *scene);
          state_->native_scene_drag_origin.reset();
          state_->native_scene_drag_preview.reset();
        }
        if (!interaction_blocked && state_->scene_canvas_viewport &&
            ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
          const auto x = static_cast<std::uint32_t>(io.MousePos.x * io.DisplayFramebufferScale.x);
          const auto y = static_cast<std::uint32_t>(io.MousePos.y * io.DisplayFramebufferScale.y);
          const auto &view = *state_->scene_canvas_viewport;
          if (x >= view.x && y >= view.y && x < view.x + view.width && y < view.y + view.height) {
            state_->native_scene_pick = NativeScenePickRequest{x, y, io.KeyCtrl};
            if (scene_editable && !io.KeyCtrl)
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
        if (!interaction_blocked && (ImGui::IsItemHovered() || ImGui::IsItemActive()) &&
            !state_->native_scene_drag_origin && !io.WantTextInput) {
          if (ImGui::IsKeyPressed(ImGuiKey_W, false))
            state_->native_scene_tool = NativeSceneTool::Move;
          if (ImGui::IsKeyPressed(ImGuiKey_E, false))
            state_->native_scene_tool = NativeSceneTool::Rotate;
          if (ImGui::IsKeyPressed(ImGuiKey_R, false)) {
            state_->native_scene_tool = NativeSceneTool::Scale;
            state_->native_scene_local_axes = true;
          }
          if (ImGui::IsKeyPressed(ImGuiKey_P, false))
            state_->native_scene_center_pivot = !state_->native_scene_center_pivot;
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
        if (!state_->native_scene_drag_origin &&
            ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && !io.WantTextInput &&
            ImGui::IsKeyPressed(ImGuiKey_F, false))
          static_cast<void>(FrameNativeSceneSelection(*state_, *scene));
        AcceptSceneMeshDrop(*state_, *scene, content, meshes, scene_editable,
                            NativeMeshDropPose(*state_));
      } else {
        CancelNativeSceneGesture(*state_);
        DrawSceneOverview(*state_, *scene, scene_editable, content, meshes);
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
        ImGui::SameLine();
        ImGui::BeginDisabled(scene == nullptr || workspace == nullptr || !workspace->Writable() ||
                             interaction_blocked || state_->close_prompt_requested);
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
      const auto records = console->Snapshot();
      ImGui::Text("%zu records | %llu dropped", records.size(),
                  static_cast<unsigned long long>(console->DroppedCount()));
      std::vector<const runtime::RuntimeLogRecord *> visible;
      visible.reserve(records.size());
      const auto minimum = static_cast<runtime::RuntimeLogSeverity>(state_->console_min_severity);
      for (const auto &record : records) {
        if (record.severity < minimum)
          continue;
        const std::string searchable = record.category + " " + record.source + " " + record.message;
        if (state_->console_filter.PassFilter(searchable.c_str()))
          visible.push_back(&record);
      }
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
      bool capturing = profile->Capturing();
      if (ImGui::Checkbox("Capture", &capturing))
        profile->SetCapturing(capturing);
      ImGui::SameLine();
      if (ImGui::Button("Clear"))
        profile->Clear();
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
      ImGui::EndDisabled();
      if (!state_->profile_export_status.empty())
        ImGui::TextWrapped("%s", state_->profile_export_status.c_str());
      ImGui::Text("%zu frames retained | %llu older frames dropped", samples.size(),
                  static_cast<unsigned long long>(profile->DroppedCount()));
      ImGui::TextDisabled("Editor frame processing: wall time after BeginFrame, before Present.");
      ImGui::TextDisabled("GPU time and process memory are not instrumented.");
      if (!samples.empty()) {
        std::vector<float> values;
        values.reserve(samples.size());
        double sum = 0.0;
        double peak = 0.0;
        float maximum = 1.0F;
        for (const auto &sample : samples) {
          values.push_back(static_cast<float>(
              std::min(sample.cpu_ms, static_cast<double>(std::numeric_limits<float>::max()))));
          sum += sample.cpu_ms;
          peak = std::max(peak, sample.cpu_ms);
          maximum = std::max(maximum, values.back());
        }
        ImGui::Text("Latest %.2f ms | Average %.2f ms | Peak %.2f ms", samples.back().cpu_ms,
                    sum / static_cast<double>(samples.size()), peak);
        ImGui::PlotLines("Frame processing (ms)", values.data(), static_cast<int>(values.size()), 0,
                         nullptr, 0.0F, maximum, ImVec2(-1.0F, 120.0F));
      }
    }
  }
  ImGui::End();
  if (content != nullptr)
    DrawContentBrowser(*state_, *content, imports, scene, meshes, scene_editable);
  DrawProjectPanel(*state_, workspace, recent_projects);
  if (state_->focus_initial_scene) {
    auto *scene_window = ImGui::FindWindowByName(
        PanelWindowName(game_running ? "nexora.game" : "nexora.scene").c_str());
    if (scene_window != nullptr && scene_window->DockNode != nullptr &&
        scene_window->DockNode->TabBar != nullptr) {
      scene_window->DockNode->SelectedTabId = scene_window->TabId;
      scene_window->DockNode->TabBar->SelectedTabId = scene_window->TabId;
      scene_window->DockNode->TabBar->NextSelectedTabId = scene_window->TabId;
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
                           play->State() != runtime::PlayState::Paused || !workspace ||
                           !workspace->Writable() || state_->close_prompt_requested);
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
    ImGui::TextUnformatted("Save scene changes before closing?");
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
    ImGui::EndPopup();
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

void EditorImGuiHost::RequestCloseConfirmation() noexcept {
  state_->play_apply_open = false;
  state_->play_apply_popup_pending = false;
  state_->game_input_focused = false;
  state_->close_prompt_requested = true;
}

CloseChoice EditorImGuiHost::TakeCloseChoice() noexcept {
  return std::exchange(state_->close_choice, CloseChoice::None);
}

bool EditorImGuiHost::GameInputFocused() const noexcept { return state_->game_input_focused; }
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
void EditorImGuiHost::SetProfileExportStatus(std::string message) {
  state_->profile_export_status = std::move(message);
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
  if (!texture.IsValid() || (renderer.device != nullptr && renderer.device != &device))
    return 0;
  renderer.device = &device;
  for (std::uint32_t index = 1; index < renderer.textures.size(); ++index) {
    auto &slot = renderer.textures[index];
    if (!slot.live) {
      slot.texture = texture;
      slot.live = true;
      return TextureId(index, slot.generation);
    }
  }
  renderer.textures.push_back({texture, 1, true});
  return TextureId(static_cast<std::uint32_t>(renderer.textures.size() - 1), 1);
}

bool EditorImGuiHost::UnregisterTexture(std::uint64_t texture_id) noexcept {
  const auto index = static_cast<std::uint32_t>(texture_id);
  const auto generation = static_cast<std::uint32_t>(texture_id >> 32U);
  auto &textures = state_->renderer.textures;
  if (index == 0 || index >= textures.size() || !textures[index].live ||
      textures[index].generation != generation)
    return false;
  textures[index].live = false;
  textures[index].texture = {};
  ++textures[index].generation;
  if (textures[index].generation == 0)
    textures[index].generation = 1;
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
  if (draw == nullptr || width == 0 || height == 0)
    return Nexora::Presentation::SurfaceStatus::InvalidDescriptor;
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
      commands.push_back({static_cast<std::int32_t>(left), static_cast<std::int32_t>(top),
                          static_cast<std::uint32_t>(right - left),
                          static_cast<std::uint32_t>(bottom - top),
                          static_cast<std::uint64_t>(command.GetTexID()), command.ElemCount,
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
  const auto status = surface.RenderUi(
      {vertices, indices, commands, uploads, sizeof(ImDrawIdx) == sizeof(std::uint32_t)});
  if (status == Nexora::Presentation::SurfaceStatus::Ready)
    state_->surface_font_generation = state_->font_generation;
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

void EditorImGuiTestAccess::SetInputTrickle(EditorImGuiHost &host, bool enabled) noexcept {
  Activate(host.state_->context);
  ImGui::GetIO().ConfigInputTrickleEventQueue = enabled;
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
std::optional<std::array<float, 2>>
EditorImGuiTestAccess::ContentAddMeshPosition(const EditorImGuiHost &host) noexcept {
  return host.state_->content_add_mesh_position;
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
std::optional<std::array<float, 2>>
EditorImGuiTestAccess::ProfileExportPosition(const EditorImGuiHost &host) noexcept {
  return host.state_->profile_export_position;
}

void EditorImGuiTestAccess::FocusHierarchy(EditorImGuiHost &host) noexcept {
  Activate(host.state_->context);
  const auto name = PanelWindowName("nexora.hierarchy");
  ImGui::SetWindowFocus(name.c_str());
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

void EditorImGuiTestAccess::FocusInspector(EditorImGuiHost &host) noexcept {
  Activate(host.state_->context);
  const auto name = PanelWindowName("nexora.inspector");
  ImGui::SetWindowFocus(name.c_str());
}
void EditorImGuiTestAccess::CollapseInspector(EditorImGuiHost &host, bool collapsed) noexcept {
  Activate(host.state_->context);
  const auto name = PanelWindowName("nexora.inspector");
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
