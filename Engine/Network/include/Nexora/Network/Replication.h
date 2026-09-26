#pragma once

#include "Nexora/Network/Api.h"
#include "Nexora/Network/EntityMapping.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

namespace nexora::network {

enum class WireType : std::uint8_t { Unsigned = 1, Signed = 2, Bytes = 3 };
enum class FieldCompatibility : std::uint8_t { Optional, Required };

struct Quantization final {
  std::int64_t minimum{};
  std::int64_t maximum{};
  std::uint8_t bits{};
  friend bool operator==(const Quantization &, const Quantization &) = default;
};

struct ReplicationField final {
  std::uint16_t id{};
  WireType wire_type{};
  FieldCompatibility compatibility{FieldCompatibility::Optional};
  std::optional<Quantization> quantization;
  std::string name;
};

class NEXORA_NETWORK_API ReplicationSchema final {
public:
  ReplicationSchema(std::uint32_t schema_id, std::uint16_t version,
                    std::vector<ReplicationField> fields);

  [[nodiscard]] bool IsValid() const noexcept { return valid_; }
  [[nodiscard]] std::uint32_t ID() const noexcept { return schema_id_; }
  [[nodiscard]] std::uint16_t Version() const noexcept { return version_; }
  [[nodiscard]] std::uint64_t Hash() const noexcept { return hash_; }
  [[nodiscard]] const std::vector<ReplicationField> &Fields() const noexcept { return fields_; }
  [[nodiscard]] const ReplicationField *Find(std::uint16_t id) const noexcept;
  [[nodiscard]] bool IsBackwardCompatibleWith(const ReplicationSchema &older) const noexcept;

private:
  std::uint32_t schema_id_{};
  std::uint16_t version_{};
  std::uint64_t hash_{};
  std::vector<ReplicationField> fields_;
  bool valid_{};
};

struct SnapshotField final {
  std::uint16_t id{};
  WireType wire_type{};
  std::vector<std::byte> payload;
  friend bool operator==(const SnapshotField &, const SnapshotField &) = default;
};

struct Snapshot final {
  std::uint64_t id{};
  std::vector<SnapshotField> fields;
  // Fields not understood by the local schema are retained byte-for-byte and
  // emitted again, allowing an older relay to forward newer optional data.
  std::vector<SnapshotField> unknown_fields;
  friend bool operator==(const Snapshot &, const Snapshot &) = default;
};

enum class SnapshotError : std::uint8_t {
  None,
  InvalidSchema,
  WrongSchema,
  Malformed,
  DuplicateField,
  WireTypeMismatch,
  MissingRequiredField,
};

NEXORA_NETWORK_API std::vector<std::byte> EncodeSnapshot(const ReplicationSchema &schema,
                                                         const Snapshot &snapshot);
NEXORA_NETWORK_API SnapshotError DecodeSnapshot(const ReplicationSchema &schema,
                                                std::span<const std::byte> encoded,
                                                Snapshot &snapshot);

struct DeltaPacket final {
  std::uint64_t snapshot_id{};
  std::uint64_t baseline_id{};
  bool full_snapshot{};
  std::vector<std::byte> bytes;
};

enum class DeltaError : std::uint8_t { None, BaselineMissing, Malformed, SnapshotRejected };

NEXORA_NETWORK_API DeltaPacket EncodeDelta(const ReplicationSchema &schema,
                                           const Snapshot &snapshot, const Snapshot *baseline);
NEXORA_NETWORK_API DeltaError DecodeDelta(const ReplicationSchema &schema,
                                          const DeltaPacket &packet, const Snapshot *baseline,
                                          Snapshot &snapshot);

class NEXORA_NETWORK_API BaselineStore final {
public:
  explicit BaselineStore(std::size_t capacity) : capacity_(capacity) {}
  void Insert(Snapshot snapshot);
  [[nodiscard]] const Snapshot *Find(std::uint64_t id) const noexcept;

private:
  std::size_t capacity_{};
  std::vector<Snapshot> snapshots_;
};

using ConnectionID = std::uint64_t;

struct InterestQuery final {
  std::vector<NetworkEntityID> entities;
  std::size_t next_cursor{};
  bool complete{};
};

class NEXORA_NETWORK_API IInterestProvider {
public:
  virtual ~IInterestProvider() = default;
  // Implementations may be spatial or explicit-subscription based. They must
  // inspect at most max_work candidates and resume from cursor.
  [[nodiscard]] virtual InterestQuery Query(ConnectionID connection, std::size_t cursor,
                                            std::size_t max_work) const = 0;
};

struct InterestChanges final {
  std::vector<NetworkEntityID> spawn;
  std::vector<NetworkEntityID> despawn;
  std::size_t work{};
  bool complete{};
};

class NEXORA_NETWORK_API InterestManager final {
public:
  [[nodiscard]] InterestChanges Update(ConnectionID connection, const IInterestProvider &provider,
                                       std::size_t budget);
  void Remove(ConnectionID connection);
  [[nodiscard]] bool Contains(ConnectionID connection, NetworkEntityID entity) const;

private:
  struct State final {
    std::size_t cursor{};
    std::vector<NetworkEntityID> active;
    std::vector<NetworkEntityID> pending;
  };
  std::unordered_map<ConnectionID, State> states_;
};

} // namespace nexora::network
