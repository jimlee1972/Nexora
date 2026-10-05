#include "EditorImGuiTestAccess.h"
#include "Nexora/EditorImGui/EditorImGui.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <chrono>
#include <iostream>
#include <thread>
#include <utility>
#include <vector>

namespace {
void Frames(nexora::editor::imgui::EditorImGuiHost &host,
            Nexora::Presentation::RenderSurface &surface, nexora::editor::ProductShell &shell,
            float scale, std::uint64_t texture_id = 0, unsigned frame_goal = 8) {
  using Nexora::Presentation::SurfaceStatus;
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  unsigned presented = 0;
  while (presented < frame_goal) {
    assert(std::chrono::steady_clock::now() < deadline);
    const auto acquired = surface.BeginFrame();
    if (acquired != SurfaceStatus::Ready) {
      const auto action = Nexora::Presentation::RecoveryAction(acquired);
      assert(action == Nexora::Presentation::SurfaceAction::Suspend ||
             action == Nexora::Presentation::SurfaceAction::RecreateSurface);
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
      continue;
    }
    const auto extent = surface.FrameInfo();
    host.SetDisplay(static_cast<float>(extent.width) / scale,
                    static_cast<float>(extent.height) / scale, scale);
    host.ProcessEvents(surface.Events());
    host.BeginFrame();
    host.DrawProductShell(shell);
    static_cast<void>(host.EndFrame());
    if (texture_id != 0)
      static_cast<void>(
          nexora::editor::imgui::EditorImGuiTestAccess::OverrideDrawTexture(host, texture_id));
    assert(host.Render(surface, extent.width, extent.height) == SurfaceStatus::Ready);
    assert(surface.Diagnostics().nativeUiRejectedTextures == 0);
    const auto status = surface.EndFrame();
    if (status == SurfaceStatus::Ready)
      ++presented;
    else {
      const auto action = Nexora::Presentation::RecoveryAction(status);
      assert(action == Nexora::Presentation::SurfaceAction::Suspend ||
             action == Nexora::Presentation::SurfaceAction::RecreateSurface);
    }
  }
}

void NativeImages() {
  using namespace nexora::editor::imgui;
  using namespace Nexora::Presentation;
  EditorImGuiHost host;
  nexora::editor::ProductShell shell;
  std::array pixels{std::byte{255}, std::byte{0}, std::byte{0}, std::byte{255}};
  const auto red = pixels;
  assert(host.RegisterNativeTexture(0, 1, pixels) == 0);
  assert(host.RegisterNativeTexture(1025, 1, pixels) == 0);
  assert(host.RegisterNativeTexture(1, 2, pixels) == 0);
  auto image = host.RegisterNativeTexture(1, 1, pixels);
  assert(image != 0);
  pixels[0] = std::byte{0};
  pixels[2] = std::byte{255};
  const auto owned = EditorImGuiTestAccess::NativeTexturePixels(host, image);
  assert(std::equal(owned.begin(), owned.end(), red.begin(), red.end()));
  auto created = CreateRenderSurface({"Editor native images", 960, 640});
  assert(created);
  Frames(host, *created.surface, shell, 1.0F, image);
  assert(created.surface->Diagnostics().nativeUiTextureUploads == 2);
  assert(created.surface->Diagnostics().nativeUiDrawCalls > 0);
  assert(created.surface->Diagnostics().nativeUiRejectedTextures == 0);
  assert(host.UnregisterTexture(image));
  const auto stale = image;
  image = host.RegisterNativeTexture(1, 1, pixels);
  assert(image != 0 && image != stale && !host.UnregisterTexture(stale));
  assert(EditorImGuiTestAccess::NativeTexturePixels(host, stale).empty());
  const auto rejected = host.GetRendererMetrics().rejected_textures;
  Frames(host, *created.surface, shell, 1.0F, stale);
  assert(host.GetRendererMetrics().rejected_textures > rejected);
  Frames(host, *created.surface, shell, 1.0F, image);
  assert(created.surface->Diagnostics().nativeUiTextureUploads == 3);
  assert(created.surface->Diagnostics().nativeUiRejectedTextures == 0);
  assert(created.surface->DrainAndDestroy() == SurfaceStatus::Ready);

  // A new native cache needs both font and live images, at unchanged DPI.
  created = CreateRenderSurface({"Editor native image replacement", 960, 640});
  assert(created);
  Frames(host, *created.surface, shell, 1.0F, image);
  assert(created.surface->Diagnostics().nativeUiTextureUploads == 2);
  Frames(host, *created.surface, shell, 1.5F, image);
  assert(created.surface->Diagnostics().nativeUiTextureUploads == 3);
  assert(created.surface->Resize(1024, 720) == Nexora::Window::WindowError::None);
  Frames(host, *created.surface, shell, 1.5F, image);
  assert(created.surface->Diagnostics().nativeUiTextureUploads == 3);

  std::array<std::uint64_t, 64> images{};
  images[0] = image;
  for (std::size_t i = 1; i < images.size(); ++i) {
    images[i] = host.RegisterNativeTexture(1, 1, pixels);
    assert(images[i] != 0);
  }
  assert(host.RegisterNativeTexture(1, 1, pixels) == 0);
  Frames(host, *created.surface, shell, 1.5F, images[0], 1);
  // More replacements than DX12's 4096-entry heap: safe descriptor recycling is mandatory.
  for (unsigned batch = 0; batch < 66; ++batch) {
    for (auto &id : images) {
      const auto old = id;
      assert(host.UnregisterTexture(old));
      id = host.RegisterNativeTexture(1, 1, pixels);
      assert(id != 0 && id != old && !host.UnregisterTexture(old));
    }
    Frames(host, *created.surface, shell, 1.5F, images[0], 1);
  }
  const auto diagnostics = created.surface->Diagnostics();
  assert(diagnostics.nativeUiTextureUploads > 4096);
  assert(diagnostics.nativeUiRejectedTextures == 0);
  const auto uploads = diagnostics.nativeUiTextureUploads;
  Frames(host, *created.surface, shell, 1.5F, images[0]);
  assert(created.surface->Diagnostics().nativeUiTextureUploads == uploads);
  assert(created.surface->DrainAndDestroy() == SurfaceStatus::Ready);
  for (const auto id : images)
    assert(host.UnregisterTexture(id));

  // The byte budget is independent of the slot budget and released on unregister.
  const std::vector<std::byte> large(1024U * 1024U * 4U, std::byte{255});
  std::array<std::uint64_t, 4> large_images{};
  for (auto &id : large_images) {
    id = host.RegisterNativeTexture(1024, 1024, large);
    assert(id != 0);
  }
  assert(host.RegisterNativeTexture(1, 1, pixels) == 0);
  assert(host.UnregisterTexture(large_images[0]));
  assert(host.RegisterNativeTexture(1, 1, pixels) != 0);
  std::cout << "native images: copied pixels, stale fallback, owner/DPI/resize, "
            << diagnostics.nativeUiTextureUploads << " uploads, bounded slots/bytes passed"
            << std::endl;
}
} // namespace

