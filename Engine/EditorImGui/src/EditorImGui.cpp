#include "Nexora/EditorImGui/EditorImGui.h"
#include "Nexora/Editor/InspectorRotation.h"
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
  ImVec2 scene_center_world{};
  float scene_pixels_per_unit = 32.0F;
  struct SceneDrag final {
    enum class Axis { Free, X, Z };
    std::vector<SceneDocument::NodeKey> entities;
    ImVec2 start_mouse;
    float pixels_per_unit{};
    Axis axis{Axis::Free};
  };
  std::optional<SceneDrag> scene_drag;
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
  std::uint32_t inspector_selection = 0;
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
  const auto content_window = PanelWindowName("nexora.content");
  const auto scene_window = PanelWindowName("nexora.scene");
  const auto game_window = PanelWindowName("nexora.game");
  ImGui::DockBuilderDockWindow(project_window.c_str(), hierarchy);
  ImGui::DockBuilderDockWindow(hierarchy_window.c_str(), hierarchy);
  ImGui::DockBuilderDockWindow(console_window.c_str(), console);
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

template <typename StateT> void ApplyPendingHierarchyRequests(StateT &state, SceneDocument *scene) {
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
                   bool recovery_available) {
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
  ImGui::BeginDisabled(recovery_available || scene->Selection().empty());
  if (ImGui::SmallButton("Delete selected")) {
    static_cast<void>(shell.RouteCommand("editor.scene.delete"));
    DeleteHierarchySelection(state, *scene);
  }
  ImGui::EndDisabled();
  if (!recovery_available && ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
      !ImGui::GetIO().WantTextInput && ImGui::IsKeyPressed(ImGuiKey_Delete, false)) {
    static_cast<void>(shell.RouteCommand("editor.scene.delete"));
    DeleteHierarchySelection(state, *scene);
  }
  ImGui::Separator();
  ImGui::BeginDisabled(scene->Selection().empty() || recovery_available);
  if (ImGui::SmallButton("Copy"))
    CopyHierarchySelection(state, *scene);
  ImGui::EndDisabled();
  ImGui::SameLine();
  ImGui::BeginDisabled(recovery_available);
  if (ImGui::SmallButton("Paste"))
    PasteHierarchySelection(state, *scene);
  ImGui::EndDisabled();
  ImGui::SameLine();
  ImGui::BeginDisabled(scene->Selection().empty() || recovery_available);
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

  ImGui::BeginDisabled(selected_node == nullptr);
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
      if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
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
  if (ImGui::BeginDragDropTarget()) {
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

template <typename StateT> void DrawSceneOverview(StateT &state, SceneDocument &scene) {
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
  const auto available = ImGui::GetContentRegionAvail();
  const ImVec2 size{std::max(available.x, 1.0F), std::max(available.y, 160.0F)};
  ImGui::InvisibleButton("##scene-overview", size,
                         ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonMiddle);
  const auto min = ImGui::GetItemRectMin();
  const auto max = ImGui::GetItemRectMax();
  const ImVec2 center{(min.x + max.x) * 0.5F, (min.y + max.y) * 0.5F};
  const auto &io = ImGui::GetIO();
  if (state.scene_drag) {
    if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
      state.scene_drag.reset();
    } else if (!io.MouseDown[ImGuiMouseButton_Left]) {
      if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
        const auto &drag = *state.scene_drag;
        const float dx =
            drag.axis == StateT::SceneDrag::Axis::Z ? 0.0F : io.MousePos.x - drag.start_mouse.x;
        const float dy =
            drag.axis == StateT::SceneDrag::Axis::X ? 0.0F : io.MousePos.y - drag.start_mouse.y;
        if (dx * dx + dy * dy >= 36.0F &&
            !scene.TranslateSelectionXZ(drag.entities, dx / drag.pixels_per_unit,
                                        dy / drag.pixels_per_unit))
          state.hierarchy_error =
              "Scene drag rejected because an entity changed or the pose is invalid.";
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
    preview_pixels = {io.MousePos.x - state.scene_drag->start_mouse.x,
                      io.MousePos.y - state.scene_drag->start_mouse.y};
    if (state.scene_drag->axis == StateT::SceneDrag::Axis::X)
      preview_pixels.y = 0.0F;
    else if (state.scene_drag->axis == StateT::SceneDrag::Axis::Z)
      preview_pixels.x = 0.0F;
    if (preview_pixels.x * preview_pixels.x + preview_pixels.y * preview_pixels.y >= 36.0F)
      draw->AddLine(state.scene_drag->start_mouse,
                    {state.scene_drag->start_mouse.x + preview_pixels.x,
                     state.scene_drag->start_mouse.y + preview_pixels.y},
                    IM_COL32(255, 199, 87, 255), 2.0F);
    else
      preview_pixels = {};
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
      if (!io.KeyCtrl && !io.KeyShift) {
        std::vector<SceneDocument::NodeKey> keys;
        for (const auto id : scene.Selection()) {
          const auto key = scene.Key(id);
          if (key)
            keys.push_back(*key);
        }
        if (!keys.empty())
          state.scene_drag = typename StateT::SceneDrag{std::move(keys), io.MousePos,
                                                        state.scene_pixels_per_unit, axis};
      }
    } else if (!io.KeyCtrl && !io.KeyShift) {
      static_cast<void>(scene.Select(std::span<const runtime::Id>{}));
      state.hierarchy_selection_anchor.reset();
    }
  }
}

template <typename StateT>
EulerDegrees InspectorAngles(StateT &state, const SceneDocument &scene, SceneDocument::NodeKey key,
                             const runtime::Transform &transform) {
  const auto degrees = scene.EulerAngles(key.id).value_or(EulerDegrees{});
  state.inspector_euler_hints.insert_or_assign(
      key.id, typename StateT::InspectorEulerHint{key, transform, degrees});
  return degrees;
}

template <typename StateT> void ApplyInspectorEuler(StateT &state, SceneDocument &scene) {
  if (!state.inspector_euler_request)
    return;
  const auto request = std::exchange(state.inspector_euler_request, std::nullopt);
  if (!scene.SetEulerField(request->entities, request->axis, request->degrees)) {
    state.inspector_error =
        "Rotation edit rejected because its values or entity generation are stale.";
    return;
  }
  for (const auto key : request->entities)
    static_cast<void>(InspectorAngles(state, scene, key, *scene.Transform(key.id)));
  state.inspector_error.clear();
}

template <typename StateT> void DrawInspector(StateT &state, SceneDocument *scene) {
  state.inspector_selection =
      scene == nullptr ? 0U : static_cast<std::uint32_t>(scene->Selection().size());
  state.inspector_transform_visible = false;
  std::unordered_set<runtime::Id> selected_entities;
  if (scene != nullptr)
    selected_entities.insert(scene->Selection().begin(), scene->Selection().end());
  std::erase_if(state.inspector_euler_hints, [&](const auto &entry) {
    const auto &hint = entry.second;
    return scene == nullptr || scene->Key(hint.entity.id) != hint.entity ||
           !selected_entities.contains(hint.entity.id);
  });
  if (scene == nullptr || scene->Selection().empty()) {
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
  ImGui::SeparatorText("Transform");
  struct Field final {
    const char *label;
    double runtime::Transform::*member;
  };
  constexpr std::array fields{
      Field{"Position X", &runtime::Transform::x}, Field{"Position Y", &runtime::Transform::y},
      Field{"Position Z", &runtime::Transform::z}, Field{"Scale X", &runtime::Transform::sx},
      Field{"Scale Y", &runtime::Transform::sy},   Field{"Scale Z", &runtime::Transform::sz}};
  for (const auto &field : fields) {
    double value = transforms.front().*(field.member);
    const bool mixed = std::ranges::any_of(
        transforms, [&](const auto &transform) { return transform.*(field.member) != value; });
    if (mixed)
      ImGui::PushItemFlag(ImGuiItemFlags_MixedValue, true);
    const bool changed = ImGui::InputScalar(field.label, ImGuiDataType_Double, &value);
    if (mixed)
      ImGui::PopItemFlag();
    if (changed) {
      for (auto &transform : transforms)
        transform.*(field.member) = value;
      state.inspector_transform_request.emplace(
          typename StateT::InspectorTransformRequest{keys, transforms});
    }
  }

  ImGui::SeparatorText("Local rotation (degrees, Z-X-Y)");
  ImGui::TextDisabled("Press Enter to apply a rotation.");
  std::vector<EulerDegrees> angles;
  angles.reserve(keys.size());
  for (std::size_t i = 0; i < keys.size(); ++i)
    angles.push_back(InspectorAngles(state, *scene, keys[i], transforms[i]));
  if (state.inspector_euler_selection != keys) {
    state.inspector_euler_selection = keys;
    state.inspector_euler_active = {};
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
  ImGui::PopID();

  if (state.inspector_transform_request) {
    const auto request = std::exchange(state.inspector_transform_request, std::nullopt);
    if (!scene->SetTransforms(request->entities, request->transforms))
      state.inspector_error =
          "Transform edit rejected because its values or entity generation are stale.";
    else
      state.inspector_error.clear();
  }
  ApplyInspectorEuler(state, *scene);
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
void DrawContentBrowser(StateT &state, ProjectContentSession &content, AssetImportQueue *imports) {
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
      if (ImGui::BeginDragDropSource()) {
        const AssetDragData payload{browser.ProjectGeneration(), item->id};
        ImGui::SetDragDropPayload(AssetDragPayload::kType.data(), &payload, sizeof(payload));
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
  dpi_scale = std::max(dpi_scale, 0.25F);
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
      io.AddMousePosEvent(static_cast<float>(event.value0), static_cast<float>(event.value1));
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
      if (event.value0 > 0 && event.value0 <= 0x10ffff &&
          !(event.value0 >= 0xd800 && event.value0 <= 0xdfff))
        io.AddInputCharacter(static_cast<unsigned int>(event.value0));
      break;
    case Nexora::Window::WindowEventType::FocusChanged:
      state_->app_focused = event.value0 != 0;
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
                                       runtime::PlaySession *play) {
  Activate(state_->context);
  state_->selector_visible = false;
  const bool recovery_available = workspace != nullptr && workspace->HasRecoveryJournal();
  if (!recovery_available &&
      ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_S, ImGuiInputFlags_RouteGlobal)) {
    static_cast<void>(shell.RouteCommand("editor.scene.save"));
    state_->scene_save_requested = true;
  }
  if (play != nullptr && !recovery_available && !ImGui::GetIO().WantTextInput) {
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
  if (scene != nullptr && !recovery_available && !ImGui::GetIO().WantTextInput &&
      ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_N, ImGuiInputFlags_RouteGlobal)) {
    static_cast<void>(shell.RouteCommand("editor.scene.create"));
    state_->hierarchy_create_request = State::HierarchyCreateRequest{
        std::string(state_->hierarchy_create_name.data()), std::nullopt};
  }
  if (scene != nullptr && !recovery_available && !ImGui::GetIO().WantTextInput &&
      ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_Z, ImGuiInputFlags_RouteGlobal)) {
    static_cast<void>(shell.RouteCommand("editor.scene.undo"));
    state_->scene_save_message = scene->Undo() ? "Undo complete." : "Nothing to undo.";
    state_->scene_save_success = true;
    state_->hierarchy_selection_anchor.reset();
  }
  if (scene != nullptr && !recovery_available && !ImGui::GetIO().WantTextInput) {
    if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_C, ImGuiInputFlags_RouteGlobal)) {
      static_cast<void>(shell.RouteCommand("editor.scene.copy"));
      CopyHierarchySelection(*state_, *scene);
    }
    if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_V, ImGuiInputFlags_RouteGlobal)) {
      static_cast<void>(shell.RouteCommand("editor.scene.paste"));
      PasteHierarchySelection(*state_, *scene);
    }
    if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_D, ImGuiInputFlags_RouteGlobal)) {
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
  ApplyPendingHierarchyRequests(*state_, scene);
  if (ImGui::Begin(hierarchy_window.c_str()))
    DrawHierarchy(*state_, scene, shell, recovery_available);
  ImGui::End();
  const auto inspector_window = PanelWindowName("nexora.inspector");
  if (ImGui::Begin(inspector_window.c_str()))
    DrawInspector(*state_, scene);
  ImGui::End();
  const auto scene_window = PanelWindowName("nexora.scene");
  if (ImGui::Begin(scene_window.c_str())) {
    ImGui::BeginDisabled(scene == nullptr || recovery_available);
    if (ImGui::Button("Undo")) {
      static_cast<void>(shell.RouteCommand("editor.scene.undo"));
      state_->scene_save_message =
          scene != nullptr && scene->Undo() ? "Undo complete." : "Nothing to undo.";
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
    if (scene != nullptr)
      DrawSceneOverview(*state_, *scene);
  }
  ImGui::End();
  const auto game_window = PanelWindowName("nexora.game");
  if (ImGui::Begin(game_window.c_str())) {
    if (play == nullptr) {
      ImGui::TextDisabled("Play session unavailable.");
    } else {
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
      }
      ImGui::Text("State: %s", state == runtime::PlayState::Stopped   ? "Stopped"
                               : state == runtime::PlayState::Playing ? "Playing"
                                                                      : "Paused");
      ImGui::Text("Fixed ticks: %llu | Manual steps: %llu",
                  static_cast<unsigned long long>(play->Stats().fixed_ticks),
                  static_cast<unsigned long long>(play->Stats().manual_steps));
      const auto snapshot = play->Inspect();
      ImGui::Text("Play World entities: %zu", snapshot.entities.size());
      ImGui::Separator();
      DrawPlayOverview(snapshot);
      ImGuiListClipper clipper;
      clipper.Begin(static_cast<int>(snapshot.entities.size()));
      while (clipper.Step())
        for (int index = clipper.DisplayStart; index < clipper.DisplayEnd; ++index) {
          const auto &entity = snapshot.entities[static_cast<std::size_t>(index)];
          ImGui::Text("#%llu  world (%.2f, %.2f, %.2f)", static_cast<unsigned long long>(entity.id),
                      entity.world_transform.x, entity.world_transform.y, entity.world_transform.z);
        }
    }
  }
  ImGui::End();
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
  if (content != nullptr)
    DrawContentBrowser(*state_, *content, imports);
  DrawProjectPanel(*state_, workspace, recent_projects);
  if (state_->focus_initial_scene) {
    auto *scene_window = ImGui::FindWindowByName(PanelWindowName("nexora.scene").c_str());
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
    if (ImGui::Button("Cancel")) {
      state_->close_choice = CloseChoice::Cancel;
      ImGui::CloseCurrentPopup();
    }
    if (!state_->scene_save_success && !state_->scene_save_message.empty())
      ImGui::TextWrapped("%s", state_->scene_save_message.c_str());
    ImGui::EndPopup();
  }
}

bool EditorImGuiHost::TakeSceneSaveRequest() noexcept {
  return std::exchange(state_->scene_save_requested, false);
}

void EditorImGuiHost::SetSceneSaveResult(std::string message, bool success) {
  state_->scene_save_message = std::move(message);
  state_->scene_save_success = success;
}

void EditorImGuiHost::RequestCloseConfirmation() noexcept { state_->close_prompt_requested = true; }

CloseChoice EditorImGuiHost::TakeCloseChoice() noexcept {
  return std::exchange(state_->close_choice, CloseChoice::None);
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
