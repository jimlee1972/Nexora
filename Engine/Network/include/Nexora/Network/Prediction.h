#pragma once

#include "Nexora/Network/Api.h"

#include <cstdint>
#include <span>
#include <vector>

namespace nexora::network {

using InputSequence = std::uint64_t;

// Fixed-point reference state used by the deterministic headless prediction contract.
struct PredictedState final {
  std::int64_t position{};
  std::int64_t velocity{};
  friend bool operator==(const PredictedState &, const PredictedState &) = default;
};

struct InputCommand final {
  InputSequence sequence{};
  std::int32_t acceleration{};
  friend bool operator==(const InputCommand &, const InputCommand &) = default;
};

struct AuthoritativeCorrection final {
  InputSequence acknowledged_sequence{};
  PredictedState state;
  friend bool operator==(const AuthoritativeCorrection &,
                         const AuthoritativeCorrection &) = default;
};

NEXORA_NETWORK_API PredictedState SimulateInput(PredictedState state,
                                                const InputCommand &command) noexcept;

class NEXORA_NETWORK_API PredictionBuffer final {
public:
  explicit PredictionBuffer(PredictedState initial = {}) : state_(initial) {}

  [[nodiscard]] InputCommand Push(std::int32_t acceleration);
  [[nodiscard]] bool Reconcile(const AuthoritativeCorrection &correction);
  [[nodiscard]] const PredictedState &State() const noexcept { return state_; }
  [[nodiscard]] std::span<const InputCommand> Pending() const noexcept { return pending_; }

private:
  PredictedState state_;
  InputSequence next_sequence_{1};
  InputSequence last_acknowledged_{};
  std::vector<InputCommand> pending_;
};

enum class ReplayEventType : std::uint8_t { Input = 1, Correction = 2 };

struct ReplayEvent final {
  std::uint64_t delivery_tick{};
  ReplayEventType type{};
  InputCommand input;
  AuthoritativeCorrection correction;
  friend bool operator==(const ReplayEvent &, const ReplayEvent &) = default;
};

class NEXORA_NETWORK_API ReplayLog final {
public:
  void RecordInput(std::uint64_t delivery_tick, InputCommand input);
  void RecordCorrection(std::uint64_t delivery_tick, AuthoritativeCorrection correction);
  [[nodiscard]] const std::vector<ReplayEvent> &Events() const noexcept { return events_; }
  [[nodiscard]] std::vector<std::byte> Encode() const;
  [[nodiscard]] static bool Decode(std::span<const std::byte> bytes, ReplayLog &log);
  [[nodiscard]] PredictedState Replay(PredictedState initial = {}) const;

private:
  std::vector<ReplayEvent> events_;
};

} // namespace nexora::network
