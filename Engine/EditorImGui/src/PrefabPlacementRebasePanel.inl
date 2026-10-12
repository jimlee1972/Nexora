template <typename StateT>
void DrawPrefabPlacementRebase(StateT &state, bool read_allowed, bool editable) {
  state.prefab_rebase_positions = {};
  state.prefab_rebase_choice_positions.clear();
  const char *popup = "Rebase instance properties?###editor.prefab.rebase.confirm";
  const auto close_popup = [&] {
    const auto id = ImGui::GetID(popup);
    auto &context = *ImGui::GetCurrentContext();
    for (int i = 0; i < context.OpenPopupStack.Size; ++i)
      if (context.OpenPopupStack[i].PopupId == id) {
        ImGui::ClosePopupToLevel(i, true);
        break;
      }
  };
  if (!state.prefab_source_scope) {
    close_popup();
    return;
  }
  const bool readable = read_allowed && state.prefab_source_read_allowed && state.app_focused;
  const auto capture = [&](std::size_t index) {
    const auto low = ImGui::GetItemRectMin(), high = ImGui::GetItemRectMax();
    state.prefab_rebase_positions[index] =
        std::array{(low.x + high.x) * .5F, (low.y + high.y) * .5F};
  };
  if (!readable) {
    state.prefab_rebase_request.reset();
    state.prefab_rebase_consent.reset();
    state.prefab_rebase_confirm = state.prefab_rebase_popup = false;
  }
  ImGui::BeginDisabled(!readable || state.prefab_rebase_confirm || state.prefab_override_confirm);
  if (ImGui::Button("Review newer source###editor.prefab.rebase.review"))
    state.prefab_rebase_request = PrefabPlacementRebaseRequest{*state.prefab_source_scope};
  capture(0);
  ImGui::EndDisabled();
  if (!state.prefab_rebase_error.empty())
    ImGui::TextWrapped("%s", PrefabOverrideDisplay(state.prefab_rebase_error).c_str());
  if (!state.prefab_rebase_report) {
    close_popup();
    return;
  }
  const auto &report = *state.prefab_rebase_report;
  ImGui::Text("Source rebase: %llu -> %llu",
              static_cast<unsigned long long>(report.scope.retained.revision),
              static_cast<unsigned long long>(report.published_revision));
  std::vector<PrefabPlacementRebaseChoice> choices;
  std::size_t unresolved{};
  for (std::size_t i = 0; i < report.rows.size(); ++i)
    if (report.rows[i].conflict) {
      const auto choice = state.prefab_rebase_choices[i];
      if (!choice)
        ++unresolved;
      else
        choices.push_back({i, choice == 1 ? PrefabPlacementRebaseDecision::KeepLocal
                                          : PrefabPlacementRebaseDecision::TakeSource});
    }
  ImGui::Text("Unresolved property conflicts: %zu", unresolved);
  const bool writable = readable && editable && state.prefab_override_authoring && !unresolved &&
                        !state.prefab_override_confirm;
  ImGui::BeginDisabled(!writable);
  if (ImGui::Button("Rebase instance properties###editor.prefab.rebase.apply")) {
    state.prefab_rebase_consent = PrefabPlacementRebaseRequest{
        report.scope, PrefabPlacementRebaseAction::Apply, report.review, std::move(choices)};
    state.prefab_rebase_confirm = state.prefab_rebase_popup = true;
  }
  capture(1);
  ImGui::EndDisabled();
  if (std::exchange(state.prefab_rebase_popup, false))
    ImGui::OpenPopup(popup);
  ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing,
                          ImVec2(.5F, .5F));
  ImGui::SetNextWindowSize(ImVec2(480, 160), ImGuiCond_Appearing);
  if (ImGui::BeginPopupModal(popup, nullptr,
                             ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings)) {
    ImGui::TextWrapped("Apply reviewed source revision %llu with these captured conflict choices?",
                       static_cast<unsigned long long>(report.published_revision));
    ImGui::TextWrapped(
        "One Undo step restores properties and revision. Scene Save persists the result.");
    const bool consent = state.prefab_rebase_consent &&
                         state.prefab_rebase_consent->scope == report.scope &&
                         state.prefab_rebase_consent->review == report.review;
    ImGui::BeginDisabled(!writable || !state.prefab_rebase_confirm || !consent);
    if (ImGui::Button("Confirm rebase")) {
      state.prefab_rebase_request = std::move(state.prefab_rebase_consent);
      state.prefab_rebase_confirm = false;
      ImGui::CloseCurrentPopup();
    }
    capture(2);
    ImGui::EndDisabled();
    ImGui::SameLine();
    const bool cancel = ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape, false);
    capture(3);
    if (!writable || !state.prefab_rebase_confirm || !consent || cancel) {
      state.prefab_rebase_confirm = false;
      state.prefab_rebase_consent.reset();
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }
  ImGui::TextDisabled("Each choice applies the complete property group. Review again to refresh.");
  if (ImGui::BeginTable("editor.prefab.rebase.rows", 5,
                        ImGuiTableFlags_Borders | ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg,
                        ImVec2(0, 180))) {
    ImGui::TableSetupColumn("Decision", ImGuiTableColumnFlags_WidthFixed, 118);
    for (const char *column : {"Scoped property", "Retained", "Local", "Published"})
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
        ImGui::PushID(i);
        if (row.conflict) {
          ImGui::BeginDisabled(!readable || state.prefab_rebase_confirm ||
                               state.prefab_override_confirm);
          for (int option = 1; option <= 2; ++option) {
            if (option == 2)
              ImGui::SameLine();
            if (ImGui::RadioButton(option == 1 ? "Local" : "Source",
                                   state.prefab_rebase_choices[static_cast<std::size_t>(i)] ==
                                       option))
              state.prefab_rebase_choices[static_cast<std::size_t>(i)] =
                  static_cast<std::uint8_t>(option);
            const auto low = ImGui::GetItemRectMin(), high = ImGui::GetItemRectMax();
            state.prefab_rebase_choice_positions.emplace(
                std::pair{static_cast<std::size_t>(i), option == 2},
                std::array{(low.x + high.x) * .5F, (low.y + high.y) * .5F});
          }
          ImGui::EndDisabled();
        } else
          ImGui::TextUnformatted("Automatic");
        ImGui::PopID();
        ImGui::TableSetColumnIndex(1);
        ImGui::TextUnformatted(
            PrefabOverrideDisplay(std::to_string(row.scope.size()) + ": " + row.group).c_str());
        if (ImGui::IsItemHovered()) {
          ImGui::BeginTooltip();
          ImGui::Text("Node: %s", row.node.ToString().c_str());
          for (const auto scope : row.scope)
            ImGui::Text("Scope: %s", scope.ToString().c_str());
          if (row.property)
            ImGui::Text("Property: %s", row.property->ToString().c_str());
          ImGui::EndTooltip();
        }
        for (int column = 2; column <= 4; ++column) {
          ImGui::TableSetColumnIndex(column);
          std::string text;
          for (const auto &value : row.values) {
            const auto &field = column == 2   ? value.retained
                                : column == 3 ? value.local
                                              : value.published;
            if (!text.empty())
              text += "; ";
            text += value.field + "=" + (field ? *field : "Absent");
            if (text.size() > 256)
              break;
          }
          ImGui::TextUnformatted(PrefabOverrideDisplay(text).c_str());
        }
      }
    ImGui::EndTable();
  }
}
