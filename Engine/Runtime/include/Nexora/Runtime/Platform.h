#pragma once

#include "Nexora/Runtime/Api.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace nexora::runtime::platform {

using NativeViewHandle = std::uintptr_t;

enum class AppState { Foreground, Background, Suspended, Terminating };
enum class Permission { Camera, Microphone, Photos, Notifications, Location };
enum class PermissionState { Unknown, Denied, Granted, Restricted };
enum class Orientation { Portrait, PortraitUpsideDown, LandscapeLeft, LandscapeRight };
enum class ThermalState { Nominal, Fair, Serious, Critical };
enum class MemoryPressure { Normal, Warning, Critical };
enum class HapticKind { Selection, Success, Warning, Error };
enum class PointerPhase { Down, Move, Up, Cancel };

struct SafeArea final {
  float left{}, top{}, right{}, bottom{};
  friend bool operator==(const SafeArea &, const SafeArea &) = default;
};

struct Policy final {
  bool reduce_quality{};
  bool pause_background_work{};
  bool release_caches{};
};

struct PointerEvent final {
  std::uint64_t pointer_id{};
  float x{}, y{};
  PointerPhase phase{};
};

struct WebViewRect final {
  float x{}, y{}, width{}, height{};
};

struct WebViewDescriptor final {
  std::string initial_url;
  WebViewRect rect;
  bool transparent{};
};

struct WebViewStats final {
  std::size_t created{};
  std::size_t destroyed{};
  std::size_t routed_pointers{};
  std::size_t rejected_operations{};
};

class NEXORA_RUNTIME_API NativeWebViewHost final {
public:
  using Create = std::function<NativeViewHandle(const WebViewDescriptor &)>;
  using Destroy = std::function<void(NativeViewHandle)>;
  using Navigate = std::function<bool(NativeViewHandle, std::string_view)>;
  using RoutePointer = std::function<bool(NativeViewHandle, const PointerEvent &)>;

  NativeWebViewHost(Create create, Destroy destroy, Navigate navigate, RoutePointer route_pointer);
  ~NativeWebViewHost();
  NativeWebViewHost(const NativeWebViewHost &) = delete;
  NativeWebViewHost &operator=(const NativeWebViewHost &) = delete;

  [[nodiscard]] std::optional<std::uint64_t> CreateView(const WebViewDescriptor &descriptor);
  bool DestroyView(std::uint64_t view);
  bool NavigateView(std::uint64_t view, std::string_view url);
  bool Route(std::uint64_t view, const PointerEvent &event);
  void DestroyAll() noexcept;
  [[nodiscard]] bool IsNativeOverlay(std::uint64_t view) const noexcept;
  [[nodiscard]] std::size_t NativeViewCount() const noexcept { return views_.size(); }
  [[nodiscard]] const WebViewStats &Stats() const noexcept { return stats_; }

private:
  Create create_;
  Destroy destroy_;
  Navigate navigate_;
  RoutePointer route_pointer_;
  std::unordered_map<std::uint64_t, NativeViewHandle> views_;
  std::uint64_t next_view_{1};
  WebViewStats stats_;
};

struct Services final {
  std::function<void(Permission)> request_permission;
  std::function<void(HapticKind)> haptic;
  std::function<bool(std::string_view)> set_clipboard;
  std::function<std::string()> get_clipboard;
  std::function<bool(std::string_view)> share;
  std::function<bool(std::string_view)> platform_login;
};

class NEXORA_RUNTIME_API Runtime final {
public:
  explicit Runtime(Services services = {}) : services_(std::move(services)) {}

  bool Transition(AppState state) noexcept;
  bool SetSafeArea(SafeArea area) noexcept;
  void SetOrientation(Orientation orientation) noexcept { orientation_ = orientation; }
  void SetPressure(ThermalState thermal, MemoryPressure memory) noexcept;
  bool SetPermission(Permission permission, PermissionState state);
  bool RequestPermission(Permission permission) const;
  bool OpenDeepLink(std::string uri);
  bool SetImeComposition(std::string text, std::size_t cursor);
  bool CommitIme(std::string text);
  void SetGpuWorkarounds(std::vector<std::string> workarounds);

  [[nodiscard]] bool Haptic(HapticKind kind) const;
  [[nodiscard]] bool SetClipboard(std::string_view text) const;
  [[nodiscard]] std::optional<std::string> Clipboard() const;
  [[nodiscard]] bool Share(std::string_view payload) const;
  [[nodiscard]] bool Login(std::string_view provider) const;
  [[nodiscard]] AppState State() const noexcept { return state_; }
  [[nodiscard]] const SafeArea &Insets() const noexcept { return safe_area_; }
  [[nodiscard]] Orientation CurrentOrientation() const noexcept { return orientation_; }
  [[nodiscard]] Policy CurrentPolicy() const noexcept { return policy_; }
  [[nodiscard]] PermissionState PermissionStatus(Permission permission) const noexcept;
  [[nodiscard]] const std::optional<std::string> &PendingDeepLink() const noexcept {
    return deep_link_;
  }
  [[nodiscard]] const std::string &ImeComposition() const noexcept { return ime_composition_; }
  [[nodiscard]] std::size_t ImeCursor() const noexcept { return ime_cursor_; }
  [[nodiscard]] const std::string &CommittedText() const noexcept { return committed_text_; }
  [[nodiscard]] bool HasGpuWorkaround(std::string_view name) const noexcept;

private:
  Services services_;
  AppState state_{AppState::Foreground};
  SafeArea safe_area_;
  Orientation orientation_{Orientation::Portrait};
  Policy policy_;
  std::unordered_map<Permission, PermissionState> permissions_;
  std::optional<std::string> deep_link_;
  std::string ime_composition_;
  std::string committed_text_;
  std::size_t ime_cursor_{};
  std::vector<std::string> gpu_workarounds_;
};

[[nodiscard]] NEXORA_RUNTIME_API bool IsValidUri(std::string_view uri) noexcept;

} // namespace nexora::runtime::platform
