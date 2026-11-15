#if defined(NEXORA_HAS_2D)
namespace {
template <class StateT>
void DrawSpriteSourceCreator(StateT &state, ProjectWorkspace *workspace,
                             ProjectContentSession *content) {
  const bool edit_requested = state.sprite_editor_open.has_value();
  const auto popup = edit_requested || (state.sprite_creator && state.sprite_creator->editing)
                         ? "Edit Sprite source###editor.content.sprite-edit"
                         : "Create Sprite source###editor.content.sprite-create";
  const auto root = workspace ? workspace->Root() : std::filesystem::path{};
  if (state.sprite_creator_scope != root) {
    state.sprite_creator_scope = root;
    state.sprite_creator_status.clear();
  }
  const auto editing = std::exchange(state.sprite_editor_open, std::nullopt);
  const auto creating = std::exchange(state.sprite_creator_open, std::nullopt);
  if (const auto open = editing ? editing : creating) {
    const auto *source_item = editing && content ? content->Browser().Find(*open) : nullptr;
    const auto source =
        source_item && source_item->twod ? source_item->twod->SpriteSource() : nullptr;
    const auto *item = content ? content->Browser().Find(source ? source->atlas : *open) : nullptr;
    const auto atlas = item && item->twod ? item->twod->Atlas() : nullptr;
    if (editing &&
        (!source || source_item->type != ".nx2sprite" ||
         source_item->thumbnail != ThumbnailState::Ready || source->id != *open || !atlas ||
         std::ranges::find(atlas->regions, source->region, &twod::Region::id) ==
             atlas->regions.end())) {
      state.sprite_creator_status = "The imported source or its Atlas region is unavailable.";
      return;
    }
    if (state.sprite_creator_allowed && workspace && atlas && !atlas->regions.empty() &&
        atlas->regions.size() <= 1024 && atlas->id == item->id &&
        item->thumbnail == ThumbnailState::Ready && item->type == item->twod->Type()) {
      state.sprite_creator.emplace();
      auto &draft = *state.sprite_creator;
      draft.project = workspace->Project().id;
      draft.root = workspace->Root();
      draft.folder = item->path.parent_path();
      draft.version = {content->Browser().ProjectGeneration(), content->Browser().Revision()};
      draft.atlas = atlas->id;
      draft.atlas_label = PathLabel(item->path);
      draft.regions = atlas->regions;
      draft.canvas = {int(draft.regions.front().width), int(draft.regions.front().height)};
      draft.pivot = {draft.canvas[0] * .5F, draft.canvas[1] * .5F};
      if (source) {
        draft.editing = SpriteSourceEditingRequest{
            draft.project, draft.root, source_item->path, draft.version, source_item->artifact_hash,
            *source,       *source};
        draft.region =
            std::size_t(std::ranges::find(draft.regions, source->region, &twod::Region::id) -
                        draft.regions.begin());
        draft.canvas = {int(source->sourceWidth), int(source->sourceHeight)};
        draft.trim = {int(source->trimX), int(source->trimY)};
        draft.pivot = {source->pivotX, source->pivotY};
        draft.ppu = source->pixelsPerUnit;
        for (std::size_t index = 0; index < 4; ++index)
          draft.borders[index] = int(source->border[index]);
      }
      state.sprite_creator_status.clear();
      ImGui::OpenPopup(popup);
    }
  }
  if (!state.sprite_creator)
    return;
  auto &draft = *state.sprite_creator;
  const bool scope = workspace && content && draft.root == workspace->Root() &&
                     draft.project == workspace->Project().id && content->Root() == draft.root;
  const bool current =
      scope && draft.version == SpriteSourceContentVersion{content->Browser().ProjectGeneration(),
                                                           content->Browser().Revision()};
  if (!current || !state.sprite_creator_allowed) {
    state.sprite_creator_request.reset();
    state.sprite_editor_request.reset();
    draft.submitted = 0;
  }
  ImGui::SetNextWindowSize(ImVec2(440, draft.editing ? 650.0F : 550.0F), ImGuiCond_Appearing);
  if (!ImGui::BeginPopupModal(popup, nullptr, ImGuiWindowFlags_NoSavedSettings)) {
    state.sprite_creator_request.reset();
    state.sprite_editor_request.reset();
    state.sprite_creator.reset();
    return;
  }
  if (!scope || draft.dismiss) {
    ImGui::CloseCurrentPopup();
    state.sprite_creator.reset();
    state.sprite_creator_request.reset();
    state.sprite_editor_request.reset();
    ImGui::EndPopup();
    return;
  }
  const auto capture = [&](std::size_t index) {
    const auto *window = ImGui::GetCurrentWindow();
    if (!window->Hidden && !window->SkipItems && ImGui::IsItemVisible()) {
      const auto lo = ImGui::GetItemRectMin(), hi = ImGui::GetItemRectMax();
      state.sprite_creator_controls[index] = std::array{lo.x, lo.y, hi.x, hi.y};
    }
  };
  ImGui::TextWrapped("Atlas: %s", draft.atlas_label.c_str());
  const auto folder = PathLabel(draft.folder);
  ImGui::TextWrapped("Destination: %s", folder.c_str());
  if (draft.editing) {
    const auto path = PathLabel(draft.editing->path);
    ImGui::TextWrapped("Source: %s", path.c_str());
    ImGui::TextWrapped("Saving updates every reference to this source. Scene Undo does not revert "
                       "saved source files.");
  } else {
    ImGui::TextUnformatted("Name (without .nx2sprite)");
    ImGui::SetNextItemWidth(-1);
    ImGui::InputText("##sprite-create-name", draft.name.data(), draft.name.size());
    capture(0);
  }
  const auto &region = draft.regions[draft.region];
  const auto label = "Region " + std::to_string(region.id) + " (" + std::to_string(region.width) +
                     " x " + std::to_string(region.height) + ")";
  ImGui::TextUnformatted("Atlas region");
  ImGui::SetNextItemWidth(-1);
  const bool combo = ImGui::BeginCombo("##sprite-create-region", label.c_str());
  capture(1);
  if (combo) {
    ImGuiListClipper clipper;
    clipper.Begin(int(draft.regions.size()));
    while (clipper.Step())
      for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
        const auto &choice = draft.regions[std::size_t(i)];
        const auto text = "Region " + std::to_string(choice.id) + " (" +
                          std::to_string(choice.width) + " x " + std::to_string(choice.height) +
                          ")";
        if (ImGui::Selectable(text.c_str(), draft.region == std::size_t(i)))
          draft.region = std::size_t(i);
      }
    ImGui::EndCombo();
  }
  const auto integers = [&](const char *title, const char *first, const char *second,
                            std::array<int, 2> &values, std::size_t index) {
    ImGui::TextUnformatted(title);
    ImGui::SetNextItemWidth(180);
    ImGui::InputInt(first, &values[0], 0, 0);
    capture(index);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(180);
    ImGui::InputInt(second, &values[1], 0, 0);
    capture(index + 1);
  };
  integers("Original size: width / height (px)", "##sprite-create-width", "##sprite-create-height",
           draft.canvas, 2);
  integers("Trim origin: X / Y (top-left px)", "##sprite-create-trim-x", "##sprite-create-trim-y",
           draft.trim, 4);
  ImGui::TextUnformatted("Pivot: X / Y (bottom-left px)");
  ImGui::SetNextItemWidth(180);
  ImGui::InputFloat("##sprite-create-pivot-x", &draft.pivot[0], 0, 0, "%.3f");
  capture(6);
  ImGui::SameLine();
  ImGui::SetNextItemWidth(180);
  ImGui::InputFloat("##sprite-create-pivot-y", &draft.pivot[1], 0, 0, "%.3f");
  capture(7);
  ImGui::TextUnformatted("Pixels per unit");
  ImGui::SetNextItemWidth(180);
  ImGui::InputFloat("##sprite-create-ppu", &draft.ppu, 0, 0, "%.3f");
  capture(8);
  const auto &selected = draft.regions[draft.region];
  twod::SpriteAsset source;
  // Validation witness only; the file owner assigns the new identity.
  source.id =
      draft.atlas == runtime::AssetUuid{1, 1} ? runtime::AssetUuid{1, 2} : runtime::AssetUuid{1, 1};
  if (draft.editing)
    source.id = draft.editing->baseline.id;
  source.atlas = draft.atlas;
  source.region = selected.id;
  source.sourceWidth = std::uint32_t(std::max(0, draft.canvas[0]));
  source.sourceHeight = std::uint32_t(std::max(0, draft.canvas[1]));
  source.trimX = std::uint32_t(std::max(0, draft.trim[0]));
  source.trimY = std::uint32_t(std::max(0, draft.trim[1]));
  source.trimWidth = selected.width;
  source.trimHeight = selected.height;
  source.pivotX = draft.pivot[0];
  source.pivotY = draft.pivot[1];
  source.pixelsPerUnit = draft.ppu;
  bool borders = true;
  if (draft.editing) {
    ImGui::TextUnformatted("Borders: left / right / top / bottom (px)");
    for (std::size_t index = 0; index < 4; ++index) {
      ImGui::PushID(int(index));
      ImGui::SetNextItemWidth(80);
      ImGui::InputInt("##sprite-edit-border", &draft.borders[index], 0, 0);
      capture(11 + index);
      ImGui::PopID();
      if (index != 3)
        ImGui::SameLine();
      borders = borders && draft.borders[index] >= 0;
      source.border[index] = std::uint32_t(std::max(0, draft.borders[index]));
    }
  }
  const bool changed = !draft.editing || source != draft.editing->baseline;
  const bool geometry = borders && draft.canvas[0] > 0 && draft.canvas[1] > 0 &&
                        draft.trim[0] >= 0 && draft.trim[1] >= 0 &&
                        twod::ValidateSpriteAsset(source);
  const bool name = draft.editing || ValidSpriteSourceName(draft.name.data());
  ImGui::TextUnformatted(draft.editing ? "Trim size follows the region. Source UUID is preserved."
                                       : "Trim size follows the region. Borders: 0 / 0 / 0 / 0.");
  const auto message = !current ? SpriteSourceCreationMessage(SpriteSourceCreationError::Stale)
                       : !state.sprite_creator_allowed
                           ? SpriteSourceCreationMessage(SpriteSourceCreationError::Denied)
                       : !name ? SpriteSourceCreationMessage(SpriteSourceCreationError::Name)
                       : !geometry
                           ? SpriteSourceCreationMessage(SpriteSourceCreationError::Geometry)
                       : !changed ? SpriteSourceEditingMessage(SpriteSourceEditingError::Unchanged)
                                  : std::string_view(draft.status);
  ImGui::TextWrapped("%.*s", int(message.size()), message.data());
  const bool allowed = state.sprite_creator_allowed && current && name && geometry && changed &&
                       !draft.submitted && !state.sprite_creator_request &&
                       !state.sprite_editor_request &&
                       state.sprite_creator_next != std::numeric_limits<std::uint64_t>::max();
  ImGui::BeginDisabled(!allowed);
  if (ImGui::Button(draft.editing ? "Save source" : "Create source")) {
    const auto identity = state.sprite_creator_next++;
    if (draft.editing) {
      auto request = *draft.editing;
      request.source = source;
      state.sprite_editor_request = SpriteSourceEditingUiRequest{identity, std::move(request)};
    } else {
      source.id = {};
      state.sprite_creator_request = SpriteSourceCreationUiRequest{
          identity,
          {draft.project, draft.root, draft.folder, draft.version, draft.name.data(), source}};
    }
    draft.submitted = identity;
  }
  capture(9);
  ImGui::EndDisabled();
  ImGui::SameLine();
  if (ImGui::Button("Cancel")) {
    ImGui::CloseCurrentPopup();
    state.sprite_creator_request.reset();
    state.sprite_editor_request.reset();
    state.sprite_creator.reset();
  }
  capture(10);
  ImGui::EndPopup();
}
} // namespace

