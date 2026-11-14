// Serialized authoring-thread presentation. The popup owns only bounded identity/path metadata;
// it performs no IO and grants no Build authority. Final capture still validates the complete
// assets.
template <typename StateT>
void DrawBuildAnimationPicker(StateT &state, const ProjectWorkspace &workspace,
                              ProjectContentSession *content, bool blocked) {
  const auto capture = []() -> std::optional<std::array<float, 2>> {
    const auto *window = ImGui::GetCurrentWindow();
    if (!ImGui::IsItemVisible() || window->Hidden)
      return std::nullopt;
    const auto low = ImGui::GetItemRectMin(), high = ImGui::GetItemRectMax();
    const ImVec2 center{(low.x + high.x) * .5F, (low.y + high.y) * .5F};
    return window->ClipRect.Contains(center) ? std::optional{std::array{center.x, center.y}}
                                             : std::nullopt;
  };
  const bool scoped =
      content && content->Root() == workspace.Root() && content->Browser().ProjectGeneration() != 0;
  auto &picker = state.build_animation_picker;
  const bool opening = detail::IconButton("Animation assets", detail::EditorIcon::Animation);
  state.build_animation_positions[0] = capture();
  // Only an explicit user open captures context. A hidden, replaced or detached popup cannot
  // silently acquire a different project/catalog when it appears again.
  if (opening) {
    picker.emplace();
    picker->owner = reinterpret_cast<std::uintptr_t>(content);
    picker->root = workspace.Root();
    picker->frame = ImGui::GetFrameCount();
    if (scoped) {
      picker->generation = content->Browser().ProjectGeneration();
      picker->revision = content->Browser().Revision();
#if defined(NEXORA_HAS_2D)
      for (const auto &item : content->Browser().Items()) {
        if (!item.twod || !item.twod->Clip() || item.twod->Id() != item.id ||
            item.type != item.twod->Type() || item.thumbnail != ThumbnailState::Ready)
          continue;
        const auto path = PathLabel(item.path);
        if (picker->choices.size() == 4096 ||
            path.size() > ProjectBuildSettings::kMaximumPathBytes ||
            !foundation::IsValidUtf8(path) || path.find('\0') != path.npos) {
          picker->error = "Imported animation choices exceed the display limits.";
          picker->choices.clear();
          break;
        }
        picker->choices.push_back({item.id, path});
      }
      std::ranges::sort(picker->choices, [](const auto &a, const auto &b) {
        return std::tie(a.path, a.id.high, a.id.low) < std::tie(b.path, b.id.high, b.id.low);
      });
      for (std::size_t i = 0; i < picker->choices.size(); ++i)
        picker->index.emplace(picker->choices[i].id, i);
#else
      picker->error = "Animation asset selection requires the TwoD feature.";
#endif
    }
    ImGui::OpenPopup("Build animation assets");
  }
  const auto display = ImGui::GetIO().DisplaySize;
  // BeginPopup enables auto-sizing. Supply the bounded size every frame because the two
  // virtualized children divide available space; auto-sizing that result would move controls.
  ImGui::SetNextWindowSize({std::min(620.0F, display.x - 32), std::min(540.0F, display.y - 48)});
  if (!ImGui::BeginPopup("Build animation assets")) {
    if (!ImGui::IsPopupOpen("Build animation assets"))
      picker.reset();
    return;
  }
  const bool current = picker && picker->owner == reinterpret_cast<std::uintptr_t>(content) &&
                       picker->root == workspace.Root() &&
                       picker->frame >= ImGui::GetFrameCount() - 1 &&
                       (scoped ? picker->generation == content->Browser().ProjectGeneration() &&
                                     picker->revision == content->Browser().Revision()
                               : picker->generation == 0);
  if (!current) {
    ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
    picker.reset();
    return;
  }
  picker->frame = ImGui::GetFrameCount();
  const bool editable = scoped && state.build_settings_ready && state.app_focused &&
                        workspace.Writable() && content->Writable() && !content->ReimportBusy() &&
                        !blocked && !state.game_build_busy && !state.game_run_busy &&
                        picker->error.empty();
  ImGui::TextUnformatted("Animation assets included in the game");
  ImGui::TextWrapped("Save Build Settings to apply changes. Selection does not start playback.");
  if (!picker->error.empty())
    ImGui::TextWrapped("%s", picker->error.c_str());
  if (!editable)
    ImGui::TextDisabled("Inspection only; animation selection is currently unavailable.");
  auto &roots = state.build_settings.animation_clips;
  std::optional<std::size_t> removal;
  std::optional<runtime::AssetUuid> addition;
  std::unordered_set<runtime::AssetUuid, runtime::AssetUuidHash> selected(roots.begin(),
                                                                          roots.end());
  ImGui::SeparatorText("Included Clips");
  const float row_height = detail::IconButtonExtent() + ImGui::GetStyle().ItemSpacing.y;
  const float roots_height = std::max(50.0F, (ImGui::GetContentRegionAvail().y - 100) * .45F);
  if (ImGui::BeginChild("Included animation roots", {0, roots_height}, ImGuiChildFlags_Borders)) {
    if (roots.empty())
      ImGui::TextDisabled("No explicit animation Clips selected.");
    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(roots.size()), row_height);
    while (clipper.Step())
      for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
        const auto slot = static_cast<std::size_t>(row);
        const auto id = roots[slot];
        ImGui::PushID(row);
        ImGui::BeginDisabled(!editable);
        if (detail::IconButton("Remove Clip", detail::EditorIcon::Delete))
          removal = slot;
        if (const auto point = capture())
          state.build_animation_remove_positions.emplace_back(id, *point);
        ImGui::EndDisabled();
        ImGui::SameLine();
        const auto found = picker->index.find(id);
        if (found == picker->index.end())
          ImGui::Text("Missing or unavailable Clip: %s", id.ToString().c_str());
        else
          ImGui::TextUnformatted(picker->choices[found->second].path.c_str());
        ImGui::PopID();
      }
  }
  ImGui::EndChild();
  ImGui::SeparatorText("Imported Clips");
  const float choices_height = std::max(50.0F, ImGui::GetContentRegionAvail().y - row_height -
                                                   ImGui::GetStyle().ItemSpacing.y);
  if (ImGui::BeginChild("Available animation Clips", {0, choices_height},
                        ImGuiChildFlags_Borders)) {
    if (picker->choices.empty())
      ImGui::TextDisabled("No imported animation Clips are available.");
    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(picker->choices.size()), row_height);
    while (clipper.Step())
      for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
        const auto &choice = picker->choices[static_cast<std::size_t>(row)];
        ImGui::PushID(row);
        ImGui::BeginDisabled(!editable || roots.size() >= 4096 || selected.contains(choice.id));
        if (detail::IconButton("Include Clip", detail::EditorIcon::Add))
          addition = choice.id;
        if (const auto point = capture())
          state.build_animation_add_positions.emplace_back(choice.id, *point);
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::TextUnformatted(choice.path.c_str());
        ImGui::PopID();
      }
  }
  ImGui::EndChild();
  if (editable && removal)
    roots.erase(roots.begin() + static_cast<std::ptrdiff_t>(*removal));
  if (editable && addition)
    roots.push_back(*addition);
  if (detail::IconButton("Close animation assets", detail::EditorIcon::Close))
    ImGui::CloseCurrentPopup();
  state.build_animation_positions[1] = capture();
  ImGui::EndPopup();
}
