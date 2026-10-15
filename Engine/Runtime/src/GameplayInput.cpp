#include "Nexora/Runtime/GameplayInput.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace nexora::runtime {
namespace {
bool ValidDevice(std::uint32_t device) { return device >= 1 && device <= 3; }
bool ValidAction(std::uint32_t user, std::uint32_t action) {
  return user < 8 && action > 0 && action <= 256;
}
template <class T> bool Descriptor(const T *value) {
  return value && value->struct_size >= sizeof(T) && value->abi_version == 1;
}
template <class T> void Copy(T *output, const T &value) {
  const auto size = output->struct_size;
  std::memcpy(output, &value, sizeof(T));
  output->struct_size = size;
}
} // namespace
GameplayInput::GameplayInput() : owner_(std::this_thread::get_id()) {}
bool GameplayInput::Owner() const noexcept { return owner_ == std::this_thread::get_id(); }
bool GameplayInput::BeginFrame() noexcept {
  if (!Owner())
    return false;
  ++sequence_;
  for (auto &user : users_) {
    user.delta_x = user.delta_y = user.wheel_x = user.wheel_y = 0;
    for (auto &[id, action] : user.actions) {
      (void)id;
      action.pressed = action.released = false;
    }
  }
  return true;
}
void GameplayInput::Evaluate(User &user) {
  for (auto &[id, action] : user.actions) {
    (void)id;
    float total = 0;
    for (const auto &binding : action.bindings) {
      const auto found = user.controls.find({binding.device, binding.device_id, binding.control});
      if (found != user.controls.end() && std::abs(found->second) > binding.deadzone)
        total += found->second * binding.scale;
    }
    const float value = std::clamp(total, -1.0F, 1.0F);
    const bool was = action.value != 0, now = value != 0;
    action.pressed |= !was && now;
    action.released |= was && !now;
    action.value = value;
  }
}
bool GameplayInput::Submit(std::uint32_t user, std::uint32_t device, std::uint32_t device_id,
                           std::uint32_t control, float value) {
  if (!Owner() || user >= users_.size() || !ValidDevice(device) || control > 511 ||
      !std::isfinite(value) || std::abs(value) > 1)
    return false;
  if (!focused_)
    return true;
  auto &state = users_[user];
  const auto key = Control{device, device_id, control};
  const auto found = state.controls.find(key);
  const auto previous = found == state.controls.end() ? 0.0F : found->second;
  if (value != 0 && value != previous) {
    state.device = device;
    state.device_id = device_id;
  }
  if (value == 0)
    state.controls.erase(key);
  else
    state.controls[key] = value;
  Evaluate(state);
  return true;
}
bool GameplayInput::SubmitPointer(std::uint32_t user, double x, double y, double wheelX,
                                  double wheelY) {
  if (!Owner() || user >= users_.size() || !std::isfinite(x) || !std::isfinite(y) ||
      !std::isfinite(wheelX) || !std::isfinite(wheelY))
    return false;
  if (!focused_)
    return true;
  auto &state = users_[user];
  const double dx = state.pointer_anchored ? x - state.x : 0;
  const double dy = state.pointer_anchored ? y - state.y : 0;
  if (dx != 0 || dy != 0 || wheelX != 0 || wheelY != 0) {
    state.device = NEXORA_INPUT_DEVICE_MOUSE;
    state.device_id = 0;
  }
  state.x = x;
  state.y = y;
  state.pointer_anchored = true;
  state.delta_x = std::clamp(state.delta_x + dx, -65536.0, 65536.0);
  state.delta_y = std::clamp(state.delta_y + dy, -65536.0, 65536.0);
  state.wheel_x = std::clamp(state.wheel_x + wheelX, -122880.0, 122880.0);
  state.wheel_y = std::clamp(state.wheel_y + wheelY, -122880.0, 122880.0);
  return true;
}
bool GameplayInput::SetFocused(bool focused) {
  if (!Owner())
    return false;
  focused_ = focused;
  if (!focused)
    for (auto &user : users_) {
      user.controls.clear();
      user.pointer_anchored = false;
      user.delta_x = user.delta_y = user.wheel_x = user.wheel_y = 0;
      user.device = user.device_id = 0;
      for (auto &[id, action] : user.actions) {
        (void)id;
        action.pressed = false;
      }
      Evaluate(user);
    }
  return true;
}
bool GameplayInput::Disconnect(std::uint32_t device, std::uint32_t device_id) {
  if (!Owner() || !ValidDevice(device))
    return false;
  for (auto &user : users_) {
    if (device == NEXORA_INPUT_DEVICE_MOUSE) {
      user.pointer_anchored = false;
      user.delta_x = user.delta_y = user.wheel_x = user.wheel_y = 0;
    }
    std::erase_if(user.controls, [&](const auto &entry) {
      return std::get<0>(entry.first) == device && std::get<1>(entry.first) == device_id;
    });
    if (user.device == device && user.device_id == device_id)
      user.device = user.device_id = 0;
    Evaluate(user);
  }
  return true;
}
int32_t GameplayInput::Bind(std::uint32_t user, std::uint32_t action,
                            const NexoraGameplayInputBindingV1 *binding) {
  if (!Owner())
    return -3;
  if (!ValidAction(user, action) || !Descriptor(binding) || !ValidDevice(binding->device) ||
      binding->control > 511 || !std::isfinite(binding->scale) || std::abs(binding->scale) > 1 ||
      !std::isfinite(binding->deadzone) || binding->deadzone < 0 || binding->deadzone >= 1 ||
      binding->reserved)
    return -1;
  auto &state = users_[user];
  auto &target = state.actions[action];
  auto found = std::find_if(target.bindings.begin(), target.bindings.end(), [&](const auto &entry) {
    return entry.device == binding->device && entry.device_id == binding->device_id &&
           entry.control == binding->control;
  });
  if (found != target.bindings.end())
    *found = *binding;
  else {
    if (target.bindings.size() >= 16)
      return -1;
    target.bindings.push_back(*binding);
  }
  Evaluate(state);
  return 0;
}
int32_t GameplayInput::Clear(std::uint32_t user, std::uint32_t action) {
  if (!Owner())
    return -3;
  if (!ValidAction(user, action))
    return -1;
  auto &state = users_[user];
  const auto found = state.actions.find(action);
  if (found != state.actions.end())
    found->second.bindings.clear();
  Evaluate(state);
  return 0;
}
NexoraGameplayInputApiV1 GameplayInput::Api() noexcept {
  NexoraGameplayInputApiV1 api{};
  api.struct_size = sizeof(api);
  api.abi_version = 1;
  api.context = this;
  api.read_action = [](void *context, uint32_t user, uint32_t action,
                       NexoraGameplayActionStateV1 *output) -> int32_t {
    if (!context)
      return -1;
    auto &self = *static_cast<GameplayInput *>(context);
    if (!self.Owner())
      return -3;
    if (!ValidAction(user, action) || !Descriptor(output))
      return -1;
    NexoraGameplayActionStateV1 result{sizeof(result), 1, self.sequence_, 0, 0, 0, 0};
    const auto found = self.users_[user].actions.find(action);
    if (found != self.users_[user].actions.end()) {
      result.value = found->second.value;
      result.held = result.value != 0;
      result.pressed = found->second.pressed;
      result.released = found->second.released;
    }
    Copy(output, result);
    return 0;
  };
  api.bind = [](void *context, uint32_t user, uint32_t action,
                const NexoraGameplayInputBindingV1 *binding) -> int32_t {
    if (!context)
      return -1;
    try {
      return static_cast<GameplayInput *>(context)->Bind(user, action, binding);
    } catch (...) {
      return -3;
    }
  };
  api.clear_bindings = [](void *context, uint32_t user, uint32_t action) -> int32_t {
    return context ? static_cast<GameplayInput *>(context)->Clear(user, action) : -1;
  };
  api.read_device = [](void *context, uint32_t user,
                       NexoraGameplayDeviceStateV1 *output) -> int32_t {
    if (!context)
      return -1;
    auto &self = *static_cast<GameplayInput *>(context);
    if (!self.Owner())
      return -3;
    if (user >= self.users_.size() || !Descriptor(output))
      return -1;
    const auto &state = self.users_[user];
    const NexoraGameplayDeviceStateV1 result{
        sizeof(result),          1, self.sequence_, state.device, state.device_id,
        self.focused_ ? 1U : 0U, 0};
    Copy(output, result);
    return 0;
  };
  api.read_pointer = [](void *context, uint32_t user,
                        NexoraGameplayPointerStateV1 *output) -> int32_t {
    if (!context)
      return -1;
    auto &self = *static_cast<GameplayInput *>(context);
    if (!self.Owner())
      return -3;
    if (user >= self.users_.size() || !Descriptor(output))
      return -1;
    const auto &state = self.users_[user];
    uint32_t buttons = 0;
    for (const auto &[control, value] : state.controls) {
      if (std::get<0>(control) == NEXORA_INPUT_DEVICE_MOUSE && std::get<2>(control) < 5 &&
          value != 0)
        buttons |= 1U << std::get<2>(control);
    }
    const NexoraGameplayPointerStateV1 result{sizeof(result),
                                              1,
                                              self.sequence_,
                                              state.x,
                                              state.y,
                                              state.delta_x,
                                              state.delta_y,
                                              state.wheel_x,
                                              state.wheel_y,
                                              buttons,
                                              self.focused_ ? 1U : 0U};
    Copy(output, result);
    return 0;
  };
  return api;
}
} // namespace nexora::runtime
