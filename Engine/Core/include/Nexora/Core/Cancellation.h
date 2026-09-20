#pragma once

#include <atomic>
#include <memory>

namespace nexora::core {

class CancellationToken final {
public:
  CancellationToken() = default;
  [[nodiscard]] bool IsCancellationRequested() const noexcept {
    return state_ && state_->load(std::memory_order_acquire);
  }

private:
  explicit CancellationToken(std::shared_ptr<std::atomic_bool> state) : state_(std::move(state)) {}
  std::shared_ptr<std::atomic_bool> state_;
  friend class CancellationSource;
};

class CancellationSource final {
public:
  CancellationSource() : state_(std::make_shared<std::atomic_bool>(false)) {}
  [[nodiscard]] CancellationToken Token() const noexcept { return CancellationToken{state_}; }
  void Cancel() noexcept { state_->store(true, std::memory_order_release); }

private:
  std::shared_ptr<std::atomic_bool> state_;
};

} // namespace nexora::core
