#pragma once
#include "Nexora/Runtime/Api.h"
#include "nexora/gameplay_input.h"
#include <array>
#include <cstdint>
#include <map>
#include <thread>
#include <tuple>
#include <vector>

namespace nexora::runtime {
// Owner thread only. BeginFrame clears transitions, Submit updates actions immediately;
// repeated captures never consume transitions. Tables borrow this object until destruction.
class NEXORA_RUNTIME_API GameplayInput final {
public:
  GameplayInput();
  GameplayInput(const GameplayInput &) = delete;
  GameplayInput &operator=(const GameplayInput &) = delete;
  [[nodiscard]] NexoraGameplayInputApiV1 Api() noexcept;
  bool BeginFrame() noexcept;
  bool Submit(std::uint32_t user, std::uint32_t device, std::uint32_t device_id,
              std::uint32_t control, float value);
  bool SubmitPointer(std::uint32_t user, double x, double y, double wheelX = 0, double wheelY = 0);
  bool SetFocused(bool focused);
  bool Disconnect(std::uint32_t device, std::uint32_t device_id);

private:
  struct Action {
    std::vector<NexoraGameplayInputBindingV1> bindings;
    float value{};
    bool pressed{}, released{};
  };
  using Control = std::tuple<std::uint32_t, std::uint32_t, std::uint32_t>;
  struct User {
    std::map<std::uint32_t, Action> actions;
    std::map<Control, float> controls;
    std::uint32_t device{}, device_id{};
    double x{}, y{}, delta_x{}, delta_y{}, wheel_x{}, wheel_y{};
    bool pointer_anchored{};
  };
  bool Owner() const noexcept;
  void Evaluate(User &user);
  int32_t Bind(std::uint32_t user, std::uint32_t action,
               const NexoraGameplayInputBindingV1 *binding);
  int32_t Clear(std::uint32_t user, std::uint32_t action);
  std::array<User, 8> users_;
  std::thread::id owner_;
  std::uint64_t sequence_{};
  bool focused_{true};
};
} // namespace nexora::runtime