int main() {
  using namespace Nexora::Presentation;
  nexora::editor::imgui::EditorImGuiHost host;
  nexora::editor::ProductShell shell;
  std::uint64_t previous_domain = 0;
  for (unsigned replacement = 0; replacement < 2; ++replacement) {
    auto created = CreateRenderSurface({"Editor native surface lifetime", 960, 640});
    if (!created) {
      std::cerr << created.reason << '\n';
#if defined(__APPLE__)
      // Metal needs a supported native host; an unavailable runner is not acceptance evidence.
      return created.status == SurfaceStatus::Unsupported ? 77 : 1;
#else
      return 1;
#endif
    }
    const auto domain = created.surface->UiResourceDomain();
    assert(domain != 0 && domain != previous_domain);
    previous_domain = domain;
    Frames(host, *created.surface, shell, 1.0F);
    auto diagnostics = created.surface->Diagnostics();
    std::cout << "replacement=" << replacement << " uploads=" << diagnostics.nativeUiTextureUploads
              << " rejected=" << diagnostics.nativeUiRejectedTextures << std::endl;
    assert(diagnostics.nativeUiDrawCalls > 0);
    assert(diagnostics.nativeUiTextureUploads == 1);
    assert(diagnostics.nativeUiRejectedTextures == 0);
    // Moving the owner preserves its cache identity and does not upload the same atlas again.
    RenderSurface moved(std::move(*created.surface));
    assert(created.surface->UiResourceDomain() == 0 && moved.UiResourceDomain() == domain);
    Frames(host, moved, shell, 1.0F);
    assert(moved.Diagnostics().nativeUiTextureUploads == 1);
    // Repeated monitor-scale round trips update one atlas per bucket; steady frames reuse it.
    std::uint64_t uploads = 1;
    for (const auto scale : std::array{1.25F, 1.5F, 2.0F, 1.0F}) {
      Frames(host, moved, shell, scale);
      diagnostics = moved.Diagnostics();
      assert(diagnostics.nativeUiTextureUploads == ++uploads);
      assert(diagnostics.nativeUiRejectedTextures == 0);
    }
    const auto steady_allocations = moved.Diagnostics().nativeUiBufferReallocations;
    Frames(host, moved, shell, 1.0F);
    assert(moved.Diagnostics().nativeUiBufferReallocations == steady_allocations);
    assert(moved.Resize(1024, 720) == Nexora::Window::WindowError::None);
    Frames(host, moved, shell, 1.0F);
    assert(moved.UiResourceDomain() == domain &&
           moved.Diagnostics().nativeUiTextureUploads == uploads);
    assert(moved.DrainAndDestroy() == SurfaceStatus::Ready);
    assert(moved.UiResourceDomain() == 0);
    // A destroyed target is rejected without dereferencing native GPU resources.
    assert(host.Render(moved, 1024, 720) == SurfaceStatus::InvalidDescriptor);
    std::cout << "DPI/move/resize/teardown passed for domain " << domain << std::endl;
  }
  NativeImages();
}
