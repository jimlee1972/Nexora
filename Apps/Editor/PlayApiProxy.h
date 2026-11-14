#pragma once
#include "Nexora/Foundation/GameplayABI.h"
#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <thread>
#include <type_traits>
#include <utility>

namespace nexora::editor::preview {
template <class Api> struct PlayProxyErrors {
  static constexpr std::int32_t invalid = NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT;
  static constexpr std::int32_t unsupported = NEXORA_GAMEPLAY_ERROR_UNSUPPORTED;
  static constexpr std::int32_t denied = NEXORA_GAMEPLAY_ERROR_LIFECYCLE;
  static constexpr std::int32_t wrong_thread = NEXORA_GAMEPLAY_ERROR_LIFECYCLE;
  static constexpr std::int32_t exception = NEXORA_GAMEPLAY_ERROR_LIFECYCLE;
};
template <> struct PlayProxyErrors<NexoraStorageApiV1> {
  static constexpr std::int32_t invalid = NEXORA_STORAGE_INVALID_ARGUMENT;
  static constexpr std::int32_t unsupported = NEXORA_STORAGE_INVALID_ARGUMENT;
  static constexpr std::int32_t denied = NEXORA_STORAGE_IO_ERROR;
  static constexpr std::int32_t wrong_thread = NEXORA_STORAGE_WRONG_THREAD;
  static constexpr std::int32_t exception = NEXORA_STORAGE_IO_ERROR;
};
// One stable, generation-owned guard. Registered work drains before the owner changes phase;
// cached C tables consult this same guard rather than bypassing it after negotiation.
class PlayServiceGate final {
public:
  enum class Phase { Live, Capture, Candidate, Retired };
  explicit PlayServiceGate(Phase phase = Phase::Live) noexcept : phase_(phase) {}
  PlayServiceGate(const PlayServiceGate &) = delete;
  PlayServiceGate &operator=(const PlayServiceGate &) = delete;
  [[nodiscard]] bool OwnerThread() const noexcept { return owner_ == std::this_thread::get_id(); }
  [[nodiscard]] Phase CurrentPhase() const noexcept { return phase_.load(); }
  bool SetPhase(Phase phase) noexcept {
    if (!OwnerThread()) {
      Reject();
      return false;
    }
    rejected_.store(false);
    phase_.store(phase);
    return true;
  }
  [[nodiscard]] bool Check(bool writes, bool external) noexcept {
    const auto phase = phase_.load();
    if (!OwnerThread() || (writes && phase == Phase::Capture) ||
        (writes && external && (phase == Phase::Candidate || phase == Phase::Retired))) {
      Reject();
      return false;
    }
    return true;
  }
  void Reject() noexcept { rejected_.store(true); }
  [[nodiscard]] bool Valid() const noexcept { return !rejected_.load(); }

private:
  const std::thread::id owner_{std::this_thread::get_id()};
  std::atomic<Phase> phase_;
  std::atomic<bool> rejected_{};
};

// Stable owner-thread proxy for a known C wire table. Only explicitly bound callbacks are
// advertised. Provider/context redirection never changes a cached proxy table's context.
// Gate, proxy and provider owners outlive every module callback/library finalizer using them.
template <class Api> class PlayApiProxy final {
public:
  PlayApiProxy(PlayServiceGate &gate, Api provider) noexcept : gate_(gate), provider_(provider) {
    static_assert(std::is_standard_layout_v<Api>);
    wire_.struct_size = std::min(provider.struct_size, static_cast<std::uint32_t>(sizeof(Api)));
    wire_.abi_version = provider.abi_version;
    wire_.context = this;
    if constexpr (requires { provider.capabilities; })
      wire_.capabilities = provider.capabilities;
  }
  PlayApiProxy(const PlayApiProxy &) = delete;
  PlayApiProxy &operator=(const PlayApiProxy &) = delete;
  PlayApiProxy(PlayApiProxy &&) = delete;
  PlayApiProxy &operator=(PlayApiProxy &&) = delete;
  template <auto Member, std::size_t MemberEnd, bool Writes, bool External = false>
  void Bind() noexcept {
    static_assert(MemberEnd <= sizeof(Api));
    using Signature = std::remove_cvref_t<decltype(provider_.*Member)>;
    if (!gate_.OwnerThread()) {
      gate_.Reject();
      return;
    }
    if (provider_.struct_size >= MemberEnd && provider_.*Member)
      wire_.*Member = Forward<Signature, Member, MemberEnd, Writes, External>::Call;
  }
  [[nodiscard]] Api Get() const noexcept { return wire_; }
  bool Redirect(Api provider) noexcept {
    if (!gate_.OwnerThread() || provider.abi_version != wire_.abi_version ||
        provider.struct_size < offsetof(Api, context) + sizeof(provider.context)) {
      gate_.Reject();
      return false;
    }
    provider_ = provider;
    return true;
  }

private:
  template <class Result> static Result Failure(std::int32_t error) noexcept {
    if constexpr (std::is_void_v<Result>)
      return;
    else if constexpr (std::is_pointer_v<Result>)
      return nullptr;
    else {
      static_assert(std::is_same_v<Result, std::int32_t>);
      return error;
    }
  }
  template <class Signature, auto Member, std::size_t MemberEnd, bool Writes, bool External>
  struct Forward;
  template <class Result, auto Member, std::size_t MemberEnd, bool Writes, bool External,
            class... Arguments>
  struct Forward<Result (*)(void *, Arguments...), Member, MemberEnd, Writes, External> {
    static Result Call(void *context, Arguments... arguments) noexcept {
      if (!context)
        return Failure<Result>(PlayProxyErrors<Api>::invalid);
      auto &self = *static_cast<PlayApiProxy *>(context);
      if (!self.gate_.Check(Writes, External))
        return Failure<Result>(self.gate_.OwnerThread() ? PlayProxyErrors<Api>::denied
                                                        : PlayProxyErrors<Api>::wrong_thread);
      if (self.provider_.abi_version != self.wire_.abi_version ||
          self.provider_.struct_size < MemberEnd || !(self.provider_.*Member))
        return Failure<Result>(PlayProxyErrors<Api>::unsupported);
      try {
        return (self.provider_.*Member)(self.provider_.context,
                                        std::forward<Arguments>(arguments)...);
      } catch (...) {
        self.gate_.Reject();
        return Failure<Result>(PlayProxyErrors<Api>::exception);
      }
    }
  };
  PlayServiceGate &gate_;
  Api provider_;
  Api wire_{};
};
} // namespace nexora::editor::preview
