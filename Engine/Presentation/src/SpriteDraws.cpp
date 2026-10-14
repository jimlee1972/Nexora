#include "Nexora/Presentation/Surface.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>
namespace Nexora::Presentation {
std::optional<PreparedSpriteDraws> PrepareSpriteDraws(const UiDrawData &sprites,
                                                      const UiDrawData &overlay,
                                                      std::uint32_t width, std::uint32_t height) {
  if (!width || !height || width > 16384 || height > 16384)
    return std::nullopt;
  PreparedSpriteDraws result;
  constexpr std::uint64_t whiteId = UINT64_MAX - 1;
  static constexpr std::byte white[]{std::byte{255}, std::byte{255}, std::byte{255},
                                     std::byte{255}};
  result.vertices = {{{0, 0}, {0, 0}, 0xFF29140A},
                     {{float(width), 0}, {1, 0}, 0xFF29140A},
                     {{float(width), float(height)}, {1, 1}, 0xFF29140A},
                     {{0, float(height)}, {0, 1}, 0xFF29140A}};
  result.indices = {0, 1, 2, 0, 2, 3};
  result.commands.push_back({0, 0, width, height, whiteId, 6, 0, 0});
  result.textureUploads.push_back({whiteId, 1, 1, 4, white, true, true});
  std::map<std::uint64_t, UiTextureUpload> textures;
  for (const auto *packet : {&sprites, &overlay}) {
    const auto stride = packet->indices32Bit ? 4U : 2U;
    if (packet->indices.size() % stride ||
        packet->vertices.size() > 1000000 - result.vertices.size() ||
        packet->indices.size() / stride > 2000000 - result.indices.size() ||
        packet->commands.size() > 100000 - result.commands.size() ||
        packet->textureUploads.size() > 128)
      return std::nullopt;
    for (const auto &v : packet->vertices)
      for (const float f : {v.position[0], v.position[1], v.uv[0], v.uv[1]})
        if (!std::isfinite(f) || std::abs(f) > 1e12F)
          return std::nullopt;
    std::vector<std::uint32_t> indices(packet->indices.size() / stride);
    for (std::size_t i = 0; i < indices.size(); ++i) {
      if (stride == 4)
        std::memcpy(&indices[i], packet->indices.data() + i * stride, 4);
      else {
        std::uint16_t value = 0;
        std::memcpy(&value, packet->indices.data() + i * stride, 2);
        indices[i] = value;
      }
    }
    for (const auto &command : packet->commands) {
      if (!command.textureId || command.textureId == whiteId || !command.elementCount ||
          command.elementCount % 3 || command.indexOffset > indices.size() ||
          command.elementCount > indices.size() - command.indexOffset || command.clipX < 0 ||
          command.clipY < 0 || std::uint32_t(command.clipX) > width ||
          command.clipWidth > width - std::uint32_t(command.clipX) ||
          std::uint32_t(command.clipY) > height ||
          command.clipHeight > height - std::uint32_t(command.clipY))
        return std::nullopt;
      for (std::size_t i = command.indexOffset; i < command.indexOffset + command.elementCount;
           ++i) {
        const auto vertex = std::int64_t(indices[i]) + command.vertexOffset;
        if (vertex < 0 || std::uint64_t(vertex) >= packet->vertices.size())
          return std::nullopt;
      }
    }
    for (const auto &upload : packet->textureUploads) {
      if (!upload.textureId || upload.textureId == whiteId || !upload.width || !upload.height ||
          upload.width > 4096 || upload.height > 4096 || upload.rowPitch != upload.width * 4 ||
          upload.pixels.size() != std::size_t(upload.rowPitch) * upload.height ||
          upload.pixels.size() > 16 * 1024 * 1024)
        return std::nullopt;
      const auto [it, inserted] = textures.emplace(upload.textureId, upload);
      if (!inserted &&
          (it->second.width != upload.width || it->second.height != upload.height ||
           it->second.nearest != upload.nearest || it->second.immutable != upload.immutable ||
           !std::equal(it->second.pixels.begin(), it->second.pixels.end(), upload.pixels.begin(),
                       upload.pixels.end())))
        return std::nullopt;
    }
    const auto vertexBase = static_cast<std::int32_t>(result.vertices.size());
    const auto indexBase = static_cast<std::uint32_t>(result.indices.size());
    result.vertices.insert(result.vertices.end(), packet->vertices.begin(), packet->vertices.end());
    result.indices.insert(result.indices.end(), indices.begin(), indices.end());
    for (auto command : packet->commands) {
      command.indexOffset += indexBase;
      const auto offset = std::int64_t(command.vertexOffset) + vertexBase;
      if (offset < INT32_MIN || offset > INT32_MAX)
        return std::nullopt;
      command.vertexOffset = static_cast<std::int32_t>(offset);
      result.commands.push_back(command);
    }
  }
  if (textures.size() > 128)
    return std::nullopt;
  for (const auto &[id, texture] : textures) {
    (void)id;
    result.textureUploads.push_back(texture);
  }
  return result;
}
} // namespace Nexora::Presentation
