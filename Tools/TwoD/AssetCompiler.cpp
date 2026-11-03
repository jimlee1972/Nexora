#include "Nexora/Core/Engine.h"
#include "Nexora/TwoD/Assets.h"
#include "Nexora/TwoD/AuthoredScene.h"
#include "Nexora/TwoD/EmissionAssets.h"
#include <charconv>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
namespace {
using namespace nexora::twod;
std::filesystem::path Path(std::string_view value) {
  return std::filesystem::path(std::u8string(value.begin(), value.end()));
}
Bytes Read(const std::filesystem::path &path) {
  const auto n = std::filesystem::file_size(path);
  if (n > 64 * 1024 * 1024)
    throw std::runtime_error("input exceeds 64 MiB");
  Bytes out(static_cast<std::size_t>(n));
  std::ifstream f(path, std::ios::binary);
  if (!f.read(reinterpret_cast<char *>(out.data()), std::streamsize(out.size())))
    throw std::runtime_error("input read failed");
  return out;
}
struct Reader {
  std::span<const std::byte> bytes;
  std::size_t offset{};
  std::uint64_t Number(unsigned n) {
    if (n > bytes.size() - offset)
      throw std::runtime_error("truncated source packet");
    std::uint64_t v = 0;
    for (unsigned i = 0; i < n; ++i)
      v |= std::uint64_t(std::to_integer<unsigned>(bytes[offset++])) << (8 * i);
    return v;
  }
  Bytes Pixels() {
    auto n = Number(4);
    if (n > bytes.size() - offset)
      throw std::runtime_error("invalid source pixels");
    Bytes p(bytes.begin() + std::ptrdiff_t(offset), bytes.begin() + std::ptrdiff_t(offset + n));
    offset += n;
    return p;
  }
};
void Write(const std::filesystem::path &path, const Bytes &bytes) {
  if (bytes.empty())
    throw std::runtime_error("invalid asset");
  const auto absolute = std::filesystem::absolute(path);
  nexora::core::Engine engine;
  nexora::core::EngineConfiguration config;
  config.worker_count = 1;
  config.content_root = absolute.parent_path();
  engine.Initialize(config);
  if (engine.Services().vfs->WriteAtomic("content://" + absolute.filename().generic_string(),
                                         bytes) != nexora::core::ReadResult::Status::Completed)
    throw std::runtime_error("atomic asset publication failed");
  engine.Shutdown();
}
} // namespace
int RunCompiler(int argc, char **argv) {
  try {
    if (argc != 4 && argc != 6)
      throw std::runtime_error("usage: NexoraTwoDAssetCompiler "
                               "pack|clip|sprite|cook|scene|scene-empty|scene-sprite input output");
    const auto bytes = Read(Path(argv[2]));
    Bytes out;
    if (std::string_view(argv[1]) == "emission") {
      if (argc != 6)
        throw std::runtime_error("emission requires base and mask Atlases");
      const auto links = DecodeEmissionAsset(bytes);
      const auto base = DecodeAtlas(Read(Path(argv[4]))), mask = DecodeAtlas(Read(Path(argv[5])));
      if (!links || !base || !mask || !ValidateEmissionDependencies(*links, *base, *mask))
        throw std::runtime_error("invalid emission metadata or dependencies");
      out = EncodeEmissionAsset(*links);
    } else if (argc != 4 && std::string_view(argv[1]) != "scene-sprite") {
      throw std::runtime_error("unexpected compiler arguments");
    } else if (std::string_view(argv[1]) == "scene-empty" ||
               std::string_view(argv[1]) == "scene-sprite") {
      nexora::runtime::World world;
      const auto id = world.LoadScene(Path(argv[2]).stem().string());
      nexora::twod::SpriteScene scene(world, id);
      Atlas atlas;
      nexora::twod::Tilemap tiles;
      if (std::string_view(argv[1]) == "scene-sprite") {
        auto decoded = DecodeAtlas(bytes);
        if (!decoded || decoded->regions.empty())
          throw std::runtime_error("scene-sprite needs an Atlas with a region");
        atlas = std::move(*decoded);
        Sprite sprite;
        sprite.atlas = atlas.id;
        sprite.region = atlas.regions.front().id;
        sprite.size = {float(atlas.regions.front().width), float(atlas.regions.front().height)};
        if (argc == 6) {
          auto number = [](const char *argument) {
            const std::string_view text(argument);
            float value{};
            const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
            if (result.ec != std::errc{} || result.ptr != text.data() + text.size())
              throw std::runtime_error("invalid Sprite size");
            return value;
          };
          sprite.size = {number(argv[4]), number(argv[5])};
        }
        if (!scene.Set(world.CreateEntity(id).id, sprite))
          throw std::runtime_error("invalid initial Sprite");
        tiles.atlas = atlas.id;
      }
      out = EncodeAuthoredScene(scene, atlas, {}, tiles);
    } else if (std::string_view(argv[1]) == "scene") {
      if (!nexora::twod::AuthoredScene::Decode(bytes))
        throw std::runtime_error("invalid authored 2D scene document");
      out = bytes;
    } else if (std::string_view(argv[1]) == "pack") {
      Reader r{bytes};
      if (r.Number(4) != 0x4932444e || r.Number(4) != 1)
        throw std::runtime_error("invalid source packet");
      AssetId id{r.Number(8), r.Number(8)};
      auto width = r.Number(4), height = r.Number(4), nearest = r.Number(4), count = r.Number(4);
      if (count < 1 || count > 1024 || nearest > 1 || width > 4096 || height > 4096)
        throw std::runtime_error("source budget");
      std::vector<Image> images;
      for (std::size_t i = 0; i < count; ++i) {
        Image image;
        image.id = std::uint32_t(r.Number(4));
        image.width = std::uint32_t(r.Number(4));
        image.height = std::uint32_t(r.Number(4));
        image.rgba = r.Pixels();
        images.push_back(std::move(image));
      }
      if (r.offset != bytes.size())
        throw std::runtime_error("trailing source bytes");
      auto atlas = PackAtlas(id, images, std::uint32_t(width), std::uint32_t(height), nearest != 0);
      if (!atlas)
        throw std::runtime_error("invalid images or atlas capacity");
      out = EncodeAtlas(*atlas);
    } else if (std::string_view(argv[1]) == "clip") {
      auto clip = DecodeClip(bytes);
      if (!clip)
        throw std::runtime_error("invalid animation");
      out = EncodeClip(*clip);
    } else if (std::string_view(argv[1]) == "sprite") {
      auto sprite = DecodeSpriteAsset(bytes);
      if (!sprite)
        throw std::runtime_error("invalid Sprite source metadata");
      out = EncodeSpriteAsset(*sprite);
    } else if (std::string_view(argv[1]) == "cook") {
      nexora::runtime::ImporterRegistry registry;
      if (!RegisterImporters(registry))
        throw std::runtime_error("import registration");
      AssetId id;
      std::string type;
      if (auto atlas = DecodeAtlas(bytes)) {
        id = atlas->id;
        type = "sprite_atlas";
      } else if (auto clip = DecodeClip(bytes)) {
        id = clip->id;
        type = "sprite_clip";
      } else if (auto sprite = DecodeSpriteAsset(bytes)) {
        id = sprite->id;
        type = "sprite_source";
      } else
        throw std::runtime_error("invalid source asset");
      auto canonical = registry.Import({id, type, argv[2], bytes});
      if (!canonical)
        throw std::runtime_error("asset import failed");
      nexora::runtime::DerivedDataCache cache;
      nexora::runtime::AssetCooker cooker;
#if defined(_WIN32)
      constexpr auto platform = "windows";
#elif defined(__APPLE__)
      constexpr auto platform = "macos";
#else
      constexpr auto platform = "linux";
#endif
      auto cooked = cooker.Cook(*canonical, platform, "twod-v1", cache);
      if (!cooked)
        throw std::runtime_error("cook failed");
      out = nexora::runtime::AssetCooker::Serialize(*cooked);
    } else
      throw std::runtime_error("unknown command");
    Write(Path(argv[3]), out);
    std::cout << out.size() << " bytes written\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}

#if defined(_WIN32)
int wmain(int argc, wchar_t **argv) {
  try {
    std::vector<std::string> arguments;
    arguments.reserve(static_cast<std::size_t>(argc));
    for (int i = 0; i < argc; ++i) {
      const auto utf8 = std::filesystem::path(argv[i]).u8string();
      arguments.emplace_back(utf8.begin(), utf8.end());
    }
    std::vector<char *> pointers;
    for (auto &argument : arguments)
      pointers.push_back(argument.data());
    return RunCompiler(argc, pointers.data());
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
#else
int main(int argc, char **argv) { return RunCompiler(argc, argv); }
#endif