std::optional<SpriteSourceCreationUiRequest> EditorImGuiHost::TakeSpriteSourceCreationRequest() {
  auto request = std::exchange(state_->sprite_creator_request, std::nullopt);
  if (!state_->app_focused || !state_->sprite_creator_allowed || !state_->sprite_creator ||
      !request || state_->sprite_creator->submitted != request->identity)
    return std::nullopt;
  return request;
}
std::optional<SpriteSourceEditingUiRequest> EditorImGuiHost::TakeSpriteSourceEditingRequest() {
  auto request = std::exchange(state_->sprite_editor_request, std::nullopt);
  if (!state_->app_focused || !state_->sprite_creator_allowed || !state_->sprite_creator ||
      !state_->sprite_creator->editing || !request ||
      state_->sprite_creator->submitted != request->identity)
    return std::nullopt;
  return request;
}
void EditorImGuiHost::SetSpriteSourceEditingResult(std::uint64_t identity, bool saved,
                                                   std::string message) {
  SetSpriteSourceCreationResult(identity, saved, std::move(message));
}
void EditorImGuiHost::SetSpriteSourceCreationResult(std::uint64_t identity, bool file_created,
                                                    std::string message) {
  if (!state_->sprite_creator || state_->sprite_creator->submitted != identity)
    return;
  state_->sprite_creator->submitted = 0;
  state_->sprite_creator->status = message;
  state_->sprite_creator->dismiss = file_created;
  state_->sprite_creator_status = std::move(message);
}
#endif
