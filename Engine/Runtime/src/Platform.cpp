#include "Nexora/Runtime/Platform.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace nexora::runtime::platform {
namespace {
bool IsFiniteNonNegative(float value) { return std::isfinite(value) && value >= 0.0F; }
} // namespace

NativeWebViewHost::NativeWebViewHost(Create create, Destroy destroy, Navigate navigate,
                                     RoutePointer route_pointer)
    : create_(std::move(create)), destroy_(std::move(destroy)), navigate_(std::move(navigate)),
      route_pointer_(std::move(route_pointer)) {}

NativeWebViewHost::~NativeWebViewHost() { DestroyAll(); }

std::optional<std::uint64_t> NativeWebViewHost::CreateView(const WebViewDescriptor &descriptor) {
  if (!create_ || !destroy_ || !IsValidUri(descriptor.initial_url) ||
      !std::isfinite(descriptor.rect.x) || !std::isfinite(descriptor.rect.y) ||
      !IsFiniteNonNegative(descriptor.rect.width) || !IsFiniteNonNegative(descriptor.rect.height) ||
      descriptor.rect.width == 0.0F || descriptor.rect.height == 0.0F) {
    ++stats_.rejected_operations;
    return std::nullopt;
  }
  const auto native = create_(descriptor);
  if (native == 0) {
    ++stats_.rejected_operations;
    return std::nullopt;
  }
  const auto id = next_view_++;
  views_.emplace(id, native);
  ++stats_.created;
  return id;
}

bool NativeWebViewHost::DestroyView(std::uint64_t view) {
  const auto found = views_.find(view);
  if (found == views_.end() || !destroy_) {
    ++stats_.rejected_operations;
    return false;
  }
  destroy_(found->second);
  views_.erase(found);
  ++stats_.destroyed;
  return true;
}

bool NativeWebViewHost::NavigateView(std::uint64_t view, std::string_view url) {
  const auto found = views_.find(view);
  if (found == views_.end() || !navigate_ || !IsValidUri(url) || !navigate_(found->second, url)) {
    ++stats_.rejected_operations;
    return false;
  }
  return true;
}

bool NativeWebViewHost::Route(std::uint64_t view, const PointerEvent &event) {
  const auto found = views_.find(view);
  if (found == views_.end() || !route_pointer_ || event.pointer_id == 0 ||
      !std::isfinite(event.x) || !std::isfinite(event.y) || !route_pointer_(found->second, event)) {
    ++stats_.rejected_operations;
    return false;
  }
  ++stats_.routed_pointers;
  return true;
}

void NativeWebViewHost::DestroyAll() noexcept {
  if (destroy_)
    for (const auto &[unused, native] : views_) {
      static_cast<void>(unused);
      destroy_(native);
      ++stats_.destroyed;
    }
  views_.clear();
}

bool NativeWebViewHost::IsNativeOverlay(std::uint64_t view) const noexcept {
  return views_.contains(view);
}

bool Runtime::Transition(AppState state) noexcept {
  if (state_ == AppState::Terminating || state_ == state)
    return false;
  state_ = state;
  return true;
}

bool Runtime::SetSafeArea(SafeArea area) noexcept {
  if (!IsFiniteNonNegative(area.left) || !IsFiniteNonNegative(area.top) ||
      !IsFiniteNonNegative(area.right) || !IsFiniteNonNegative(area.bottom))
    return false;
  safe_area_ = area;
  return true;
}

void Runtime::SetPressure(ThermalState thermal, MemoryPressure memory) noexcept {
  policy_.reduce_quality = thermal >= ThermalState::Serious;
  policy_.pause_background_work = thermal == ThermalState::Critical;
  policy_.release_caches = memory >= MemoryPressure::Warning;
}

bool Runtime::SetPermission(Permission permission, PermissionState state) {
  if (state == PermissionState::Unknown)
    return permissions_.erase(permission) != 0;
  permissions_[permission] = state;
  return true;
}

bool Runtime::RequestPermission(Permission permission) const {
  if (!services_.request_permission)
    return false;
  services_.request_permission(permission);
  return true;
}

bool Runtime::OpenDeepLink(std::string uri) {
  if (!IsValidUri(uri))
    return false;
  deep_link_ = std::move(uri);
  return true;
}

bool Runtime::SetImeComposition(std::string text, std::size_t cursor) {
  if (cursor > text.size())
    return false;
  ime_composition_ = std::move(text);
  ime_cursor_ = cursor;
  return true;
}

bool Runtime::CommitIme(std::string text) {
  if (text.empty())
    return false;
  committed_text_ += text;
  ime_composition_.clear();
  ime_cursor_ = 0;
  return true;
}

void Runtime::SetGpuWorkarounds(std::vector<std::string> workarounds) {
  std::erase_if(workarounds, [](const auto &entry) { return entry.empty(); });
  std::ranges::sort(workarounds);
  workarounds.erase(std::unique(workarounds.begin(), workarounds.end()), workarounds.end());
  gpu_workarounds_ = std::move(workarounds);
}

bool Runtime::Haptic(HapticKind kind) const {
  if (!services_.haptic)
    return false;
  services_.haptic(kind);
  return true;
}
bool Runtime::SetClipboard(std::string_view text) const {
  return services_.set_clipboard && services_.set_clipboard(text);
}
std::optional<std::string> Runtime::Clipboard() const {
  if (!services_.get_clipboard)
    return std::nullopt;
  return services_.get_clipboard();
}
bool Runtime::Share(std::string_view payload) const {
  return !payload.empty() && services_.share && services_.share(payload);
}
bool Runtime::Login(std::string_view provider) const {
  return !provider.empty() && services_.platform_login && services_.platform_login(provider);
}
PermissionState Runtime::PermissionStatus(Permission permission) const noexcept {
  const auto found = permissions_.find(permission);
  return found == permissions_.end() ? PermissionState::Unknown : found->second;
}
bool Runtime::HasGpuWorkaround(std::string_view name) const noexcept {
  return std::ranges::binary_search(gpu_workarounds_, name);
}

bool IsValidUri(std::string_view uri) noexcept {
  const auto separator = uri.find("://");
  return separator != std::string_view::npos && separator > 0 && separator + 3 < uri.size() &&
         uri.find_first_of("\r\n\t ") == std::string_view::npos;
}

} // namespace nexora::runtime::platform
