std::string PrefabOverrideDisplay(std::string_view value) {
  std::string result;
  constexpr char hex[] = "0123456789abcdef";
  auto length = std::min<std::size_t>(value.size(), 256);
  while (length < value.size() && length &&
         (static_cast<unsigned char>(value[length]) & 0xc0) == 0x80)
    --length;
  const auto clipped = value.substr(0, length);
  for (const unsigned char byte : clipped) {
    if (byte >= 32 && byte != 127 && byte != '\\')
      result += static_cast<char>(byte);
    else {
      result += "\\x";
      result += hex[byte >> 4];
      result += hex[byte & 15];
    }
  }
  if (value.size() > clipped.size())
    result += " [display truncated]";
  return result;
}
bool PrefabSourceSelectionAllowed(const PrefabPlacementOverrideReport &report,
                                  std::span<const std::size_t> selected) {
  if (!report.can_apply_to_source || selected.empty() ||
      report.published_revision != report.scope.retained.revision ||
      report.scope.retained.revision == std::numeric_limits<std::uint64_t>::max() ||
      std::ranges::any_of(report.rows, &PrefabPlacementOverrideRow::structural))
    return false;
  for (const auto index : selected) {
    if (index >= report.rows.size())
      return false;
    const auto &row = report.rows[index];
    if (!row.property || !row.scope.empty() || row.source != report.scope.retained ||
        row.target.document_generation != report.scope.node.document_generation ||
        (row.field.starts_with("opaque/") && (!row.retained || !row.local)))
      return false;
  }
  return true;
}
template <typename StateT>
void DrawPrefabPlacementOverrides(StateT &state, bool read_allowed, bool editable) {
  state.prefab_override_positions = {};
  state.prefab_override_row_positions.clear();
  const auto close_cancelled_popup = [&] {
    const auto id = ImGui::GetID("Revert instance properties?###editor.prefab.instance.confirm");
    auto &context = *ImGui::GetCurrentContext();
    for (int i = 0; i < context.OpenPopupStack.Size; ++i)
      if (context.OpenPopupStack[i].PopupId == id) {
        ImGui::ClosePopupToLevel(i, true);
        break;
      }
  };
  if (!state.prefab_source_scope) {
    close_cancelled_popup();
    return;
  }
  const bool readable = read_allowed && state.prefab_source_read_allowed && state.app_focused;
  const auto capture = [&](std::size_t index) {
    const auto low = ImGui::GetItemRectMin(), high = ImGui::GetItemRectMax();
    state.prefab_override_positions[index] =
        std::array{(low.x + high.x) * .5F, (low.y + high.y) * .5F};
  };
  if (!readable) {
    state.prefab_override_request.reset();
    state.prefab_override_confirm = state.prefab_override_popup = false;
    state.prefab_override_consent.reset();
  }
  ImGui::BeginDisabled(!readable);
  if (detail::IconButton("Review overrides###editor.prefab.instance.review",
                         detail::EditorIcon::Inspect))
    state.prefab_override_request = PrefabPlacementOverrideRequest{*state.prefab_source_scope};
  capture(0);
  ImGui::EndDisabled();
  if (!state.prefab_override_error.empty())
    ImGui::TextWrapped("%s", PrefabOverrideDisplay(state.prefab_override_error).c_str());
  if (!state.prefab_override_report) {
    close_cancelled_popup();
    return;
  }
  const auto &report = *state.prefab_override_report;
  ImGui::Text("Captured property differences: %zu", report.rows.size());
  if (report.published_revision)
    ImGui::Text("Retained / published: %llu / %llu",
                static_cast<unsigned long long>(report.scope.retained.revision),
                static_cast<unsigned long long>(*report.published_revision));
  else
    ImGui::Text("Retained: %llu; published source unavailable",
                static_cast<unsigned long long>(report.scope.retained.revision));
  ImGui::TextDisabled("Review again to refresh captured values.");
  const bool structural = std::ranges::any_of(report.rows, &PrefabPlacementOverrideRow::structural);
  const bool writable = readable && editable && state.prefab_override_authoring && !structural &&
                        !report.rows.empty();
  ImGui::BeginDisabled(!writable);
  if (detail::IconButton("Revert instance properties###editor.prefab.instance.revert",
                         detail::EditorIcon::Revert)) {
    state.prefab_override_consent = PrefabPlacementOverrideRequest{
        report.scope, PrefabPlacementOverrideAction::Revert, report.review};
    state.prefab_override_confirm = true;
    state.prefab_override_popup = true;
  }
  capture(1);
  ImGui::EndDisabled();
  std::vector<std::size_t> selected;
  for (std::size_t i = 0; i < state.prefab_override_selected.size(); ++i)
    if (state.prefab_override_selected[i])
      selected.push_back(i);
  ImGui::BeginDisabled(!writable || selected.empty());
  if (detail::IconButton("Revert selected properties###editor.prefab.instance.selected",
                         detail::EditorIcon::Revert)) {
    state.prefab_override_consent =
        PrefabPlacementOverrideRequest{report.scope, PrefabPlacementOverrideAction::RevertSelected,
                                       report.review, std::move(selected)};
    state.prefab_override_confirm = state.prefab_override_popup = true;
  }
  capture(4);
  ImGui::EndDisabled();
  const bool source_allowed = writable && PrefabSourceSelectionAllowed(report, selected);
  ImGui::BeginDisabled(!source_allowed || state.prefab_override_confirm);
  if (detail::IconButton("Apply selected to source###editor.prefab.instance.apply-source",
                         detail::EditorIcon::Publish)) {
    state.prefab_override_consent = PrefabPlacementOverrideRequest{
        report.scope, PrefabPlacementOverrideAction::ApplySelectedToSource, report.review,
        std::move(selected)};
    state.prefab_override_confirm = state.prefab_override_popup = true;
  }
  capture(5);
  ImGui::EndDisabled();
  if (!report.can_apply_to_source)
    ImGui::TextDisabled("Source application requires a current non-nested source revision.");
  ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
  ImGui::TextWrapped("Selected fields restore the whole property.");
  ImGui::PopStyleColor();
  if (structural)
    ImGui::TextWrapped("Hierarchy changes require structural reconciliation.");
  const bool source_apply =
      state.prefab_override_consent &&
      state.prefab_override_consent->action == PrefabPlacementOverrideAction::ApplySelectedToSource;
  const char *title = source_apply
                          ? "Apply selected properties to source?###editor.prefab.instance.confirm"
                          : "Revert instance properties?###editor.prefab.instance.confirm";
  if (std::exchange(state.prefab_override_popup, false))
    ImGui::OpenPopup(title);
  ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing,
                          ImVec2(.5F, .5F));
  ImGui::SetNextWindowSize(ImVec2(source_apply ? 520.F : 480.F, source_apply ? 220.F : 160.F),
                           ImGuiCond_Appearing);
  if (ImGui::BeginPopupModal(title, nullptr,
                             ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings)) {
    const bool targeted =
        state.prefab_override_consent &&
        state.prefab_override_consent->action == PrefabPlacementOverrideAction::RevertSelected;
    if (source_apply) {
      ImGui::TextWrapped("Apply the selected whole properties to source revision %llu?",
                         static_cast<unsigned long long>(report.scope.retained.revision));
      ImGui::Text("Source: %s", report.scope.retained.asset.ToString().c_str());
      ImGui::TextWrapped("This publishes a new source revision. The instance keeps its retained "
                         "revision until Rebase. Source history is separate from Scene Undo.");
    } else {
      ImGui::TextWrapped(
          targeted ? "Restore selected properties to retained source revision %llu?"
                   : "Restore all supported instance properties to retained source revision %llu?",
          static_cast<unsigned long long>(report.scope.retained.revision));
      ImGui::TextWrapped("This creates one Undo step. Scene Save persists the result.");
    }
    const bool consent = state.prefab_override_consent &&
                         state.prefab_override_consent->scope == report.scope &&
                         state.prefab_override_consent->review == report.review;
    const bool allowed = writable && consent &&
                         (!source_apply || PrefabSourceSelectionAllowed(
                                               report, state.prefab_override_consent->rows));
    ImGui::BeginDisabled(!allowed || !state.prefab_override_confirm);
    if (detail::IconButton(source_apply ? "Confirm source apply" : "Confirm revert",
                           detail::EditorIcon::Confirm)) {
      state.prefab_override_request = std::move(state.prefab_override_consent);
      state.prefab_override_confirm = false;
      ImGui::CloseCurrentPopup();
    }
    capture(2);
    ImGui::EndDisabled();
    ImGui::SameLine();
    const bool cancel = detail::IconButton("Cancel", detail::EditorIcon::Cancel) ||
                        ImGui::IsKeyPressed(ImGuiKey_Escape, false);
    capture(3);
    if (!allowed || !state.prefab_override_confirm || cancel) {
      state.prefab_override_confirm = false;
      state.prefab_override_consent.reset();
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }
  if (ImGui::BeginTable("editor.prefab.instance.diff", 5,
                        ImGuiTableFlags_Borders | ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg,
                        ImVec2(0, 180))) {
    ImGui::TableSetupColumn("Select", ImGuiTableColumnFlags_WidthFixed, 44);
    for (const char *column : {"Scoped field", "Retained", "Local", "Source"})
      ImGui::TableSetupColumn(column);
    ImGui::TableHeadersRow();
    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(report.rows.size()),
                  ImGui::GetFrameHeight() + 2 * ImGui::GetStyle().CellPadding.y);
    while (clipper.Step())
      for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
        const auto &row = report.rows[static_cast<std::size_t>(i)];
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        bool chosen = state.prefab_override_selected[static_cast<std::size_t>(i)];
        ImGui::PushID(static_cast<int>(report.review));
        ImGui::PushID(static_cast<int>(report.review >> 32U));
        ImGui::PushID(static_cast<int>(report.scope.node.document_generation));
        ImGui::PushID(static_cast<int>(report.scope.node.document_generation >> 32U));
        ImGui::PushID(static_cast<int>(report.scope.node.entity_generation));
        ImGui::PushID(static_cast<int>(report.scope.node.entity_generation >> 32U));
        ImGui::PushID(i);
        ImGui::BeginDisabled(!readable || row.structural || state.prefab_override_confirm);
        if (ImGui::Checkbox("##selected", &chosen))
          state.prefab_override_selected[static_cast<std::size_t>(i)] = chosen;
        const auto low = ImGui::GetItemRectMin(), high = ImGui::GetItemRectMax();
        state.prefab_override_row_positions.emplace(
            static_cast<std::size_t>(i),
            std::array{(low.x + high.x) * .5F, (low.y + high.y) * .5F});
        ImGui::EndDisabled();
        ImGui::PopID();
        for (int part = 0; part < 6; ++part)
          ImGui::PopID();
        ImGui::TableSetColumnIndex(1);
        const auto field = std::to_string(row.scope.size()) + ": " + row.field;
        ImGui::TextUnformatted(PrefabOverrideDisplay(field).c_str());
        ImGui::TableSetColumnIndex(2);
        ImGui::TextUnformatted(row.retained ? PrefabOverrideDisplay(*row.retained).c_str()
                                            : "Absent");
        ImGui::TableSetColumnIndex(3);
        ImGui::TextUnformatted(row.local ? PrefabOverrideDisplay(*row.local).c_str() : "Absent");
        ImGui::TableSetColumnIndex(4);
        const auto source = row.source.asset.ToString() + ":" + std::to_string(row.source.revision);
        ImGui::TextUnformatted(source.c_str());
        if (ImGui::IsItemHovered()) {
          ImGui::BeginTooltip();
          ImGui::Text("Node: %s", row.node.ToString().c_str());
          for (const auto id : row.scope)
            ImGui::Text("Scope: %s", id.ToString().c_str());
          if (row.property)
            ImGui::Text("Property: %s", row.property->ToString().c_str());
          else
            ImGui::TextUnformatted("No retained source field identity.");
          ImGui::EndTooltip();
        }
      }
    ImGui::EndTable();
  }
}
