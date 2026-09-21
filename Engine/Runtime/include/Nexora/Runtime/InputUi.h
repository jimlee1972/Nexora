#pragma once

#include "Nexora/Runtime/Api.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace nexora::runtime {

using InputUserId = std::uint32_t;
using PointerId = std::uint64_t;
using UIElementId = std::uint64_t;

enum class InputDeviceKind { Keyboard, Mouse, Gamepad, Touch, Virtual };
enum class InputEventKind { Button, Axis, PointerDown, PointerMove, PointerUp, Text };
struct RawInputEvent final {
  std::uint64_t sequence{};
  InputDeviceKind device{InputDeviceKind::Keyboard};
  std::uint32_t device_id{};
  InputEventKind kind{InputEventKind::Button};
  std::string control;
  float value{};
  PointerId pointer{};
  float x{}, y{};
  std::string text;
};

// Device assignment is additive: keyboard, mouse, gamepad and touch may all
// contribute to one user in the same frame. Sequence IDs suppress duplicate
// delivery at the platform/backend boundary.
class NEXORA_RUNTIME_API InputSystem final {
public:
  bool Assign(InputUserId user, InputDeviceKind kind, std::uint32_t device_id);
  bool Unassign(InputUserId user, InputDeviceKind kind, std::uint32_t device_id);
  bool Push(RawInputEvent event);
  [[nodiscard]] std::vector<RawInputEvent> Consume(InputUserId user);
  void EndFrame();

private:
  static std::uint64_t DeviceKey(InputDeviceKind kind, std::uint32_t id) noexcept;
  std::unordered_map<std::uint64_t, InputUserId> owners_;
  std::unordered_set<std::uint64_t> sequences_;
  std::unordered_map<InputUserId, std::vector<RawInputEvent>> events_;
};

struct InputBinding final {
  std::string control;
  float scale{1.0F};
};
class NEXORA_RUNTIME_API ActionMap final {
public:
  bool Bind(std::string action, InputBinding binding);
  [[nodiscard]] std::unordered_map<std::string, float>
  Evaluate(const std::vector<RawInputEvent> &events) const;

private:
  std::unordered_map<std::string, std::vector<InputBinding>> bindings_;
};

struct Rect final {
  float x{}, y{}, width{}, height{};
};
struct RectTransform final {
  float anchor_min_x{}, anchor_min_y{};
  float anchor_max_x{}, anchor_max_y{};
  float offset_left{}, offset_top{}, offset_right{}, offset_bottom{};
  [[nodiscard]] Rect Resolve(Rect parent) const noexcept;
};
enum class UIElementKind { Panel, Image, Label, Button, ScrollView, VirtualizedList };
enum class PointerPhase { Capture, Target, Bubble };
struct UIPointerEvent final {
  PointerId pointer{};
  float x{}, y{};
  bool pressed{};
};

struct UIElement final {
  UIElementId id{};
  UIElementId parent{};
  UIElementKind kind{UIElementKind::Panel};
  RectTransform transform{};
  Rect bounds{};
  bool enabled{true};
  bool hit_test{true};
  std::string localization_key;
  std::string text;
  std::function<bool(const UIPointerEvent &, PointerPhase)> on_pointer;
};
enum class UIInputPolicy { PassThrough, ConsumeOnHit, BlockBelow };

// Resolve() checks the active locale first, then the fallback locale (default "en")
// when the key is untranslated there, and only returns the raw key if neither has it.
class NEXORA_RUNTIME_API LocalizationTable final {
public:
  bool SetLocale(std::string locale);
  bool SetFallbackLocale(std::string locale);
  void Set(std::string locale, std::string key, std::string value);
  [[nodiscard]] std::string Resolve(std::string_view key) const;
  [[nodiscard]] std::uint64_t Generation() const noexcept { return generation_; }

private:
  std::string locale_{"en"};
  std::string fallback_locale_{"en"};
  std::uint64_t generation_{1};
  std::unordered_map<std::string, std::unordered_map<std::string, std::string>> values_;
};

class NEXORA_RUNTIME_API UIDocument final {
public:
  explicit UIDocument(float logical_width = 1920, float logical_height = 1080);
  UIElementId Create(UIElementKind kind, UIElementId parent = 0);
  bool Remove(UIElementId id);
  [[nodiscard]] UIElement *Find(UIElementId id);
  [[nodiscard]] const UIElement *Find(UIElementId id) const;
  void Layout();
  void SetEnabled(bool enabled, const LocalizationTable *localization = nullptr);
  void RefreshLocalization(const LocalizationTable &localization);
  [[nodiscard]] bool Dispatch(const UIPointerEvent &event);
  bool CapturePointer(PointerId pointer, UIElementId owner);
  void ReleasePointer(PointerId pointer, UIElementId owner);
  void SetLogicalResolution(float width, float height);
  [[nodiscard]] Rect LogicalViewport() const noexcept { return viewport_; }
  [[nodiscard]] std::size_t ElementCount() const noexcept { return elements_.size(); }
  [[nodiscard]] std::uint64_t AppliedLocalizationGeneration() const noexcept {
    return localization_generation_;
  }
  UIInputPolicy policy{UIInputPolicy::ConsumeOnHit};
  int priority{};

private:
  [[nodiscard]] UIElementId HitTest(float x, float y) const;
  [[nodiscard]] std::vector<UIElementId> Path(UIElementId target) const;
  Rect viewport_{};
  UIElementId next_id_{1};
  bool enabled_{true};
  std::uint64_t localization_generation_{};
  std::vector<UIElement> elements_;
  std::unordered_map<PointerId, UIElementId> captures_;
};

class NEXORA_RUNTIME_API UIRouter final {
public:
  void Add(UIDocument &document);
  [[nodiscard]] bool Dispatch(const UIPointerEvent &event);

private:
  std::vector<UIDocument *> documents_;
};

// Maintains only enough rows for the visible window plus overscan.
class NEXORA_RUNTIME_API VirtualizedListModel final {
public:
  void Configure(std::size_t item_count, float item_extent, float viewport_extent,
                 std::size_t overscan = 1);
  void ScrollTo(float offset);
  [[nodiscard]] std::size_t FirstVisible() const noexcept { return first_; }
  [[nodiscard]] std::size_t RealizedCount() const noexcept { return realized_; }

private:
  void Update() noexcept;
  std::size_t item_count_{}, overscan_{1}, first_{}, realized_{};
  float item_extent_{1}, viewport_extent_{}, offset_{};
};

struct NineSliceBorders final {
  float left{}, top{}, right{}, bottom{};
};
[[nodiscard]] NEXORA_RUNTIME_API std::vector<Rect> BuildNineSlice(Rect rect,
                                                                  NineSliceBorders borders);

// Platform IME edits UTF-8 composition as a separate range. Commit creates one
// local undo record; indices are code-point boundaries, never arbitrary bytes.
class NEXORA_RUNTIME_API TextEditBuffer final {
public:
  void Set(std::string utf8);
  bool BeginComposition(std::size_t codepoint_index);
  bool UpdateComposition(std::string utf8);
  bool CommitComposition();
  bool Undo();
  [[nodiscard]] const std::string &Text() const noexcept { return text_; }
  [[nodiscard]] const std::string &Composition() const noexcept { return composition_; }

private:
  static std::optional<std::size_t> ByteOffset(std::string_view text, std::size_t codepoint);
  std::string text_, composition_;
  std::size_t composition_byte_{};
  bool composing_{};
  std::vector<std::string> undo_;
};

} // namespace nexora::runtime
