// Serialized Inspector owner; imported metadata only, no filesystem or Runtime mutation.
using InspectorSpriteSourceIndex =
    std::unordered_map<runtime::AssetUuid, const ContentItem *, runtime::AssetUuidHash>;
bool InspectorSpriteSourceAvailable(const InspectorSpriteSourceIndex &index,
                                    runtime::AssetUuid id) {
  const auto found = index.find(id);
  const auto *item = found == index.end() ? nullptr : found->second;
  const auto source = item && item->twod ? item->twod->SpriteSource() : nullptr;
  if (!source || item->id != source->id || item->type != item->twod->Type())
    return false;
  const auto dependencyIt = index.find(source->atlas);
  const auto *dependency = dependencyIt == index.end() ? nullptr : dependencyIt->second;
  const auto atlas = dependency && dependency->twod ? dependency->twod->Atlas() : nullptr;
  const auto *region = atlas ? atlas->Find(source->region) : nullptr;
  // Presentation only. Assignment still validates the complete bounded closure through the model.
  return atlas && dependency->id == atlas->id && dependency->type == dependency->twod->Type() &&
         region && region->width == source->trimWidth && region->height == source->trimHeight;
}

template <typename StateT>
void DrawSpriteSourceInspector(StateT &state, SceneDocument &scene, ProjectContentSession *content,
                               const std::vector<SceneDocument::NodeKey> &keys,
                               std::span<const OpaqueComponentInfo> components, bool editable) {
  if (keys.size() > UnknownComponentStore::kMaximumComponents) {
    state.inspector_sprite_popup.reset();
    state.inspector_sprite_label = "Selection exceeds Sprite source transaction capacity";
    ImGui::TextDisabled("Sprite source edits require at most %zu selected entities.",
                        UnknownComponentStore::kMaximumComponents);
    return;
  }
  bool present{}, firstPresent{}, mixed{}, unsupported{}, missing{};
  // The ordinary Inspector has already obtained owning previews. Do not repeat its linear
  // document lookups per selected entity, or ContentBrowser::Find per source/Atlas reference.
  std::unordered_map<runtime::Id, std::optional<runtime::AssetUuid>> references;
  for (const auto &component : components)
    if (component.type == kSpriteSourceReferenceType ||
        component.type_name == kSpriteSourceReferenceName) {
      auto source = ReadSpriteSourceReference(component);
      unsupported |= !source;
      references.insert_or_assign(component.entity, source);
    }
  InspectorSpriteSourceIndex assets;
  if (content &&
      std::ranges::any_of(references, [](const auto &entry) { return entry.second.has_value(); }))
    for (const auto &item : content->Browser().Items())
      if (item.twod && (item.twod->SpriteSource() || item.twod->Atlas()))
        assets.emplace(item.id, &item);
  std::optional<runtime::AssetUuid> first;
  for (std::size_t index = 0; index < keys.size(); ++index) {
    const auto reference = references.find(keys[index].id);
    const bool found = reference != references.end();
    const auto source = found ? reference->second : std::nullopt;
    if (index == 0) {
      firstPresent = found;
      first = source;
    } else {
      mixed |= found != firstPresent || source != first;
    }
    present |= found;
    missing |= source && !InspectorSpriteSourceAvailable(assets, *source);
  }
  const auto found = first ? assets.find(*first) : assets.end();
  const auto *item = found == assets.end() ? nullptr : found->second;
  state.inspector_sprite_label = unsupported ? "Unsupported reference"
                                 : mixed     ? "Mixed"
                                 : !present  ? "None"
                                 : missing   ? "Missing source or Atlas"
                                 : item      ? PathLabel(item->path)
                                             : "Missing source or Atlas";
  const SpriteSourceContentVersion version{content ? content->Browser().ProjectGeneration() : 0,
                                           content ? content->Browser().Revision() : 0};
  const bool allowed = editable && state.app_focused && content && content->Writable() &&
                       version.project_generation && !unsupported &&
                       keys.size() <= UnknownComponentStore::kMaximumComponents;
  ImGui::SeparatorText("Sprite source");
  const auto capture = [&](std::optional<std::array<float, 2>> &position) {
    if (ImGui::IsItemVisible() && !ImGui::GetCurrentWindow()->Hidden) {
      const auto low = ImGui::GetItemRectMin(), high = ImGui::GetItemRectMax();
      const auto center = ImVec2{(low.x + high.x) * .5F, (low.y + high.y) * .5F};
      if (ImGui::GetCurrentWindow()->ClipRect.Contains(center))
        position = std::array{center.x, center.y};
    }
  };
  std::optional<runtime::AssetUuid> assignment;
  ImGui::BeginDisabled(!allowed);
  ImGui::SetNextItemWidth(std::max(1.0F, ImGui::GetContentRegionAvail().x -
                                             detail::IconButtonExtent() -
                                             ImGui::GetStyle().ItemSpacing.x));
  const bool open =
      ImGui::BeginCombo("##editor.inspector.sprite-source", state.inspector_sprite_label.c_str());
  capture(state.inspector_sprite_positions[0]);
  if (open) {
    auto &popup = state.inspector_sprite_popup;
    // A retained popup owns metadata, paths and keys, never a model borrow. Only a newly opened
    // popup may capture context; a hidden/interrupted popup cannot silently acquire a new target.
    if (!popup && ImGui::IsWindowAppearing() && allowed) {
      popup.emplace();
      popup->owner = reinterpret_cast<std::uintptr_t>(content);
      popup->root = content->Root();
      popup->version = version;
      popup->keys = keys;
      popup->frame = ImGui::GetFrameCount();
      if (popup->catalog.PublishContent(content->Browser(), &popup->error))
        for (const auto &candidate : content->Browser().Items())
          if (popup->catalog.ResolveAsset(candidate.id, version))
            popup->choices.emplace_back(candidate.id, PathLabel(candidate.path));
    }
    const bool current = popup && allowed &&
                         popup->owner == reinterpret_cast<std::uintptr_t>(content) &&
                         popup->root == content->Root() && popup->version == version &&
                         popup->keys == keys && popup->frame >= ImGui::GetFrameCount() - 1;
    if (!current) {
      ImGui::CloseCurrentPopup();
    } else {
      popup->frame = ImGui::GetFrameCount();
      if (!popup->error.empty())
        ImGui::TextWrapped("%s", popup->error.c_str());
      else if (popup->choices.empty())
        ImGui::TextDisabled("No imported Sprite sources are available.");
      for (const auto &[asset, label] : popup->choices) {
        ImGui::PushID(asset.ToString().c_str());
        if (ImGui::Selectable(label.c_str(), !mixed && first == asset))
          assignment = asset;
        std::optional<std::array<float, 2>> position;
        capture(position);
        if (position) {
          state.inspector_sprite_choices.emplace_back(asset, *position);
          if (!state.inspector_sprite_positions[1])
            state.inspector_sprite_positions[1] = position;
        }
        ImGui::PopID();
      }
    }
    if (assignment && current) {
      CancelSceneGestures(state);
      const bool accepted = AssignSpriteSources(scene, popup->keys, *assignment, popup->version,
                                                *content, popup->catalog, allowed);
      CancelInspectorDrafts(state);
      state.inspector_error =
          accepted ? "" : "Sprite source assignment rejected; references preserved.";
    }
    ImGui::EndCombo();
    if (!current || assignment)
      popup.reset();
  } else {
    state.inspector_sprite_popup.reset();
  }
  ImGui::EndDisabled();
  ImGui::SameLine();
  ImGui::BeginDisabled(!allowed || !present);
  bool referenceChanged = assignment.has_value();
  if (detail::IconButton("Clear Sprite source###editor.inspector.sprite-source.clear",
                         detail::EditorIcon::Delete)) {
    referenceChanged = true;
    CancelSceneGestures(state);
    const bool accepted = ClearSpriteSources(scene, keys, version, *content, allowed);
    CancelInspectorDrafts(state);
    state.inspector_sprite_popup.reset();
    state.inspector_error =
        accepted ? "" : "Sprite source clearing rejected; references preserved.";
  }
  capture(state.inspector_sprite_positions[2]);
  ImGui::EndDisabled();
  if (unsupported)
    ImGui::TextWrapped(
        "Unsupported Sprite reference data is preserved. Use a compatible Editor to edit it.");
  else if (missing)
    ImGui::TextWrapped("Some Sprite sources or Atlas dependencies are unavailable. References are "
                       "preserved; reimport or clear them.");
  else if (!allowed)
    ImGui::TextDisabled("Source editing requires a writable project and stopped Play.");

  // The owning component preview predates this frame's controls. Do not display its old source
  // after assignment/Clear. Next frame obtains a fresh preview and imported metadata index.
  if (referenceChanged || unsupported || mixed || missing || !present || !item ||
      !version.project_generation)
    return;
  const bool expanded = ImGui::CollapsingHeader("Source details###editor.inspector.sprite-details");
  capture(state.inspector_sprite_positions[3]);
  if (!expanded)
    return;
  const auto &source = *item->twod->SpriteSource();
  const auto &atlas = *assets.at(source.atlas)->twod->Atlas();
  const auto &region = *atlas.Find(source.region);
  std::array<char, 768> text{};
  std::snprintf(text.data(), text.size(),
                "Source UUID: %s\nOriginal: %u x %u px\n"
                "Trim (top-left): %u, %u; %u x %u px\n"
                "Pivot (bottom-left): %.6g, %.6g px\nPixels per unit: %.6g\n"
                "Borders (L/R/T/B): %u / %u / %u / %u px\n"
                "Atlas UUID: %s\nAtlas: %u x %u px\n"
                "Region %u (top-left): %u, %u; %u x %u px\n",
                source.id.ToString().c_str(), source.sourceWidth, source.sourceHeight, source.trimX,
                source.trimY, source.trimWidth, source.trimHeight, double(source.pivotX),
                double(source.pivotY), double(source.pixelsPerUnit), source.border[0],
                source.border[1], source.border[2], source.border[3], atlas.id.ToString().c_str(),
                atlas.width, atlas.height, region.id, region.x, region.y, region.width,
                region.height);
  state.inspector_sprite_details = text.data();
  twod::SpriteInstance witness;
  witness.id = 1;
  witness.matrix = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
  const auto resolved = twod::ResolveSpriteAsset(source, atlas, std::move(witness));
  if (resolved && resolved->localQuad) {
    const auto &quad = *resolved->localQuad;
    std::snprintf(text.data(), text.size(),
                  "Local bounds (units, before entity transform):\n(%.6g, %.6g) to (%.6g, %.6g)",
                  double(quad.minimum.x), double(quad.minimum.y), double(quad.maximum.x),
                  double(quad.maximum.y));
    state.inspector_sprite_details += text.data();
  } else {
    state.inspector_sprite_details += "Local bounds unavailable for this source geometry.";
  }
  ImGui::TextWrapped("%s", state.inspector_sprite_details.c_str());
}
