#include "Nexora/Editor/TwoDAssetImport.h"
#include "PlayGameplayModule.h"
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>
#if defined(NEXORA_PLAY_2D_NATIVE)
#include "Nexora/EditorImGui/EditorImGui.h"
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#undef None
#endif

namespace {
using namespace nexora;
constexpr runtime::AssetUuid atlasId{17, 23}, clipId{19, 29};
NexoraGameplayHostV3 host{};
NexoraTwoDApiV1 api{};
NexoraGameplayUiApiV1 ui{};
NexoraGameplayInputApiV1 input{};
NexoraTwoDInstanceHandle handle{}, previous{};
runtime::Id parent{}, child{};
bool fail_create{}, fail_start{}, stopped{}, destroyed{};
unsigned stop_count{}, destroy_count{};
void Check(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
void Write(const std::filesystem::path &path, std::span<const std::byte> bytes) {
  std::ofstream output(path, std::ios::binary);
  output.write(reinterpret_cast<const char *>(bytes.data()),
               static_cast<std::streamsize>(bytes.size()));
  Check(bool(output), "write fixture");
}
NexoraTwoDSprite Sprite() {
  return {atlasId.high, atlasId.low, 1, 8, {1, 1}, {0.5F, 0.5F}, {1, 1, 1, 1}, 0, 0};
}
int32_t Create(void **state, const NexoraGameplayHostV3 *wire) {
  host = *wire;
  stopped = destroyed = false;
  api = {};
  api.struct_size = sizeof(api);
  if (!(host.capabilities & NEXORA_GAMEPLAY_CAPABILITY_2D_API) || !host.get_twod_api ||
      host.get_twod_api(host.context, &api))
    return NEXORA_GAMEPLAY_ERROR_LIFECYCLE;
  NexoraGameplayServicesApiV1 services{};
  services.struct_size = sizeof(services);
  services.abi_version = 1;
  ui = {};
  ui.struct_size = sizeof(ui);
  ui.abi_version = 1;
  input = {};
  input.struct_size = sizeof(input);
  input.abi_version = 1;
  NexoraStorageApiV1 storage{};
  storage.struct_size = sizeof(storage);
  storage.abi_version = 1;
  if (!(host.capabilities & NEXORA_GAMEPLAY_CAPABILITY_SERVICES_API) ||
      NexoraGetGameplayServicesV1(&host, &services) || services.get_ui_api(services.context, &ui) ||
      services.get_input_api(services.context, &input) ||
      services.get_storage_api(services.context, &storage))
    return NEXORA_GAMEPLAY_ERROR_LIFECYCLE;
  NexoraTwoDSprite missing{};
  NexoraTwoDInstance instance{};
  if (api.get_sprite(api.context, parent, &missing) != NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT ||
      (previous.owner && api.read_instances(api.context, &previous, 1, &instance) !=
                             NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT))
    return NEXORA_GAMEPLAY_ERROR_LIFECYCLE;
  const auto sprite = Sprite();
  const NexoraTwoDCollider collider{0, {0, 0}, {0.5F, 0.5F}, 0.5F, 1, UINT32_MAX};
  instance = {{{0, 0, 0}, {0, 0, 0, 1}, {1, 1, 1}}, sprite};
  if (api.set_sprite(api.context, parent, &sprite) || api.set_sprite(api.context, child, &sprite) ||
      api.set_collider(api.context, child, &collider) ||
      api.create_instances(api.context, &instance, 1, &handle))
    return NEXORA_GAMEPLAY_ERROR_LIFECYCLE;
  previous = handle;
  *state = &api;
  return fail_create ? NEXORA_GAMEPLAY_ERROR_LIFECYCLE : NEXORA_GAMEPLAY_OK;
}
int32_t Start(void *) {
  if (api.play_animation(api.context, parent, clipId.high, clipId.low, 1))
    return NEXORA_GAMEPLAY_ERROR_LIFECYCLE;
  return fail_start ? NEXORA_GAMEPLAY_ERROR_LIFECYCLE : NEXORA_GAMEPLAY_OK;
}
int32_t Update(void *, double seconds) {
  NexoraTwoDEvent event{};
  NexoraTwoDAnimationResult result{};
  if (ui.begin_frame(ui.context, 320, 240) || ui.panel(ui.context, {0, 0, 320, 240}, 0x330000ffU) ||
      ui.end_frame(ui.context))
    return NEXORA_GAMEPLAY_ERROR_LIFECYCLE;
  const auto status = api.advance_animation(api.context, parent, seconds, &event, 1, &result);
  return status || result.event_count != 1 || event.length != 6 ||
                 std::string_view(event.name, event.length) != "frozen"
             ? NEXORA_GAMEPLAY_ERROR_LIFECYCLE
             : NEXORA_GAMEPLAY_OK;
}
void Stop(void *) {
  ++stop_count;
  NexoraTwoDSprite sprite{};
  stopped = api.get_sprite(api.context, parent, &sprite) == NEXORA_GAMEPLAY_OK;
}
void Destroy(void *) {
  ++destroy_count;
  NexoraTwoDInstance instance{};
  game::GameplayTransformWire transform{};
  destroyed = api.read_instances(api.context, &handle, 1, &instance) == NEXORA_GAMEPLAY_OK &&
              host.read_component(host.context, parent, game::TransformComponentType(), &transform,
                                  sizeof(transform)) == NEXORA_GAMEPLAY_OK;
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
int32_t PlainCreate(void **state, const NexoraGameplayHostV3 *wire) {
  *state = nullptr;
  return !(wire->capabilities & NEXORA_GAMEPLAY_CAPABILITY_2D_API) && !wire->get_twod_api
             ? NEXORA_GAMEPLAY_OK
             : NEXORA_GAMEPLAY_ERROR_LIFECYCLE;
}
int32_t PlainLoader(std::uint32_t version, NexoraGameModuleV3 *module) {
  Loader(version, module);
  module->create = PlainCreate;
  module->on_start = [](void *) -> int32_t { return NEXORA_GAMEPLAY_OK; };
  module->update = [](void *, double) -> int32_t { return NEXORA_GAMEPLAY_OK; };
  module->on_stop = [](void *) {};
  module->destroy = [](void *) {};
  return NEXORA_GAMEPLAY_OK;
}
#if defined(NEXORA_PLAY_2D_NATIVE)
Window FindNativeWindow(Display *display, Window parentWindow) {
  char *name{};
  if (XFetchName(display, parentWindow, &name) && name) {
    const bool match = std::string_view(name) == "Editor Play 2D native";
    XFree(name);
    if (match)
      return parentWindow;
  }
  Window root{}, parentResult{}, *children{};
  unsigned count{};
  if (!XQueryTree(display, parentWindow, &root, &parentResult, &children, &count))
    return 0;
  Window found{};
  for (unsigned i = 0; i < count && !found; ++i)
    found = FindNativeWindow(display, children[i]);
  if (children)
    XFree(children);
  return found;
}
unsigned Channel(unsigned long pixel, unsigned long mask) {
  while (mask && !(mask & 1)) {
    mask >>= 1;
    pixel >>= 1;
  }
  return mask ? static_cast<unsigned>((pixel & mask) * 255 / mask) : 0;
}
void NativeFrame(editor::imgui::EditorImGuiHost &uiHost, editor::ProductShell &shell,
                 Nexora::Presentation::RenderSurface &surface,
                 const editor::preview::PlayTwoDFrame &frame, Display *display,
                 const std::filesystem::path &capture) {
  using Nexora::Presentation::SurfaceStatus;
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  do {
    const auto acquired = surface.BeginFrame();
    if (acquired != SurfaceStatus::Ready) {
      std::this_thread::sleep_for(std::chrono::milliseconds(2));
      continue;
    }
    uiHost.SetDisplay(640, 480, 1);
    uiHost.ProcessEvents(surface.Events());
    uiHost.BeginFrame();
    uiHost.DrawProductShell(shell);
    static_cast<void>(uiHost.EndFrame());
    Check(uiHost.Render(surface, 640, 480, frame.DrawData()) == SurfaceStatus::Ready,
          "Editor 2D composed draw");
    Check(surface.EndFrame() == SurfaceStatus::Ready, "Editor 2D native present");
    XSync(display, False);
    const auto window = FindNativeWindow(display, DefaultRootWindow(display));
    if (!window)
      continue;
    auto *image = XGetImage(display, window, 0, 0, 640, 480, AllPlanes, ZPixmap);
    Check(image != nullptr, "Editor native capture");
    const auto pixel = XGetPixel(image, 200, 150);
    const auto outside = XGetPixel(image, 10, 150);
    const bool red = Channel(pixel, image->red_mask) > 250 &&
                     Channel(pixel, image->green_mask) < 3 && Channel(pixel, image->blue_mask) < 3;
    const bool bounded =
        !(Channel(outside, image->red_mask) > 250 && Channel(outside, image->green_mask) < 3);
    if (red && bounded && !capture.empty()) {
      std::ofstream file(capture, std::ios::binary);
      file << "P6\n640 480\n255\n";
      for (unsigned y = 0; y < 480; ++y)
        for (unsigned x = 0; x < 640; ++x) {
          const auto value = XGetPixel(image, x, y);
          const char rgb[]{static_cast<char>(Channel(value, image->red_mask)),
                           static_cast<char>(Channel(value, image->green_mask)),
                           static_cast<char>(Channel(value, image->blue_mask))};
          file.write(rgb, 3);
        }
      Check(bool(file), "native screenshot publication");
    }
    XDestroyImage(image);
    if (red && bounded)
      return;
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
  } while (std::chrono::steady_clock::now() < deadline);
  throw std::runtime_error("Editor 2D Sprite/UI pixels or viewport clip mismatch");
}
#endif
void Run(const std::filesystem::path &root, const std::filesystem::path &capture) {
#if defined(NEXORA_PLAY_2D_NATIVE)
  editor::imgui::EditorImGuiHost uiHost;
  editor::ProductShell shell;
  auto surface = Nexora::Presentation::CreateRenderSurface({"Editor Play 2D native", 640, 480});
  Check(bool(surface), "Editor native surface");
  Display *display = XOpenDisplay(nullptr);
  Check(display != nullptr, "native X display");
#else
  static_cast<void>(capture);
#endif
  runtime::World authoring;
  const auto scene = authoring.LoadScene("Main");
  Check(authoring.Activate(scene), "activate authoring");
  editor::SceneDocument document(authoring, scene);
  parent = document.Create("Parent");
  child = document.Create("Child", parent);
  const auto baseline = document.CaptureRuntimeScene();
  Check(bool(baseline), "source baseline");
  editor::preview::PlayTwoDBinding binding;
  {
    editor::ProjectWorkspace workspace;
    Check(workspace.Create(root, "Editor Play 2D"), "project fixture");
    editor::AssetWorkspace index;
    editor::ProjectContentSession content;
    std::string error;
    Check(index.ImportTree(root / "Content") && content.Open(workspace, index, 13), "empty index");
    const auto empty = editor::preview::CapturePlayTwoD(workspace, content, document, false, error);
    Check(empty && !empty->assets && !empty->scene && error.empty(), "empty 3D project path");
    const twod::Image image{1, 1, 1, {std::byte{255}, std::byte{0}, std::byte{0}, std::byte{255}}};
    const auto atlas = twod::PackAtlas(atlasId, std::span(&image, 1), 4);
    Check(bool(atlas), "atlas fixture");
    const auto bytes = twod::EncodeAtlas(*atlas);
    Write(root / "Content/Atlas.nx2atlas", bytes);
    Write(root / "Content/Clip.nx2clip",
          twod::EncodeClip({clipId, atlasId, true, {{1, 0.1, "frozen"}}}));
    Check(index.ImportTree(root / "Content", {}, {},
                           editor::AssetIdentityMode::PersistentReadWrite) &&
              content.Open(workspace, index, 13),
          "typed index");
    Check(!editor::preview::CapturePlayTwoD(workspace, content, document, false, error) &&
              !error.empty(),
          "unauthorized capture");
    auto captured = editor::preview::CapturePlayTwoD(workspace, content, document, true, error);
    Check(captured && captured->scene == scene && captured->assets && error.empty() &&
              captured->assets->Matches(workspace, content, true),
          "project preparation");
    binding = std::move(*captured);
    Write(root / "Content/Atlas.nx2atlas", {});
    Check(!binding.assets->Matches(workspace, content, true), "activation accepted changed bytes");
    Write(root / "Content/Atlas.nx2atlas", bytes);
    Check(binding.assets->Matches(workspace, content, true), "restored source scope");
  }
  Check(!editor::preview::PlayTwoDServices::Bind(authoring, binding), "authoring World admitted");
  runtime::PlaySession play(authoring);
  editor::preview::PlayGameplayModule module;
  std::uint64_t previousTexture{};
  for (unsigned cycle = 0; cycle < 32; ++cycle) {
    Check(play.Start(0.1, [](runtime::World &, double) { return true; }), "start Play clone");
    auto &world = *play.PlayWorld();
    auto invalid = binding;
    invalid.scene = 0;
    Check(!module.Load(world, Loader, {}, invalid) && !module.IsBoundTo(&world), "invalid binding");
    fail_create = true;
    Check(!module.Load(world, Loader, {}, binding) && !module.IsLoaded() && !destroyed,
          "failed Create cleanup");
    fail_create = false;
    fail_start = true;
    Check(!module.Load(world, Loader, {}, binding) && !module.IsLoaded() && destroyed && !stopped,
          "failed Start destroy lifetime");
    fail_start = false;
    Check(module.Load(world, Loader, {}, binding) && module.IsBoundTo(&world), "bind actual clone");
    const auto stops = stop_count, destroys = destroy_count;
    Check(module.Load(world, Loader, {}, binding) && stop_count == stops + 1 &&
              destroy_count == destroys + 1,
          "replacement did not unload the previous owner");
    NexoraTwoDApiV1 short_api{};
    short_api.struct_size = 1;
    Check(host.get_twod_api(host.context, &short_api) == NEXORA_GAMEPLAY_ERROR_UNSUPPORTED &&
              short_api.struct_size == 1 && !short_api.context,
          "short API output changed");
    int32_t cross_thread{};
    std::thread worker([&] { cross_thread = api.set_camera(api.context, nullptr); });
    worker.join();
    Check(cross_thread == NEXORA_GAMEPLAY_ERROR_LIFECYCLE, "wrong-thread API admitted");
    const NexoraGameplayInputBindingV1 keyBinding{
        sizeof(keyBinding), 1, NEXORA_INPUT_DEVICE_KEYBOARD, 0, NEXORA_INPUT_KEY_A, 1, 0, 0};
    Check(input.bind(input.context, 0, 42, &keyBinding) == 0, "action binding");
    using namespace Nexora::Window;
    WindowEvent press{};
    press.type = WindowEventType::Key;
    press.value0 = static_cast<int>(Key::A);
    press.value1 = 1;
    module.SetInputFocus(true);
    module.ProcessInput(std::span(&press, 1)); // Acquisition batch is always discarded.
    NexoraGameplayActionStateV1 action{};
    action.struct_size = sizeof(action);
    action.abi_version = 1;
    Check(input.read_action(input.context, 0, 42, &action) == 0 && !action.held,
          "acquisition leaked through services");
    module.ProcessInput(std::span(&press, 1));
    Check(input.read_action(input.context, 0, 42, &action) == 0 && action.held && action.pressed,
          "accepted key missing from services");
    module.SetInputFocus(false);
    Check(input.read_action(input.context, 0, 42, &action) == 0 && !action.held,
          "revocation retained service input");
    Check(module.Update(0.1), "frozen clip after project owners destroyed");
    const auto frame = module.TwoDFrame({40, 30, 320, 240});
    Check(frame && frame->sprites.statistics.visible == 3 && frame->data.commands.size() >= 2 &&
              !frame->sprites.retainedAtlases.empty(),
          "scene, pool and UI extraction");
    const auto texture = frame->sprites.retainedAtlases.front().texture;
    Check(texture && texture != previousTexture && !frame->data.uploads.empty() &&
              frame->data.uploads.front().pixels[0] == std::byte{255},
          "frozen texture ownership");
    previousTexture = texture;
#if defined(NEXORA_PLAY_2D_NATIVE)
    NativeFrame(uiHost, shell, *surface.surface, *frame, display,
                cycle == 31 ? capture : std::filesystem::path{});
#endif
    for (const auto &command : frame->data.commands)
      Check(command.clipX >= 40 && command.clipY >= 30 &&
                command.clipX + command.clipWidth <= 360 &&
                command.clipY + command.clipHeight <= 270,
            "Game viewport clipping");
    Check(api.rebuild_queries(api.context, 0) == NEXORA_GAMEPLAY_OK &&
              host.despawn_entity(host.context, parent) == NEXORA_GAMEPLAY_OK &&
              !world.FindEntity(parent) && !world.FindEntity(child) &&
              api.rebuild_queries(api.context, 0) == NEXORA_GAMEPLAY_OK,
          "descendant metadata not forgotten");
    // Restore the clone for teardown/reload callbacks; source state and history stay untouched.
    module.Unload();
    const auto retired = module.TakeRetiredTwoDTextures();
#if defined(NEXORA_PLAY_2D_NATIVE)
    Check(surface.surface->ReleaseSpriteTextures(retired) ==
              Nexora::Presentation::SurfaceStatus::Ready,
          "native texture retirement");
#endif
    Check(std::find(retired.begin(), retired.end(), texture) != retired.end() &&
              frame->data.uploads.front().pixels[0] == std::byte{255},
          "retirement or frame lifetime");
    Check(play.Stop() && play.Start(0.1, [](runtime::World &, double) { return true; }),
          "fresh session after subtree deletion");
    Check(module.Load(*play.PlayWorld(), Loader, {}, binding), "fresh scene metadata");
    module.Unload();
    Check(stopped && destroyed, "Stop/Destroy lost services or World");
    NexoraTwoDApiV1 unavailable{};
    unavailable.struct_size = sizeof(unavailable);
    Check(host.get_twod_api(host.context, &unavailable) == NEXORA_GAMEPLAY_ERROR_LIFECYCLE &&
              !unavailable.context,
          "unbound getter returned a bridge");
    Check(module.Load(*play.PlayWorld(), PlainLoader), "3D-only module advertised 2D");
    module.Unload();
    Check(play.Stop() && document.CaptureRuntimeScene() == baseline && authoring.FindEntity(child),
          "authoring scene changed");
  }
  Check(document.Undo() && !authoring.FindEntity(child) && document.Redo() &&
            document.CaptureRuntimeScene() == baseline,
        "source Undo/Redo history changed");
  binding.assets.reset();
#if defined(NEXORA_PLAY_2D_NATIVE)
  Check(surface.surface->Diagnostics().nativeUiRejectedTextures == 0, "native stale texture");
  XCloseDisplay(display);
  Check(surface.surface->DrainAndDestroy() == Nexora::Presentation::SurfaceStatus::Ready,
        "native drain");
#endif
}
} // namespace
int main(int argc, char **argv) {
  const auto root = std::filesystem::temp_directory_path() /
                    ("nexora-editor-play-2d-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  try {
    Run(root, argc > 1 ? argv[1] : std::filesystem::path{});
    std::filesystem::remove_all(root);
    std::cout << "Editor Play 2D: 32 real clone lifetimes, project freeze, descendant cleanup and "
                 "3D isolation passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::filesystem::remove_all(root);
    std::cerr << error.what() << '\n';
    return 1;
  }
}
