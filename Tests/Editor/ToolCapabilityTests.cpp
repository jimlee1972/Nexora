#include "Nexora/Editor/EditorProduction.h"

#include <algorithm>
#include <iostream>
#include <stdexcept>

using namespace nexora::editor;
namespace {
void Require(bool condition, const char *message) {
  if (!condition)
    throw std::runtime_error(message);
}
ToolDescriptor Reference() {
  ToolDescriptor result{"material.scalar", "Scalar PBR", CapabilityState::Implemented, {}};
  result.provider_id = "nexora.reference";
  result.required_permissions = 15;
  result.document_types = {"nexora.material.scalar.v1"};
  result.contributions = {"inspector.material.scalar", "preview.material.scalar"};
  return result;
}
template <class Change> void Reject(Change change) {
  SpecializedToolRegistry registry;
  Require(registry.Register(Reference()), "reference admission failed");
  auto invalid = Reference();
  invalid.id = "invalid";
  change(invalid);
  std::string error = "old error";
  Require(!registry.Register(std::move(invalid), &error) && !error.empty() &&
              registry.Tools().size() == 1 && registry.Find("material.scalar") &&
              !registry.Find("invalid"),
          "invalid admission mutated the retained reference capability");
}
void VerifyInvalidMetadata() {
  Reject([](auto &d) { d.schema_version = 2; });
  Reject([](auto &d) { d.interface_version = 0; });
  Reject([](auto &d) { d.state = static_cast<CapabilityState>(99); });
  Reject([](auto &d) { d.required_permissions = 16; });
  Reject([](auto &d) { d.required_permissions = 0; });
  Reject([](auto &d) { d.provider_id = "../provider"; });
  Reject([](auto &d) { d.provider_id.assign(129, 'a'); });
  Reject([](auto &d) { d.id = "UpperCase"; });
  Reject([](auto &d) { d.id.assign(129, 'a'); });
  Reject([](auto &d) { d.title.assign(257, 'a'); });
  Reject([](auto &d) { d.title = std::string(1, static_cast<char>(0xFF)); });
  Reject([](auto &d) { d.title = std::string("title\0hidden", 12); });
  Reject([](auto &d) { d.reason.assign(1025, 'a'); });
  Reject([](auto &d) { d.reason = std::string(1, static_cast<char>(0xFF)); });
  Reject([](auto &d) { d.state = CapabilityState::ReadOnly; });
  Reject([](auto &d) { d.state = CapabilityState::Unavailable; });
  Reject([](auto &d) { d.document_types = {"one", "one"}; });
  Reject([](auto &d) { d.contributions = {"one", "one"}; });
  Reject([](auto &d) { d.document_types = {"bad/type"}; });
  Reject([](auto &d) { d.contributions = {"bad\ncontribution"}; });
  Reject([](auto &d) { d.contributions = {std::string(129, 'a')}; });
  Reject([](auto &d) { d.document_types.assign(17, "too-many"); });
  Reject([](auto &d) { d.contributions.assign(17, "too-many"); });
  Reject([](auto &d) { d.budget.document_bytes = 0; });
  Reject([](auto &d) {
    d.budget.document_bytes = SpecializedToolRegistry::kMaximumDocumentBytes + 1;
  });
  Reject([](auto &d) { d.budget.preview_bytes = 0; });
  Reject(
      [](auto &d) { d.budget.preview_bytes = SpecializedToolRegistry::kMaximumPreviewBytes + 1; });
  Reject([](auto &d) { d.budget.pending_operations = 0; });
  Reject([](auto &d) { d.budget.pending_operations = 65; });
}
void VerifyBoundariesAndOwnership() {
  SpecializedToolRegistry registry;
  auto maximum_lists = Reference();
  maximum_lists.document_types.clear();
  maximum_lists.contributions.clear();
  for (int i = 0; i < 16; ++i) {
    maximum_lists.document_types.push_back("document" + std::to_string(i));
    maximum_lists.contributions.push_back("contribution" + std::to_string(i));
  }
  Require(SpecializedToolRegistry::Validate(maximum_lists),
          "exact document and contribution count budgets rejected");
  auto exact = Reference();
  exact.id.assign(128, 'a');
  exact.provider_id.assign(128, 'b');
  exact.title.assign(256, 'c');
  exact.reason.assign(1024, 'd');
  exact.document_types.clear();
  exact.contributions.clear();
  for (int i = 0; i < 16; ++i) {
    auto name = std::string("type") + std::to_string(i);
    name.resize(128, 'x');
    exact.document_types.push_back(std::move(name));
  }
  // 128 + 128 + 256 + 1024 + 16*128 + 4*128 = 4096 owning text bytes.
  for (int i = 0; i < 4; ++i) {
    auto name = std::string("contribution") + std::to_string(i);
    name.resize(128, 'x');
    exact.contributions.push_back(std::move(name));
  }
  exact.budget = {SpecializedToolRegistry::kMaximumDocumentBytes,
                  SpecializedToolRegistry::kMaximumPreviewBytes,
                  SpecializedToolRegistry::kMaximumPendingOperations};
  std::string error = "old error";
  Require(registry.Register(exact, &error) && error.empty(), "exact metadata bounds rejected");
  auto overflow = exact;
  overflow.contributions.push_back("x");
  Require(!SpecializedToolRegistry::Validate(overflow, &error) && !error.empty(),
          "descriptor total-text byte overflow accepted");
  Require(!registry.Register(exact, &error) && !error.empty() && registry.Tools().size() == 1,
          "duplicate admission changed the registry");
  const auto snapshot = registry.Snapshot();
  Require(registry.Remove(exact.id) && !registry.Remove(exact.id) && registry.Tools().empty() &&
              snapshot.size() == 1 && snapshot.front().provider_id == exact.provider_id &&
              snapshot.front().document_types == exact.document_types &&
              snapshot.front().budget.preview_bytes == exact.budget.preview_bytes,
          "removal invalidated owning metadata or failed to remove the live capability");
  for (std::size_t i = 0; i < SpecializedToolRegistry::kMaximumTools; ++i) {
    auto descriptor = Reference();
    descriptor.id = "tool" + std::to_string(i);
    Require(registry.Register(std::move(descriptor)), "exact registry capacity rejected");
  }
  auto extra = Reference();
  extra.id = "extra";
  Require(!registry.Register(extra, &error) && !error.empty() &&
              registry.Tools().size() == SpecializedToolRegistry::kMaximumTools,
          "registry overflow mutated admitted metadata");
  Require(registry.Remove("tool0") && registry.Register(extra) && registry.Find("extra") &&
              std::ranges::is_sorted(registry.Tools(), {}, &ToolDescriptor::id),
          "explicit removal did not release discovery capacity or retain sorted lookup");
}
void VerifyFallbackAndCompatibility() {
  SpecializedToolRegistry registry;
  Require(registry.Register({"legacy", "Existing capability", CapabilityState::Implemented, {}}),
          "original four-field aggregate compatibility changed");
  auto readonly = Reference();
  readonly.id = "readonly";
  readonly.state = CapabilityState::ReadOnly;
  readonly.title = "Scalar PBR \xE6\x9D\x90\xE8\xB3\xAA";
  readonly.reason = "Provider disabled; source payload is retained";
  auto unavailable = readonly;
  unavailable.id = "missing";
  unavailable.state = CapabilityState::Unavailable;
  unavailable.reason = "Production backend is absent";
  Require(registry.Register(readonly) && registry.Register(unavailable) &&
              registry.Find("readonly")->reason == readonly.reason &&
              registry.Find("missing")->state == CapabilityState::Unavailable &&
              registry.Find("missing")->document_types == readonly.document_types,
          "fallback discovery lost its diagnostic or document identity");
  // Discovery removal preserves copied fallback diagnostics for an already-open UI.
  const auto observation = registry.Snapshot();
  const auto missing = std::ranges::find(observation, "missing", &ToolDescriptor::id);
  Require(registry.Remove("missing") && observation.size() == 3 && missing != observation.end() &&
              missing->reason == unavailable.reason,
          "discovery removal invalidated copied fallback diagnostics");
  auto unicode = Reference();
  unicode.title.clear();
  for (int i = 0; i < 128; ++i)
    unicode.title += "\xC2\xB5";
  Require(SpecializedToolRegistry::Validate(unicode), "exact UTF-8 title byte budget rejected");
  unicode.title.push_back('x');
  Require(!SpecializedToolRegistry::Validate(unicode), "UTF-8 title byte overflow accepted");
}
} // namespace
int main() {
  try {
    VerifyInvalidMetadata();
    VerifyBoundariesAndOwnership();
    VerifyFallbackAndCompatibility();
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
