#pragma once
#include "Nexora/Editor/EditorWorkspace.h"
#include <array>
#include <variant>

namespace nexora::editor {
// Explicit portable wire layouts, never offsets into borrowed plugin objects.
enum class ReflectedKind : std::uint8_t {
  Boolean,
  Integer,
  Unsigned,
  Number,
  Enum,
  Flags,
  Vector2,
  Vector3,
  Vector4,
  Color,
  EntityReference,
  AssetReference
};
struct ReflectedChoice final {
  std::uint64_t value{};
  std::string label;
  friend bool operator==(const ReflectedChoice &, const ReflectedChoice &) = default;
};
struct ReflectedField final {
  std::string path;
  ReflectedKind kind{};
  std::size_t offset{}, elements{1};
  std::vector<ReflectedChoice> choices{};
  friend bool operator==(const ReflectedField &, const ReflectedField &) = default;
};
struct ReflectedComponent final {
  runtime::TypeId type{};
  std::string name;
  std::size_t bytes{};
  std::vector<ReflectedField> fields;
  friend bool operator==(const ReflectedComponent &, const ReflectedComponent &) = default;
};
using ReflectedValue = std::variant<bool, std::int64_t, std::uint64_t, double,
                                    std::array<double, 4>, foundation::Uuid>;
struct ReflectedProperty final {
  std::string path;
  ReflectedKind kind{};
  std::size_t offset{};
  std::vector<ReflectedChoice> choices;
  ReflectedValue value;
  bool mixed{};
};
struct ReflectedObservation final {
  std::uint64_t catalog_revision{};
  ReflectedComponent component;
  std::vector<std::pair<SceneDocument::NodeKey, OpaqueComponent>> sources;
  std::vector<ReflectedProperty> properties;
};
class NEXORA_EDITOR_API ReflectedInspector final {
public:
  static constexpr std::size_t kMaximumTypes = 64, kMaximumFields = 64, kMaximumElements = 32,
                               kMaximumTargets = 256, kMaximumObservationBytes = 1024 * 1024,
                               kMaximumSchemaBytes = 256 * 1024;
  // Copies a validated complete catalog. Rejects preserve the previous catalog/revision.
  bool SetComponents(std::vector<ReflectedComponent> components);
  [[nodiscard]] const std::vector<ReflectedComponent> &Components() const noexcept {
    return components_;
  }
  [[nodiscard]] std::uint64_t Revision() const noexcept { return revision_; }
  [[nodiscard]] std::optional<ReflectedObservation>
  Inspect(const SceneDocument &, std::span<const SceneDocument::NodeKey>, runtime::TypeId) const;
  // Revalidates complete owning observation, catalog revision, exact bytes and current selection.
  // The authoring caller supplies current access/Play/recovery authority; false always rejects.
  bool Apply(SceneDocument &, const ReflectedObservation &, std::size_t property,
             const ReflectedValue &, bool authorized) const;
  // Flags checkbox operation: preserve every target's other bits in one validated batch.
  bool ApplyFlag(SceneDocument &, const ReflectedObservation &, std::size_t property,
                 std::uint64_t bit, bool enabled, bool authorized) const;
  // Optional project metadata: .nexora/inspector.reflection. Missing means empty catalog.
  // Bounded regular, nonaliased read; malformed input returns no catalog and never mutates sources.
  [[nodiscard]] static std::optional<ReflectedInspector>
  LoadProject(const std::filesystem::path &root, std::string *error = nullptr);

private:
  bool ApplyEdit(SceneDocument &, const ReflectedObservation &, std::size_t, const ReflectedValue &,
                 bool, std::optional<std::uint64_t> flag_bit) const;
  std::uint64_t revision_{1};
  std::vector<ReflectedComponent> components_;
  runtime::ReflectionRegistry reflection_;
};
} // namespace nexora::editor
