#include "PlayGameplayModule.h"
#include <array>
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>
#if defined(NEXORA_EDITOR_SOURCE_NATIVE)
#include "Nexora/EditorImGui/EditorImGui.h"
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#undef None
#endif

namespace {
using namespace nexora;
constexpr runtime::AssetUuid atlasId{17, 23}, sourceId{31, 37};
runtime::Id entity{};
NexoraTwoDApiV1 twodApi{};
NexoraTwoDSourceApiV1 sourceApi{};
NexoraTwoDInstanceHandle handle{};
bool stopped{}, destroyed{};
void Check(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
void Write(const std::filesystem::path &path, std::span<const std::byte> bytes) {
  std::ofstream output(path, std::ios::binary);
  output.write(reinterpret_cast<const char *>(bytes.data()), std::streamsize(bytes.size()));
  Check(bool(output), "source fixture write");
}
twod::Bytes Source(float pivot) {
  twod::SpriteAsset value;
  value.id = sourceId;
  value.atlas = atlasId;
  value.region = 1;
  value.sourceWidth = value.sourceHeight = 4;
  value.trimX = value.trimY = value.trimWidth = value.trimHeight = 1;
  value.pivotX = pivot;
  value.pivotY = 2;
  value.pixelsPerUnit = 1;
  return twod::EncodeSpriteAsset(value);
}
int32_t Create(void **state, const NexoraGameplayHostV3 *host) {
  stopped = destroyed = false;
  twodApi = {};
  twodApi.struct_size = sizeof(twodApi);
  sourceApi = {};
  sourceApi.struct_size = sizeof(sourceApi);
  if (!host->get_twod_api || host->get_twod_api(host->context, &twodApi) ||
      NexoraGetTwoDSourcesV1(&twodApi, &sourceApi))
    return NEXORA_GAMEPLAY_ERROR_UNSUPPORTED;
  const NexoraTransformV2 transform{{1, 0, 0}, {0, 0, 0, 1}, {1, 1, 1}};
  const NexoraTwoDCamera camera{{0, 0}, 10, 0};
  NexoraTwoDSourceSpriteV1 sprite{
      {0, 0, 0, 8, {1, 1}, {0.5F, 0.5F}, {1, 0, 0, 1}, 0, 0}, sourceId.high, sourceId.low};
  if (host->write_component(host->context, entity, game::TransformComponentType(), &transform,
                            sizeof(transform)) ||
      twodApi.set_camera(twodApi.context, &camera) ||
      sourceApi.set_sprite(sourceApi.context, entity, &sprite))
    return NEXORA_GAMEPLAY_ERROR_LIFECYCLE;
  sprite.sprite.color[0] = 0;
  sprite.sprite.color[2] = 1;
  const NexoraTwoDSourceInstanceV1 instance{{{-1, 0, 0}, {0, 0, 0, 1}, {-1, 1, 1}}, sprite};
  if (sourceApi.create_instances(sourceApi.context, &instance, 1, &handle))
    return NEXORA_GAMEPLAY_ERROR_LIFECYCLE;
  NexoraTwoDSprite legacy{};
  legacy.order = 123;
  if (twodApi.get_sprite(twodApi.context, entity, &legacy) != NEXORA_GAMEPLAY_ERROR_UNSUPPORTED ||
      legacy.order != 123)
    return NEXORA_GAMEPLAY_ERROR_LIFECYCLE;
  *state = &sourceApi;
  return NEXORA_GAMEPLAY_OK;
}
void Stop(void *) {
  NexoraTwoDSourceSpriteV1 value{};
  stopped = sourceApi.get_sprite(sourceApi.context, entity, &value) == 0 &&
            value.source_high == sourceId.high && value.source_low == sourceId.low;
}
void Destroy(void *) { destroyed = twodApi.destroy_instances(twodApi.context, &handle, 1) == 0; }
int32_t Loader(std::uint32_t version, NexoraGameModuleV3 *module) {
  *module = {};
  module->struct_size = sizeof(*module);
  module->abi_version = version;
  module->create = Create;
  module->on_start = [](void *) -> int32_t { return 0; };
  module->update = [](void *, double) -> int32_t { return 0; };
  module->on_stop = Stop;
  module->destroy = Destroy;
  return 0;
}
#if defined(NEXORA_EDITOR_SOURCE_NATIVE)
Window FindWindow(Display *display, Window parent) {
  char *name{};
  if (XFetchName(display, parent, &name) && name) {
    const bool match = std::string_view(name) == "Editor Sprite source native";
    XFree(name);
    if (match)
      return parent;
  }
  Window root{}, ancestor{}, *children{};
  unsigned count{};
  if (!XQueryTree(display, parent, &root, &ancestor, &children, &count))
    return 0;
  Window found{};
  for (unsigned i = 0; i < count && !found; ++i)
    found = FindWindow(display, children[i]);
  if (children)
    XFree(children);
  return found;
}
unsigned Channel(unsigned long pixel, unsigned long mask) {
  while (mask && !(mask & 1)) {
    mask >>= 1;
    pixel >>= 1;
  }
  return mask ? unsigned((pixel & mask) * 255 / mask) : 0;
}
void Save(XImage *image, const std::filesystem::path &path) {
  std::ofstream output(path, std::ios::binary);
  output << "P6\n640 480\n255\n";
  for (unsigned y = 0; y < 480; ++y)
    for (unsigned x = 0; x < 640; ++x) {
      const auto pixel = XGetPixel(image, x, y);
      const char rgb[]{char(Channel(pixel, image->red_mask)),
                       char(Channel(pixel, image->green_mask)),
                       char(Channel(pixel, image->blue_mask))};
      output.write(rgb, 3);
    }
  Check(bool(output), "source native capture");
}
void NativeFrame(editor::imgui::EditorImGuiHost &uiHost, editor::ProductShell &shell,
                 Nexora::Presentation::RenderSurface &surface,
                 const editor::preview::PlayTwoDFrame &frame, Display *display, bool shifted,
                 const std::filesystem::path &capture) {
  using Nexora::Presentation::SurfaceStatus;
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  do {
    if (surface.BeginFrame() != SurfaceStatus::Ready)
      continue;
    uiHost.SetDisplay(640, 480, 1);
    uiHost.ProcessEvents(surface.Events());
    uiHost.BeginFrame();
    uiHost.DrawProductShell(shell);
    static_cast<void>(uiHost.EndFrame());
    Check(uiHost.Render(surface, 640, 480, frame.DrawData()) == SurfaceStatus::Ready &&
              surface.EndFrame() == SurfaceStatus::Ready,
          "source Editor native draw/present");
    XSync(display, False);
    const auto window = FindWindow(display, DefaultRootWindow(display));
    if (!window)
      continue;
    auto *image = XGetImage(display, window, 0, 0, 640, 480, AllPlanes, ZPixmap);
    Check(image != nullptr, "source Editor readback");
    const auto red = [&](unsigned x) {
      const auto pixel = XGetPixel(image, x, 137);
      return Channel(pixel, image->red_mask) > 250 && Channel(pixel, image->green_mask) < 3 &&
             Channel(pixel, image->blue_mask) < 3;
    };
    const auto blue = [&](unsigned x) {
      const auto pixel = XGetPixel(image, x, 137);
      return Channel(pixel, image->blue_mask) > 250 && Channel(pixel, image->green_mask) < 3 &&
             Channel(pixel, image->red_mask) < 3;
    };
    const bool match = red(shifted ? 188 : 212) && blue(shifted ? 212 : 188) && !red(152) &&
                       !blue(152) && !red(236) && !blue(236);
    Save(image, capture); // Retain the last real readback even if the assertion fails.
    XDestroyImage(image);
    if (match)
      return;
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
  } while (std::chrono::steady_clock::now() < deadline);
  throw std::runtime_error("source trim/Pivot/negative-scale Editor pixels mismatch");
}
#endif
void Run(const std::filesystem::path &root, const std::filesystem::path &capture) {
  runtime::World authoring;
  const auto scene = authoring.LoadScene("Main");
  Check(authoring.Activate(scene), "activate source scene");
  editor::SceneDocument document(authoring, scene);
  entity = document.Create("Source Sprite");
  const auto baseline = document.CaptureRuntimeScene();
  std::array<editor::preview::PlayTwoDBinding, 2> bindings;
  {
    editor::ProjectWorkspace workspace;
    Check(workspace.Create(root, "Source Play"), "source project");
    const twod::Image white{
        1, 1, 1, {std::byte{255}, std::byte{255}, std::byte{255}, std::byte{255}}};
    const auto atlas = twod::PackAtlas(atlasId, std::span(&white, 1), 4);
    Check(bool(atlas), "source white Atlas");
    Write(root / "Content/Atlas.nx2atlas", twod::EncodeAtlas(*atlas));
    Write(root / "Content/Sprite.nx2sprite", Source(2));
    editor::AssetWorkspace index;
    Check(
        index.ImportTree(root / "Content", {}, {}, editor::AssetIdentityMode::PersistentReadWrite),
        "source index");
    editor::ProjectContentSession content;
    Check(content.Open(workspace, index, 13), "source Content");
    std::string error;
    auto first = editor::preview::CapturePlayTwoD(workspace, content, document, true, error);
    Check(first && first->assets && error.empty(), "source Play capture");
    bindings[0] = std::move(*first);
    Write(root / "Content/Sprite.nx2sprite", Source(3));
    Check(!bindings[0].assets->Matches(workspace, content, true) && content.Reimport(sourceId),
          "stale source or reimport");
    auto second = editor::preview::CapturePlayTwoD(workspace, content, document, true, error);
    Check(second && second->assets && error.empty(), "reimported source Play capture");
    bindings[1] = std::move(*second);
  }
  std::filesystem::remove_all(root); // Both snapshots must render without project/source owners.
#if defined(NEXORA_EDITOR_SOURCE_NATIVE)
  auto surface =
      Nexora::Presentation::CreateRenderSurface({"Editor Sprite source native", 640, 480});
  Check(bool(surface), "source native surface");
  editor::imgui::EditorImGuiHost uiHost;
  editor::ProductShell shell;
  Display *display = XOpenDisplay(nullptr);
  Check(display != nullptr, "source native display");
  const auto attempt =
      capture.parent_path() /
      ("attempt-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  std::filesystem::create_directories(attempt);
  std::cout << "Source Editor native evidence: " << attempt << '\n';
#else
  static_cast<void>(capture);
#endif
  runtime::PlaySession play(authoring);
  std::optional<float> firstX;
  for (unsigned phase = 0; phase < 3; ++phase) {
    const bool shifted = phase == 1;
    Check(play.Start(0.1, [](runtime::World &, double) { return true; }), "source Play clone");
    editor::preview::PlayGameplayModule module;
    Check(module.Load(*play.PlayWorld(), Loader, {}, bindings[shifted ? 1 : 0]) &&
              module.Update(0.1),
          "source gameplay module");
    const auto frame = module.TwoDFrame({40, 30, 320, 240});
    Check(frame && frame->sprites.statistics.visible == 2 &&
              frame->sprites.retainedAtlases.size() == 1 && frame->data.vertices.size() == 8 &&
              frame->data.indices.size() == 12,
          "source scene/pool extraction");
    if (phase == 0)
      firstX = frame->data.vertices[0].position[0];
    else
      Check((*firstX == frame->data.vertices[0].position[0]) == !shifted,
            "source generation geometry or retained snapshot changed");
#if defined(NEXORA_EDITOR_SOURCE_NATIVE)
    NativeFrame(uiHost, shell, *surface.surface, *frame, display, shifted,
                attempt / (std::to_string(phase) + ".ppm"));
    if (phase == 2)
      std::filesystem::copy_file(attempt / "2.ppm", capture,
                                 std::filesystem::copy_options::overwrite_existing);
#endif
    module.Unload();
    Check(stopped && destroyed && play.Stop() && document.CaptureRuntimeScene() == baseline,
          "source module teardown or authoring isolation");
#if defined(NEXORA_EDITOR_SOURCE_NATIVE)
    Check(surface.surface->ReleaseSpriteTextures(module.TakeRetiredTwoDTextures()) ==
              Nexora::Presentation::SurfaceStatus::Ready,
          "source native texture retirement");
#endif
  }
#if defined(NEXORA_EDITOR_SOURCE_NATIVE)
  Check(surface.surface->Diagnostics().nativeUiRejectedTextures == 0 &&
            surface.surface->DrainAndDestroy() == Nexora::Presentation::SurfaceStatus::Ready,
        "source native drain");
  XCloseDisplay(display);
#endif
}
} // namespace
int main(int argc, char **argv) {
  const auto root = std::filesystem::temp_directory_path() /
                    ("nexora-source-play-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  try {
    Run(root, argc > 1 ? argv[1] : std::filesystem::path{});
    std::filesystem::remove_all(root);
    std::cout << "PASS Editor source snapshots, C gameplay, negative-scale pool and reimport\n";
    return 0;
  } catch (const std::exception &error) {
    std::filesystem::remove_all(root);
    std::cerr << error.what() << '\n';
    return 1;
  }
}
