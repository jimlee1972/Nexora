#include "Nexora/Runtime/PresentationV2.h"

#include <array>
#include <cmath>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}

int RunTests() {
  using namespace nexora::runtime;
  using namespace nexora::runtime::presentation;
  using namespace nexora::runtime::presentation_v2;

  Timeline timeline{10.0};
  Require(timeline.AddTrack({"camera.fov", {{0.0, 60.0F}, {10.0, 80.0F}}}),
          "timeline track was rejected");
  Require(timeline.Seek(2.5) && std::abs(*timeline.Evaluate("camera.fov") - 65.0F) < 0.001F,
          "timeline seek/evaluate failed");
  Require(timeline.Seek(999.0) && timeline.Time() == 10.0 &&
              std::abs(*timeline.Evaluate("camera.fov") - 80.0F) < 0.001F,
          "timeline scrub did not clamp deterministically");

  CameraRig rig;
  Require(rig.SetLayers({{0, 1.0F, {0, 0, 5, 60}}, {10, 1.0F, {10, 2, 3, 70}},
                         {10, 3.0F, {30, 6, 7, 90}}}),
          "camera rig rejected valid layers");
  const auto camera = rig.Resolve();
  Require(camera && std::abs(camera->yaw - 25.0F) < 0.001F &&
              std::abs(camera->distance - 6.0F) < 0.001F,
          "camera rig priority blend failed");

  const std::array flex_items{FlexItem{1, 20, 1}, FlexItem{2, 20, 3}};
  const auto flex = ResolveFlex({0, 0, 100, 20}, flex_items, FlexDirection::Row, 0);
  Require(flex.size() == 2 && std::abs(flex[0].rect.width - 35.0F) < 0.001F &&
              std::abs(flex[1].rect.width - 65.0F) < 0.001F,
          "flex layout grow resolution failed");
  const std::array grid_items{GridItem{1, 0, 0, 1, 1}, GridItem{2, 1, 0, 1, 2}};
  const auto grid = ResolveGrid({0, 0, 100, 100}, 2, 2, grid_items, 10, 10);
  Require(grid.size() == 2 && std::abs(grid[0].rect.width - 45.0F) < 0.001F &&
              std::abs(grid[1].rect.height - 100.0F) < 0.001F,
          "grid layout span resolution failed");

  LocalizationTable localization;
  localization.Set("en", "welcome", "Welcome");
  RichText rich;
  rich.Add({"welcome", true, "headline"});
  rich.Add({"!", false, "headline"});
  Require(rich.Resolve(localization) == "Welcome!", "RichText bypassed existing localization");
  Theme theme;
  Require(theme.SetColor("accent", "#ff8800"), "theme token was rejected");
  StyleSheet styles;
  Require(styles.SetRule("headline", {.opacity = 0.5F, .font_scale = 2.0F,
                                      .color_token = std::string{"accent"}}),
          "style rule was rejected");
  const std::array<std::string, 1> classes{"headline"};
  Style inline_style;
  inline_style.opacity = 0.75F;
  const auto resolved = styles.Resolve(classes, inline_style, theme);
  Require(std::abs(resolved.opacity - 0.75F) < 0.001F &&
              std::abs(resolved.font_scale - 2.0F) < 0.001F &&
              resolved.color == "#ff8800",
          "style cascade/theme resolution failed");

  AccessibilityTree semantics;
  Require(semantics.SetNodes({{1, 0, AccessibilityRole::Generic, "Root", true, false},
                              {2, 1, AccessibilityRole::Button, "Play", true, true}}),
          "valid accessibility tree was rejected");
  Require(!semantics.SetNodes({{1, 0, AccessibilityRole::Button, "", true, true}}),
          "unnamed accessible button was accepted");
  Require(semantics.ReadingOrder() == std::vector<UIElementId>({1, 2}),
          "accessibility reading order changed");

  const SurfaceVertex a{{0, 0, 0}, 0, 0};
  const SurfaceVertex b{{1, 0, 0}, 1, 0};
  const SurfaceVertex c{{0, 1, 0}, 0, 1};
  const auto hit = ProjectSurfacePointer({{0.25F, 0.25F, 1.0F}, {0, 0, -1}}, a, b, c,
                                         {100, 200, 400, 200}, 7, true);
  Require(hit && std::abs(hit->u - 0.25F) < 0.001F && std::abs(hit->v - 0.25F) < 0.001F &&
              std::abs(hit->event.x - 200.0F) < 0.001F &&
              std::abs(hit->event.y - 250.0F) < 0.001F,
          "surface world-ray to UV to UI mapping failed");

  RoomAudioGraph rooms;
  Require(rooms.AddPortal(1, 2, 0.5F) && rooms.AddPortal(2, 3, 0.5F) &&
              rooms.AddPortal(1, 3, 0.1F),
          "room portal graph rejected valid portals");
  Require(std::abs(rooms.Transmission(1, 3) - 0.25F) < 0.001F,
          "room audio did not choose the strongest portal route");

  VideoPlayer player{4};
  AdaptiveMediaStream stream;
  Require(stream.Configure(MediaProtocol::Hls,
                           {{1, 0.0, 2.0, "seg-1.ts"}, {2, 2.0, 2.0, "seg-2.ts"}}),
          "HLS segment model was rejected");
  Require(stream.SegmentFor(2.5)->sequence == 2 && stream.Seek(2.0, player),
          "adaptive stream seek failed");
  Require(stream.SubmitDecoded(player, {1, 2.0, 900}) && player.Tick(2.0)->texture == 900,
          "adaptive stream changed the V1 VideoPlayer decoded-frame contract");

#if NEXORA_MEDIA_DRM_ENABLED
  Require(true, "DRM provider boundary compiled");
#else
  static_assert(NEXORA_MEDIA_DRM_ENABLED == 0);
#endif
#if NEXORA_CAPTURE_ENCODER_ENABLED
  Require(true, "capture/encoder boundary compiled");
#else
  static_assert(NEXORA_CAPTURE_ENCODER_ENABLED == 0);
#endif
  return 0;
}
} // namespace

int main() {
  try {
    return RunTests();
  } catch (const std::exception &) {
    return 1;
  }
}
