#pragma once
#include "Nexora/Presentation/Surface.h"
#include <array>
#include <string_view>

// Native compositor acceptance only. Source UUID admission and Editor integration belong to the
// parent workflow; this fixture supplies projected textured quads and a separate 3D scene view.
struct SceneUiFixture final {
  using Surface = Nexora::Presentation::ISurface;
  using Status = Nexora::Presentation::SurfaceStatus;
  std::string_view failure;
  std::optional<Nexora::Presentation::SceneTextureReference> previous;
  bool Draw(Surface &surface, std::uint32_t width, std::uint32_t height) {
    using namespace Nexora::Presentation;
    if (surface.Acquire() != Status::Ready || surface.SceneTexture())
      return Fail("sampled Scene acquire/reference phase failed");
    std::array<SceneVertex, 3> sceneVertices{{{{-.6F, -.8F, .5F}, {0, 0, 1}},
                                              {{1, -.8F, .5F}, {0, 0, 1}},
                                              {{.2F, .8F, .5F}, {0, 0, 1}}}};
    const std::array<std::uint16_t, 3> sceneIndices{0, 1, 2};
    SceneDrawData scene{sceneVertices, sceneIndices};
    scene.light_direction[0] = scene.light_direction[1] = 0;
    scene.light_direction[2] = -1;
    scene.base_color[0] = scene.base_color[2] = 0;
    scene.base_color[1] = 1;
    scene.sampledByUi = true;
    if (surface.DrawScene(scene) != Status::InvalidDescriptor)
      return Fail("direct Scene accepted sampled opt-in");
    scene.offscreen = true;
    if (surface.DrawScene(scene) != Status::Ready)
      return Fail("offscreen Scene preparation failed");
    const auto reference = surface.SceneTexture();
    if (!reference || (previous && *previous == *reference))
      return Fail("prepared Scene reference absent or reused across frames");
    const auto w = static_cast<float>(width), h = static_cast<float>(height);
    std::array<UiVertex, 16> vertices{};
    const auto quad = [&](std::size_t n, float x, float y, float right, float bottom,
                          std::uint32_t color) {
      vertices[n] = {{x, y}, {0, 0}, color};
      vertices[n + 1] = {{right, y}, {1, 0}, color};
      vertices[n + 2] = {{right, bottom}, {1, 1}, color};
      vertices[n + 3] = {{x, bottom}, {0, 1}, color};
    };
    quad(0, 0, 0, w, h, 0xffff0000U);
    quad(4, 0, 0, w / 2, h, UINT32_MAX);
    quad(8, w / 16, h * 3 / 8, w * 3 / 16, h * 5 / 8, 0xff0000ffU);
    quad(12, w / 4 - 8, h / 8 - 8, w / 4 + 8, h / 8 + 8, 0xff00ffffU);
    const std::array<std::uint16_t, 24> indices{0, 1, 2,  0, 2,  3,  4,  5,  6,  4,  6,  7,
                                                8, 9, 10, 8, 10, 11, 12, 13, 14, 12, 14, 15};
    constexpr std::uint64_t whiteId = 15001;
    std::array<UiDrawCommand, 4> commands{{{0, 0, width, height, whiteId, 6, 0, 0},
                                           {0, 0, width / 2, height, 0, 6, 6, 0},
                                           {0, 0, width, height, whiteId, 6, 12, 0},
                                           {0, 0, width, height, whiteId, 6, 18, 0}}};
    commands[1].sceneTexture = *reference;
    const std::array<std::byte, 4> white{std::byte{255}, std::byte{255}, std::byte{255},
                                         std::byte{255}};
    const std::array uploads{UiTextureUpload{whiteId, 1, 1, 4, white, true, true}};
    const UiDrawData packet{vertices, std::as_bytes(std::span(indices)), commands, uploads};
    const auto before = surface.Diagnostics();
    auto bad = *reference;
    bad.surface ^= UINT64_C(1) << 62;
    commands[1].sceneTexture = bad;
    if (surface.RenderUi(packet) != Status::InvalidDescriptor)
      return Fail("foreign Scene reference was accepted");
    if (previous) {
      commands[1].sceneTexture = *previous;
      if (surface.RenderUi(packet) != Status::InvalidDescriptor)
        return Fail("stale Scene reference was accepted");
    }
    if (surface.Diagnostics().nativeUiTextureUploads != before.nativeUiTextureUploads ||
        surface.Diagnostics().nativeUiDrawCalls != before.nativeUiDrawCalls)
      return Fail("invalid reference changed native UI resources or recorded draws");
    commands[1].sceneTexture = *reference;
    if (surface.RenderUi(packet) != Status::Ready || surface.SceneTexture() ||
        surface.RenderUi(packet) != Status::InvalidDescriptor)
      return Fail("Scene/UI composition or single-submission admission failed");
    previous = reference;
    // Reuse caller storage and change material constants after the sampled draw. The first view
    // and the one UI pass must keep their own fenced resources through this second scene draw.
    scene.sampledByUi = scene.offscreen = false;
    scene.base_color[0] = 1;
    scene.base_color[1] = 0;
    scene.viewport = {width / 2, height / 4, width / 2, height / 2};
    auto overlap = scene;
    overlap.viewport.x = width / 2 - 1;
    if (surface.DrawScene(overlap) != Status::InvalidDescriptor ||
        surface.DrawScene(scene) != Status::Ready || surface.Present() != Status::Ready)
      return Fail("disjoint native Scene after sampled UI failed");
    return true;
  }
  static bool Pixels(std::array<unsigned, 3> scene, std::array<unsigned, 3> sprite,
                     std::array<unsigned, 3> overlay, std::array<unsigned, 3> direct,
                     std::array<unsigned, 3> background) {
    return scene[1] > 180 && scene[0] < 40 && scene[2] < 40 && sprite[0] > 240 && sprite[1] < 20 &&
           sprite[2] < 20 && overlay[0] > 240 && overlay[1] > 240 && overlay[2] < 20 &&
           direct[0] > 180 && direct[1] < 40 && direct[2] < 40 && background[2] > 240 &&
           background[0] < 20 && background[1] < 20;
  }

  static bool Orientation(const std::array<std::array<unsigned, 3>, 4> &image,
                          const std::array<std::array<unsigned, 3>, 4> &direct, bool srgb) {
    const std::array<float, 3> clear{.025F, .045F, .09F};
    unsigned covered{};
    for (std::size_t i = 0; i < image.size(); ++i) {
      const bool green = image[i][1] > 180 && image[i][0] < 40 && image[i][2] < 40;
      const bool red = direct[i][0] > 180 && direct[i][1] < 40 && direct[i][2] < 40;
      if (green != red)
        return false;
      covered += green ? 1U : 0U;
      if (!green)
        for (std::size_t channel = 0; channel < clear.size(); ++channel) {
          const auto linear = clear[channel];
          const auto transferred = srgb ? 1.055F * std::pow(linear, 1.0F / 2.4F) - .055F : linear;
          const auto expected = std::lround(transferred * 255);
          if (std::abs(static_cast<long>(image[i][channel]) - expected) > 2)
            return false;
        }
    }
    return covered == 3;
  }

private:
  bool Fail(std::string_view message) {
    failure = message;
    return false;
  }
};
