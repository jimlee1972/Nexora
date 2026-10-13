#include "EditorImGuiTestAccess.h"

#include <array>
#include <iostream>
#include <stdexcept>

namespace {
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
void Run(float dpi) {
  using namespace nexora::editor;
  using namespace std::chrono_literals;
  using Access = imgui::EditorImGuiTestAccess;
  ProfileSession profile{3};
  ProfileSession *active = &profile;
  imgui::EditorImGuiHost host;
  host.SetDisplay(1280, 1800, dpi);
  Access::ConfigureSyntheticInput(host);
  Nexora::Window::WindowEvent focus;
  focus.type = Nexora::Window::WindowEventType::FocusChanged;
  focus.value0 = 1;
  host.ProcessEvents(std::array{focus});
  ProductShell shell;
  const auto draw = [&] {
    const auto attempts = profile.MeshMemory().attempts;
    host.BeginFrame();
    host.DrawProductShell(shell, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                          active);
    const auto result = host.EndFrame();
    Require(result.vertices, "mesh-memory widgets did not produce draw data");
    // Clear legitimately resets the counter; drawing must never add an observation.
    Require(profile.MeshMemory().attempts <= attempts, "widgets attempted a memory observation");
  };
  draw();
  Access::FocusProfiler(host);
  draw();
  draw();
  Require(!Access::ProfileMeshMemory(host).native_allocation_bytes,
          "unsampled native allocation value fabricated");
  const auto start = std::chrono::steady_clock::time_point{};
  Require(profile.ObserveMeshMemory(7, start, 65536, 192), "owner sample rejected");
  draw();
  const auto copied = Access::ProfileMeshMemory(host);
  Require(copied.surface_domain == 7 && copied.native_allocation_bytes == 65536 &&
              copied.cpu_key_payload_bytes == 192,
          "Profiler did not copy native memory observation");
  Require(profile.ObserveMeshMemory(7, start + 250ms, 131072, 384) &&
              Access::ProfileMeshMemory(host).native_allocation_bytes ==
                  copied.native_allocation_bytes,
          "host retained a mutable observation borrow");
  draw();
  Require(Access::ProfileMeshMemory(host).native_allocation_bytes == 131072 &&
              Access::ProfileMeshMemory(host).observed_peak_cpu_bytes == 384,
          "next widget frame did not publish copied metrics");
  const auto click = [&](bool clear) {
    const auto point =
        clear ? Access::ProfileClearPosition(host) : Access::ProfileCapturePosition(host);
    Require(point.has_value(), "Profiler control unavailable");
    Nexora::Window::WindowEvent pointer, button;
    pointer.type = Nexora::Window::WindowEventType::Pointer;
    pointer.value0 = static_cast<int>((*point)[0] * dpi);
    pointer.value1 = static_cast<int>((*point)[1] * dpi);
    button.type = Nexora::Window::WindowEventType::PointerButton;
    button.value0 = 0;
    button.value1 = 1;
    host.ProcessEvents(std::array{pointer, button});
    draw();
    button.value1 = 0;
    host.ProcessEvents(std::array{button});
    draw();
  };
  click(false);
  Require(!profile.Capturing() && !profile.ObserveMeshMemory(7, start + 500ms, 262144, 768),
          "actual Capture control did not pause mesh observations");
  click(true);
  Require(!profile.Capturing() && profile.MeshMemorySamples().empty() &&
              !Access::ProfileMeshMemory(host).native_allocation_bytes &&
              !Access::ProfileMeshMemory(host).observed_peak_native_bytes,
          "actual Clear resumed capture or retained native measurements");
  click(false);
  Require(profile.Capturing() && profile.ObserveMeshMemory(7, start + 750ms, 0, 0),
          "actual Resume or valid zero allocation failed");
  draw();
  Require(Access::ProfileMeshMemory(host).native_allocation_bytes == 0,
          "zero allocation displayed as unavailable");
  Require(profile.ObserveMeshMemory(7, start + 1000ms, std::nullopt, std::nullopt),
          "unavailable owner observation rejected");
  draw();
  Require(!Access::ProfileMeshMemory(host).native_allocation_bytes &&
              Access::ProfileMeshMemory(host).observed_peak_native_bytes == 0,
          "unavailable draw reused old success or discarded observed peak");
  Require(profile.ObserveMeshMemory(8, start + 1250ms, 4096, 48), "new owner sample rejected");
  draw();
  Require(Access::ProfileMeshMemory(host).surface_domain == 8 &&
              Access::ProfileMeshMemory(host).observed_peak_native_bytes == 4096 &&
              profile.MeshMemorySamples().size() == 1,
          "new owner retained another surface's observations");
  active = nullptr;
  draw();
  Require(!Access::ProfileMeshMemory(host).native_allocation_bytes &&
              !Access::ProfileMeshMemory(host).surface_domain,
          "detached profile retained a copied old observation");
}
} // namespace
int main() {
  try {
    Run(1);
    Run(2);
    std::cout << "PASS: actual 1x/2x mesh-memory widgets, capture/clear, ownership and unavailable "
                 "states\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
