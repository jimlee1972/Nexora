#pragma once
#include "Nexora/Cryptography/Api.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace nexora::cryptography {
enum class Provider : std::uint8_t { Unavailable, OpenSsl };
enum class Verification : std::uint8_t {
  Verified,
  InvalidInput,
  InvalidSignature,
  BackendUnavailable,
  BackendFailure
};
using Sha256Digest = std::array<std::byte, 32>;
inline constexpr std::size_t kMaximumMessageBytes = 64 * 1024 * 1024;
// Stateless synchronous calls; valid input spans outlive the call and are never retained.
// Public keys are raw 32-byte RFC8032 Ed25519 keys selected by the caller's trust policy.
// Signatures are raw 64-byte pure Ed25519, with no prehash/context variant. Empty messages work.
// Shape/budget rejection precedes backend work. No backend never accepts a signature.
[[nodiscard]] NEXORA_CRYPTOGRAPHY_API Provider ActiveProvider() noexcept;
[[nodiscard]] NEXORA_CRYPTOGRAPHY_API Verification
VerifyEd25519(std::span<const std::byte> public_key, std::span<const std::byte> signature,
              std::span<const std::byte> message) noexcept;
// Owning digest; nullopt for oversized input, unavailable provider or provider failure.
[[nodiscard]] NEXORA_CRYPTOGRAPHY_API std::optional<Sha256Digest>
Sha256(std::span<const std::byte> message) noexcept;
} // namespace nexora::cryptography
