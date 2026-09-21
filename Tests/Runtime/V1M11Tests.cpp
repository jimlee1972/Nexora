#include "Nexora/Runtime/Platform.h"

#include <chrono>
#include <iostream>
#include <stdexcept>

namespace platform = nexora::runtime::platform;
namespace {
void Require(bool condition, const char *message) {
  if (!condition)
    throw std::runtime_error(message);
}

void PlatformServices() {
  platform::Permission requested{};
  platform::HapticKind haptic{};
  std::string clipboard;
  platform::Runtime runtime{{
      [&](platform::Permission value) { requested = value; },
      [&](platform::HapticKind value) { haptic = value; },
      [&](std::string_view value) {
        clipboard = value;
        return true;
      },
      [&] { return clipboard; },
      [](std::string_view payload) { return payload == "score=42"; },
      [](std::string_view provider) { return provider == "test"; },
  }};

  Require(runtime.Transition(platform::AppState::Background) &&
              runtime.Transition(platform::AppState::Foreground) &&
              !runtime.Transition(platform::AppState::Foreground),
          "lifecycle transition contract failed");
  Require(runtime.SetSafeArea({1, 2, 3, 4}) && !runtime.SetSafeArea({-1, 0, 0, 0}),
          "safe area validation failed");
  runtime.SetOrientation(platform::Orientation::LandscapeLeft);
  runtime.SetPressure(platform::ThermalState::Critical, platform::MemoryPressure::Warning);
  Require(runtime.CurrentPolicy().reduce_quality && runtime.CurrentPolicy().pause_background_work &&
              runtime.CurrentPolicy().release_caches,
          "pressure policy failed");
  Require(
      runtime.RequestPermission(platform::Permission::Camera) &&
          requested == platform::Permission::Camera &&
          runtime.SetPermission(platform::Permission::Camera, platform::PermissionState::Granted) &&
          runtime.PermissionStatus(platform::Permission::Camera) ==
              platform::PermissionState::Granted,
      "permission bridge failed");
  Require(runtime.Haptic(platform::HapticKind::Success) &&
              haptic == platform::HapticKind::Success && runtime.SetClipboard("hello") &&
              runtime.Clipboard() == "hello" && runtime.Share("score=42") && runtime.Login("test"),
          "native service bridge failed");
  Require(runtime.OpenDeepLink("nexora://join/42") && !runtime.OpenDeepLink("not a uri"),
          "deep link validation failed");
  Require(runtime.SetImeComposition("input", 3) && !runtime.SetImeComposition("x", 2) &&
              runtime.CommitIme("輸入") && runtime.ImeComposition().empty() &&
              runtime.CommittedText() == "輸入",
          "IME contract failed");
  runtime.SetGpuWorkarounds({"disable_subpass_merge", "", "disable_subpass_merge"});
  Require(runtime.HasGpuWorkaround("disable_subpass_merge") && !runtime.HasGpuWorkaround("unknown"),
          "GPU workaround registry failed");
}

void WebViewOwnership() {
  std::size_t native_destroyed = 0;
  platform::NativeWebViewHost host{
      [](const platform::WebViewDescriptor &) { return platform::NativeViewHandle{123}; },
      [&](platform::NativeViewHandle handle) { native_destroyed += handle == 123; },
      [](platform::NativeViewHandle handle, std::string_view url) {
        return handle == 123 && url == "https://nexora.dev/next";
      },
      [](platform::NativeViewHandle handle, const platform::PointerEvent &event) {
        return handle == 123 && event.pointer_id == 9;
      }};
  const auto view = host.CreateView({"https://nexora.dev", {0, 0, 320, 240}, false});
  Require(view && host.IsNativeOverlay(*view) && host.NativeViewCount() == 1,
          "native overlay creation failed");
  Require(host.NavigateView(*view, "https://nexora.dev/next") &&
              host.Route(*view, {9, 10, 20, platform::PointerPhase::Down}) &&
              !host.Route(999, {9, 10, 20, platform::PointerPhase::Down}),
          "native WebView routing failed");
  Require(host.DestroyView(*view) && native_destroyed == 1 && host.NativeViewCount() == 0 &&
              host.Stats().created == 1 && host.Stats().destroyed == 1 &&
              host.Stats().routed_pointers == 1 && host.Stats().rejected_operations == 1,
          "native WebView ownership failed");
  Require(!host.CreateView({"bad", {0, 0, 0, 10}, false}), "invalid WebView descriptor accepted");
}

void Baseline() {
  platform::Runtime runtime;
  const auto begin = std::chrono::steady_clock::now();
  for (std::size_t index = 0; index < 10000; ++index) {
    runtime.SetPressure(index % 2 ? platform::ThermalState::Nominal
                                  : platform::ThermalState::Serious,
                        platform::MemoryPressure::Normal);
    runtime.SetOrientation(index % 2 ? platform::Orientation::Portrait
                                     : platform::Orientation::LandscapeRight);
  }
  Require(std::chrono::steady_clock::now() - begin < std::chrono::seconds(2),
          "platform event baseline failed");
}
} // namespace

int main() {
  try {
    PlatformServices();
    WebViewOwnership();
    Baseline();
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
