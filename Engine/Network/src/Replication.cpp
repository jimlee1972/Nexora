#include "Nexora/Network/Replication.h"

#include <algorithm>
#include <limits>
#include <unordered_set>

namespace nexora::network {
namespace {
constexpr std::uint32_t kSnapshotMagic = 0x4e535250; // "PRSN" in little endian.
constexpr std::uint32_t kDeltaMagic = 0x544c444e;    // "NDLT" in little endian.

template <typename T> void Write(std::vector<std::byte> &out, T value) {
  for (std::size_t i = 0; i < sizeof(T); ++i) {
    out.push_back(static_cast<std::byte>((value >> (i * 8)) & 0xff));
  }
}

template <typename T> bool Read(std::span<const std::byte> bytes, std::size_t &offset, T &value) {
  if (offset > bytes.size() || bytes.size() - offset < sizeof(T)) {
    return false;
  }
  value = 0;
  for (std::size_t i = 0; i < sizeof(T); ++i) {
    value |= static_cast<T>(std::to_integer<std::uint8_t>(bytes[offset++])) << (i * 8);
  }
  return true;
}

void HashByte(std::uint64_t &hash, std::uint8_t value) {
  hash ^= value;
  hash *= 1099511628211ull;
}

template <typename T> void HashValue(std::uint64_t &hash, T value) {
  for (std::size_t i = 0; i < sizeof(T); ++i) {
    HashByte(hash, static_cast<std::uint8_t>((value >> (i * 8)) & 0xff));
  }
}

bool EntityLess(NetworkEntityID a, NetworkEntityID b) {
  return a.index < b.index || (a.index == b.index && a.generation < b.generation);
}

std::vector<SnapshotField> CanonicalFields(const Snapshot &snapshot) {
  auto fields = snapshot.fields;
  fields.insert(fields.end(), snapshot.unknown_fields.begin(), snapshot.unknown_fields.end());
  std::sort(fields.begin(), fields.end(), [](const auto &a, const auto &b) { return a.id < b.id; });
  return fields;
}
} // namespace

ReplicationSchema::ReplicationSchema(std::uint32_t schema_id, std::uint16_t version,
                                     std::vector<ReplicationField> fields)
    : schema_id_(schema_id), version_(version), fields_(std::move(fields)) {
  std::sort(fields_.begin(), fields_.end(),
            [](const auto &a, const auto &b) { return a.id < b.id; });
  valid_ = schema_id_ != 0 && version_ != 0 && !fields_.empty();
  std::uint16_t previous{};
  for (const auto &field : fields_) {
    valid_ = valid_ && field.id != 0 && field.id != previous && !field.name.empty();
    if (field.quantization) {
      valid_ = valid_ && field.wire_type != WireType::Bytes && field.quantization->bits > 0 &&
               field.quantization->bits <= 63 &&
               field.quantization->minimum <= field.quantization->maximum;
    }
    previous = field.id;
  }
  if (!valid_) {
    return;
  }
  hash_ = 14695981039346656037ull;
  HashValue(hash_, schema_id_);
  HashValue(hash_, version_);
  for (const auto &field : fields_) {
    HashValue(hash_, field.id);
    HashByte(hash_, static_cast<std::uint8_t>(field.wire_type));
    HashByte(hash_, static_cast<std::uint8_t>(field.compatibility));
    HashByte(hash_, field.quantization.has_value());
    if (field.quantization) {
      HashValue(hash_, static_cast<std::uint64_t>(field.quantization->minimum));
      HashValue(hash_, static_cast<std::uint64_t>(field.quantization->maximum));
      HashByte(hash_, field.quantization->bits);
    }
  }
}

const ReplicationField *ReplicationSchema::Find(std::uint16_t id) const noexcept {
  const auto found =
      std::lower_bound(fields_.begin(), fields_.end(), id,
                       [](const auto &field, auto value) { return field.id < value; });
  return found != fields_.end() && found->id == id ? &*found : nullptr;
}

bool ReplicationSchema::IsBackwardCompatibleWith(const ReplicationSchema &older) const noexcept {
  if (!valid_ || !older.valid_ || schema_id_ != older.schema_id_ || version_ < older.version_) {
    return false;
  }
  for (const auto &old : older.fields_) {
    const auto *field = Find(old.id);
    if (!field || field->wire_type != old.wire_type || field->quantization != old.quantization) {
      return false;
    }
  }
  return std::all_of(fields_.begin(), fields_.end(), [&older](const auto &field) {
    return older.Find(field.id) || field.compatibility == FieldCompatibility::Optional;
  });
}

std::vector<std::byte> EncodeSnapshot(const ReplicationSchema &schema, const Snapshot &snapshot) {
  if (!schema.IsValid()) {
    return {};
  }
  const auto fields = CanonicalFields(snapshot);
  if (fields.size() > std::numeric_limits<std::uint16_t>::max()) {
    return {};
  }
  std::vector<std::byte> out;
  Write(out, kSnapshotMagic);
  Write(out, schema.ID());
  Write(out, schema.Version());
  Write(out, schema.Hash());
  Write(out, snapshot.id);
  Write(out, static_cast<std::uint16_t>(fields.size()));
  std::uint16_t previous{};
  for (const auto &field : fields) {
    if (field.id == 0 || field.id == previous ||
        field.payload.size() > std::numeric_limits<std::uint32_t>::max()) {
      return {};
    }
    if (const auto *known = schema.Find(field.id); known && known->wire_type != field.wire_type) {
      return {};
    }
    Write(out, field.id);
    Write(out, static_cast<std::uint8_t>(field.wire_type));
    Write(out, static_cast<std::uint32_t>(field.payload.size()));
    out.insert(out.end(), field.payload.begin(), field.payload.end());
    previous = field.id;
  }
  return out;
}

SnapshotError DecodeSnapshot(const ReplicationSchema &schema, std::span<const std::byte> encoded,
                             Snapshot &snapshot) {
  if (!schema.IsValid()) {
    return SnapshotError::InvalidSchema;
  }
  std::size_t offset{};
  std::uint32_t magic{}, schema_id{};
  std::uint16_t version{}, count{};
  std::uint64_t encoded_hash{}, id{};
  if (!Read(encoded, offset, magic) || !Read(encoded, offset, schema_id) ||
      !Read(encoded, offset, version) || !Read(encoded, offset, encoded_hash) ||
      !Read(encoded, offset, id) || !Read(encoded, offset, count) || magic != kSnapshotMagic ||
      schema_id != schema.ID() || version == 0) {
    return SnapshotError::WrongSchema;
  }
  (void)encoded_hash; // Exposed in the header for handshake/capture diagnostics; compatibility is
                      // field based.
  Snapshot decoded{id, {}, {}};
  std::uint16_t previous{};
  for (std::uint16_t index = 0; index < count; ++index) {
    std::uint16_t field_id{};
    std::uint8_t wire{};
    std::uint32_t size{};
    if (!Read(encoded, offset, field_id) || !Read(encoded, offset, wire) ||
        !Read(encoded, offset, size) || field_id == 0 || field_id <= previous ||
        offset > encoded.size() || encoded.size() - offset < size || wire < 1 || wire > 3) {
      return field_id == previous ? SnapshotError::DuplicateField : SnapshotError::Malformed;
    }
    SnapshotField field{field_id,
                        static_cast<WireType>(wire),
                        {encoded.begin() + static_cast<std::ptrdiff_t>(offset),
                         encoded.begin() + static_cast<std::ptrdiff_t>(offset + size)}};
    offset += size;
    if (const auto *known = schema.Find(field_id)) {
      if (known->wire_type != field.wire_type) {
        return SnapshotError::WireTypeMismatch;
      }
      decoded.fields.push_back(std::move(field));
    } else {
      decoded.unknown_fields.push_back(std::move(field));
    }
    previous = field_id;
  }
  if (offset != encoded.size()) {
    return SnapshotError::Malformed;
  }
  for (const auto &field : schema.Fields()) {
    if (field.compatibility == FieldCompatibility::Required &&
        std::none_of(decoded.fields.begin(), decoded.fields.end(),
                     [&field](const auto &value) { return value.id == field.id; })) {
      return SnapshotError::MissingRequiredField;
    }
  }
  snapshot = std::move(decoded);
  return SnapshotError::None;
}

DeltaPacket EncodeDelta(const ReplicationSchema &schema, const Snapshot &snapshot,
                        const Snapshot *baseline) {
  DeltaPacket packet{snapshot.id, baseline ? baseline->id : 0, baseline == nullptr, {}};
  Write(packet.bytes, kDeltaMagic);
  Write(packet.bytes, static_cast<std::uint8_t>(packet.full_snapshot));
  if (!baseline) {
    auto full = EncodeSnapshot(schema, snapshot);
    packet.bytes.insert(packet.bytes.end(), full.begin(), full.end());
    return packet;
  }
  const auto fields = CanonicalFields(snapshot);
  const auto old_fields = CanonicalFields(*baseline);
  const auto bit_bytes = (schema.Fields().size() + 7) / 8;
  Write(packet.bytes, static_cast<std::uint16_t>(bit_bytes));
  const auto bit_offset = packet.bytes.size();
  packet.bytes.resize(packet.bytes.size() + bit_bytes);
  for (std::size_t index = 0; index < schema.Fields().size(); ++index) {
    const auto id = schema.Fields()[index].id;
    const auto current =
        std::find_if(fields.begin(), fields.end(), [id](const auto &f) { return f.id == id; });
    const auto old = std::find_if(old_fields.begin(), old_fields.end(),
                                  [id](const auto &f) { return f.id == id; });
    if ((current == fields.end()) != (old == old_fields.end()) ||
        (current != fields.end() && *current != *old)) {
      packet.bytes[bit_offset + index / 8] |= static_cast<std::byte>(1u << (index % 8));
      Write(packet.bytes, static_cast<std::uint8_t>(current != fields.end()));
      if (current != fields.end()) {
        Write(packet.bytes, static_cast<std::uint32_t>(current->payload.size()));
        packet.bytes.insert(packet.bytes.end(), current->payload.begin(), current->payload.end());
      }
    }
  }
  return packet;
}

DeltaError DecodeDelta(const ReplicationSchema &schema, const DeltaPacket &packet,
                       const Snapshot *baseline, Snapshot &snapshot) {
  std::size_t offset{};
  std::uint32_t magic{};
  std::uint8_t full{};
  if (!Read(std::span{packet.bytes}, offset, magic) ||
      !Read(std::span{packet.bytes}, offset, full) || magic != kDeltaMagic || full > 1 ||
      static_cast<bool>(full) != packet.full_snapshot) {
    return DeltaError::Malformed;
  }
  if (full) {
    auto error = DecodeSnapshot(schema, std::span{packet.bytes}.subspan(offset), snapshot);
    return error == SnapshotError::None ? DeltaError::None : DeltaError::SnapshotRejected;
  }
  if (!baseline || baseline->id != packet.baseline_id) {
    return DeltaError::BaselineMissing;
  }
  std::uint16_t bit_bytes{};
  const auto expected = (schema.Fields().size() + 7) / 8;
  if (!Read(std::span{packet.bytes}, offset, bit_bytes) || bit_bytes != expected ||
      offset + bit_bytes > packet.bytes.size()) {
    return DeltaError::Malformed;
  }
  const auto bits = std::span{packet.bytes}.subspan(offset, bit_bytes);
  offset += bit_bytes;
  auto decoded = *baseline;
  decoded.id = packet.snapshot_id;
  for (std::size_t index = 0; index < schema.Fields().size(); ++index) {
    if ((std::to_integer<std::uint8_t>(bits[index / 8]) & (1u << (index % 8))) == 0) {
      continue;
    }
    std::uint8_t present{};
    if (!Read(std::span{packet.bytes}, offset, present) || present > 1) {
      return DeltaError::Malformed;
    }
    const auto id = schema.Fields()[index].id;
    std::erase_if(decoded.fields, [id](const auto &field) { return field.id == id; });
    if (present) {
      std::uint32_t size{};
      if (!Read(std::span{packet.bytes}, offset, size) || offset + size > packet.bytes.size()) {
        return DeltaError::Malformed;
      }
      decoded.fields.push_back(
          {id,
           schema.Fields()[index].wire_type,
           {packet.bytes.begin() + static_cast<std::ptrdiff_t>(offset),
            packet.bytes.begin() + static_cast<std::ptrdiff_t>(offset + size)}});
      offset += size;
    }
  }
  if (offset != packet.bytes.size()) {
    return DeltaError::Malformed;
  }
  std::sort(decoded.fields.begin(), decoded.fields.end(),
            [](const auto &a, const auto &b) { return a.id < b.id; });
  for (const auto &field : schema.Fields()) {
    if (field.compatibility == FieldCompatibility::Required &&
        std::none_of(decoded.fields.begin(), decoded.fields.end(),
                     [&field](const auto &value) { return value.id == field.id; })) {
      return DeltaError::SnapshotRejected;
    }
  }
  snapshot = std::move(decoded);
  return DeltaError::None;
}

void BaselineStore::Insert(Snapshot snapshot) {
  std::erase_if(snapshots_, [&snapshot](const auto &item) { return item.id == snapshot.id; });
  snapshots_.push_back(std::move(snapshot));
  while (snapshots_.size() > capacity_) {
    snapshots_.erase(snapshots_.begin());
  }
}

const Snapshot *BaselineStore::Find(std::uint64_t id) const noexcept {
  const auto found = std::find_if(snapshots_.begin(), snapshots_.end(),
                                  [id](const auto &snapshot) { return snapshot.id == id; });
  return found == snapshots_.end() ? nullptr : &*found;
}

InterestChanges InterestManager::Update(ConnectionID connection, const IInterestProvider &provider,
                                        std::size_t budget) {
  InterestChanges changes;
  if (connection == 0 || budget == 0) {
    return changes;
  }
  auto &state = states_[connection];
  auto query = provider.Query(connection, state.cursor, budget);
  changes.work = std::min(budget, query.entities.size());
  if (query.entities.size() > budget) {
    query.entities.resize(budget);
    query.complete = false;
  }
  state.pending.insert(state.pending.end(), query.entities.begin(), query.entities.end());
  state.cursor = query.next_cursor;
  if (!query.complete) {
    return changes;
  }
  std::sort(state.pending.begin(), state.pending.end(), EntityLess);
  state.pending.erase(std::unique(state.pending.begin(), state.pending.end()), state.pending.end());
  std::sort(state.active.begin(), state.active.end(), EntityLess);
  std::set_difference(state.pending.begin(), state.pending.end(), state.active.begin(),
                      state.active.end(), std::back_inserter(changes.spawn), EntityLess);
  std::set_difference(state.active.begin(), state.active.end(), state.pending.begin(),
                      state.pending.end(), std::back_inserter(changes.despawn), EntityLess);
  state.active = std::move(state.pending);
  state.pending.clear();
  state.cursor = 0;
  changes.complete = true;
  return changes;
}

void InterestManager::Remove(ConnectionID connection) { states_.erase(connection); }

bool InterestManager::Contains(ConnectionID connection, NetworkEntityID entity) const {
  const auto found = states_.find(connection);
  return found != states_.end() &&
         std::find(found->second.active.begin(), found->second.active.end(), entity) !=
             found->second.active.end();
}

} // namespace nexora::network
