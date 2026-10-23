#include "Nexora/TwoD/GameplayUi.h"
#include "Nexora/TwoD/Scene.h"
#include "Nexora/TwoD/Tilemap.h"
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#undef None
#include <chrono>
#include <fstream>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <thread>
using namespace nexora;
using namespace Nexora::Presentation;
namespace {
void Check(bool v, const char *m) {
  if (!v)
    throw std::runtime_error(m);
}
unsigned Channel(unsigned long p, unsigned long mask) {
  while (!(mask & 1)) {
    mask >>= 1;
    p >>= 1;
  }
  return unsigned((p & mask) * 255 / mask);
}
std::shared_ptr<const twod::Atlas> Solid(twod::AssetId id, std::array<unsigned, 4> color,
                                         bool nearest) {
  twod::Image image{1, 2, 2, {}};
  for (unsigned i = 0; i < 4; ++i)
    for (auto c : color)
      image.rgba.push_back(std::byte(c));
  auto a = twod::PackAtlas(id, std::span(&image, 1), 8, 8, nearest);
  Check(bool(a), "atlas fixture");
  return std::make_shared<const twod::Atlas>(std::move(*a));
}
void WaitPixels(Display *display, ::Window window, const std::function<bool(XImage *)> &predicate,
                const char *reason) {
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
  do {
    XSync(display, False);
    auto *image = XGetImage(display, window, 0, 0, 320, 240, AllPlanes, ZPixmap);
    Check(image != nullptr, "X11 readback");
    const bool valid = predicate(image);
    XDestroyImage(image);
    if (valid)
      return;
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
  } while (std::chrono::steady_clock::now() < deadline);
  throw std::runtime_error(reason);
}
void Pixels(Display *display, ::Window window, const char *path) {
  for (unsigned retry = 0; retry < 100; ++retry) {
    XSync(display, False);
    auto *image = XGetImage(display, window, 0, 0, 320, 240, AllPlanes, ZPixmap);
    Check(image != nullptr, "X11 readback");
    const auto p = XGetPixel(image, 160, 120);
    const auto r = Channel(p, image->red_mask), g = Channel(p, image->green_mask),
               b = Channel(p, image->blue_mask);
    // Opaque red -> half-alpha green texture -> half-alpha red tint: linear (0.75,0.25,0).
    // Swapchain attachments can encode sRGB or retain UNORM.
    const auto hud = XGetPixel(image, 10, 10);
    const bool linear = r >= 190 && r <= 192 && g >= 63 && g <= 65;
    const bool encoded = r >= 224 && r <= 226 && g >= 136 && g <= 138;
    const bool valid = (linear || encoded) && b < 3 && Channel(hud, image->blue_mask) > 250;
    if (valid) {
      std::ofstream f(path, std::ios::binary);
      f << "P6\n320 240\n255\n";
      for (unsigned y = 0; y < 240; ++y)
        for (unsigned x = 0; x < 320; ++x) {
          auto px = XGetPixel(image, x, y);
          const char rgb[]{char(Channel(px, image->red_mask)), char(Channel(px, image->green_mask)),
                           char(Channel(px, image->blue_mask))};
          f.write(rgb, 3);
        }
      XDestroyImage(image);
      Check(bool(f), "capture write");
      return;
    }
    XDestroyImage(image);
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }
  throw std::runtime_error("native sprite alpha/order/HUD pixel mismatch");
}
} // namespace
int main(int argc, char **argv) {
  try {
    Check(argc == 2, "capture path required");
    auto windows = Nexora::Window::CreateWindowSystem();
    Check(bool(windows), "window system");
    auto created = windows->Create({"Nexora 2D acceptance", 320, 240, true, true});
    Check(bool(created), "window create");
    SurfaceDescriptor descriptor;
    descriptor.window = created.handle;
    descriptor.width = 320;
    descriptor.height = 240;
    descriptor.backend = SurfaceBackend::Vulkan;
    descriptor.enableGpuTiming = true;
    auto surface = CreateSurface(descriptor, *windows);
    Check(bool(surface), "native Vulkan surface");
    auto red = Solid({1, 1}, {255, 0, 0, 255}, true),
         green = Solid({2, 2}, {0, 255, 0, 128}, false),
         blue = Solid({3, 3}, {0, 0, 255, 255}, true);
    runtime::World world;
    auto scene = world.LoadScene("2D pixels");
    twod::SpriteScene sprites(world, scene);
    twod::Sprite s;
    s.atlas = red->id;
    s.region = 1;
    s.size = {4, 4};
    Check(sprites.Set(world.CreateEntity(scene).id, s), "red");
    // A real multi-instance batch before two cross-atlas transparent batches.
    for (unsigned i = 1; i < 256; ++i) {
      const auto id = world.CreateEntity(scene).id;
      Check(sprites.Set(id, s), "opaque instance");
      if (i == 255) {
        runtime::WorldCommandBuffer commands;
        commands.SetTransform(id, {4.5, 0, 0});
        Check(commands.Apply(world), "last instance transform");
      }
    }
    s.atlas = green->id;
    Check(sprites.Set(world.CreateEntity(scene).id, s), "green");
    s.atlas = red->id;
    s.color[3] = 0.5F;
    Check(sprites.Set(world.CreateEntity(scene).id, s), "alpha red");
    twod::Camera camera;
    camera.width = 320;
    camera.height = 240;
    camera.verticalSize = 10;
    auto frame = sprites.Extract(camera, [&](auto id) -> std::optional<twod::AtlasBinding> {
      return id == red->id ? twod::AtlasBinding{red, 101} : twod::AtlasBinding{green, 102};
    });
    Check(frame && frame->batches.size() == 3, "transparent cross-atlas batches");
    auto uploads = frame->Uploads();
    const std::array<UiVertex, 4> hv{{{{0, 0}, {0, 0}, UINT32_MAX},
                                      {{30, 0}, {1, 0}, UINT32_MAX},
                                      {{30, 30}, {1, 1}, UINT32_MAX},
                                      {{0, 30}, {0, 1}, UINT32_MAX}}};
    const std::array<std::uint16_t, 6> hi{0, 1, 2, 0, 2, 3};
    const UiDrawCommand hc{0, 0, 320, 240, 103, 6, 0, 0};
    const UiTextureUpload hu{103,        blue->width, blue->height, blue->width * 4,
                             blue->rgba, true,        true};
    auto packet = PrepareSpriteDraws(
        {frame->vertices, std::as_bytes(std::span(frame->indices)), frame->batches, uploads, true},
        {hv, std::as_bytes(std::span(hi)), std::span(&hc, 1), std::span(&hu, 1), false}, 320, 240,
        true);
    Check(bool(packet) && packet->spriteInstances.size() == 258 && packet->vertices.size() == 8 &&
              packet->indices.size() == 12 && packet->commands[1].instanceCount == 256 &&
              packet->commands[2].firstInstance == 256,
          "compact instance packet plus legacy HUD");
    auto malformed = *packet;
    malformed.commands[1].instanceCount = UINT32_MAX;
    Check(surface->RenderUi(malformed.View()) == SurfaceStatus::InvalidDescriptor,
          "malformed instances before acquire");
    const auto render = [&] {
      static_cast<void>(windows->PumpEvents());
      Check(surface->Acquire() == SurfaceStatus::Ready, "acquire");
      const auto uploadsBefore = surface->Diagnostics().nativeUiTextureUploads;
      Check(surface->RenderUi(malformed.View()) == SurfaceStatus::InvalidDescriptor &&
                surface->Diagnostics().nativeUiTextureUploads == uploadsBefore,
            "invalid instance range must not upload or consume an acquired frame");
      Check(surface->ReleaseSpriteTextures(std::span<const std::uint64_t>{}) ==
                SurfaceStatus::InvalidDescriptor,
            "retirement during frame");
      Check(surface->RenderUi(packet->View()) == SurfaceStatus::Ready, "sprite native triangles");
      Check(surface->Present() == SurfaceStatus::Ready, "present");
    };
    for (unsigned i = 0; i < 8; ++i)
      render();
    auto diagnostics = surface->Diagnostics();
    Check(diagnostics.nativeUiTextureUploads == 4 && diagnostics.nativeUiRejectedTextures == 0 &&
              diagnostics.nativeUiDrawCalls == 40,
          "immutable atlas upload/batches");
    Check(diagnostics.spriteInstancedDrawCalls == 24 && diagnostics.spriteGpuInstances == 8 * 258 &&
              diagnostics.spriteInstanceUploadBytes == 8 * 258 * sizeof(SpriteGpuInstance),
          "native instanced draws and exact compact upload bytes");
    auto *display = XOpenDisplay(nullptr);
    Check(display != nullptr, "display");
    const auto native = reinterpret_cast<::Window>(windows->NativeHandle(created.handle));
    Pixels(display, native, argv[1]);
    WaitPixels(
        display, native,
        [&](XImage *image) {
          const auto last = XGetPixel(image, 268, 120);
          return Channel(last, image->red_mask) > 250 && Channel(last, image->green_mask) < 3 &&
                 Channel(last, image->blue_mask) < 3;
        },
        "last GPU instance in the batch must use its own transform");
    const std::uint64_t release = 101;
    Check(surface->ReleaseSpriteTextures(std::span(&release, 1)) == SurfaceStatus::Ready,
          "retire generation");
    render();
    Check(surface->Diagnostics().nativeUiTextureUploads == 5, "generation reload");
    auto invalid = packet->textureUploads;
    invalid[1].nearest = !invalid[1].nearest;
    auto view = packet->View();
    view.textureUploads = invalid;
    Check(surface->Acquire() == SurfaceStatus::Ready, "invalid fixture acquire");
    Check(surface->RenderUi(view) == SurfaceStatus::InvalidDescriptor,
          "resident sampler alias accepted");
    Check(surface->Present() == SurfaceStatus::Ready, "invalid fixture present");
    twod::Image checker{1, 2, 2, {}};
    for (const auto &color : std::array<std::array<unsigned, 4>, 4>{
             {{255, 0, 0, 255}, {0, 255, 0, 255}, {0, 0, 255, 255}, {255, 255, 255, 255}}})
      for (auto channel : color)
        checker.rgba.push_back(std::byte(channel));
    auto checkerAtlas = twod::PackAtlas({4, 4}, std::span(&checker, 1), 8, 8, true);
    Check(bool(checkerAtlas), "checker atlas");
    auto checkerOwner = std::make_shared<const twod::Atlas>(*checkerAtlas);
    runtime::World checkerWorld;
    auto checkerSceneId = checkerWorld.LoadScene("UV pivot flip");
    twod::SpriteScene checkerScene(checkerWorld, checkerSceneId);
    const auto checkerEntity = checkerWorld.CreateEntity(checkerSceneId).id;
    twod::Sprite checkerSprite;
    checkerSprite.atlas = checkerAtlas->id;
    checkerSprite.region = 1;
    checkerSprite.size = {4, 4};
    checkerSprite.pivot = {0, 0};
    for (unsigned flips = 0; flips < 4; ++flips) {
      checkerSprite.flipX = (flips & 1) != 0;
      checkerSprite.flipY = (flips & 2) != 0;
      Check(checkerScene.Set(checkerEntity, checkerSprite), "checker sprite");
      auto cf = checkerScene.Extract(camera, [&](auto) -> std::optional<twod::AtlasBinding> {
        return twod::AtlasBinding{checkerOwner, 104};
      });
      Check(bool(cf), "checker extraction");
      auto cu = cf->Uploads();
      auto cp = PrepareSpriteDraws(
          {cf->vertices, std::as_bytes(std::span(cf->indices)), cf->batches, cu, true}, {}, 320,
          240, true);
      Check(bool(cp), "checker packet");
      for (unsigned i = 0; i < 3; ++i) {
        Check(surface->Acquire() == SurfaceStatus::Ready, "checker acquire");
        Check(surface->RenderUi(cp->View()) == SurfaceStatus::Ready, "checker render");
        Check(surface->Present() == SurfaceStatus::Ready, "checker present");
      }
      WaitPixels(
          display, native,
          [&](XImage *image) {
            const auto pixel = XGetPixel(image, 200, 40), outside = XGetPixel(image, 136, 96);
            const std::array<unsigned, 3> actual{Channel(pixel, image->red_mask),
                                                 Channel(pixel, image->green_mask),
                                                 Channel(pixel, image->blue_mask)};
            const std::array<std::array<unsigned, 3>, 4> expected{
                {{255, 0, 0}, {0, 255, 0}, {0, 0, 255}, {255, 255, 255}}};
            bool correct = true;
            for (unsigned c = 0; c < 3; ++c)
              correct &= std::abs(int(actual[c]) - int(expected[flips][c])) <= 2;
            correct &= Channel(outside, image->red_mask) < 150 &&
                       Channel(outside, image->green_mask) < 150;
            return correct;
          },
          "native UV, nearest, pivot or flip mismatch");
    }
    Check(surface->Diagnostics().nativeUiTextureUploads == 6, "nearest generation resident");
    auto linearAtlas = *checkerAtlas;
    linearAtlas.id = {5, 5};
    linearAtlas.nearest = false;
    auto linearOwner = std::make_shared<const twod::Atlas>(linearAtlas);
    checkerSprite.atlas = linearAtlas.id;
    checkerSprite.flipX = checkerSprite.flipY = false;
    Check(checkerScene.Set(checkerEntity, checkerSprite), "linear sprite");
    auto lf = checkerScene.Extract(camera, [&](auto) -> std::optional<twod::AtlasBinding> {
      return twod::AtlasBinding{linearOwner, 105};
    });
    Check(bool(lf), "linear extraction");
    auto lu = lf->Uploads();
    auto lp = PrepareSpriteDraws(
        {lf->vertices, std::as_bytes(std::span(lf->indices)), lf->batches, lu, true}, {}, 320, 240,
        true);
    Check(bool(lp), "linear packet");
    for (unsigned i = 0; i < 3; ++i) {
      Check(surface->Acquire() == SurfaceStatus::Ready, "linear acquire");
      Check(surface->RenderUi(lp->View()) == SurfaceStatus::Ready, "linear render");
      Check(surface->Present() == SurfaceStatus::Ready, "linear present");
    }
    WaitPixels(
        display, native,
        [](XImage *image) {
          const auto pixel = XGetPixel(image, 200, 40);
          const auto red = Channel(pixel, image->red_mask),
                     green = Channel(pixel, image->green_mask),
                     blue = Channel(pixel, image->blue_mask);
          return red > 100 && red < 240 && green > 60 && green < 200 && blue < 3;
        },
        "linear sampling or atlas gutter bleed");
    twod::Tilemap tiles;
    tiles.atlas = checkerAtlas->id;
    const std::array cells{twod::Tile{-1, 0, 0, 1, true}, twod::Tile{0, 0, 0, 1, true}};
    Check(tiles.Paint(cells), "native tiles");
    for (bool loaded : {true, false, true}) {
      Check(tiles.SetChunkLoaded({-1, 0}, loaded), "tile chunk state");
      auto reopened = twod::Tilemap::Decode(tiles.Encode());
      Check(bool(reopened), "tile state reopen");
      auto tf = reopened->Extract(camera, [&](auto) -> std::optional<twod::AtlasBinding> {
        return twod::AtlasBinding{checkerOwner, 104};
      });
      Check(tf && tf->statistics.visible == (loaded ? 2U : 1U), "tile visible count");
      auto tu = tf->Uploads();
      auto tp = PrepareSpriteDraws(
          {tf->vertices, std::as_bytes(std::span(tf->indices)), tf->batches, tu, true}, {}, 320,
          240, true);
      Check(bool(tp), "tile packet");
      for (unsigned i = 0; i < 3; ++i) {
        Check(surface->Acquire() == SurfaceStatus::Ready, "tile acquire");
        Check(surface->RenderUi(tp->View()) == SurfaceStatus::Ready, "tile draw");
        Check(surface->Present() == SurfaceStatus::Ready, "tile present");
      }
      WaitPixels(
          display, native,
          [&](XImage *image) {
            const auto left = XGetPixel(image, 143, 103), right = XGetPixel(image, 167, 103);
            return (Channel(left, image->red_mask) > 250) == loaded &&
                   Channel(right, image->red_mask) > 250;
          },
          "tile chunk boundary render");
      auto shapes = reopened->CollisionShapes();
      twod::SpatialIndex index;
      Check(shapes && index.Build(*shapes), "tile query build");
      twod::Shape query;
      query.center = {-0.5F, 0.5F};
      query.radius = 0.1F;
      Check(index.Query(query).size() == (loaded ? 1U : 0U), "tile chunk boundary query");
    }
    // Exercise the public image service through the real native UI texture pipeline.
    twod::GameplayUi imageUi;
    NexoraGameplayUiApiV1 uiApi{};
    uiApi.struct_size = sizeof(uiApi);
    NexoraGameplayUiDrawApiV1 drawApi{};
    drawApi.struct_size = sizeof(drawApi);
    drawApi.abi_version = 1;
    Check(imageUi.GetApi(&uiApi) == 0 && imageUi.GetDrawApi(&drawApi) == 0, "image UI tables");
    std::array<std::uint8_t, 8 * 8 * 4> imagePixels{};
    for (unsigned y = 0; y < 8; ++y)
      for (unsigned x = 0; x < 8; ++x) {
        const bool border = x < 2 || y < 2 || x >= 6 || y >= 6;
        const auto offset = (y * 8 + x) * 4;
        imagePixels[offset] = border ? 0 : 255;
        imagePixels[offset + 2] = border ? 255 : 0;
        imagePixels[offset + 3] = 255;
      }
    NexoraUiImageV1 imageDescriptor{sizeof(imageDescriptor), 8, 8, 0, 500, imagePixels.data(),
                                    imagePixels.size()};
    std::uint64_t imageHandle{};
    Check(drawApi.register_image(drawApi.context, &imageDescriptor, &imageHandle) == 0,
          "native image register");
    imageUi.BeginInput({});
    Check(uiApi.begin_frame(uiApi.context, 320, 240) == 0, "native image begin");
    NexoraUiImageDrawV1 imageDraw{sizeof(imageDraw), UINT32_MAX,         imageHandle,
                                  {0, 0, 8, 8},      {20, 20, 120, 100}, {24, 24, 100, 80}};
    Check(drawApi.nine_slice(drawApi.context, &imageDraw, {2, 2, 2, 2}, {8, 8, 8, 8}) == 0,
          "native nine-slice");
    Check(uiApi.end_frame(uiApi.context) == 0, "native image end");
    auto imageFrame = imageUi.Frame();
    for (unsigned i = 0; i < 3; ++i) {
      Check(surface->Acquire() == SurfaceStatus::Ready, "native image acquire");
      Check(surface->RenderUi(imageFrame.DrawData()) == SurfaceStatus::Ready, "native image draw");
      Check(surface->Present() == SurfaceStatus::Ready, "native image present");
    }
    WaitPixels(
        display, native,
        [](XImage *image) {
          const auto corner = XGetPixel(image, 25, 25), center = XGetPixel(image, 70, 60),
                     outside = XGetPixel(image, 21, 21);
          return Channel(corner, image->blue_mask) > 240 &&
                 Channel(center, image->red_mask) > 250 && Channel(center, image->blue_mask) < 3 &&
                 Channel(outside, image->blue_mask) < 100 &&
                 Channel(outside, image->red_mask) < 100;
        },
        "native image nine-slice UV or clip mismatch");
    Check(drawApi.remove_image(drawApi.context, imageHandle) == 0, "native image remove");
    imageUi.BeginInput({});
    Check(imageUi.CollectRetiredTextures().empty(), "retained frame blocks native retirement");
    imageFrame = {};
    const auto imageRetired = imageUi.CollectRetiredTextures();
    Check(imageRetired.size() == 1 &&
              surface->ReleaseSpriteTextures(imageRetired) == SurfaceStatus::Ready,
          "native image GPU retirement");
    Check(surface->DrainAndDestroy() == SurfaceStatus::Ready, "shutdown");
    Check(windows->Destroy(created.handle) == Nexora::Window::WindowError::None, "window shutdown");
    XCloseDisplay(display);
    std::cout
        << "Native 2D alpha/order/HUD, sampler, atlas and image nine-slice retirement passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
