#include "Nexora/Editor/TwoDPlayContext.h"
#include "Nexora/Runtime/GameplayModuleHost.h"
#include <algorithm>
#include <array>
#include <iostream>
#include <stdexcept>
#include <thread>
#if defined(NEXORA_PLAY_CONTEXT_NATIVE)
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#undef None
#include <chrono>
#include <fstream>
#endif

namespace {
using namespace nexora;
constexpr twod::AssetId atlasId{0x123456789abcdef0ULL, 0xfedcba9876543210ULL};
constexpr twod::AssetId clipId{0xabcdef0012345678ULL, 0x8765432100fedcbaULL};
runtime::Id entity{};
NexoraTwoDApiV1 api{};
NexoraTwoDInstanceHandle handle{};
bool stopped{}, destroyed{}, failStart{}, failCreate{};
void Check(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
twod::Atlas Atlas() {
  std::array<twod::Image, 2> images{{{1, 2, 2, {}}, {2, 2, 2, {}}}};
  for (unsigned i = 0; i < 4; ++i) {
    for (unsigned c : {255U, 0U, 0U, 255U})
      images[0].rgba.push_back(std::byte(c));
    for (unsigned c : {0U, 255U, 0U, 255U})
      images[1].rgba.push_back(std::byte(c));
  }
  auto atlas = twod::PackAtlas(atlasId, images, 16, 16, true);
  Check(bool(atlas), "atlas fixture");
  return std::move(*atlas);
}
NexoraTwoDSprite Sprite() {
  return {atlasId.high, atlasId.low, 1, 8, {2, 2}, {0.5F, 0.5F}, {1, 1, 1, 1}, 1, 0};
}
int32_t Create(void **state, const NexoraGameplayHostV3 *host) {
  stopped = destroyed = false;
  api = {};
  api.struct_size = sizeof(api);
  if (!(host->capabilities & NEXORA_GAMEPLAY_CAPABILITY_2D_API) || !host->get_twod_api ||
      host->get_twod_api(host->context, &api) != NEXORA_GAMEPLAY_OK)
    return NEXORA_GAMEPLAY_ERROR_LIFECYCLE;
  *state = &api;
  const auto sprite = Sprite();
  const auto result = api.set_sprite(api.context, entity, &sprite);
  return failCreate ? NEXORA_GAMEPLAY_ERROR_LIFECYCLE : result;
}
int32_t Start(void *) {
  const NexoraTwoDCamera camera{{0, 0}, 10, 0};
  const NexoraTwoDTile tile{3, 0, 0, 1, 1};
  const NexoraTwoDInstance instance{{{-3, 0, 0}, {0, 0, 0, 1}, {1, 1, 1}}, Sprite()};
  if (api.set_camera(api.context, &camera) || api.paint_tiles(api.context, &tile, 1) ||
      api.create_instances(api.context, &instance, 1, &handle) ||
      api.play_animation(api.context, entity, clipId.high, clipId.low, 1))
    return NEXORA_GAMEPLAY_ERROR_LIFECYCLE;
  return failStart ? NEXORA_GAMEPLAY_ERROR_LIFECYCLE : NEXORA_GAMEPLAY_OK;
}
int32_t Update(void *, double delta) {
  std::array<NexoraTwoDEvent, 2> events{};
  NexoraTwoDAnimationResult result{};
  const auto status =
      api.advance_animation(api.context, entity, delta, events.data(), events.size(), &result);
  if (status || result.region != 2 || result.event_count != 1 || events[0].length != 5 ||
      std::string_view(events[0].name, events[0].length) != "green")
    return NEXORA_GAMEPLAY_ERROR_LIFECYCLE;
  return NEXORA_GAMEPLAY_OK;
}
void Stop(void *) {
  NexoraTwoDCamera camera{};
  stopped =
      api.get_camera(api.context, &camera) == NEXORA_GAMEPLAY_OK && camera.vertical_size == 10;
}
void Destroy(void *) {
  NexoraTwoDInstance value{};
  destroyed = api.read_instances(api.context, &handle, 1, &value) == NEXORA_GAMEPLAY_OK;
}
int32_t Loader(std::uint32_t version, NexoraGameModuleV3 *module) {
  *module = {};
  module->struct_size = sizeof(*module);
  module->abi_version = version;
  module->create = Create;
  module->on_start = Start;
  module->update = Update;
  module->on_stop = Stop;
  module->destroy = Destroy;
  return NEXORA_GAMEPLAY_OK;
}
NexoraGameplayHostV3 Host(editor::TwoDPlayContext &context) {
  NexoraGameplayHostV3 host{};
  host.struct_size = sizeof(host);
  host.abi_version = NEXORA_GAMEPLAY_ABI_VERSION;
  host.capabilities = NEXORA_GAMEPLAY_CAPABILITY_2D_API;
  host.context = &context;
  host.get_twod_api = [](void *opaque, NexoraTwoDApiV1 *out) {
    return static_cast<editor::TwoDPlayContext *>(opaque)->GetApi(out);
  };
  return host;
}
void SceneOrder() {
  runtime::World source;
  const auto sceneId = source.LoadSceneSnapshot("NEXORA_SCENE 1 \"Stored high ID first\" 0 2\n"
                                                "20 0 0 0 0 0 0 60 0.1 1000 1 0 0\n"
                                                "10 0 0 0 0 0 0 60 0.1 1000 1 0 0\n");
  Check(bool(sceneId), "non-ID scene storage fixture");
  twod::SpriteScene sprites(source, *sceneId);
  twod::Sprite sprite;
  sprite.atlas = atlasId;
  sprite.region = 1;
  sprite.size = {2, 2};
  Check(sprites.Set(10, sprite), "lower-ID opaque Sprite");
  sprite.color[3] = 0.5F;
  Check(sprites.Set(20, sprite), "higher-ID translucent Sprite");
  const auto scene = sprites.Snapshot();
  Check(bool(scene), "non-ID scene snapshot");
  auto atlas = std::make_shared<const twod::Atlas>(Atlas());
  twod::Tilemap tiles;
  tiles.atlas = atlasId;
  twod::Bytes documentBytes;
  const auto number = [&](std::uint32_t value) {
    for (unsigned i = 0; i < 4; ++i)
      documentBytes.push_back(std::byte((value >> (8 * i)) & 255));
  };
  const auto blob = [&](std::span<const std::byte> bytes) {
    number(static_cast<std::uint32_t>(bytes.size()));
    documentBytes.insert(documentBytes.end(), bytes.begin(), bytes.end());
  };
  number(0x4432444e);
  number(1);
  blob(*scene);
  blob(twod::EncodeAtlas(*atlas));
  blob({});
  blob(tiles.Encode());
  editor::TwoDDocument document;
  Check(document.Reopen(documentBytes), "non-ID document reopen");
  const auto poses = document.World().SceneWorldPoses(document.Scene().SceneId());
  Check(poses && poses->size() == 2 && (*poses)[0].id == 20 && (*poses)[1].id == 10,
        "fixture did not retain non-ID storage order");
  auto context = editor::TwoDPlayContext::Create(document, 1);
  Check(bool(context), "non-ID Play context");
  const twod::Camera camera{{0, 0}, 20, 0, 320, 240};
  const auto expected = document.Scene().Extract(
      camera, [&](auto) { return std::optional{twod::AtlasBinding{atlas, 88}}; });
  const auto actual = context->ExtractFrame(320, 240, 88);
  Check(expected && actual && expected->vertices.size() == 8 && actual->vertices.size() == 8 &&
            expected->vertices[0].color != expected->vertices[4].color &&
            std::equal(expected->vertices.begin(), expected->vertices.end(),
                       actual->vertices.begin(), actual->vertices.end(),
                       [](const auto &a, const auto &b) {
                         return a.color == b.color && a.position[0] == b.position[0] &&
                                a.position[1] == b.position[1] && a.uv[0] == b.uv[0] &&
                                a.uv[1] == b.uv[1];
                       }),
        "Play changed the existing entity-ID transparency tie order");
}
void Run() {
  SceneOrder();
  editor::TwoDDocument document;
  Check(!editor::TwoDPlayContext::Create(document, 0) &&
            !editor::TwoDPlayContext::Create(document, 4097),
        "pool budget admitted");
  auto empty = editor::TwoDPlayContext::Create(document, 1);
  Check(bool(empty), "empty context creation");
  const auto emptyFrame = empty->ExtractFrame(320, 240, 1);
  Check(emptyFrame && emptyFrame->statistics.visible == 0, "empty document cannot be inspected");
  Check(document.ReplaceAtlas(Atlas()), "document atlas");
  const twod::Clip clip{clipId, atlasId, true, {{1, 0.1, "red"}, {2, 0.1, "green"}}};
  Check(document.ReplaceClip(clip), "document clip");
  twod::Sprite sprite;
  sprite.atlas = atlasId;
  sprite.region = 1;
  sprite.size = {2, 2};
  const auto id = document.AddSprite(sprite);
  Check(bool(id), "document sprite");
  entity = *id;
  Check(document.SetCollider(entity, {}), "document collider");
  const std::array tiles{twod::Tile{-17, 0, 0, 1, true}, twod::Tile{0, 3, 0, 1, false}};
  Check(document.Paint(tiles) && document.SetChunkLoaded({-2, 0}, false), "document tiles");
  document.MarkSaved();
  const auto baseline = document.Save();
  std::optional<twod::SpriteFrame> retained;
  NexoraTwoDInstanceHandle previous{};
  for (unsigned cycle = 0; cycle < 32; ++cycle) {
    document.SetReadOnly(cycle % 2 != 0);
    auto context = editor::TwoDPlayContext::Create(document, 1);
    Check(context && context->World().Kind() == runtime::WorldKind::Play &&
              context->World().ActiveEntityCount() == 1 && context->Scene().Find(entity) &&
              context->Scene().FindCollider(entity) &&
              context->Tiles().Encode() == document.Tiles().Encode(),
          "session snapshot lost metadata");
    NexoraTwoDApiV1 untouched{};
    untouched.struct_size = sizeof(untouched) - 1;
    const auto original = untouched;
    Check(context->GetApi(&untouched) == NEXORA_GAMEPLAY_ERROR_UNSUPPORTED &&
              untouched.struct_size == original.struct_size && !untouched.context,
          "short API output changed");
    failStart = false;
    runtime::GameplayModuleHost module(Host(*context));
    Check(module.Load(Loader), "real module Create/Start");
    if (previous.owner) {
      NexoraTwoDInstance unchanged{};
      unchanged.sprite.region = 99;
      Check(api.read_instances(api.context, &previous, 1, &unchanged) ==
                    NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT &&
                unchanged.sprite.region == 99,
            "previous session pool handle admitted");
    }
    previous = handle;
    Check(api.rebuild_queries(api.context, 0) == NEXORA_GAMEPLAY_OK &&
              api.rebuild_queries(api.context, 1) == NEXORA_GAMEPLAY_OK,
          "copied collision query rebuild");
    const NexoraTwoDShape query{0, {0, {0, 0}, {0.5F, 0.5F}, 0.1F, 1, UINT32_MAX}};
    std::array<std::uint64_t, 4> hits{};
    std::uint32_t hitCount{};
    Check(api.overlap(api.context, 0, &query, hits.data(), hits.size(), &hitCount) ==
                  NEXORA_GAMEPLAY_OK &&
              hitCount == 1 && hits[0] == entity,
          "copied collider lost its isolated World identity");
    const auto beforeAdvance = context->ExtractFrame(320, 240, cycle + 100);
    Check(beforeAdvance && beforeAdvance->statistics.visible == 4 &&
              beforeAdvance->statistics.unavailable == 0 && context->InstanceCount() == 1,
          "scene/pool/loaded tiles extraction");
    // Asset replacement and history on the source cannot change this session's resolvers.
    document.SetReadOnly(false);
    auto changedAtlas = Atlas();
    std::fill(changedAtlas.rgba.begin(), changedAtlas.rgba.end(), std::byte{42});
    auto changedClip = clip;
    changedClip.frames[1].event = "other";
    Check(document.ReplaceAtlas(changedAtlas) && document.ReplaceClip(changedClip),
          "source asset replacement");
    Check(module.Update(0.1), "frozen clip changed after source replacement");
    auto frame = context->ExtractFrame(320, 240, cycle + 100);
    Check(frame && frame->statistics.visible == 4 && frame->retainedAtlases.size() == 1 &&
              frame->retainedAtlases[0].atlas->rgba != changedAtlas.rgba &&
              !std::equal(frame->vertices.begin(), frame->vertices.end(),
                          beforeAdvance->vertices.begin(), beforeAdvance->vertices.end(),
                          [](const auto &a, const auto &b) {
                            return a.uv[0] == b.uv[0] && a.uv[1] == b.uv[1];
                          }),
          "frozen Atlas or animation extraction");
    NexoraTwoDInstance extra{};
    extra.sprite = Sprite();
    extra.transform.scale = {1, 1, 1};
    extra.transform.rotation.w = 1;
    NexoraTwoDInstanceHandle rejected{999, 999, 999};
    Check(api.create_instances(api.context, &extra, 1, &rejected) != NEXORA_GAMEPLAY_OK &&
              rejected.owner == 999 && context->InstanceCount() == 1,
          "pool quota partial publication");
    Check(!context->ExtractFrame(0, 240, 1) && !context->ExtractFrame(320, 240, 0) &&
              !context->ExtractFrame(320, 240, UINT64_MAX - 1),
          "invalid frame geometry/texture admitted");
    int32_t foreign{};
    bool foreignFrame{};
    std::thread wrong([&] {
      NexoraTwoDApiV1 out{};
      out.struct_size = sizeof(out);
      foreign = context->GetApi(&out);
      foreignFrame = bool(context->ExtractFrame(320, 240, 1));
    });
    wrong.join();
    Check(foreign == NEXORA_GAMEPLAY_ERROR_LIFECYCLE && !foreignFrame,
          "wrong-thread session admitted");
    module.Unload();
    Check(stopped && destroyed && context->InstanceCount() == 1,
          "services did not outlive Stop/Destroy");
    const auto child = context->World().CreateEntity(context->Scene().SceneId()).id;
    runtime::WorldCommandBuffer parent;
    parent.SetParent(child, entity);
    const auto childSprite = Sprite();
    const NexoraTwoDCollider childCollider{0, {0, 0}, {0.5F, 0.5F}, 0.5F, 1, UINT32_MAX};
    Check(parent.Apply(context->World()) &&
              api.set_sprite(api.context, child, &childSprite) == NEXORA_GAMEPLAY_OK &&
              api.set_collider(api.context, child, &childCollider) == NEXORA_GAMEPLAY_OK,
          "Play child metadata");
    runtime::WorldCommandBuffer destroy;
    destroy.DestroyEntity(entity);
    Check(destroy.Apply(context->World()), "Play deletion");
    for (auto gone : destroy.LastDestroyed())
      context->ForgetEntity(gone);
    Check(destroy.LastDestroyed().size() == 2 && !context->Scene().Find(entity) &&
              !context->Scene().FindCollider(entity) && !context->Scene().Find(child) &&
              !context->Scene().FindCollider(child),
          "retired metadata remained");
    retained = std::move(frame);
    context.reset();
    Check(retained->Uploads().size() == 1 && retained->retainedAtlases[0].atlas->id == atlasId,
          "frame did not retain Atlas after session destruction");
    Check(document.Undo() && document.Undo() && document.Save() == baseline && !document.Dirty(),
          "Play altered source/history");
    Check(document.Redo() && document.Redo() && document.Undo() && document.Undo() &&
              document.Save() == baseline,
          "Play consumed authoring history");
  }
  auto failed = editor::TwoDPlayContext::Create(document, 1);
  failStart = true;
  runtime::GameplayModuleHost module(Host(*failed));
  Check(!module.Load(Loader) && destroyed && failed->InstanceCount() == 1,
        "failed Start did not retain services until Destroy");
  module.Unload();
  failed.reset();
  Check(document.Save() == baseline && !document.Dirty(), "failed Start altered source");
  auto failedCreate = editor::TwoDPlayContext::Create(document, 1);
  failCreate = true;
  runtime::GameplayModuleHost createModule(Host(*failedCreate));
  Check(!createModule.Load(Loader) && !destroyed && failedCreate->Scene().Find(entity),
        "failed Create ownership");
  failCreate = false;
  createModule.Unload();
  failedCreate.reset();
  Check(document.Save() == baseline, "failed Create altered source");

  auto orphan = [&] {
    editor::TwoDDocument temporary;
    Check(temporary.Reopen(baseline), "temporary document reopen");
    return editor::TwoDPlayContext::Create(temporary, 1);
  }();
  Check(bool(orphan), "orphan context creation");
  failStart = false;
  runtime::GameplayModuleHost orphanModule(Host(*orphan));
  Check(orphanModule.Load(Loader) && orphanModule.Update(0.1),
        "context retained a destroyed source document");
  const auto orphanFrame = orphan->ExtractFrame(320, 240, 42);
  Check(orphanFrame && orphanFrame->statistics.visible == 4, "orphan frame assets");
  orphanModule.Unload();
  orphan.reset();

  auto limited = editor::TwoDPlayContext::Create(document, 1);
  NexoraTwoDApiV1 limitApi{};
  limitApi.struct_size = sizeof(limitApi);
  Check(limited && limited->GetApi(&limitApi) == NEXORA_GAMEPLAY_OK, "frame budget context");
  const NexoraTwoDCamera wide{{0, 0}, 1000, 0};
  Check(limitApi.set_camera(limitApi.context, &wide) == NEXORA_GAMEPLAY_OK &&
            limitApi.set_chunk_loaded(limitApi.context, -2, 0, 1) == NEXORA_GAMEPLAY_OK,
        "frame budget camera/chunk");
  std::vector<NexoraTwoDTile> cells;
  for (int i = 0; i < 99999; ++i)
    cells.push_back({i % 317 - 158, i / 317 - 158, 0, 1, 0});
  Check(limitApi.paint_tiles(limitApi.context, cells.data(), cells.size()) == NEXORA_GAMEPLAY_OK &&
            limited->Tiles().Tiles().size() == 99999,
        "frame budget tile preparation");
  const auto exact = limited->ExtractFrame(320, 240, 43);
  Check(exact && exact->statistics.input == 100000 && exact->statistics.visible == 100000,
        "exact aggregate frame budget rejected");
  const NexoraTwoDTile last{99999 % 317 - 158, 99999 / 317 - 158, 0, 1, 0};
  Check(limitApi.paint_tiles(limitApi.context, &last, 1) == NEXORA_GAMEPLAY_OK &&
            limited->Tiles().Tiles().size() == 100000,
        "overflow frame fixture");
  Check(!limited->ExtractFrame(320, 240, 43), "aggregate scene/tile input budget exceeded");
  Check(document.Save() == baseline && !document.Dirty(), "frame budget test altered source");
}
#if defined(NEXORA_PLAY_CONTEXT_NATIVE)
unsigned Channel(unsigned long pixel, unsigned long mask) {
  while (!(mask & 1)) {
    mask >>= 1;
    pixel >>= 1;
  }
  return unsigned((pixel & mask) * 255 / mask);
}
void Native(const char *capture) {
  using namespace Nexora::Presentation;
  auto windows = Nexora::Window::CreateWindowSystem();
  Check(bool(windows), "native window system");
  auto window = windows->Create({"Nexora isolated 2D Play", 320, 240, true, true});
  Check(bool(window), "native window");
  SurfaceDescriptor descriptor;
  descriptor.window = window.handle;
  descriptor.width = 320;
  descriptor.height = 240;
  descriptor.backend = SurfaceBackend::Vulkan;
  auto surface = CreateSurface(descriptor, *windows);
  Check(bool(surface), "native Vulkan surface");
  auto *display = XOpenDisplay(nullptr);
  Check(display != nullptr, "native display");
  const auto native = reinterpret_cast<::Window>(windows->NativeHandle(window.handle));
  editor::TwoDDocument document;
  Check(document.ReplaceAtlas(Atlas()) &&
            document.ReplaceClip({clipId, atlasId, true, {{1, 0.1, "red"}, {2, 0.1, "green"}}}),
        "native document assets");
  twod::Sprite sprite;
  sprite.atlas = atlasId;
  sprite.region = 1;
  sprite.size = {2, 2};
  const auto id = document.AddSprite(sprite);
  Check(bool(id), "native document sprite");
  entity = *id;
  document.MarkSaved();
  auto context = editor::TwoDPlayContext::Create(document, 1);
  Check(bool(context), "native Play context");
  failStart = false;
  runtime::GameplayModuleHost module(Host(*context));
  Check(module.Load(Loader), "native gameplay callbacks");
  std::vector<std::array<unsigned, 3>> samples;
  const auto draw = [&](const twod::SpriteFrame &frame, unsigned color, bool save) {
    auto uploads = frame.Uploads();
    auto packet = PrepareSpriteDraws(
        {frame.vertices, std::as_bytes(std::span(frame.indices)), frame.batches, uploads, true}, {},
        320, 240);
    Check(bool(packet), "native combined frame packet");
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    do {
      static_cast<void>(windows->PumpEvents());
      Check(surface->Acquire() == SurfaceStatus::Ready, "native acquire");
      Check(surface->RenderUi(packet->View()) == SurfaceStatus::Ready, "native draw");
      Check(surface->Present() == SurfaceStatus::Ready, "native present");
      XSync(display, False);
      auto *image = XGetImage(display, native, 0, 0, 320, 240, AllPlanes, ZPixmap);
      Check(image != nullptr, "native pixels");
      const auto pixel = XGetPixel(image, 160, 120);
      const std::array channels{Channel(pixel, image->red_mask), Channel(pixel, image->green_mask),
                                Channel(pixel, image->blue_mask)};
      const bool correct =
          channels[color] > 250 && channels[(color + 1) % 3] < 3 && channels[(color + 2) % 3] < 3;
      if (correct && save) {
        std::ofstream file(capture, std::ios::binary);
        file << "P6\n320 240\n255\n";
        for (unsigned y = 0; y < 240; ++y)
          for (unsigned x = 0; x < 320; ++x) {
            const auto p = XGetPixel(image, x, y);
            const char rgb[]{char(Channel(p, image->red_mask)), char(Channel(p, image->green_mask)),
                             char(Channel(p, image->blue_mask))};
            file.write(rgb, 3);
          }
        Check(bool(file), "native capture");
      }
      XDestroyImage(image);
      if (correct) {
        samples.push_back(channels);
        return;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(2));
    } while (std::chrono::steady_clock::now() < deadline);
    throw std::runtime_error("native frozen/animated/replacement Atlas pixel mismatch");
  };
  auto red = context->ExtractFrame(320, 240, 500);
  Check(bool(red), "native initial frame");
  draw(*red, 0, false);
  auto blue = Atlas();
  for (std::size_t i = 0; i < blue.rgba.size(); i += 4) {
    blue.rgba[i] = blue.rgba[i + 1] = std::byte{0};
    blue.rgba[i + 2] = blue.rgba[i + 3] = std::byte{255};
  }
  Check(document.ReplaceAtlas(blue), "native source replacement");
  const auto source = document.Save();
  const auto stillRed = context->ExtractFrame(320, 240, 500);
  Check(bool(stillRed), "native frozen frame");
  draw(*stillRed, 0, false);
  Check(module.Update(0.1), "native animation Update");
  auto green = context->ExtractFrame(320, 240, 500);
  Check(bool(green), "native animated frame");
  draw(*green, 1, false);
  module.Unload();
  Check(stopped && destroyed, "native callback teardown order");
  context.reset();
  draw(*green, 1, false);
  auto replacement = editor::TwoDPlayContext::Create(document, 1);
  Check(bool(replacement), "native replacement context");
  auto fresh = replacement->ExtractFrame(320, 240, 501);
  Check(bool(fresh), "native replacement frame");
  draw(*fresh, 2, true);
  Check(document.Save() == source && document.Dirty(), "native source mutated by Play");
  const std::array<std::uint64_t, 2> retired{500, 501};
  Check(surface->ReleaseSpriteTextures(retired) == SurfaceStatus::Ready &&
            // Two frozen Atlas generations plus PrepareSpriteDraws' immutable white background.
            surface->Diagnostics().nativeUiTextureUploads == 3,
        "native immutable generations/retirement");
  XCloseDisplay(display);
  Check(surface->DrainAndDestroy() == SurfaceStatus::Ready, "native surface teardown");
  surface.reset();
  Check(windows->Destroy(window.handle) == Nexora::Window::WindowError::None,
        "native window teardown");
  std::cout << "2D Play native evidence: {\"rgb\":[";
  for (std::size_t i = 0; i < samples.size(); ++i) {
    if (i)
      std::cout << ',';
    std::cout << '[' << samples[i][0] << ',' << samples[i][1] << ',' << samples[i][2] << ']';
  }
  std::cout << "],\"native_texture_uploads\":3,\"source_bytes\":\"unchanged\"}\n";
  std::cout << "2D Play native pixels: frozen red, animated green, retained green, new-session "
               "blue; source preserved\n";
}
#endif
} // namespace
int main(int argc, char **argv) {
  try {
    Run();
#if defined(NEXORA_PLAY_CONTEXT_NATIVE)
    Check(argc == 2, "capture path required");
    Native(argv[1]);
#else
    static_cast<void>(argc);
    static_cast<void>(argv);
#endif
    std::cout << "2D Play context: 32 isolated sessions, frozen assets, callbacks, frames and "
                 "source history passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
