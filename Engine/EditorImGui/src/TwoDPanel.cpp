#include "Nexora/EditorImGui/TwoDPanel.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <imgui.h>
namespace nexora::editor::imgui {
namespace {
void Result(TwoDPanelState &s, bool result) {
  s.status = result ? "Edit applied." : "Edit rejected; check references and document access.";
}
void Frame(ImDrawList &list, const twod::SpriteFrame &frame, ImVec2 origin) {
  for (const auto &batch : frame.batches)
    for (std::size_t i = batch.indexOffset; i < batch.indexOffset + batch.elementCount; i += 6) {
      const auto *a = &frame.vertices[frame.indices[i]], *b = &frame.vertices[frame.indices[i + 1]],
                 *c = &frame.vertices[frame.indices[i + 2]],
                 *d = &frame.vertices[frame.indices[i + 5]];
      const auto p = [&](const auto *v) {
        return ImVec2(origin.x + v->position[0], origin.y + v->position[1]);
      };
      list.AddImageQuad(ImTextureID(batch.textureId), p(a), p(b), p(c), p(d), {a->uv[0], a->uv[1]},
                        {b->uv[0], b->uv[1]}, {c->uv[0], c->uv[1]}, {d->uv[0], d->uv[1]}, a->color);
    }
}
} // namespace
void DrawTwoDPanel(TwoDDocument &doc, TwoDPanelState &state, bool allowed, std::uint64_t texture,
                   float seconds) {
  if (!ImGui::Begin("2D Studio###editor.twod")) {
    ImGui::End();
    return;
  }
  const bool writable = allowed && !doc.ReadOnly();
  ImGui::TextUnformatted(doc.Dirty() ? "Unsaved changes" : "Saved document");
  ImGui::BeginDisabled(!writable);
  if (ImGui::Button("Undo"))
    Result(state, doc.Undo());
  ImGui::SameLine();
  if (ImGui::Button("Redo"))
    Result(state, doc.Redo());
  ImGui::SameLine();
  if (ImGui::Button("Save"))
    state.saveRequested = true;
  ImGui::SameLine();
  if (ImGui::Button("Open"))
    state.openRequested = true;
  if (ImGui::Button("Import atlas"))
    state.importAtlasRequested = true;
  ImGui::SameLine();
  if (ImGui::Button("Import clip"))
    state.importClipRequested = true;
  ImGui::EndDisabled();
  if (!state.status.empty())
    ImGui::TextWrapped("%s", state.status.c_str());
  if (ImGui::BeginTabBar("2D tools")) {
    const auto requestedTab = state.requestedTab;
    state.requestedTab.reset();
    const auto flags = [&](TwoDPanelTab tab) {
      return requestedTab == tab ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None;
    };
    if (ImGui::BeginTabItem("Scene", nullptr, flags(TwoDPanelTab::Scene))) {
      const auto &atlas = doc.Atlas();
      if (!state.region && !atlas.regions.empty())
        state.region = atlas.regions.front().id;
      ImGui::BeginDisabled(!writable || !texture || !atlas.Find(state.region));
      if (ImGui::Button("Add sprite")) {
        twod::Sprite s;
        s.atlas = atlas.id;
        s.region = state.region;
        auto id = doc.AddSprite(s);
        if (id)
          state.selected = *id;
        Result(state, bool(id));
      }
      ImGui::EndDisabled();
      if (ImGui::BeginCombo("Entity", std::to_string(state.selected).c_str())) {
        for (const auto &[id, s] : doc.Scene().Sprites()) {
          (void)s;
          const auto label = std::to_string(id);
          if (ImGui::Selectable(label.c_str(), id == state.selected))
            state.selected = id;
        }
        ImGui::EndCombo();
      }
      if (const auto *current = doc.Scene().Find(state.selected)) {
        auto sprite = *current;
        const auto *entity = doc.World().FindEntity(state.selected);
        auto pose = entity->transform;
        float xy[]{float(pose.x), float(pose.y)}, scale[]{float(pose.sx), float(pose.sy)};
        bool changed = false;
        ImGui::BeginDisabled(!writable);
        changed |= ImGui::DragFloat2("Position", xy, 0.02F);
        changed |= ImGui::DragFloat2("Scale", scale, 0.02F);
        changed |= ImGui::DragFloat2("Size", &sprite.size.x, 0.02F, 0.001F, 1000);
        changed |= ImGui::DragFloat2("Pivot", &sprite.pivot.x, 0.01F, -10, 10);
        changed |= ImGui::ColorEdit4("Tint", sprite.color.data());
        changed |= ImGui::Checkbox("Flip X", &sprite.flipX);
        ImGui::SameLine();
        changed |= ImGui::Checkbox("Flip Y", &sprite.flipY);
        changed |= ImGui::Checkbox("Y sort", &sprite.sortY);
        changed |= ImGui::InputInt("Layer", &sprite.layer);
        changed |= ImGui::InputInt("Order", &sprite.order);
        if (changed) {
          pose.x = xy[0];
          pose.y = xy[1];
          pose.sx = scale[0];
          pose.sy = scale[1];
          Result(state, doc.EditSprite(state.selected, sprite, pose));
        }
        auto collider = doc.Scene().FindCollider(state.selected)
                            ? *doc.Scene().FindCollider(state.selected)
                            : twod::Collider{};
        int kind = int(collider.kind);
        bool collisionChanged = ImGui::Combo("Collider", &kind, "Circle\0AABB\0");
        collider.kind = twod::ShapeKind(kind);
        collisionChanged |= ImGui::DragFloat2("Collision offset", &collider.offset.x, 0.02F);
        collisionChanged |=
            collider.kind == twod::ShapeKind::Circle
                ? ImGui::DragFloat("Radius", &collider.radius, 0.02F, 0, 1000)
                : ImGui::DragFloat2("Half extent", &collider.halfExtent.x, 0.02F, 0, 1000);
        if (ImGui::Button("Apply collider") || collisionChanged)
          Result(state, doc.SetCollider(state.selected, collider));
        ImGui::EndDisabled();
      }
      ImGui::DragFloat2("Camera center", &state.camera.center.x, 0.05F);
      ImGui::DragFloat("View height", &state.camera.verticalSize, 0.1F, 0.1F, 1000);
      auto &camera = state.camera;
      if (!std::isfinite(camera.center.x) || !std::isfinite(camera.center.y) ||
          std::abs(camera.center.x) > 1000000 || std::abs(camera.center.y) > 1000000)
        camera.center = {};
      if (!std::isfinite(camera.verticalSize))
        camera.verticalSize = 20;
      camera.verticalSize = std::clamp(camera.verticalSize, 0.1F, 1000.0F);
      if (!std::isfinite(camera.rotationRadians) || std::abs(camera.rotationRadians) > 1000000)
        camera.rotationRadians = 0;

      const auto origin = ImGui::GetCursorScreenPos();
      auto size = ImGui::GetContentRegionAvail();
      size.x = std::clamp(size.x, 64.0F, 4096.0F);
      size.y = std::clamp(size.y, 160.0F, 4096.0F);
      state.camera.width = std::uint32_t(size.x);
      state.camera.height = std::uint32_t(size.y);
      auto *draw = ImGui::GetWindowDrawList();
      draw->AddRectFilled(origin, {origin.x + size.x, origin.y + size.y},
                          IM_COL32(10, 20, 41, 255));
      draw->PushClipRect(origin, {origin.x + size.x, origin.y + size.y}, true);
      const auto resolve = [&](twod::AssetId id) -> std::optional<twod::AtlasBinding> {
        if (id != atlas.id || !texture)
          return std::nullopt;
        return twod::AtlasBinding{std::make_shared<const twod::Atlas>(atlas), texture};
      };
      if (texture) {
        if (auto tiles = doc.Tiles().Extract(state.camera, resolve))
          Frame(*draw, *tiles, origin);
        if (auto frame = doc.Scene().Extract(state.camera, resolve))
          Frame(*draw, *frame, origin);
      }
      const auto scale = double(state.camera.height) / state.camera.verticalSize;
      const double cosine = std::cos(state.camera.rotationRadians),
                   sine = std::sin(state.camera.rotationRadians);
      const auto p = [&](math::Vector2 v) {
        const double x = double(v.x) - state.camera.center.x,
                     y = double(v.y) - state.camera.center.y;
        return ImVec2(origin.x + float((cosine * x + sine * y) * scale) + size.x / 2,
                      origin.y - float((-sine * x + cosine * y) * scale) + size.y / 2);
      };
      if (auto shapes = doc.Scene().CollisionShapes())
        for (const auto &shape : *shapes) {
          if (shape.kind == twod::ShapeKind::Circle)
            draw->AddCircle(p(shape.center), float(shape.radius * scale),
                            IM_COL32(80, 255, 120, 255), 32);
          else {
            auto bounds = twod::ShapeBounds(shape);
            draw->AddQuad(p(bounds.minimum), p({bounds.maximum.x, bounds.minimum.y}),
                          p(bounds.maximum), p({bounds.minimum.x, bounds.maximum.y}),
                          IM_COL32(80, 255, 120, 255));
          }
        }
      draw->PopClipRect();
      ImGui::Dummy(size);
      ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("Atlas", nullptr, flags(TwoDPanelTab::Atlas))) {
      ImGui::Text("%u x %u; %zu regions; %s", doc.Atlas().width, doc.Atlas().height,
                  doc.Atlas().regions.size(), doc.Atlas().nearest ? "Nearest" : "Linear");
      ImGui::TextWrapped("PNG rectangles or uniform grids are sliced by "
                         "Tools/TwoD/PrepareAssets.py. Import the generated .nx2atlas here.");
      if (texture)
        for (const auto &r : doc.Atlas().regions) {
          const auto &a = doc.Atlas();
          ImGui::PushID(int(r.id));
          if (ImGui::ImageButton(
                  "region", ImTextureID(texture), {48, 48},
                  {float(r.x) / float(a.width), float(r.y) / float(a.height)},
                  {float(r.x + r.width) / float(a.width), float(r.y + r.height) / float(a.height)}))
            state.region = r.id;
          ImGui::SameLine();
          ImGui::Text("Region %u: %u x %u%s", r.id, r.width, r.height,
                      state.region == r.id ? " (selected)" : "");
          ImGui::PopID();
        }
      ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("Animation", nullptr, flags(TwoDPanelTab::Animation))) {
      auto clip = doc.Clip();
      ImGui::BeginDisabled(!writable || !doc.Atlas().Find(state.region));
      if (ImGui::Button("Append selected region")) {
        if (clip.frames.empty()) {
          clip.id = {doc.Atlas().id.high ^ 0x434c4950U, doc.Atlas().id.low};
          clip.atlas = doc.Atlas().id;
        }
        clip.frames.push_back({state.region, 0.1, {}});
        Result(state, doc.ReplaceClip(clip));
      }
      bool changed = ImGui::Checkbox("Loop", &clip.loop);
      for (std::size_t i = 0; i < clip.frames.size(); ++i) {
        ImGui::PushID(int(i));
        auto &f = clip.frames[i];
        float duration = float(f.seconds);
        changed |= ImGui::DragFloat("Seconds", &duration, 0.01F, 0.001F, 3600);
        f.seconds = duration;
        char event[257]{};
        std::snprintf(event, sizeof(event), "%s", f.event.c_str());
        if (ImGui::InputText("Event", event, sizeof(event))) {
          f.event = event;
          changed = true;
        }
        ImGui::Text("Region %u", f.region);
        if (i > 0 && ImGui::Button("Move earlier")) {
          std::swap(clip.frames[i], clip.frames[i - 1]);
          changed = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Remove")) {
          clip.frames.erase(clip.frames.begin() + std::ptrdiff_t(i));
          changed = true;
          ImGui::PopID();
          break;
        }
        ImGui::PopID();
      }
      if (changed)
        Result(state, doc.ReplaceClip(clip));
      ImGui::EndDisabled();
      ImGui::Checkbox("Pause preview", &state.previewPaused);
      if (state.previewRevision != doc.Revision()) {
        state.previewRevision = doc.Revision();
        static_cast<void>(state.preview.Play(doc.Clip()));
      }
      state.preview.Pause(state.previewPaused);
      if (auto update = state.preview.Advance(std::clamp(double(seconds), 0.0, 0.25))) {
        if (texture) {
          if (const auto *r = doc.Atlas().Find(update->region)) {
            const auto &a = doc.Atlas();
            ImGui::Image(ImTextureID(texture), {128, 128},
                         {float(r->x) / float(a.width), float(r->y) / float(a.height)},
                         {float(r->x + r->width) / float(a.width),
                          float(r->y + r->height) / float(a.height)});
          }
        }
        for (const auto &event : update->events)
          ImGui::Text("Event: %s", event.c_str());
      }
      ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("Tilemap", nullptr, flags(TwoDPanelTab::Tilemap))) {
      ImGui::InputInt("Cell X", &state.tileX);
      ImGui::InputInt("Cell Y", &state.tileY);
      ImGui::InputInt("Tile layer", &state.tileLayer);
      ImGui::Checkbox("Collision tile", &state.collision);
      ImGui::BeginDisabled(!writable || !doc.Atlas().Find(state.region));
      const twod::Tile tile{state.tileX, state.tileY, state.tileLayer, state.region,
                            state.collision};
      if (ImGui::Button("Paint"))
        Result(state, doc.Paint(std::span(&tile, 1)));
      ImGui::SameLine();
      if (ImGui::Button("Erase")) {
        auto erase = tile;
        erase.region = 0;
        Result(state, doc.Paint(std::span(&erase, 1)));
      }
      if (ImGui::Button("Unload chunk"))
        Result(state, doc.SetChunkLoaded(twod::Tilemap::Chunk(state.tileX, state.tileY), false));
      ImGui::SameLine();
      if (ImGui::Button("Load chunk"))
        Result(state, doc.SetChunkLoaded(twod::Tilemap::Chunk(state.tileX, state.tileY), true));
      ImGui::EndDisabled();
      ImGui::Text("%zu painted cells; 16 x 16 chunks", doc.Tiles().Tiles().size());
      ImGui::EndTabItem();
    }
    ImGui::EndTabBar();
  }
  ImGui::End();
}
} // namespace nexora::editor::imgui
