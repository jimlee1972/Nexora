#include "Nexora/Network/Prediction.h"

#include <algorithm>
#include <limits>
#include <type_traits>

namespace nexora::network {
namespace {
constexpr std::uint32_t kReplayMagic = 0x594c5052; // "RPLY" in little endian.
constexpr std::uint16_t kReplayVersion = 1;

template <typename T> void Write(std::vector<std::byte> &out, T value) {
  using U = std::make_unsigned_t<T>;
  const auto bits = static_cast<U>(value);
  for (std::size_t index = 0; index < sizeof(T); ++index) {
    out.push_back(static_cast<std::byte>((bits >> (index * 8)) & 0xff));
  }
}

template <typename T> bool Read(std::span<const std::byte> bytes, std::size_t &offset, T &value) {
  using U = std::make_unsigned_t<T>;
  if (offset > bytes.size() || bytes.size() - offset < sizeof(T)) {
    return false;
  }
  U bits{};
  for (std::size_t index = 0; index < sizeof(T); ++index) {
    bits |= static_cast<U>(std::to_integer<std::uint8_t>(bytes[offset++])) << (index * 8);
  }
  value = static_cast<T>(bits);
  return true;
}
std::int64_t SaturatingAdd(std::int64_t left, std::int64_t right) noexcept {
  if (right > 0 && left > std::numeric_limits<std::int64_t>::max() - right) {
    return std::numeric_limits<std::int64_t>::max();
  }
  if (right < 0 && left < std::numeric_limits<std::int64_t>::min() - right) {
    return std::numeric_limits<std::int64_t>::min();
  }
  return left + right;
}
} // namespace

PredictedState SimulateInput(PredictedState state, const InputCommand &command) noexcept {
  state.velocity = SaturatingAdd(state.velocity, command.acceleration);
  state.position = SaturatingAdd(state.position, state.velocity);
  return state;
}

InputCommand PredictionBuffer::Push(std::int32_t acceleration) {
  const InputCommand command{next_sequence_++, acceleration};
  pending_.push_back(command);
  state_ = SimulateInput(state_, command);
  return command;
}

bool PredictionBuffer::Reconcile(const AuthoritativeCorrection &correction) {
  if (correction.acknowledged_sequence < last_acknowledged_ ||
      correction.acknowledged_sequence >= next_sequence_) {
    return false;
  }
  last_acknowledged_ = correction.acknowledged_sequence;
  std::erase_if(pending_, [&correction](const auto &command) {
    return command.sequence <= correction.acknowledged_sequence;
  });
  state_ = correction.state;
  for (const auto &command : pending_) {
    state_ = SimulateInput(state_, command);
  }
  return true;
}

void ReplayLog::RecordInput(std::uint64_t delivery_tick, InputCommand input) {
  events_.push_back({delivery_tick, ReplayEventType::Input, input, {}});
}

void ReplayLog::RecordCorrection(std::uint64_t delivery_tick, AuthoritativeCorrection correction) {
  events_.push_back({delivery_tick, ReplayEventType::Correction, {}, correction});
}

std::vector<std::byte> ReplayLog::Encode() const {
  if (events_.size() > std::numeric_limits<std::uint32_t>::max()) {
    return {};
  }
  std::vector<std::byte> bytes;
  Write(bytes, kReplayMagic);
  Write(bytes, kReplayVersion);
  Write(bytes, static_cast<std::uint32_t>(events_.size()));
  for (const auto &event : events_) {
    Write(bytes, event.delivery_tick);
    Write(bytes, static_cast<std::uint8_t>(event.type));
    if (event.type == ReplayEventType::Input) {
      Write(bytes, event.input.sequence);
      Write(bytes, event.input.acceleration);
    } else if (event.type == ReplayEventType::Correction) {
      Write(bytes, event.correction.acknowledged_sequence);
      Write(bytes, event.correction.state.position);
      Write(bytes, event.correction.state.velocity);
    } else {
      return {};
    }
  }
  return bytes;
}

bool ReplayLog::Decode(std::span<const std::byte> bytes, ReplayLog &log) {
  std::size_t offset{};
  std::uint32_t magic{}, count{};
  std::uint16_t version{};
  if (!Read(bytes, offset, magic) || !Read(bytes, offset, version) || !Read(bytes, offset, count) ||
      magic != kReplayMagic || version != kReplayVersion || count > (bytes.size() - offset) / 21) {
    return false;
  }
  ReplayLog decoded;
  decoded.events_.reserve(count);
  for (std::uint32_t index = 0; index < count; ++index) {
    ReplayEvent event;
    std::uint8_t type{};
    if (!Read(bytes, offset, event.delivery_tick) || !Read(bytes, offset, type) || type < 1 ||
        type > 2) {
      return false;
    }
    event.type = static_cast<ReplayEventType>(type);
    if (event.type == ReplayEventType::Input) {
      if (!Read(bytes, offset, event.input.sequence) ||
          !Read(bytes, offset, event.input.acceleration)) {
        return false;
      }
    } else if (!Read(bytes, offset, event.correction.acknowledged_sequence) ||
               !Read(bytes, offset, event.correction.state.position) ||
               !Read(bytes, offset, event.correction.state.velocity)) {
      return false;
    }
    decoded.events_.push_back(event);
  }
  if (offset != bytes.size()) {
    return false;
  }
  log = std::move(decoded);
  return true;
}

PredictedState ReplayLog::Replay(PredictedState initial) const {
  auto ordered = events_;
  std::stable_sort(ordered.begin(), ordered.end(), [](const auto &left, const auto &right) {
    return left.delivery_tick < right.delivery_tick;
  });
  PredictedState state = initial;
  for (const auto &event : ordered) {
    if (event.type == ReplayEventType::Input) {
      state = SimulateInput(state, event.input);
    } else {
      state = event.correction.state;
    }
  }
  return state;
}

} // namespace nexora::network
