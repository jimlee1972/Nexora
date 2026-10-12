#pragma once

#include "Nexora/Editor/MaterialImport.h"
#include "Nexora/Foundation/Types.h"
#include "Nexora/Runtime/AssetPipeline.h"
#include <span>
#include <thread>
#include <vector>

namespace nexora::editor {
struct MaterialToolScope final {
  foundation::Uuid project;
  runtime::AssetUuid asset;
  std::uint64_t generation{};
  friend bool operator==(const MaterialToolScope &, const MaterialToolScope &) = default;
};
struct MaterialToolSnapshot final {
  MaterialToolScope scope;
  std::uint64_t serial{};
  std::string source, saved_source;
  MaterialAsset material;
  bool dirty{}, can_undo{}, can_redo{};
};
// Construction-thread owner. Owning snapshots and validated canonical history; no file IO,
// native callback, workspace borrow or authority grant. Host checks writer/Play/current source
// independently before deferred authoring/publication, and acknowledges only confirmed saves.
class NEXORA_EDITOR_API MaterialToolDocument final {
public:
  static constexpr std::size_t kMaximumHistory = 64;
  MaterialToolDocument() = default;
  MaterialToolDocument(const MaterialToolDocument &) = delete;
  MaterialToolDocument &operator=(const MaterialToolDocument &) = delete;
  bool Open(MaterialToolScope, std::string_view source, bool discard_dirty = false,
            std::string *error = nullptr);
  [[nodiscard]] std::optional<MaterialToolSnapshot> Snapshot() const;
  bool Apply(MaterialToolScope, std::uint64_t serial, std::span<const std::byte> output,
             bool editable, std::string *error = nullptr);
  bool Undo(MaterialToolScope, std::uint64_t serial, bool editable, std::string *error = nullptr);
  bool Redo(MaterialToolScope, std::uint64_t serial, bool editable, std::string *error = nullptr);
  // No publication is performed or inferred. Exact previous source and caller-confirmed new
  // source must match this document; history stays intact and saved baseline advances atomically.
  bool AcknowledgeSave(MaterialToolScope, std::uint64_t serial,
                       std::string_view expected_previous_source, std::string_view published_source,
                       std::string *error = nullptr);
  bool Close(MaterialToolScope, std::uint64_t serial, bool discard_dirty = false,
             std::string *error = nullptr);

private:
  struct State final {
    std::string source;
    MaterialAsset material;
  };
  [[nodiscard]] bool Owner(std::string *error) const;
  [[nodiscard]] bool Matches(MaterialToolScope, std::uint64_t serial, std::string *error) const;
  [[nodiscard]] bool Dirty() const noexcept;
  static std::optional<State> Parse(std::string_view, std::string *error);
  const std::thread::id owner_{std::this_thread::get_id()};
  std::uint64_t serial_{};
  MaterialToolScope scope_;
  std::optional<State> current_;
  std::string saved_source_, saved_canonical_;
  std::vector<State> undo_, redo_;
};
} // namespace nexora::editor
