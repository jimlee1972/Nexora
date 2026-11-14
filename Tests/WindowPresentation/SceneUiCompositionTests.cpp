#include "SceneUiComposition.h"
#include "SceneViewAdmission.h"
#include <array>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
using namespace Nexora::Presentation;
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
void References() {
  detail::SceneUiComposition owner, foreign;
  Require(!owner.Reference() && !owner.Publish(0), "unacquired view published");
  owner.Begin(7);
  foreign.Begin(7);
  Require(owner.Publish(0) && foreign.Publish(0) && !owner.Publish(0),
          "duplicate scene publication admitted");
  const auto reference = *owner.Reference();
  Require(reference != *foreign.Reference(), "surface domains alias");
  std::array<UiVertex, 4> vertices{{{{0, 0}, {0, 0}, UINT32_MAX},
                                    {{320, 0}, {1, 0}, UINT32_MAX},
                                    {{320, 240}, {1, 1}, UINT32_MAX},
                                    {{0, 240}, {0, 1}, UINT32_MAX}}};
  const std::array<std::uint32_t, 6> indices{0, 1, 2, 0, 2, 3};
  std::array<UiDrawCommand, 1> commands{{{0, 0, 160, 240, 21, 6, 0, 0}}};
  UiDrawData packet{vertices, std::as_bytes(std::span(indices)), commands, {}, true};
  Require(owner.Validate(packet, 320, 240).valid && !owner.Validate(packet, 320, 240).sampled,
          "ordinary UI path changed");
  commands[0].textureId = 0;
  commands[0].sceneTexture = reference;
  const auto plan = owner.Validate(packet, 320, 240);
  Require(plan.valid && plan.sampled && plan.bounds.x == 0 && plan.bounds.width == 160 &&
              plan.bounds.height == 240,
          "scene image clip not admitted");
  const auto original = commands[0];
  for (unsigned error = 0; error < 12; ++error) {
    auto &command = commands[0];
    command = original;
    switch (error) {
    case 0:
      command.sceneTexture = *foreign.Reference();
      break;
    case 1:
      --command.sceneTexture.frame;
      break;
    case 2:
      ++command.sceneTexture.view;
      break;
    case 3:
      command.sceneTexture.surface = 0;
      break;
    case 4:
      command.textureId = 21;
      break;
    case 5:
      command.instanceCount = 1;
      break;
    case 6:
      command.clipX = -1;
      break;
    case 7:
      command.clipWidth = UINT32_MAX;
      break;
    case 8:
      command.clipHeight = 0;
      break;
    case 9:
      command.indexOffset = UINT32_MAX;
      break;
    case 10:
      command.elementCount = UINT32_MAX;
      break;
    case 11:
      command.vertexOffset = INT32_MIN;
      break;
    }
    Require(!owner.Validate(packet, 320, 240).valid, "malformed/foreign scene reference admitted");
  }
  commands[0] = original;
  vertices[0].uv[0] = std::numeric_limits<float>::quiet_NaN();
  Require(!owner.Validate(packet, 320, 240).valid, "nonfinite scene geometry admitted");
  vertices[0].uv[0] = 0;
  const std::array<std::uint16_t, 6> small{0, 1, 2, 0, 2, 3};
  packet.indices = std::as_bytes(std::span(small));
  packet.indices32Bit = false;
  Require(owner.Validate(packet, 320, 240).valid, "16-bit scene indices rejected");
  std::array pair{original, original};
  pair[1].clipX = 200;
  pair[1].clipWidth = 120;
  packet.commands = pair;
  const auto multiple = owner.Validate(packet, 320, 240);
  Require(multiple.valid && multiple.sampled && multiple.bounds.width == 320,
          "multiple UI clips did not reserve their union");
  owner.Complete();
  Require(owner.Sampled() && !owner.Validate(packet, 320, 240).valid,
          "reference remained callable after UI completion");
  owner.Begin(8);
  Require(!owner.Sampled() && !owner.Reference() && !owner.Validate(packet, 320, 240).valid,
          "stale frame reference survived acquisition");
  Require(owner.Publish(0) && owner.Reference()->frame == 8 &&
              !owner.Validate(packet, 320, 240).valid,
          "new frame revived old references");
  pair[0].sceneTexture = pair[1].sceneTexture = *owner.Reference();
  Require(owner.Validate(packet, 320, 240).valid, "valid new frame could not recover");
  // Legacy Sprite and linear paths do not silently interpret typed Scene references as IDs.
  pair[0].textureId = pair[1].textureId = 21;
  const auto references = pair;
  pair[0].sceneTexture = pair[1].sceneTexture = {};
  TwoDLinearDrawData linear{};
  linear.world = packet;
  Require(PrepareSpriteDraws(packet, {}, 320, 240, false).has_value() &&
              ValidateTwoDLinearDraws(linear, 320, 240),
          "ordinary Sprite/linear baseline is invalid");
  pair = references;
  Require(!PrepareSpriteDraws(packet, {}, 320, 240, false) &&
              !ValidateTwoDLinearDraws(linear, 320, 240),
          "Scene reference entered an unsupported Sprite/linear path");
}
void DualReferences() {
  detail::SceneUiComposition owner;
  owner.Begin(19);
  Require(owner.Publish(0), "first dual reference rejected");
  const auto first = *owner.Reference();
  Require(owner.Publish(1), "second dual reference rejected");
  const auto second = *owner.Reference();
  Require(first != second && first.surface == second.surface && first.frame == second.frame &&
              !owner.Publish(0) && !owner.Publish(1) && !owner.Publish(2),
          "duplicate/out-of-budget dual publication admitted");
  const std::array<UiVertex, 3> vertices{
      {{{0, 0}, {0, 0}, UINT32_MAX}, {{20, 0}, {1, 0}, UINT32_MAX}, {{0, 20}, {0, 1}, UINT32_MAX}}};
  const std::array<std::uint16_t, 3> indices{0, 1, 2};
  std::array<UiDrawCommand, 2> commands{{{0, 0, 20, 20, 0, 3}, {20, 0, 20, 20, 0, 3}}};
  commands[0].sceneTexture = first;
  commands[1].sceneTexture = second;
  UiDrawData packet{vertices, std::as_bytes(std::span(indices)), commands, {}};
  auto plan = owner.Validate(packet, 40, 20);
  Require(plan.valid && plan.sampled && plan.views[0] && plan.views[1] && plan.bounds.width == 40,
          "two owning images did not produce two explicit sampled views");
  commands[1].sceneTexture.view = 2;
  Require(!owner.Validate(packet, 40, 20).valid, "unpublished third view admitted");
  commands[1].sceneTexture = second;
  owner.Complete();
  Require(!owner.Validate(packet, 40, 20).valid, "dual submission was replayed");
  owner.Begin(20);
  Require(owner.Publish(0) && owner.Publish(1) && !owner.Validate(packet, 40, 20).valid,
          "dual references survived a frame boundary");
  detail::SceneViewAdmission views;
  SceneDrawData sample;
  sample.sampledByUi = sample.offscreen = true;
  Require(views.Commit(sample, 40, 20, false), "first sampled view rejected");
  auto shadowed = sample;
  shadowed.shadow = SceneDirectionalShadow{};
  Require(!views.CanSubmit(shadowed, 40, 20, false) && !views.CanSubmit(sample, 40, 20, true) &&
              views.Commit(sample, 40, 20, false) && views.Count() == 2 &&
              !views.Commit(sample, 40, 20, false),
          "dual sampled admission lost shadow/phase/two-view bounds");
}
void Views() {
  detail::SceneViewAdmission views;
  SceneDrawData offscreen, right;
  offscreen.offscreen = offscreen.sampledByUi = true;
  right.viewport = {160, 0, 160, 240};
  Require(views.Commit(offscreen, 320, 240, false), "sampled offscreen view rejected");
  Require(!views.CanSubmit(right, 320, 240, true), "uncomposited view released admission");
  views.SampledOffscreen({0, 0, 160, 240});
  Require(views.CanSubmit(right, 320, 240, true), "disjoint direct view blocked after sampling");
  auto overlap = right;
  overlap.viewport.x = 159;
  Require(!views.Commit(overlap, 320, 240, true) && views.Commit(right, 320, 240, true) &&
              !views.Commit(right, 320, 240, true),
          "overlap or third view changed sampled admission");
  views.Reset();
  offscreen.sampledByUi = false;
  Require(views.Commit(offscreen, 320, 240, false), "legacy offscreen view rejected");
  views.SampledOffscreen({0, 0, 160, 240});
  Require(!views.CanSubmit(right, 320, 240, true), "legacy exclusive scene behavior changed");
  views.Reset();
  offscreen.sampledByUi = true;
  offscreen.hdr = true;
  Require(!views.Commit(offscreen, 320, 240, false), "HDR sampled view silently admitted");
  offscreen.hdr = false;
  offscreen.offscreen = false;
  Require(!views.Commit(offscreen, 320, 240, false), "direct sampled view silently admitted");
}
} // namespace
int main() {
  try {
    References();
    DualReferences();
    Views();
    std::cout << "PASS fenced Scene/UI references and view admission\n";
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
