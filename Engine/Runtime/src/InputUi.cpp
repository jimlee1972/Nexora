#include "Nexora/Runtime/InputUi.h"

#include <algorithm>
#include <cmath>

namespace nexora::runtime {
namespace {
bool IsValidUtf8(std::string_view text) noexcept {
  for (std::size_t byte = 0; byte < text.size();) {
    const auto first = static_cast<unsigned char>(text[byte]);
    std::size_t width = 0;
    std::uint32_t codepoint = 0;
    if (first <= 0x7FU) {
      width = 1;
      codepoint = first;
    } else if (first >= 0xC2U && first <= 0xDFU) {
      width = 2;
      codepoint = first & 0x1FU;
    } else if (first >= 0xE0U && first <= 0xEFU) {
      width = 3;
      codepoint = first & 0x0FU;
    } else if (first >= 0xF0U && first <= 0xF4U) {
      width = 4;
      codepoint = first & 0x07U;
    } else {
      return false;
    }
    if (byte + width > text.size())
      return false;
    for (std::size_t index = 1; index < width; ++index) {
      const auto continuation = static_cast<unsigned char>(text[byte + index]);
      if ((continuation & 0xC0U) != 0x80U)
        return false;
      codepoint = (codepoint << 6U) | (continuation & 0x3FU);
    }
    if ((width == 2 && codepoint < 0x80U) ||
        (width == 3 && codepoint < 0x800U) ||
        (width == 4 && codepoint < 0x10000U) || codepoint > 0x10FFFFU ||
        (codepoint >= 0xD800U && codepoint <= 0xDFFFU))
      return false;
    byte += width;
  }
  return true;
}
} // namespace

std::uint64_t InputSystem::DeviceKey(InputDeviceKind kind, std::uint32_t id) noexcept {
  return (static_cast<std::uint64_t>(kind) << 32U) | id;
}
bool InputSystem::Assign(InputUserId user, InputDeviceKind kind, std::uint32_t id) {
  return owners_.emplace(DeviceKey(kind, id), user).second;
}
bool InputSystem::Unassign(InputUserId user, InputDeviceKind kind, std::uint32_t id) {
  const auto it = owners_.find(DeviceKey(kind, id));
  if (it == owners_.end() || it->second != user)
    return false;
  owners_.erase(it);
  return true;
}
bool InputSystem::Push(RawInputEvent event) {
  const auto owner = owners_.find(DeviceKey(event.device, event.device_id));
  if (owner == owners_.end() || !sequences_.insert(event.sequence).second)
    return false;
  events_[owner->second].push_back(std::move(event));
  return true;
}
std::vector<RawInputEvent> InputSystem::Consume(InputUserId user) {
  auto it = events_.find(user);
  if (it == events_.end())
    return {};
  auto result = std::move(it->second);
  events_.erase(it);
  return result;
}
void InputSystem::EndFrame() {
  sequences_.clear();
  events_.clear();
}

bool ActionMap::Bind(std::string action, InputBinding binding) {
  if (action.empty() || binding.control.empty())
    return false;
  bindings_[std::move(action)].push_back(std::move(binding));
  return true;
}
std::unordered_map<std::string, float>
ActionMap::Evaluate(const std::vector<RawInputEvent> &events) const {
  std::unordered_map<std::string, float> values;
  for (const auto &[action, bindings] : bindings_)
    for (const auto &binding : bindings)
      for (const auto &event : events)
        if (event.control == binding.control)
          values[action] += event.value * binding.scale;
  return values;
}

Rect RectTransform::Resolve(Rect parent) const noexcept {
  const float left = parent.x + parent.width * anchor_min_x + offset_left;
  const float top = parent.y + parent.height * anchor_min_y + offset_top;
  const float right = parent.x + parent.width * anchor_max_x + offset_right;
  const float bottom = parent.y + parent.height * anchor_max_y + offset_bottom;
  return {left, top, std::max(0.0F, right - left), std::max(0.0F, bottom - top)};
}

bool LocalizationTable::SetLocale(std::string locale) {
  if (locale.empty() || locale == locale_)
    return false;
  locale_ = std::move(locale);
  ++generation_;
  return true;
}
bool LocalizationTable::SetFallbackLocale(std::string locale) {
  if (locale.empty() || locale == fallback_locale_)
    return false;
  fallback_locale_ = std::move(locale);
  ++generation_;
  return true;
}
void LocalizationTable::Set(std::string locale, std::string key, std::string value) {
  auto &table = values_[std::move(locale)];
  const auto found = table.find(key);
  if (found != table.end() && found->second == value)
    return;
  table.insert_or_assign(std::move(key), std::move(value));
  ++generation_;
}

std::string LocalizationTable::Resolve(std::string_view key) const {
  const auto locale = values_.find(locale_);
  if (locale != values_.end()) {
    const auto value = locale->second.find(std::string(key));
    if (value != locale->second.end())
      return value->second;
  }
  if (locale_ != fallback_locale_) {
    const auto fallback = values_.find(fallback_locale_);
    if (fallback != values_.end()) {
      const auto value = fallback->second.find(std::string(key));
      if (value != fallback->second.end())
        return value->second;
    }
  }
  return std::string(key);
}
UIDocument::UIDocument(float width, float height) { SetLogicalResolution(width, height); }
UIElementId UIDocument::Create(UIElementKind kind, UIElementId parent) {
  if (parent != 0 && Find(parent) == nullptr)
    return 0;
  const auto id = next_id_++;
  UIElement element;
  element.id = id;
  element.parent = parent;
  element.kind = kind;
  elements_.push_back(std::move(element));
  return id;
}
bool UIDocument::Remove(UIElementId id) {
  if (!Find(id))
    return false;
  std::unordered_set<UIElementId> removed{id};
  bool changed = true;
  while (changed) {
    changed = false;
    for (const auto &element : elements_)
      if (removed.contains(element.parent) && removed.insert(element.id).second)
        changed = true;
  }
  std::erase_if(elements_, [&](const UIElement &e) { return removed.contains(e.id); });
  std::erase_if(captures_, [&](const auto &entry) { return removed.contains(entry.second); });
  return true;
}
UIElement *UIDocument::Find(UIElementId id) {
  const auto it =
      std::find_if(elements_.begin(), elements_.end(), [id](const auto &e) { return e.id == id; });
  return it == elements_.end() ? nullptr : &*it;
}
const UIElement *UIDocument::Find(UIElementId id) const {
  const auto it =
      std::find_if(elements_.begin(), elements_.end(), [id](const auto &e) { return e.id == id; });
  return it == elements_.end() ? nullptr : &*it;
}
void UIDocument::Layout() {
  for (auto &element : elements_) {
    const auto *parent = Find(element.parent);
    element.bounds = element.transform.Resolve(parent ? parent->bounds : viewport_);
  }
}
void UIDocument::SetEnabled(bool enabled, const LocalizationTable *localization) {
  enabled_ = enabled;
  if (enabled && localization && localization_generation_ != localization->Generation())
    RefreshLocalization(*localization);
}
void UIDocument::RefreshLocalization(const LocalizationTable &localization) {
  for (auto &element : elements_)
    if (!element.localization_key.empty())
      element.text = localization.Resolve(element.localization_key);
  localization_generation_ = localization.Generation();
}
UIElementId UIDocument::HitTest(float x, float y) const {
  for (auto it = elements_.rbegin(); it != elements_.rend(); ++it)
    if (it->enabled && it->hit_test && x >= it->bounds.x && y >= it->bounds.y &&
        x < it->bounds.x + it->bounds.width && y < it->bounds.y + it->bounds.height)
      return it->id;
  return 0;
}
std::vector<UIElementId> UIDocument::Path(UIElementId target) const {
  std::vector<UIElementId> result;
  while (target) {
    result.push_back(target);
    const auto *e = Find(target);
    target = e ? e->parent : 0;
  }
  std::reverse(result.begin(), result.end());
  return result;
}
bool UIDocument::Dispatch(const UIPointerEvent &event) {
  if (!enabled_)
    return false;
  const auto capture = captures_.find(event.pointer);
  const auto target = capture == captures_.end() ? HitTest(event.x, event.y) : capture->second;
  if (!target)
    return policy == UIInputPolicy::BlockBelow;
  const auto path = Path(target);
  for (const auto id : path)
    if (id != target) {
      auto *e = Find(id);
      if (e && e->on_pointer && e->on_pointer(event, PointerPhase::Capture))
        return true;
    }
  auto *element = Find(target);
  if (element && element->on_pointer && element->on_pointer(event, PointerPhase::Target))
    return true;
  for (auto it = path.rbegin(); it != path.rend(); ++it)
    if (*it != target) {
      auto *e = Find(*it);
      if (e && e->on_pointer && e->on_pointer(event, PointerPhase::Bubble))
        return true;
    }
  return policy != UIInputPolicy::PassThrough;
}
bool UIDocument::CapturePointer(PointerId pointer, UIElementId owner) {
  if (!Find(owner))
    return false;
  const auto [it, inserted] = captures_.emplace(pointer, owner);
  return inserted || it->second == owner;
}
void UIDocument::ReleasePointer(PointerId pointer, UIElementId owner) {
  const auto it = captures_.find(pointer);
  if (it != captures_.end() && it->second == owner)
    captures_.erase(it);
}
void UIDocument::SetLogicalResolution(float width, float height) {
  viewport_ = {0, 0, std::max(1.0F, width), std::max(1.0F, height)};
}

void UIRouter::Add(UIDocument &document) { documents_.push_back(&document); }
bool UIRouter::Dispatch(const UIPointerEvent &event) {
  std::stable_sort(documents_.begin(), documents_.end(),
                   [](const auto *a, const auto *b) { return a->priority > b->priority; });
  for (auto *document : documents_)
    if (document->Dispatch(event))
      return true;
  return false;
}

void VirtualizedListModel::Configure(std::size_t count, float extent, float viewport,
                                     std::size_t overscan) {
  item_count_ = count;
  item_extent_ = std::max(0.001F, extent);
  viewport_extent_ = std::max(0.0F, viewport);
  overscan_ = overscan;
  Update();
}
void VirtualizedListModel::ScrollTo(float offset) {
  offset_ = std::max(0.0F, offset);
  Update();
}
void VirtualizedListModel::Update() noexcept {
  first_ = std::min(item_count_, static_cast<std::size_t>(offset_ / item_extent_));
  const auto visible = static_cast<std::size_t>(std::ceil(viewport_extent_ / item_extent_));
  realized_ = std::min(item_count_ - first_, visible + overscan_ * 2);
}

std::vector<Rect> BuildNineSlice(Rect rect, NineSliceBorders b) {
  b.left = std::clamp(b.left, 0.0F, rect.width);
  b.right = std::clamp(b.right, 0.0F, rect.width - b.left);
  b.top = std::clamp(b.top, 0.0F, rect.height);
  b.bottom = std::clamp(b.bottom, 0.0F, rect.height - b.top);
  const float xs[]{rect.x, rect.x + b.left, rect.x + rect.width - b.right, rect.x + rect.width};
  const float ys[]{rect.y, rect.y + b.top, rect.y + rect.height - b.bottom, rect.y + rect.height};
  std::vector<Rect> result;
  result.reserve(9);
  for (int y = 0; y < 3; ++y)
    for (int x = 0; x < 3; ++x)
      result.push_back({xs[x], ys[y], xs[x + 1] - xs[x], ys[y + 1] - ys[y]});
  return result;
}

std::optional<std::size_t> TextEditBuffer::ByteOffset(std::string_view text, std::size_t point) {
  if (!IsValidUtf8(text))
    return std::nullopt;
  std::size_t current = 0;
  for (std::size_t byte = 0; byte < text.size();) {
    if (current == point)
      return byte;
    const auto c = static_cast<unsigned char>(text[byte]);
    const std::size_t width = c < 0x80             ? 1
                              : (c & 0xE0) == 0xC0 ? 2
                              : (c & 0xF0) == 0xE0 ? 3
                                                   : 4;
    byte += width;
    ++current;
  }
  return current == point ? std::optional{text.size()} : std::nullopt;
}
void TextEditBuffer::Set(std::string utf8) {
  if (ByteOffset(utf8, 0)) {
    text_ = std::move(utf8);
    composition_.clear();
    composing_ = false;
    undo_.clear();
  }
}
bool TextEditBuffer::BeginComposition(std::size_t point) {
  const auto byte = ByteOffset(text_, point);
  if (!byte || composing_)
    return false;
  composition_byte_ = *byte;
  composition_.clear();
  composing_ = true;
  return true;
}
bool TextEditBuffer::UpdateComposition(std::string utf8) {
  if (!composing_ || !ByteOffset(utf8, 0))
    return false;
  composition_ = std::move(utf8);
  return true;
}
bool TextEditBuffer::CommitComposition() {
  if (!composing_)
    return false;
  undo_.push_back(text_);
  text_.insert(composition_byte_, composition_);
  composition_.clear();
  composing_ = false;
  return true;
}
bool TextEditBuffer::Undo() {
  if (undo_.empty())
    return false;
  text_ = std::move(undo_.back());
  undo_.pop_back();
  return true;
}

} // namespace nexora::runtime
