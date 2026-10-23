#include "Nexora/Presentation/Surface.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>
namespace Nexora::Presentation {
std::optional<PreparedSpriteDraws>
PrepareSpriteDraws(const UiDrawData &sprites, const UiDrawData &overlay, std::uint32_t width,
                   std::uint32_t height, bool instanceQuads, std::uint32_t background) {
  if (!width || !height || width > 16384 || height > 16384)
    return std::nullopt;
  PreparedSpriteDraws result;
  constexpr std::uint64_t whiteId = UINT64_MAX - 1;
  static constexpr std::byte white[]{std::byte{255}, std::byte{255}, std::byte{255},
                                     std::byte{255}};
  result.vertices = {{{0, 0}, {0, 0}, background},
                     {{float(width), 0}, {1, 0}, background},
                     {{float(width), float(height)}, {1, 1}, background},
                     {{0, float(height)}, {0, 1}, background}};
  result.indices = {0, 1, 2, 0, 2, 3};
  result.commands.push_back({0, 0, width, height, whiteId, 6, 0, 0});
  result.textureUploads.push_back({whiteId, 1, 1, 4, white, true, true});
  std::map<std::uint64_t, UiTextureUpload> textures;
  for (const auto *packet : {&sprites, &overlay}) {
    if (!packet->spriteInstances.empty() || !ValidateSpriteInstances(*packet))
      return std::nullopt;
    for (const auto &command : packet->commands)
      if (command.instanceCount)
        return std::nullopt;
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
    if (instanceQuads && packet == &sprites) {
      for (auto command : packet->commands) {
        if (command.elementCount % 6)
          return std::nullopt;
        command.firstInstance = static_cast<std::uint32_t>(result.spriteInstances.size());
        const auto end = command.indexOffset + command.elementCount;
        for (std::size_t i = command.indexOffset; i < end; i += 6) {
          if (result.spriteInstances.size() >= 100000)
            return std::nullopt;
          const auto at = [&](unsigned n) -> const UiVertex & {
            return packet
                ->vertices[std::size_t(std::int64_t(indices[i + n]) + command.vertexOffset)];
          };
          const auto &a = at(0), &b = at(1), &c = at(2), &d = at(5);
          if (indices[i] != indices[i + 3] || indices[i + 2] != indices[i + 4] ||
              a.color != b.color || a.color != c.color || a.color != d.color ||
              a.uv[0] != d.uv[0] || b.uv[0] != c.uv[0] || a.uv[1] != b.uv[1] || c.uv[1] != d.uv[1])
            return std::nullopt;
          SpriteGpuInstance instance;
          for (unsigned axis = 0; axis < 2; ++axis) {
            instance.origin[axis] = a.position[axis];
            instance.axisX[axis] = b.position[axis] - a.position[axis];
            instance.axisY[axis] = d.position[axis] - a.position[axis];
            const auto expected = double(b.position[axis]) + d.position[axis] - a.position[axis];
            if (std::abs(expected - c.position[axis]) > 1e-4 * std::max(1.0, std::abs(expected)))
              return std::nullopt;
          }
          instance.uv[0] = a.uv[0];
          instance.uv[1] = a.uv[1];
          instance.uv[2] = c.uv[0];
          instance.uv[3] = c.uv[1];
          instance.color = a.color;
          result.spriteInstances.push_back(instance);
        }
        command.instanceCount =
            static_cast<std::uint32_t>(result.spriteInstances.size()) - command.firstInstance;
        command.elementCount = 6;
        command.indexOffset = 0;
        command.vertexOffset = 0;
        result.commands.push_back(command);
      }
      continue;
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
  if (!ValidateSpriteInstances(result.View()))
    return std::nullopt;
  return result;
}
} // namespace Nexora::Presentation
