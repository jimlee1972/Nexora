template <typename StateT>
void DrawPrefabPlacementSource(StateT &state, const SceneDocument *scene,
                               bool interaction_allowed) {
  state.prefab_source_position.reset();
  state.prefab_source_frame = ImGui::GetFrameCount();
  const auto clear = [&] {
    state.prefab_source_scope.reset();
    state.prefab_source_request.reset();
    state.prefab_source_report.reset();
    state.prefab_override_report.reset();
    state.prefab_rebase_report.reset();
    state.prefab_rebase_request.reset();
    state.prefab_rebase_consent.reset();
    state.prefab_rebase_choices.clear();
    state.prefab_rebase_error.clear();
    state.prefab_rebase_confirm = state.prefab_rebase_popup = false;
    state.prefab_override_consent.reset();
    state.prefab_override_selected.clear();
    state.prefab_override_request.reset();
    state.prefab_override_error.clear();
    state.prefab_override_confirm = state.prefab_override_popup = false;
  };
  if (!scene || scene->Selection().size() != 1 || !state.scene_file_context ||
      state.scene_file_token.project.IsNil() || state.prefab_source_root.empty() ||
      state.scene_file_token.document_generation != scene->Generation()) {
    clear();
    return;
  }
  const auto key = scene->Key(scene->Selection().front());
  const SceneDocument::PrefabPlacement *placement{};
  const SceneDocument::PrefabPlacementNode *node{};
  if (key)
    for (const auto &candidate : scene->PrefabPlacements()) {
      const auto found =
          std::ranges::find(candidate.nodes, key->id, &SceneDocument::PrefabPlacementNode::target);
      if (found != candidate.nodes.end()) {
        placement = &candidate;
        node = &*found;
        break;
      }
    }
  if (!placement || !node) {
    clear();
    return;
  }
  const bool same = state.prefab_source_scope &&
                    state.prefab_source_scope->root == state.prefab_source_root &&
                    state.prefab_source_scope->source == state.scene_file_token &&
                    state.prefab_source_scope->node == *key &&
                    state.prefab_source_scope->instance == placement->instance &&
                    state.prefab_source_scope->retained ==
                        PrefabRevisionReference{placement->source, placement->revision} &&
                    state.prefab_source_scope->source_node == node->source_node &&
                    std::ranges::equal(state.prefab_source_scope->scope, node->scope);
  if (!same) {
    clear();
    state.prefab_source_scope =
        PrefabPlacementSourceRequest{state.prefab_source_root,
                                     state.scene_file_token,
                                     *key,
                                     placement->instance,
                                     node->source_node,
                                     {placement->source, placement->revision},
                                     node->scope};
  }
  const bool allowed = interaction_allowed && state.prefab_source_read_allowed && state.app_focused;
  if (!allowed)
    state.prefab_source_request.reset();
  ImGui::SeparatorText("Prefab instance source");
  ImGui::TextWrapped("Source: %s", placement->source.ToString().c_str());
  ImGui::Text("Retained revision: %llu", static_cast<unsigned long long>(placement->revision));
  if (!node->scope.empty())
    ImGui::Text("Nested scope depth: %zu", node->scope.size());
  ImGui::BeginDisabled(!allowed);
  if (ImGui::Button("Inspect source###editor.prefab.source.inspect"))
    state.prefab_source_request = state.prefab_source_scope;
  const auto low = ImGui::GetItemRectMin(), high = ImGui::GetItemRectMax();
  state.prefab_source_position = std::array{(low.x + high.x) * .5F, (low.y + high.y) * .5F};
  ImGui::EndDisabled();
  if (state.prefab_source_report) {
    const auto &report = *state.prefab_source_report;
    ImGui::TextUnformatted(report.resolved ? "Captured source: available"
                                           : "Captured source: unresolved");
    if (report.published_revision)
      ImGui::Text("Published revision at inspection: %llu",
                  static_cast<unsigned long long>(*report.published_revision));
    else
      ImGui::TextDisabled("Published source unavailable at inspection.");
    if (report.scoped_source) {
      if (report.scoped_source->asset != placement->source)
        ImGui::TextWrapped("Scoped source: %s", report.scoped_source->asset.ToString().c_str());
      ImGui::Text("Scoped revision: %llu",
                  static_cast<unsigned long long>(report.scoped_source->revision));
    }
    ImGui::TextDisabled("Inspect again to refresh captured source status.");
  }
}
