#pragma once

#include "Nexora/Presentation/Surface.h"
#include <array>
#include <string_view>

// The same CPU arrays are rewritten after the first draw: adapters must copy every borrowed
// geometry/instance/material byte. Modes isolate instance, geometry and palette differences.
struct SceneViewPairFixture final {
  using Surface = Nexora::Presentation::ISurface;
  using Status = Nexora::Presentation::SurfaceStatus;
  std::string_view failure;
  bool Draw(Surface &surface, unsigned mode, std::uint32_t width, std::uint32_t height,
            bool retained) {
    using namespace Nexora::Presentation;
    if (!surface.SupportsBoundedSceneViews())
      return Fail("bounded scene view capability unavailable");
    const auto w = static_cast<float>(width), h = static_cast<float>(height);
    const std::array<UiVertex, 4> uiVertices{{{{0, 0}, {0, 0}, 0xffff0000U},
                                              {{w, 0}, {0, 0}, 0xffff0000U},
                                              {{w, h}, {0, 0}, 0xffff0000U},
                                              {{0, h}, {0, 0}, 0xffff0000U}}};
    const std::array<std::uint16_t, 6> uiIndices{0, 1, 2, 0, 2, 3};
    const std::array commands{UiDrawCommand{0, 0, width, height, 901, 6, 0, 0}};
    const std::array<std::byte, 4> white{std::byte{255}, std::byte{255}, std::byte{255},
                                         std::byte{255}};
    const std::array uploads{UiTextureUpload{901, 1, 1, 4, white}};
    const UiDrawData ui{uiVertices, std::as_bytes(std::span(uiIndices)), commands, uploads};
    if (surface.Acquire() != Status::Ready || surface.RenderUi(ui) != Status::Ready)
      return Fail("concurrent PBR acquire/UI failed");
    std::array<SceneVertex, 3> vertices{};
    const std::array<std::uint16_t, 3> indices{0, 1, 2};
    std::array<SceneInstance, 1> instances{};
    std::array<SceneMaterial, 64> materials{};
    const std::array batches{SceneMeshBatch{0, 3, 0, 1, 63}};
    SceneDrawData draw{vertices, indices};
    draw.instances = instances;
    draw.materials = materials;
    draw.batches = batches;
    draw.pbr = true;
    draw.cameraPosition = {0, 0, 3};
    draw.retainMeshGeometry = retained && surface.SupportsSceneGeometryResidency();
    const auto prepare = [&](bool right) {
      vertices = {{{{-0.8F, -0.8F, 0.5F}, {0, 0, 1}},
                   {{0.8F, -0.8F, 0.5F}, {0, 0, 1}},
                   {{0, 0.8F, 0.5F}, {0, 0, 1}}}};
      if (right && mode == 1)
        for (auto &vertex : vertices)
          vertex.position[0] -= 0.68F;
      instances[0] = {};
      if (right && mode == 0)
        instances[0].translation[0] = -0.68F;
      materials[63].baseColor = {0, 0, 0, 1};
      materials[63].emission = right ? std::array{0.0F, 1.0F, 0.0F} : std::array{1.0F, 0.0F, 0.0F};
      materials[63].unlit = true;
      draw.viewport = {right ? width / 2 : 0, height / 4, width / 2, height / 2};
    };
    prepare(false);
    if (surface.DrawScene(draw) != Status::Ready)
      return Fail("first concurrent PBR draw failed");
    if (surface.DrawScene(draw) != Status::InvalidDescriptor)
      return Fail("overlapping PBR view was admitted");
    prepare(true);
    const std::array<std::uint16_t, 3> invalidIndices{0, 1, UINT16_MAX};
    auto invalid = draw;
    invalid.indices = invalidIndices;
    if (surface.DrawScene(invalid) != Status::InvalidDescriptor)
      return Fail("malformed second view consumed admission");
    if (surface.DrawScene(draw) != Status::Ready)
      return Fail("second concurrent PBR draw failed");
    if (surface.DrawScene(draw) != Status::InvalidDescriptor || surface.Present() != Status::Ready)
      return Fail("third PBR view or pair presentation failed");
    return true;
  }
  template <class Rgb> static bool Pixels(const Rgb &left, const Rgb &right, const Rgb &outside) {
    return left[0] > 180 && left[1] < 80 && right[1] > 180 && right[0] < 80 && outside[2] > 220 &&
           outside[0] < 10 && outside[1] < 10;
  }

private:
  bool Fail(std::string_view message) {
    failure = message;
    return false;
  }
};
