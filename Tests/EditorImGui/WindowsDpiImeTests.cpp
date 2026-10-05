#include "EditorImGuiTestAccess.h"
#include "Nexora/EditorImGui/EditorImGui.h"

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <imm.h>

#include <array>
#include <cassert>

namespace {
POINT CandidatePosition(HIMC context) {
  CANDIDATEFORM candidate{};
  assert(ImmGetCandidateWindow(context, 0, &candidate));
  assert(candidate.dwStyle == CFS_CANDIDATEPOS);
  return candidate.ptCurrentPos;
}
} // namespace

int main() {
  using nexora::editor::imgui::EditorImGuiHost;
  using nexora::editor::imgui::EditorImGuiTestAccess;
  using namespace Nexora;

  constexpr wchar_t title[] = L"Nexora Editor DPI IME contract";
  auto created = Presentation::CreateRenderSurface(
      {"Nexora Editor DPI IME contract", 640, 360, true, Presentation::SurfaceBackend::Dx12,
       Presentation::PresentMode::Immediate, Presentation::ColorSpace::Srgb});
  assert(created);
  auto window = FindWindowW(L"Nexora.Window", title);
  assert(window);
  DWORD windowProcess = 0;
  GetWindowThreadProcessId(window, &windowProcess);
  assert(windowProcess == GetCurrentProcessId());

  auto inputContext = ImmGetContext(window);
  HIMC ownedInputContext = nullptr;
  HIMC previousInputContext = nullptr;
  if (!inputContext) {
    ownedInputContext = ImmCreateContext();
    assert(ownedInputContext);
    previousInputContext = ImmAssociateContext(window, ownedInputContext);
    inputContext = ImmGetContext(window);
  }
  assert(inputContext);

  {
    EditorImGuiHost host;
    // Repeat monitor-scale round trips, including the fractional 125% and 200% buckets.
    // Observe the actual Win32 candidate window; no installed IME composition is implied.
    constexpr std::array scales{1.0F, 1.25F, 1.5F, 2.0F, 1.5F, 1.25F, 1.0F};
    for (const auto scale : scales) {
      host.SetDisplay(640.0F / scale, 360.0F / scale, scale);
      host.BeginFrame();
      host.UpdateImeCandidate(*created.surface);
      EditorImGuiTestAccess::InvokeImeCallback(host, 40.0F, 60.0F, true);
      auto candidate = CandidatePosition(inputContext);
      assert(candidate.x == static_cast<LONG>(40.0F * scale));
      assert(candidate.y == static_cast<LONG>(60.0F * scale));

      // A hidden candidate or an ended frame cannot move the native IME window.
      EditorImGuiTestAccess::InvokeImeCallback(host, 10.0F, 20.0F, false);
      auto unchanged = CandidatePosition(inputContext);
      assert(unchanged.x == candidate.x && unchanged.y == candidate.y);
      static_cast<void>(host.EndFrame());
      EditorImGuiTestAccess::InvokeImeCallback(host, 80.0F, 80.0F, true);
      unchanged = CandidatePosition(inputContext);
      assert(unchanged.x == candidate.x && unchanged.y == candidate.y);
    }
  }

  ImmReleaseContext(window, inputContext);
  if (ownedInputContext) {
    ImmAssociateContext(window, previousInputContext);
    assert(ImmDestroyContext(ownedInputContext));
  }
  assert(created.surface->DrainAndDestroy() == Presentation::SurfaceStatus::Ready);
}
