#include "Nexora/Editor/ExtensionTrust.h"
#include <algorithm>
#include <iostream>
#include <stdexcept>

namespace {
using namespace nexora;
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
std::vector<std::byte> Hex(std::string_view text) {
  const auto digit = [](char c) {
    return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1;
  };
  Require(text.size() % 2 == 0, "Malformed published fixture");
  std::vector<std::byte> bytes;
  for (std::size_t i = 0; i < text.size(); i += 2) {
    Require(digit(text[i]) >= 0 && digit(text[i + 1]) >= 0, "Malformed fixture digit");
    bytes.push_back(static_cast<std::byte>((digit(text[i]) << 4) | digit(text[i + 1])));
  }
  return bytes;
}
void Run() {
  using Status = editor::ExtensionTrustStatus;
  const auto key = Hex("d75a980182b10ab7d54bfed3c964073a0ee172f3daa62325af021a68f707511a");
  auto signature = Hex("e5564300c360ac729086e2cc806e828a84877f1eb8e5d974d873e06522490155"
                       "5fb8821590a33bacc61e39701cf9b46bd25bf5f0595bbe24655141438e7a100b");
  editor::ExtensionTrust trust;
  Require(trust.Verify("known.vendor", {}, signature).status == Status::UntrustedPublisher,
          "Unconfigured publisher accepted a signature");
  const auto initial = trust.Revision();
  std::array<std::byte, 32> empty{}, identity{};
  identity[0] = std::byte{1};
  Require(!trust.SetPublisher("", key) && !trust.SetPublisher("bad/name", key) &&
              !trust.SetPublisher("bad\nname", key) &&
              !trust.SetPublisher(std::string(129, 'x'), key) &&
              !trust.SetPublisher("known.vendor", empty) &&
              !trust.SetPublisher("known.vendor", identity) &&
              !trust.SetPublisher("known.vendor", std::span(key).first(31)) &&
              trust.Revision() == initial,
          "Rejected trust configuration mutated state");
  Require(trust.SetPublisher("known.vendor", key) && trust.Revision() > initial,
          "Real publisher configuration failed");
  const auto revision = trust.Revision();
  Require(trust.SetPublisher("known.vendor", key) && trust.Revision() == revision,
          "Identical key rotated trust");
  const auto result = trust.Verify("known.vendor", {}, signature);
  const bool available = cryptography::ActiveProvider() != cryptography::Provider::Unavailable;
  Require(result.status == (available ? Status::Verified : Status::BackendUnavailable) &&
              result.trust_revision == revision && result.artifact_digest.has_value() == available,
          "Actual provider signature result/digest contract failed");
  auto copy = trust.Snapshot();
  copy.front().publisher = "forged";
  copy.front().public_key.fill(std::byte{});
  Require(trust.Verify("known.vendor", {}, signature).status == result.status &&
              trust.Verify("forged", {}, signature).status == Status::UntrustedPublisher &&
              trust.Verify("Known.vendor", {}, signature).status == Status::UntrustedPublisher,
          "Copied registry/case-sensitive publisher bypassed trust");
  signature[8] ^= std::byte{1};
  const auto tampered = trust.Verify("known.vendor", {}, signature);
  Require(tampered.status != Status::Verified && !tampered.artifact_digest,
          "Tampered signature received trusted digest");
  signature[8] ^= std::byte{1};
  const std::array changed{std::byte{1}};
  Require(trust.Verify("known.vendor", changed, signature).status != Status::Verified,
          "Altered artifact accepted old signature");
  auto rotated = key;
  rotated[3] ^= std::byte{1};
  Require(trust.SetPublisher("known.vendor", rotated) && trust.Revision() > revision &&
              trust.Verify("known.vendor", {}, signature).status != Status::Verified,
          "Key rotation retained old signature authorization");
  Require(trust.SetPublisher("known.vendor", key), "Restore public fixture key failed");
  for (std::size_t i = 1; i < editor::ExtensionTrust::kMaximumPublishers; ++i)
    Require(trust.SetPublisher("budget" + std::to_string(i), key),
            "Exact publisher capacity rejected");
  const auto saturated = trust.Revision();
  Require(!trust.SetPublisher("overflow", key) && trust.Revision() == saturated &&
              trust.Snapshot().size() == editor::ExtensionTrust::kMaximumPublishers,
          "Publisher limit lost trusted configuration");
  Require(trust.RemovePublisher("known.vendor") && trust.Revision() > saturated &&
              trust.Verify("known.vendor", {}, signature).status == Status::UntrustedPublisher,
          "Key revocation failed");
  const auto after = trust.Revision();
  Require(!trust.RemovePublisher("missing") && trust.Revision() == after,
          "Missing revocation mutated trust");
  Require(trust.Verify("budget1", {}, std::span(signature).first(63)).status ==
              Status::InvalidInput,
          "Malformed signature bypassed policy");
}
} // namespace
int main() {
  try {
    Run();
    std::cout << "Real signature trust/publisher/key rotation/revocation/budgets passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
