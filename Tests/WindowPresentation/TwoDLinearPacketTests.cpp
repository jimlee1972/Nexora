// Windows RPC headers define this token; public headers must tolerate it.
#define small char
#include "Nexora/Presentation/Surface.h"
#undef small
#include <array>
#include <cassert>
#include <limits>
using namespace Nexora::Presentation;
int main() {
  std::array<UiVertex, 4> vertices{{{{0, 0}, {0, 0}, 0xffffffff},
                                    {{320, 0}, {1, 0}, 0xffffffff},
                                    {{320, 240}, {1, 1}, 0xffffffff},
                                    {{0, 240}, {0, 1}, 0xffffffff}}};
  std::array<std::uint32_t, 6> indices{0, 1, 2, 0, 2, 3};
  UiDrawCommand command{0, 0, 320, 240, 7, 6, 0, 0};
  std::array<std::byte, 4> pixel{std::byte{128}, std::byte{128}, std::byte{128}, std::byte{255}};
  UiTextureUpload upload{7, 1, 1, 4, pixel, true, true};
  TwoDLinearDrawData data{{vertices, std::as_bytes(std::span(indices)), std::span(&command, 1),
                           std::span(&upload, 1), true},
                          {},
                          {}};
  assert(ValidateTwoDLinearDraws(data, 320, 240));
  assert(!ValidateTwoDLinearDraws(data, 0, 240));
  assert(!ValidateTwoDLinearDraws(data, 16385, 240));
  data.viewport = {2, 3, 310, 230};
  assert(ValidateTwoDLinearDraws(data, 320, 240));
  data.viewport = {310, 0, 20, 240};
  assert(!ValidateTwoDLinearDraws(data, 320, 240));
  data.viewport = {0, 0, 320, 0};
  assert(!ValidateTwoDLinearDraws(data, 320, 240));
  data.viewport = {};
  indices[0] = 4;
  assert(!ValidateTwoDLinearDraws(data, 320, 240));
  indices[0] = 0;
  command.clipX = -1;
  assert(!ValidateTwoDLinearDraws(data, 320, 240));
  command.clipX = 0;
  command.clipWidth = 321;
  assert(!ValidateTwoDLinearDraws(data, 320, 240));
  command.clipWidth = 320;
  vertices[0].uv[0] = std::numeric_limits<float>::quiet_NaN();
  assert(!ValidateTwoDLinearDraws(data, 320, 240));
  vertices[0].uv[0] = 0;
  auto copy = pixel;
  UiTextureUpload second = upload;
  second.pixels = copy;
  data.overlay.textureUploads = std::span(&second, 1);
  assert(ValidateTwoDLinearDraws(data, 320, 240)); // equal bytes, different owners
  copy[0] = std::byte{129};
  assert(!ValidateTwoDLinearDraws(data, 320, 240));
  data.overlay = {};
  upload.rowPitch = 8;
  assert(!ValidateTwoDLinearDraws(data, 320, 240));
  upload.rowPitch = 4;
  upload.textureId = UINT64_MAX - 1;
  assert(!ValidateTwoDLinearDraws(data, 320, 240));
  upload.textureId = 7;
  SpriteGpuInstance instance{};
  instance.axisX[0] = 320;
  instance.axisY[1] = 240;
  data.world.spriteInstances = std::span(&instance, 1);
  command.instanceCount = 1;
  command.firstInstance = 0;
  assert(ValidateTwoDLinearDraws(data, 320, 240));
  command.firstInstance = 1;
  assert(!ValidateTwoDLinearDraws(data, 320, 240));
}
