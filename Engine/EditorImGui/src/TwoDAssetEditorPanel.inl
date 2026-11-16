#if defined(NEXORA_HAS_2D)
namespace {
// Bounded CPU preview of the immutable imported pixels. The GUI performs no IO or GPU allocation.
void DrawAtlasSlicePreview(const twod::Atlas &atlas, std::optional<twod::Region> selected) {
  if (!atlas.width || !atlas.height ||
      atlas.rgba.size() != std::size_t(atlas.width) * atlas.height * 4)
    return;
  const float extent = std::min(300.0F, ImGui::GetContentRegionAvail().x);
  const float scale = extent / float(std::max(atlas.width, atlas.height));
  const auto origin = ImGui::GetCursorScreenPos();
  auto *draw = ImGui::GetWindowDrawList();
  const auto width = std::min(atlas.width, 64U), height = std::min(atlas.height, 64U);
  for (std::uint32_t y = 0; y < height; ++y)
    for (std::uint32_t x = 0; x < width; ++x) {
      const auto px = x * atlas.width / width, py = y * atlas.height / height;
      const auto index = (std::size_t(py) * atlas.width + px) * 4;
      const auto color = IM_COL32(
          std::to_integer<int>(atlas.rgba[index]), std::to_integer<int>(atlas.rgba[index + 1]),
          std::to_integer<int>(atlas.rgba[index + 2]), std::to_integer<int>(atlas.rgba[index + 3]));
      draw->AddRectFilled({origin.x + float(x) * float(atlas.width) * scale / float(width),
                           origin.y + float(y) * float(atlas.height) * scale / float(height)},
                          {origin.x + float(x + 1) * float(atlas.width) * scale / float(width),
                           origin.y + float(y + 1) * float(atlas.height) * scale / float(height)},
                          color);
    }
  for (const auto &region : atlas.regions) {
    const bool active = selected && selected->id == region.id;
    draw->AddRect({origin.x + float(region.x) * scale, origin.y + float(region.y) * scale},
                  {origin.x + float(region.x + region.width) * scale,
                   origin.y + float(region.y + region.height) * scale},
                  active ? IM_COL32(255, 215, 0, 255) : IM_COL32(0, 220, 255, 255), 0, 0,
                  active ? 2.0F : 1.0F);
  }
  ImGui::Dummy({float(atlas.width) * scale, float(atlas.height) * scale});
}
template <class StateT>
void DrawTwoDAssetEditor(StateT &state, ProjectWorkspace *workspace,
                         ProjectContentSession *content) {
  constexpr auto popup = "Atlas / Clip editor###editor.content.twod-asset-edit";
  if (const auto open = std::exchange(state.asset_editor_open, std::nullopt)) {
    const auto *item = content ? content->Browser().Find(*open) : nullptr;
    if (state.sprite_creator_allowed && workspace && item && item->twod &&
        item->thumbnail == ThumbnailState::Ready && item->twod->Id() == *open &&
        item->type == item->twod->Type() && (item->twod->Atlas() || item->twod->Clip())) {
      state.asset_editor.emplace();
      auto &draft = *state.asset_editor;
      draft.baseline = {workspace->Project().id,
                        workspace->Root(),
                        item->path,
                        {content->Browser().ProjectGeneration(), content->Browser().Revision()},
                        item->id,
                        item->type,
                        item->artifact_hash,
                        {},
                        {}};
      if (const auto atlas = item->twod->Atlas()) {
        draft.atlas = *atlas;
        draft.baseline.baseline = twod::EncodeAtlas(*atlas);
      } else {
        draft.clip = *item->twod->Clip();
        draft.baseline.baseline = twod::EncodeClip(draft.clip);
        const auto *dependency = content->Browser().Find(draft.clip.atlas);
        if (dependency && dependency->twod && dependency->twod->Atlas())
          draft.atlas = *dependency->twod->Atlas();
        SpriteSourceCatalog catalog;
        if (catalog.PublishAvailableContent(content->Browser()))
          for (const auto &candidate : content->Browser().Items())
            if (auto snapshot = catalog.ResolveAsset(candidate.id, draft.baseline.version)) {
              draft.sources.emplace_back(candidate.id, PathLabel(candidate.path));
              draft.snapshots.emplace(candidate.id, std::move(*snapshot));
            }
        static_cast<void>(draft.preview.Play(draft.clip));
      }
      state.asset_editor_status.clear();
      ImGui::OpenPopup(popup);
    }
  }
  if (!state.asset_editor)
    return;
  auto &draft = *state.asset_editor;
  const bool scope = workspace && content && draft.baseline.root == workspace->Root() &&
                     draft.baseline.project == workspace->Project().id &&
                     content->Root() == draft.baseline.root;
  const bool current =
      scope &&
      draft.baseline.version == SpriteSourceContentVersion{content->Browser().ProjectGeneration(),
                                                           content->Browser().Revision()};
  const bool authorized = current && state.sprite_creator_allowed;
  if (!authorized) {
    state.asset_editor_request.reset();
    draft.submitted = 0;
  }
  const auto work = ImGui::GetMainViewport()->WorkSize;
  ImGui::SetNextWindowSize({std::min(660.0F, std::max(1.0F, work.x - 16.0F)),
                            std::min(740.0F, std::max(1.0F, work.y - 16.0F))},
                           ImGuiCond_Appearing);
  if (!ImGui::BeginPopupModal(popup, nullptr, ImGuiWindowFlags_NoSavedSettings)) {
    state.asset_editor.reset();
    state.asset_editor_request.reset();
    return;
  }
  if (!scope || draft.dismiss) {
    ImGui::CloseCurrentPopup();
    state.asset_editor.reset();
    state.asset_editor_request.reset();
    ImGui::EndPopup();
    return;
  }
  const auto capture = [&](std::size_t index) {
    if (!ImGui::GetCurrentWindow()->Hidden && ImGui::IsItemVisible()) {
      const auto lo = ImGui::GetItemRectMin(), hi = ImGui::GetItemRectMax();
      state.asset_editor_controls[index] = std::array{lo.x, lo.y, hi.x, hi.y};
    }
  };
  const bool atlas = draft.baseline.type == ".nx2atlas";
  ImGui::TextWrapped("%s", PathLabel(draft.baseline.path).c_str());
  ImGui::TextWrapped(
      "Shared asset: Save preserves UUID and updates references. Scene Undo does not "
      "revert saved asset files. Cancel discards this draft.");
  const auto oldRegions = draft.atlas.regions;
  const auto oldSelected = draft.selected;
  const auto oldClip = draft.clip;
  bool changed{}, history{};
  ImGui::BeginDisabled(!authorized || draft.submitted);
  const auto undo = [&]<class T>(T &value, std::vector<T> &past, std::vector<T> &future) {
    ImGui::BeginDisabled(past.empty());
    if (ImGui::Button("Undo draft")) {
      future.push_back(value);
      value = std::move(past.back());
      past.pop_back();
      history = true;
    }
    capture(0);
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(future.empty());
    if (ImGui::Button("Redo draft")) {
      past.push_back(value);
      value = std::move(future.back());
      future.pop_back();
      history = true;
    }
    capture(1);
    ImGui::EndDisabled();
  };
  if (atlas) {
    auto regions = std::pair{std::move(draft.atlas.regions), draft.selected};
    undo(regions, draft.atlasUndo, draft.atlasRedo);
    draft.atlas.regions = std::move(regions.first);
    draft.selected = regions.second;
    ImGui::Text("%u x %u pixels; one-pixel extruded gutters required", draft.atlas.width,
                draft.atlas.height);
    ImGui::InputInt2("Cell width / height", draft.cell.data());
    capture(2);
    ImGui::InputInt2("First X / Y", draft.offset.data());
    capture(3);
    ImGui::InputInt2("Gap X / Y", draft.gap.data());
    capture(4);
    const bool grid = draft.cell[0] > 0 && draft.cell[1] > 0 && draft.cell[0] <= 4096 &&
                      draft.cell[1] <= 4096 && draft.offset[0] >= 1 && draft.offset[1] >= 1 &&
                      draft.offset[0] <= 4096 && draft.offset[1] <= 4096 && draft.gap[0] >= 2 &&
                      draft.gap[1] >= 2 && draft.gap[0] <= 4096 && draft.gap[1] <= 4096;
    ImGui::BeginDisabled(!grid);
    if (ImGui::Button("Slice grid")) {
      auto candidate = draft.atlas;
      candidate.regions.clear();
      for (int y = draft.offset[1]; y + draft.cell[1] < int(candidate.height);
           y += draft.cell[1] + draft.gap[1])
        for (int x = draft.offset[0]; x + draft.cell[0] < int(candidate.width);
             x += draft.cell[0] + draft.gap[0])
          if (candidate.regions.size() <= 1024)
            candidate.regions.push_back(
                {std::uint32_t(candidate.regions.size() + 1), std::uint32_t(x), std::uint32_t(y),
                 std::uint32_t(draft.cell[0]), std::uint32_t(draft.cell[1])});
      if (twod::ValidateAtlas(candidate)) {
        draft.atlas.regions = std::move(candidate.regions);
        draft.selected = 0;
        changed = true;
      } else
        draft.status = "Grid must yield 1..1024 non-overlapping regions with gutters.";
    }
    capture(5);
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(draft.atlas.regions.size() >= 1024);
    if (ImGui::Button("Add slice")) {
      std::uint32_t next = draft.atlas.regions.empty() ? 1 : draft.atlas.regions.back().id + 1;
      if (next) {
        draft.atlas.regions.push_back({next, 1, 1, 1, 1});
        draft.selected = draft.atlas.regions.size() - 1;
        changed = true;
      }
    }
    capture(6);
    ImGui::EndDisabled();
    if (!draft.atlas.regions.empty()) {
      draft.selected = std::min(draft.selected, draft.atlas.regions.size() - 1);
      const auto label = "Region " + std::to_string(draft.atlas.regions[draft.selected].id);
      if (ImGui::BeginCombo("Selected slice", label.c_str())) {
        for (std::size_t i = 0; i < draft.atlas.regions.size(); ++i) {
          ImGui::PushID(int(i));
          if (ImGui::Selectable(("Region " + std::to_string(draft.atlas.regions[i].id)).c_str(),
                                i == draft.selected))
            draft.selected = i;
          ImGui::PopID();
        }
        ImGui::EndCombo();
      }
      capture(7);
      auto &region = draft.atlas.regions[draft.selected];
      changed |= ImGui::InputScalar("Region ID", ImGuiDataType_U32, &region.id);
      capture(8);
      changed |= ImGui::InputScalar("X", ImGuiDataType_U32, &region.x);
      capture(9);
      changed |= ImGui::InputScalar("Y", ImGuiDataType_U32, &region.y);
      capture(10);
      changed |= ImGui::InputScalar("Width", ImGuiDataType_U32, &region.width);
      capture(11);
      changed |= ImGui::InputScalar("Height", ImGuiDataType_U32, &region.height);
      capture(12);
      if (ImGui::Button("Remove slice")) {
        draft.atlas.regions.erase(draft.atlas.regions.begin() + std::ptrdiff_t(draft.selected));
        changed = true;
      }
      capture(15);
      if (changed && !draft.atlas.regions.empty()) {
        const auto selected = std::min(draft.selected, draft.atlas.regions.size() - 1);
        std::vector<std::pair<twod::Region, std::size_t>> ordered;
        ordered.reserve(draft.atlas.regions.size());
        for (std::size_t i = 0; i < draft.atlas.regions.size(); ++i)
          ordered.emplace_back(draft.atlas.regions[i], i);
        std::ranges::sort(ordered, [](const auto &a, const auto &b) {
          return a.first.id == b.first.id ? a.second < b.second : a.first.id < b.first.id;
        });
        for (std::size_t i = 0; i < ordered.size(); ++i) {
          draft.atlas.regions[i] = ordered[i].first;
          if (ordered[i].second == selected)
            draft.selected = i;
        }
      }
    }
  } else {
    undo(draft.clip, draft.clipUndo, draft.clipRedo);
    changed |= ImGui::Checkbox("Loop", &draft.clip.loop);
    capture(2);
    const bool sources = twod::ClipUsesSources(draft.clip);
    ImGui::BeginDisabled(draft.clip.frames.size() >= 4096);
    if (sources || draft.clip.frames.empty()) {
      if (ImGui::BeginCombo("Append source frame", "Choose imported Sprite source")) {
        for (const auto &[id, label] : draft.sources) {
          ImGui::PushID(id.ToString().c_str());
          if (ImGui::Selectable(label.c_str())) {
            draft.clip.atlas = {};
            draft.clip.frames.push_back({0, .1, {}, id});
            draft.selected = draft.clip.frames.size() - 1;
            changed = true;
          }
          if (id == draft.sources.front().first)
            capture(5);
          if (ImGui::IsItemHovered())
            ImGui::SetTooltip("%s", label.c_str());
          ImGui::PopID();
        }
        ImGui::EndCombo();
      }
      capture(3);
    }
    if (!sources && draft.atlas.id != runtime::AssetUuid{}) {
      if (ImGui::BeginCombo("Append region frame", "Choose Atlas region")) {
        for (const auto &region : draft.atlas.regions)
          if (ImGui::Selectable(("Region " + std::to_string(region.id)).c_str())) {
            draft.clip.atlas = draft.atlas.id;
            draft.clip.frames.push_back({region.id, .1, {}});
            draft.selected = draft.clip.frames.size() - 1;
            changed = true;
          }
        ImGui::EndCombo();
      }
      capture(4);
    }
    ImGui::EndDisabled();
    if (!draft.clip.frames.empty()) {
      draft.selected = std::min(draft.selected, draft.clip.frames.size() - 1);
      if (ImGui::BeginCombo("Selected frame", std::to_string(draft.selected + 1).c_str())) {
        ImGuiListClipper clipper;
        clipper.Begin(int(draft.clip.frames.size()));
        while (clipper.Step())
          for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i)
            if (ImGui::Selectable(std::to_string(i + 1).c_str(), draft.selected == std::size_t(i)))
              draft.selected = std::size_t(i);
        ImGui::EndCombo();
      }
      capture(7);
      auto &frame = draft.clip.frames[draft.selected];
      changed |= ImGui::InputDouble("Seconds", &frame.seconds, 0, 0, "%.6f");
      capture(8);
      std::array<char, 257> event{};
      std::copy(frame.event.begin(), frame.event.end(), event.begin());
      if (ImGui::InputText("Event", event.data(), event.size())) {
        frame.event = event.data();
        changed = true;
      }
      capture(9);
      if (frame.source) {
        if (ImGui::BeginCombo("Source", frame.source->ToString().c_str())) {
          for (const auto &[id, label] : draft.sources) {
            ImGui::PushID(id.ToString().c_str());
            if (ImGui::Selectable(label.c_str(), *frame.source == id)) {
              frame.source = id;
              changed = true;
            }
            if (id == draft.sources.back().first)
              capture(6);
            if (ImGui::IsItemHovered())
              ImGui::SetTooltip("%s", label.c_str());
            ImGui::PopID();
          }
          ImGui::EndCombo();
        }
      } else
        changed |= ImGui::InputScalar("Region ID", ImGuiDataType_U32, &frame.region);
      capture(10);
      ImGui::BeginDisabled(draft.selected == 0);
      if (ImGui::Button("Move earlier")) {
        std::swap(draft.clip.frames[draft.selected], draft.clip.frames[draft.selected - 1]);
        --draft.selected;
        changed = true;
      }
      capture(11);
      ImGui::EndDisabled();
      ImGui::SameLine();
      ImGui::BeginDisabled(draft.selected + 1 == draft.clip.frames.size());
      if (ImGui::Button("Move later")) {
        std::swap(draft.clip.frames[draft.selected], draft.clip.frames[draft.selected + 1]);
        ++draft.selected;
        changed = true;
      }
      capture(12);
      ImGui::EndDisabled();
      ImGui::SameLine();
      if (ImGui::Button("Remove frame")) {
        draft.clip.frames.erase(draft.clip.frames.begin() + std::ptrdiff_t(draft.selected));
        changed = true;
      }
      capture(15);
    }
  }
  if (changed && !history) {
    if (atlas) {
      draft.atlasUndo.emplace_back(oldRegions, oldSelected);
      draft.atlasRedo.clear();
      if (draft.atlasUndo.size() > 32)
        draft.atlasUndo.erase(draft.atlasUndo.begin());
    } else {
      draft.clipUndo.push_back(oldClip);
      draft.clipRedo.clear();
      if (draft.clipUndo.size() > 32)
        draft.clipUndo.erase(draft.clipUndo.begin());
    }
  }
  if (!atlas && (changed || history)) {
    draft.frame.reset();
    static_cast<void>(draft.preview.Play(draft.clip));
  }
  ImGui::EndDisabled();
  if (!atlas) {
    ImGui::Checkbox("Pause preview", &draft.paused);
    if (ImGui::Button("Restart preview")) {
      draft.frame.reset();
      static_cast<void>(draft.preview.Play(draft.clip));
    }
    draft.preview.Pause(draft.paused);
    if (twod::ValidateClip(draft.clip)) {
      if (auto frame =
              draft.preview.Advance(std::clamp(double(ImGui::GetIO().DeltaTime), 0.0, .25)))
        draft.frame = std::move(frame);
    } else {
      draft.frame.reset();
    }
    if (draft.frame) {
      ImGui::Text("Preview region: %u%s", draft.frame->region,
                  draft.frame->finished ? " (finished)" : "");
      if (draft.frame->source)
        ImGui::Text("Source: %s", draft.frame->source->ToString().c_str());
      for (const auto &event : draft.frame->events)
        ImGui::Text("Event: %s", event.c_str());
    }
  }
  if (draft.dirty || changed || history) {
    draft.valid = atlas ? twod::ValidateAtlas(draft.atlas) : twod::ValidateClip(draft.clip);
    draft.baseline.proposed =
        draft.valid ? (atlas ? twod::EncodeAtlas(draft.atlas) : twod::EncodeClip(draft.clip))
                    : twod::Bytes{};
    draft.dirty = false;
  }
  const bool valid = draft.valid;
  const auto &proposed = draft.baseline.proposed;
  const auto message = !current      ? TwoDAssetEditingMessage(TwoDAssetEditingError::Stale)
                       : !authorized ? TwoDAssetEditingMessage(TwoDAssetEditingError::Denied)
                       : !valid      ? TwoDAssetEditingMessage(TwoDAssetEditingError::Data)
                                     : std::string_view(draft.status);
  ImGui::TextWrapped("%.*s", int(message.size()), message.data());
  ImGui::BeginDisabled(!authorized || !valid || proposed == draft.baseline.baseline ||
                       draft.submitted || state.asset_editor_request ||
                       state.asset_editor_next == std::numeric_limits<std::uint64_t>::max());
  if (ImGui::Button("Save asset")) {
    auto request = draft.baseline;
    draft.submitted = state.asset_editor_next++;
    state.asset_editor_request = TwoDAssetEditingUiRequest{draft.submitted, std::move(request)};
  }
  capture(13);
  ImGui::EndDisabled();
  ImGui::SameLine();
  const bool cancel = ImGui::Button("Cancel");
  capture(14);
  if (valid && atlas) {
    const auto index = std::min(draft.selected, draft.atlas.regions.size() - 1);
    DrawAtlasSlicePreview(draft.atlas, draft.atlas.regions[index]);
  }
  if (!atlas && draft.frame) {
    if (draft.frame->source) {
      const auto found = draft.snapshots.find(*draft.frame->source);
      if (found != draft.snapshots.end()) {
        const auto &snapshot = found->second;
        DrawAtlasSlicePreview(*snapshot.atlas, *snapshot.atlas->Find(snapshot.source->region));
        ImGui::Text("Trim: %u x %u; Pivot: %.3f, %.3f; PPU: %.3f", snapshot.source->trimWidth,
                    snapshot.source->trimHeight, double(snapshot.source->pivotX),
                    double(snapshot.source->pivotY), double(snapshot.source->pixelsPerUnit));
      }
    } else if (const auto *region = draft.atlas.Find(draft.frame->region))
      DrawAtlasSlicePreview(draft.atlas, *region);
  }
  if (cancel) {
    ImGui::CloseCurrentPopup();
    state.asset_editor_request.reset();
    state.asset_editor.reset();
  }
  ImGui::EndPopup();
}
} // namespace
std::optional<TwoDAssetEditingUiRequest> EditorImGuiHost::TakeTwoDAssetEditingRequest() {
  auto request = std::exchange(state_->asset_editor_request, std::nullopt);
  if (!state_->app_focused || !state_->sprite_creator_allowed || !state_->asset_editor ||
      !request || state_->asset_editor->submitted != request->identity)
    return std::nullopt;
  return request;
}
void EditorImGuiHost::SetTwoDAssetEditingResult(std::uint64_t identity, bool saved,
                                                std::string message) {
  if (!state_->asset_editor || state_->asset_editor->submitted != identity)
    return;
  state_->asset_editor->submitted = 0;
  state_->asset_editor->status = message;
  state_->asset_editor->dismiss = saved;
  state_->asset_editor_status = std::move(message);
}
#endif
