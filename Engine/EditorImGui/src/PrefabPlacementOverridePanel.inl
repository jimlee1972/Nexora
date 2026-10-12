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
template <typename StateT>
void DrawPrefabPlacementOverrides(StateT &state, bool read_allowed, bool editable) {
  state.prefab_override_positions = {};
  if (!state.prefab_source_scope)
    return;
  const bool readable = read_allowed && state.prefab_source_read_allowed && state.app_focused;
  const auto capture = [&](std::size_t index) {
    const auto low = ImGui::GetItemRectMin(), high = ImGui::GetItemRectMax();
    state.prefab_override_positions[index] =
        std::array{(low.x + high.x) * .5F, (low.y + high.y) * .5F};
  };
  if (!readable) {
    state.prefab_override_request.reset();
    state.prefab_override_confirm = state.prefab_override_popup = false;
  }
  ImGui::BeginDisabled(!readable);
  if (ImGui::Button("Review overrides###editor.prefab.instance.review"))
    state.prefab_override_request = PrefabPlacementOverrideRequest{*state.prefab_source_scope};
  capture(0);
  ImGui::EndDisabled();
  if (!state.prefab_override_error.empty())
    ImGui::TextWrapped("%s", PrefabOverrideDisplay(state.prefab_override_error).c_str());
  if (!state.prefab_override_report)
    return;
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
  if (ImGui::Button("Revert instance properties###editor.prefab.instance.revert")) {
    state.prefab_override_confirm = true;
    state.prefab_override_popup = true;
  }
  capture(1);
  ImGui::EndDisabled();
  if (structural)
    ImGui::TextWrapped("Hierarchy changes require structural reconciliation.");
  if (std::exchange(state.prefab_override_popup, false))
    ImGui::OpenPopup("Revert instance properties?###editor.prefab.instance.confirm");
  ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing,
                          ImVec2(.5F, .5F));
  ImGui::SetNextWindowSize(ImVec2(480, 160), ImGuiCond_Appearing);
  if (ImGui::BeginPopupModal("Revert instance properties?###editor.prefab.instance.confirm",
                             nullptr,
                             ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings)) {
    ImGui::TextWrapped(
        "Restore all supported instance properties to retained source revision %llu?",
        static_cast<unsigned long long>(report.scope.retained.revision));
    ImGui::TextWrapped("This creates one Undo step. Scene Save persists the result.");
    ImGui::BeginDisabled(!writable || !state.prefab_override_confirm);
    if (ImGui::Button("Confirm revert")) {
      state.prefab_override_request = PrefabPlacementOverrideRequest{
          report.scope, PrefabPlacementOverrideAction::Revert, report.review};
      state.prefab_override_confirm = false;
      ImGui::CloseCurrentPopup();
    }
    capture(2);
    ImGui::EndDisabled();
    ImGui::SameLine();
    const bool cancel = ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape, false);
    capture(3);
    if (!writable || !state.prefab_override_confirm || cancel) {
      state.prefab_override_confirm = false;
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }
  if (ImGui::BeginTable("editor.prefab.instance.diff", 4,
                        ImGuiTableFlags_Borders | ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg,
                        ImVec2(0, 180))) {
    for (const char *column : {"Scoped field", "Retained", "Local", "Source"})
      ImGui::TableSetupColumn(column);
    ImGui::TableHeadersRow();
    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(report.rows.size()), ImGui::GetTextLineHeightWithSpacing());
    while (clipper.Step())
      for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
        const auto &row = report.rows[static_cast<std::size_t>(i)];
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        const auto field = std::to_string(row.scope.size()) + ": " + row.field;
        ImGui::TextUnformatted(PrefabOverrideDisplay(field).c_str());
        ImGui::TableSetColumnIndex(1);
        ImGui::TextUnformatted(row.retained ? PrefabOverrideDisplay(*row.retained).c_str()
                                            : "Absent");
        ImGui::TableSetColumnIndex(2);
        ImGui::TextUnformatted(row.local ? PrefabOverrideDisplay(*row.local).c_str() : "Absent");
        ImGui::TableSetColumnIndex(3);
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
