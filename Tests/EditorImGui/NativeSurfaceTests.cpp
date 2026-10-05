#include "Nexora/EditorImGui/EditorImGui.h"

#include <array>
#include <cassert>
#include <chrono>
#include <iostream>
#include <thread>
#include <utility>

namespace {
void Frames(nexora::editor::imgui::EditorImGuiHost &host,
            Nexora::Presentation::RenderSurface &surface, nexora::editor::ProductShell &shell,
            float scale) {
  using Nexora::Presentation::SurfaceStatus;
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  unsigned presented = 0;
  while (presented < 8) {
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
    assert(host.Render(surface, extent.width, extent.height) == SurfaceStatus::Ready);
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
}
