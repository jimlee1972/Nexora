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
  if (!state.prefab_isolation_open || !state.prefab_isolation)
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
    if (observation.dirty && action != PrefabIsolationAction::Save &&
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
    const bool confirming = state.prefab_confirmation.has_value();
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
    if (confirming) {
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
        static_cast<void>(editable->Undo());
      capture(8);
      ImGui::SameLine();
      if (ImGui::Button("Redo"))
        static_cast<void>(editable->Redo());
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
            static_cast<void>(editable->Rename(key, state.prefab_name.data()));
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
                static_cast<void>(editable->SetTransform(key.id, next));
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
    if (!state.prefab_status.empty())
      ImGui::TextWrapped("%s", state.prefab_status.c_str());
  }
  ImGui::End();
}
#if defined(NEXORA_EDITOR_IMGUI_TEST_ACCESS)
std::optional<std::array<float, 2>>
EditorImGuiTestAccess::PrefabControlPosition(const EditorImGuiHost &host,
                                             std::size_t control) noexcept {
  return control < host.state_->prefab_positions.size() ? host.state_->prefab_positions[control]
                                                        : std::nullopt;
}
#endif
