// Included inside the EditorImGui namespace after its private State definition.
bool EditorImGuiHost::SetPrefabIsolation(std::optional<PrefabIsolationObservation> value) {
  if (value && (value->project.IsNil() || !value->project_scope || !value->owner_generation ||
                (value->open && (value->asset.IsNil() || !value->document_generation)) ||
                (!value->open && (!value->asset.IsNil() || value->document_generation ||
                                  value->revision || value->dirty))))
    return false;
  auto &state = *state_;
  const auto changed = [&] {
    if (value.has_value() != state.prefab_isolation.has_value())
      return true;
    if (!value)
      return false;
    const auto &old = *state.prefab_isolation;
    return value->project != old.project || value->project_scope != old.project_scope ||
           value->owner_generation != old.owner_generation || value->asset != old.asset ||
           value->document_generation != old.document_generation || value->source != old.source;
  };
  if (changed()) {
    ImGui::SetCurrentContext(state.context);
    const auto *window = ImGui::FindWindowByName("Prefab Isolation###editor.prefab-isolation");
    if (window && ImGui::GetCurrentContext()->ActiveIdWindow == window)
      ImGui::ClearActiveID();
    state.prefab_request.reset();
    state.prefab_confirmation.reset();
    state.prefab_review.reset();
    state.prefab_can_revert = false;
    state.prefab_can_apply = false;
    state.prefab_apply_confirmation = false;
    state.prefab_revert_confirmation = false;
    state.prefab_selected.clear();
    state.prefab_targeted = false;
    state.prefab_target = {};
    state.prefab_edit_key.reset();
    state.prefab_name_active = false;
    state.prefab_position_active = {};
    state.prefab_status.clear();
  }
  state.prefab_isolation = std::move(value);
  return true;
}
void EditorImGuiHost::OpenPrefabIsolation() noexcept { state_->prefab_isolation_open = true; }
void EditorImGuiHost::SetPrefabReview(std::optional<SceneComparison> changes, bool can_revert,
                                      std::span<const PrefabPropertySelection> selected,
                                      bool targeted, bool can_apply_to_source) {
  if (selected.size() > PrefabAssets::kMaximumProperties)
    changes.reset();
  if (changes) {
    bool valid = changes->rows.size() <= SceneComparison::kMaximumRows;
    std::size_t bytes{};
    const auto account = [&](std::string_view text) {
      if (text.size() > SceneComparison::kMaximumValueBytes ||
          text.size() > SceneComparison::kMaximumResultBytes - bytes ||
          !foundation::IsValidUtf8(text) ||
          std::ranges::any_of(text, [](unsigned char c) { return c < 32 || c == 127; }))
        return false;
      bytes += text.size();
      return true;
    };
    if (valid)
      for (const auto &row : changes->rows) {
        if (row.stable_path.size() > 1024 || !account(row.stable_path) ||
            (row.base && !account(*row.base)) || (row.local && !account(*row.local)) ||
            (row.remote && !account(*row.remote))) {
          valid = false;
          break;
        }
      }
    if (!valid)
      changes.reset();
  }
  state_->prefab_review = std::move(changes);
  state_->prefab_can_revert = state_->prefab_review && can_revert;
  state_->prefab_can_apply = state_->prefab_review && can_apply_to_source;
  state_->prefab_apply_confirmation = false;
  state_->prefab_revert_confirmation = false;
  state_->prefab_selected.clear();
  if (state_->prefab_review)
    state_->prefab_selected.assign(selected.begin(), selected.end());
  state_->prefab_targeted = state_->prefab_review && targeted;
}
std::optional<PrefabIsolationRequest> EditorImGuiHost::TakePrefabIsolationRequest() {
  return std::exchange(state_->prefab_request, std::nullopt);
}
void EditorImGuiHost::SetPrefabIsolationStatus(std::string message, bool success) {
  state_->prefab_status = message.size() <= 4096 && foundation::IsValidUtf8(message)
                              ? std::move(message)
                              : "The prefab action returned an invalid diagnostic.";
  if (success)
    state_->prefab_confirmation.reset();
}
void EditorImGuiHost::DrawPrefabIsolation(const SceneDocument *document, SceneDocument *editable,
                                          const ProjectWorkspace &workspace,
                                          bool authoring_allowed) {
  ImGui::SetCurrentContext(state_->context);
  auto &state = *state_;
  state.prefab_positions = {};
  state.prefab_property_positions.clear();
  if (!state.prefab_isolation_open) {
    SetPrefabReview(std::nullopt);
    state.prefab_request.reset();
    state.prefab_confirmation.reset();
    return;
  }
  if (!state.prefab_isolation)
    return;
  const auto observation = *state.prefab_isolation;
  const bool current =
      observation.project == workspace.Project().id &&
      (observation.open ? document && document->Generation() == observation.document_generation
                        : !document);
  const bool interaction =
      current && authoring_allowed && state.app_focused &&
      !state.build_console_interaction_blocked && state.play_command == PlayCommand::None &&
      !state.close_prompt_requested && state.close_choice == CloseChoice::None &&
      state.scene_file_dialog == State::FileDialog::None && !state.scene_file_output &&
      !state.scene_tab_dialog && !state.scene_tab_output && !state.hierarchy_rename_target &&
      !state.content_rename_target && !state.game_input_binding_open &&
      !workspace.HasRecoveryJournal() && !workspace.HasExternalChange();
  const bool writable = interaction && observation.writable && workspace.Writable();
  const bool edit = writable && editable && editable == document;
  if (!edit) {
    state.prefab_name_active = false;
    state.prefab_position_active = {};
  }
  const auto capture = [&](std::size_t index) {
    const auto low = ImGui::GetItemRectMin(), high = ImGui::GetItemRectMax();
    state.prefab_positions[index] = std::array{(low.x + high.x) * .5F, (low.y + high.y) * .5F};
  };
  const auto request = [&](PrefabIsolationAction action, foundation::Uuid target = {}) {
    PrefabIsolationRequest next{action, observation, target, false};
    if (observation.dirty && action != PrefabIsolationAction::Review &&
        action != PrefabIsolationAction::SelectReview && action != PrefabIsolationAction::Revert &&
        action != PrefabIsolationAction::ApplyToSource && action != PrefabIsolationAction::Save &&
        action != PrefabIsolationAction::Variant)
      state.prefab_confirmation = std::move(next);
    else
      state.prefab_request = std::move(next);
  };
  ImGui::SetNextWindowPos({60, 60}, ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSize({650, 650}, ImGuiCond_FirstUseEver);
  if (ImGui::Begin("Prefab Isolation###editor.prefab-isolation", &state.prefab_isolation_open)) {
    ImGui::TextUnformatted("Edit this prefab independently of the active scene.");
    if (observation.open)
      ImGui::Text("%s | revision %llu%s", observation.asset.ToString().c_str(),
                  static_cast<unsigned long long>(observation.revision),
                  observation.dirty ? " | unsaved" : "");
    ImGui::SetNextItemWidth(360);
    ImGui::InputText("Asset UUID", state.prefab_target.data(), state.prefab_target.size());
    capture(0);
    const auto parsed = foundation::Uuid::Parse(state.prefab_target.data());
    const bool target = parsed && !parsed.Value().IsNil();
    const bool confirming = state.prefab_confirmation.has_value() ||
                            state.prefab_revert_confirmation || state.prefab_apply_confirmation;
    ImGui::BeginDisabled(!writable || !target || confirming ||
                         !observation.source.document_generation);
    if (ImGui::Button("Create from scene"))
      request(PrefabIsolationAction::Create, parsed.Value());
    capture(1);
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(!interaction || !target || confirming);
    if (ImGui::Button("Open"))
      request(PrefabIsolationAction::Open, parsed.Value());
    capture(2);
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(!writable || !target || !observation.open || observation.dirty ||
                         confirming);
    if (ImGui::Button("Create variant"))
      request(PrefabIsolationAction::Variant, parsed.Value());
    capture(3);
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(!writable || !state.prefab_can_apply || confirming);
    if (ImGui::Button("Apply to source"))
      state.prefab_apply_confirmation = true;
    capture(20);
    ImGui::EndDisabled();
    ImGui::BeginDisabled(!writable || !observation.open || confirming);
    if (ImGui::Button("Save prefab"))
      request(PrefabIsolationAction::Save);
    capture(4);
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(!interaction || !observation.open || confirming);
    if (ImGui::Button("Close prefab"))
      request(PrefabIsolationAction::Close);
    capture(5);
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(!interaction || !observation.open || confirming);
    if (ImGui::Button("Review changes"))
      request(PrefabIsolationAction::Review);
    capture(15);
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(!writable || !state.prefab_can_revert || confirming);
    if (ImGui::Button("Revert properties"))
      state.prefab_revert_confirmation = true;
    capture(16);
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(!interaction || !state.prefab_review || state.prefab_selected.empty() ||
                         confirming);
    if (ImGui::Button("Review selected")) {
      request(PrefabIsolationAction::SelectReview);
      if (state.prefab_request)
        state.prefab_request->selected = state.prefab_selected;
    }
    capture(19);
    ImGui::EndDisabled();
    if (state.prefab_apply_confirmation) {
      ImGui::TextUnformatted(state.prefab_targeted
                                 ? "Publish selected properties to the current source?"
                                 : "Publish reviewed properties to the current source?");
      ImGui::TextWrapped("The variant keeps its retained base until explicit Rebase. Local "
                         "edits/history stay unchanged.");
      ImGui::BeginDisabled(!writable);
      if (ImGui::Button("Confirm source apply")) {
        request(PrefabIsolationAction::ApplyToSource);
        state.prefab_apply_confirmation = false;
      }
      capture(21);
      ImGui::EndDisabled();
      ImGui::SameLine();
      if (ImGui::Button("Cancel source apply"))
        state.prefab_apply_confirmation = false;
      capture(22);
    }
    if (state.prefab_revert_confirmation) {
      ImGui::TextUnformatted(state.prefab_targeted
                                 ? "Restore selected properties? Use Undo to recover your edits."
                                 : "Restore source properties? Use Undo to recover your edits.");
      ImGui::BeginDisabled(!writable);
      if (ImGui::Button("Confirm property revert")) {
        request(PrefabIsolationAction::Revert);
        state.prefab_revert_confirmation = false;
      }
      capture(17);
      ImGui::EndDisabled();
      ImGui::SameLine();
      if (ImGui::Button("Cancel revert"))
        state.prefab_revert_confirmation = false;
      capture(18);
    }
    if (state.prefab_confirmation) {
      ImGui::TextUnformatted("Discard this prefab's unsaved changes before continuing?");
      ImGui::BeginDisabled(!interaction);
      if (ImGui::Button("Discard prefab changes")) {
        auto confirmed = *state.prefab_confirmation;
        confirmed.discard_dirty = true;
        state.prefab_request = std::move(confirmed);
        state.prefab_confirmation.reset();
      }
      capture(6);
      ImGui::EndDisabled();
      ImGui::SameLine();
      if (ImGui::Button("Keep editing"))
        state.prefab_confirmation.reset();
      capture(7);
    }
    if (document && current) {
      ImGui::BeginDisabled(!edit || confirming);
      if (ImGui::Button("Undo"))
        if (editable->Undo())
          SetPrefabReview(std::nullopt);
      capture(8);
      ImGui::SameLine();
      if (ImGui::Button("Redo"))
        if (editable->Redo())
          SetPrefabReview(std::nullopt);
      capture(9);
      ImGui::EndDisabled();
      const auto nodes = document->Nodes();
      if (nodes.size() <= PrefabAssets::kMaximumNodes) {
        std::unordered_map<runtime::Id, std::vector<std::size_t>> children;
        for (std::size_t i = 0; i < nodes.size(); ++i)
          children[nodes[i].parent].push_back(i);
        std::vector<std::pair<std::size_t, std::size_t>> rows, pending;
        for (auto root = children[0].rbegin(); root != children[0].rend(); ++root)
          pending.emplace_back(*root, 0);
        while (!pending.empty() && rows.size() < nodes.size()) {
          const auto row = pending.back();
          pending.pop_back();
          rows.push_back(row);
          if (const auto found = children.find(nodes[row.first].id); found != children.end())
            for (auto child = found->second.rbegin(); child != found->second.rend(); ++child)
              pending.emplace_back(*child, row.second + 1);
        }
        ImGui::BeginChild("Prefab hierarchy", {0, 200}, ImGuiChildFlags_Borders);
        ImGuiListClipper clipper;
        clipper.Begin(static_cast<int>(rows.size()));
        while (clipper.Step())
          for (int index = clipper.DisplayStart; index < clipper.DisplayEnd; ++index) {
            const auto row = rows[static_cast<std::size_t>(index)];
            const auto &node = nodes[row.first];
            const float indent = static_cast<float>(std::min<std::size_t>(row.second, 16)) * 12.0F;
            if (indent)
              ImGui::Indent(indent);
            ImGui::PushID(static_cast<int>(index));
            const auto name = std::string(node.name.substr(0, 1024));
            ImGui::BeginDisabled(!interaction || confirming);
            if (ImGui::Selectable(name.c_str(), state.prefab_edit_key == node.Key())) {
              state.prefab_edit_key = node.Key();
              state.prefab_name_active = false;
              state.prefab_position_active = {};
              if (edit) {
                const std::array keys{node.Key()};
                static_cast<void>(editable->Select(keys));
              }
            }
            if (index == 0)
              capture(10);
            ImGui::EndDisabled();
            ImGui::PopID();
            if (indent)
              ImGui::Unindent(indent);
          }
        ImGui::EndChild();
      }
      if (state.prefab_edit_key &&
          document->Key(state.prefab_edit_key->id) == state.prefab_edit_key) {
        const auto key = *state.prefab_edit_key;
        const auto full_name = document->Name(key.id);
        const auto name = std::string(full_name.substr(0, 1024));
        if (!state.prefab_name_active)
          std::snprintf(state.prefab_name.data(), state.prefab_name.size(), "%s", name.c_str());
        ImGui::BeginDisabled(!edit || confirming || full_name.size() > 1024);
        const bool committed =
            ImGui::InputText("Name", state.prefab_name.data(), state.prefab_name.size(),
                             ImGuiInputTextFlags_EnterReturnsTrue);
        capture(11);
        if (ImGui::IsItemActivated()) {
          state.prefab_name_active = true;
          state.prefab_name_original = name;
        }
        if (committed) {
          if (name == state.prefab_name_original)
            if (editable->Rename(key, state.prefab_name.data()))
              SetPrefabReview(std::nullopt);
          state.prefab_name_active = false;
        } else if (ImGui::IsItemDeactivated())
          state.prefab_name_active = false;
        ImGui::EndDisabled();
        if (const auto transform = document->Transform(key.id)) {
          constexpr std::array labels{"Position X", "Position Y", "Position Z"};
          const std::array values{transform->x, transform->y, transform->z};
          for (std::size_t axis = 0; axis < values.size(); ++axis) {
            if (!state.prefab_position_active[axis])
              std::snprintf(state.prefab_position[axis].data(), state.prefab_position[axis].size(),
                            "%.17g", values[axis]);
            ImGui::BeginDisabled(!edit || confirming);
            const bool entered = ImGui::InputText(labels[axis], state.prefab_position[axis].data(),
                                                  state.prefab_position[axis].size(),
                                                  ImGuiInputTextFlags_EnterReturnsTrue);
            capture(12 + axis);
            if (ImGui::IsItemActivated()) {
              state.prefab_position_active[axis] = true;
              state.prefab_position_original[axis] = values[axis];
            }
            if (entered) {
              double value{};
              const std::string_view text(state.prefab_position[axis].data());
              const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
              if (values[axis] == state.prefab_position_original[axis] &&
                  result.ec == std::errc{} && result.ptr == text.data() + text.size() &&
                  std::isfinite(value) && std::abs(value) <= 1.0e8) {
                auto next = *transform;
                if (axis == 0)
                  next.x = value;
                else if (axis == 1)
                  next.y = value;
                else
                  next.z = value;
                if (editable->SetTransform(key.id, next))
                  SetPrefabReview(std::nullopt);
              }
              state.prefab_position_active[axis] = false;
            } else if (ImGui::IsItemDeactivated())
              state.prefab_position_active[axis] = false;
            ImGui::EndDisabled();
          }
        }
        if (const auto opaque = document->OpaqueComponents(key))
          for (const auto &component : *opaque)
            ImGui::Text("Preserved component %llu: %zu bytes",
                        static_cast<unsigned long long>(component.type), component.data.size());
      }
    }
    if (state.prefab_review) {
      ImGui::Text("Reviewed changes: %zu (source references are inspection only)",
                  state.prefab_review->rows.size());
      ImGui::BeginChild("Prefab changes", {0, 150}, ImGuiChildFlags_Borders);
      if (ImGui::BeginTable("Property differences", 3,
                            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                ImGuiTableFlags_SizingStretchSame)) {
        ImGui::TableSetupColumn("Property");
        ImGui::TableSetupColumn("Source");
        ImGui::TableSetupColumn("Your edit");
        ImGui::TableHeadersRow();
        ImGuiListClipper clipper;
        clipper.Begin(static_cast<int>(state.prefab_review->rows.size()));
        while (clipper.Step())
          for (int index = clipper.DisplayStart; index < clipper.DisplayEnd; ++index) {
            const auto &row = state.prefab_review->rows[static_cast<std::size_t>(index)];
            const auto display = [](const std::optional<std::string> &value) {
              if (!value)
                return std::string("<absent>");
              if (value->size() <= 256)
                return *value;
              std::size_t count = 256;
              while (count && (static_cast<unsigned char>((*value)[count]) & 0xc0) == 0x80)
                --count;
              return value->substr(0, count) + "... (" + std::to_string(value->size()) + " bytes)";
            };
            auto label = row.stable_path;
            if (label.starts_with("nodes/")) {
              const auto node_end = label.find('/', 6);
              const auto fields = label.find("/fields/", node_end);
              const auto property =
                  fields == std::string::npos ? node_end : label.find('/', fields + 8);
              if (property != std::string::npos)
                label = label.substr(6, 8) + " / " + label.substr(property + 1);
            }
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            std::optional<PrefabPropertySelection> property;
            const auto &path = row.stable_path;
            if (path.starts_with("nodes/") && path.size() > 87 &&
                std::string_view(path).substr(42, 8) == "/fields/") {
              const auto node = foundation::Uuid::Parse(std::string_view(path).substr(6, 36));
              const auto field = foundation::Uuid::Parse(std::string_view(path).substr(50, 36));
              if (node && field && !node.Value().IsNil() && !field.Value().IsNil())
                property = PrefabPropertySelection{node.Value(), field.Value()};
            }
            if (property) {
              const auto found = std::ranges::find_if(state.prefab_selected, [&](const auto &item) {
                return item.node == property->node && item.field == property->field;
              });
              bool checked = found != state.prefab_selected.end();
              ImGui::PushID(index);
              ImGui::BeginDisabled(
                  !interaction || confirming ||
                  (!checked && state.prefab_selected.size() >= PrefabAssets::kMaximumProperties));
              if (ImGui::Checkbox("##restore-field", &checked)) {
                if (checked)
                  state.prefab_selected.push_back(*property);
                else
                  state.prefab_selected.erase(found);
                // A previous full/selected candidate never authorizes a changed UI selection.
                state.prefab_can_revert = false;
                state.prefab_can_apply = false;
                state.prefab_apply_confirmation = false;
                state.prefab_revert_confirmation = false;
              }
              const auto low = ImGui::GetItemRectMin(), high = ImGui::GetItemRectMax();
              state.prefab_property_positions.emplace_back(
                  static_cast<std::size_t>(index),
                  std::array{(low.x + high.x) * .5F, (low.y + high.y) * .5F});
              ImGui::EndDisabled();
              ImGui::PopID();
              ImGui::SameLine();
            }
            ImGui::TextUnformatted(label.c_str());
            ImGui::SetItemTooltip("%s", row.stable_path.c_str());
            ImGui::TableNextColumn();
            const auto before = display(row.base);
            ImGui::TextUnformatted(before.c_str());
            ImGui::TableNextColumn();
            const auto after = display(row.local);
            ImGui::TextUnformatted(after.c_str());
          }
        ImGui::EndTable();
      }
      ImGui::EndChild();
    }
    if (!state.prefab_status.empty())
      ImGui::TextWrapped("%s", state.prefab_status.c_str());
  }
  ImGui::End();
}
#if defined(NEXORA_EDITOR_IMGUI_TEST_ACCESS)
std::optional<std::array<float, 2>>
EditorImGuiTestAccess::PrefabPropertyPosition(const EditorImGuiHost &host,
                                              std::size_t row) noexcept {
  for (const auto &[index, position] : host.state_->prefab_property_positions)
    if (index == row)
      return position;
  return std::nullopt;
}
std::optional<std::array<float, 2>>
EditorImGuiTestAccess::PrefabControlPosition(const EditorImGuiHost &host,
                                             std::size_t control) noexcept {
  return control < host.state_->prefab_positions.size() ? host.state_->prefab_positions[control]
                                                        : std::nullopt;
}
#endif
