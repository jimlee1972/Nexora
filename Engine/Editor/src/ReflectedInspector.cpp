#include "Nexora/Editor/ReflectedInspector.h"
#include <algorithm>
#include <bit>
#include <charconv>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <limits>
#include <locale>
#include <set>
#include <sstream>

namespace nexora::editor {
namespace {
constexpr std::array names{"bool",    "int64", "uint64",           "double",
                           "enum",    "flags", "vector2",          "vector3",
                           "vector4", "color", "entity_reference", "asset_reference"};
bool Safe(std::string_view text) {
  if (text.empty() || text.size() > 256 || !foundation::IsValidUtf8(text))
    return false;
  for (std::size_t i = 0; i < text.size(); ++i) {
    const auto c = static_cast<unsigned char>(text[i]);
    if (c < 32 || c == 127 ||
        (c == 0xc2 && i + 1 < text.size() && static_cast<unsigned char>(text[i + 1]) >= 0x80 &&
         static_cast<unsigned char>(text[i + 1]) <= 0x9f))
      return false;
  }
  return true;
}
std::size_t Width(ReflectedKind kind) {
  switch (kind) {
  case ReflectedKind::Boolean:
    return 1;
  case ReflectedKind::Vector2:
  case ReflectedKind::AssetReference:
    return 16;
  case ReflectedKind::Vector3:
    return 24;
  case ReflectedKind::Vector4:
  case ReflectedKind::Color:
    return 32;
  default:
    return 8;
  }
}
std::uint64_t Read64(std::span<const std::uint8_t> bytes, std::size_t offset) {
  std::uint64_t value{};
  for (std::size_t i = 0; i < 8; ++i)
    value |= std::uint64_t{bytes[offset + i]} << (8 * i);
  return value;
}
void Write64(std::span<std::uint8_t> bytes, std::size_t offset, std::uint64_t value) {
  for (std::size_t i = 0; i < 8; ++i)
    bytes[offset + i] = static_cast<std::uint8_t>(value >> (8 * i));
}
bool Allowed(const ReflectedProperty &property, const ReflectedValue &value) {
  switch (property.kind) {
  case ReflectedKind::Boolean:
    return std::holds_alternative<bool>(value);
  case ReflectedKind::Integer:
    return std::holds_alternative<std::int64_t>(value);
  case ReflectedKind::Number: {
    const auto p = std::get_if<double>(&value);
    return p && std::isfinite(*p);
  }
  case ReflectedKind::Vector2:
  case ReflectedKind::Vector3:
  case ReflectedKind::Vector4:
  case ReflectedKind::Color: {
    const auto p = std::get_if<std::array<double, 4>>(&value);
    if (!p)
      return false;
    for (std::size_t i = 0; i < Width(property.kind) / 8; ++i)
      if (!std::isfinite((*p)[i]) ||
          (property.kind == ReflectedKind::Color && ((*p)[i] < 0 || (*p)[i] > 1)))
        return false;
    return true;
  }
  case ReflectedKind::AssetReference:
    return std::holds_alternative<foundation::Uuid>(value);
  default: {
    const auto p = std::get_if<std::uint64_t>(&value);
    if (!p)
      return false;
    if (property.kind == ReflectedKind::Enum)
      return std::ranges::any_of(property.choices, [&](const auto &c) { return c.value == *p; });
    if (property.kind == ReflectedKind::Flags) {
      std::uint64_t mask{};
      for (const auto &c : property.choices)
        mask |= c.value;
      return (*p & ~mask) == 0;
    }
    return true;
  }
  }
}
std::optional<ReflectedValue> Decode(const ReflectedProperty &property,
                                     std::span<const std::uint8_t> bytes) {
  const auto offset = property.offset;
  if (offset > bytes.size() || Width(property.kind) > bytes.size() - offset)
    return {};
  ReflectedValue value;
  switch (property.kind) {
  case ReflectedKind::Boolean:
    if (bytes[offset] > 1)
      return {};
    value = bytes[offset] != 0;
    break;
  case ReflectedKind::Integer:
    value = std::bit_cast<std::int64_t>(Read64(bytes, offset));
    break;
  case ReflectedKind::Number:
    value = std::bit_cast<double>(Read64(bytes, offset));
    break;
  case ReflectedKind::Vector2:
  case ReflectedKind::Vector3:
  case ReflectedKind::Vector4:
  case ReflectedKind::Color: {
    std::array<double, 4> lanes{};
    for (std::size_t i = 0; i < Width(property.kind) / 8; ++i)
      lanes[i] = std::bit_cast<double>(Read64(bytes, offset + i * 8));
    value = lanes;
    break;
  }
  case ReflectedKind::AssetReference:
    value = foundation::Uuid{Read64(bytes, offset), Read64(bytes, offset + 8)};
    break;
  default:
    value = Read64(bytes, offset);
    break;
  }
  return Allowed(property, value) ? std::optional(value) : std::nullopt;
}
void Encode(const ReflectedProperty &property, const ReflectedValue &value,
            std::span<std::uint8_t> bytes) {
  const auto offset = property.offset;
  if (const auto boolean = std::get_if<bool>(&value))
    bytes[offset] = *boolean ? 1 : 0;
  else if (const auto integer = std::get_if<std::int64_t>(&value))
    Write64(bytes, offset, std::bit_cast<std::uint64_t>(*integer));
  else if (const auto number = std::get_if<std::uint64_t>(&value))
    Write64(bytes, offset, *number);
  else if (const auto floating = std::get_if<double>(&value))
    Write64(bytes, offset, std::bit_cast<std::uint64_t>(*floating));
  else if (const auto uuid = std::get_if<foundation::Uuid>(&value)) {
    Write64(bytes, offset, uuid->high);
    Write64(bytes, offset + 8, uuid->low);
  } else {
    const auto &lanes = std::get<std::array<double, 4>>(value);
    for (std::size_t i = 0; i < Width(property.kind) / 8; ++i)
      Write64(bytes, offset + i * 8, std::bit_cast<std::uint64_t>(lanes[i]));
  }
}
bool Unsigned(std::istream &stream, std::uint64_t &value) {
  std::string word;
  if (!(stream >> word) || word.empty())
    return false;
  const auto parsed = std::from_chars(word.data(), word.data() + word.size(), value);
  return parsed.ec == std::errc{} && parsed.ptr == word.data() + word.size();
}
bool End(std::istream &stream) {
  std::string extra;
  return !(stream >> extra);
}
} // namespace
bool ReflectedInspector::SetComponents(std::vector<ReflectedComponent> components) {
  if (components.size() > kMaximumTypes || revision_ == std::numeric_limits<std::uint64_t>::max())
    return false;
  runtime::ReflectionRegistry reflection;
  std::size_t metadata_bytes{};
  const auto bounded_name = [&](std::string_view name) {
    if (!Safe(name) || name.size() > kMaximumSchemaBytes - metadata_bytes)
      return false;
    metadata_bytes += name.size();
    return true;
  };
  for (const auto &component : components) {
    if (!component.type || !bounded_name(component.name) || !component.bytes ||
        component.bytes > UnknownComponentStore::kMaximumComponentBytes ||
        component.fields.empty() || component.fields.size() > kMaximumFields)
      return false;
    runtime::TypeDescriptor descriptor{component.name, component.type, {}};
    std::set<std::string> paths;
    std::set<std::string> expanded_paths;
    std::vector<std::pair<std::size_t, std::size_t>> ranges;
    for (const auto &field : component.fields) {
      if (!bounded_name(field.path) || !paths.insert(field.path).second ||
          static_cast<std::size_t>(field.kind) >= names.size() || !field.elements ||
          field.elements > kMaximumElements || field.offset > component.bytes)
        return false;
      const auto size = Width(field.kind) * field.elements;
      for (std::size_t element = 0; element < field.elements; ++element)
        if (!expanded_paths
                 .insert(field.path +
                         (field.elements > 1 ? "[" + std::to_string(element) + "]" : ""))
                 .second)
          return false;
      if (size > component.bytes - field.offset)
        return false;
      for (const auto &[begin, end] : ranges)
        if (field.offset < end && begin < field.offset + size)
          return false;
      ranges.emplace_back(field.offset, field.offset + size);
      if (field.kind == ReflectedKind::Enum || field.kind == ReflectedKind::Flags) {
        if (field.choices.empty() || field.choices.size() > 32)
          return false;
        std::set<std::uint64_t> values;
        std::set<std::string> labels;
        for (const auto &choice : field.choices)
          if (!bounded_name(choice.label) || choice.label.find("##") != std::string::npos ||
              !values.insert(choice.value).second || !labels.insert(choice.label).second ||
              (field.kind == ReflectedKind::Flags &&
               (!choice.value || !std::has_single_bit(choice.value))))
            return false;
      } else if (!field.choices.empty())
        return false;
      descriptor.fields.push_back(
          {field.path, runtime::HashTypeName(names[static_cast<std::size_t>(field.kind)]),
           field.offset, size});
    }
    if (!reflection.Register(std::move(descriptor)))
      return false;
  }
  components_ = std::move(components);
  reflection_ = std::move(reflection);
  ++revision_;
  return true;
}
std::optional<ReflectedObservation>
ReflectedInspector::Inspect(const SceneDocument &scene,
                            std::span<const SceneDocument::NodeKey> keys,
                            runtime::TypeId type) const {
  const auto found = std::ranges::find(components_, type, &ReflectedComponent::type);
  if (found == components_.end() || keys.empty() || keys.size() > kMaximumTargets ||
      found->bytes > kMaximumObservationBytes / keys.size())
    return {};
  ReflectedObservation result{revision_, *found, {}, {}};
  std::set<runtime::Id> unique;
  for (const auto key : keys) {
    if (!unique.insert(key.id).second)
      return {};
    const auto components = scene.OpaqueComponents(key);
    if (!components)
      return {};
    const auto value = std::ranges::find(*components, type, &OpaqueComponent::type);
    if (value == components->end() || value->type_name != found->name ||
        value->data.size() != found->bytes)
      return {};
    result.sources.emplace_back(key, *value);
  }
  const auto *metadata = reflection_.FindById(type);
  if (!metadata || metadata->fields.size() != found->fields.size())
    return {};
  for (const auto &field : found->fields)
    for (std::size_t element = 0; element < field.elements; ++element) {
      ReflectedProperty property{
          field.path + (field.elements > 1 ? "[" + std::to_string(element) + "]" : ""),
          field.kind,
          field.offset + Width(field.kind) * element,
          field.choices,
          {},
          false};
      const auto first = Decode(property, result.sources.front().second.data);
      if (!first)
        return {};
      property.value = *first;
      for (const auto &source : result.sources) {
        const auto next = Decode(property, source.second.data);
        if (!next)
          return {};
        property.mixed |= *next != property.value;
      }
      result.properties.push_back(std::move(property));
    }
  return result;
}
bool ReflectedInspector::Apply(SceneDocument &scene, const ReflectedObservation &observation,
                               std::size_t property, const ReflectedValue &value,
                               bool authorized) const {
  return ApplyEdit(scene, observation, property, value, authorized, {});
}
bool ReflectedInspector::ApplyFlag(SceneDocument &scene, const ReflectedObservation &observation,
                                   std::size_t property, std::uint64_t bit, bool enabled,
                                   bool authorized) const {
  return ApplyEdit(scene, observation, property, std::uint64_t{enabled ? bit : 0}, authorized, bit);
}
bool ReflectedInspector::ApplyEdit(SceneDocument &scene, const ReflectedObservation &observation,
                                   std::size_t property, const ReflectedValue &value,
                                   bool authorized, std::optional<std::uint64_t> flag_bit) const {
  if (!authorized || observation.catalog_revision != revision_ || observation.sources.empty() ||
      observation.sources.size() > kMaximumTargets ||
      observation.sources.size() != scene.Selection().size())
    return false;
  std::vector<SceneDocument::NodeKey> keys;
  for (std::size_t i = 0; i < observation.sources.size(); ++i) {
    if (observation.sources[i].first.id != scene.Selection()[i])
      return false;
    keys.push_back(observation.sources[i].first);
  }
  const auto fresh = Inspect(scene, keys, observation.component.type);
  if (!fresh || fresh->component != observation.component ||
      fresh->sources != observation.sources || property >= fresh->properties.size() ||
      property >= observation.properties.size() ||
      observation.properties[property].path != fresh->properties[property].path ||
      observation.properties[property].offset != fresh->properties[property].offset ||
      observation.properties[property].kind != fresh->properties[property].kind ||
      !Allowed(fresh->properties[property], value))
    return false;
  const auto &field = fresh->properties[property];
  if (flag_bit && (field.kind != ReflectedKind::Flags ||
                   !std::ranges::any_of(field.choices, [&](const auto &choice) {
                     return choice.value == *flag_bit;
                   })))
    return false;
  std::vector<SceneDocument::OpaqueComponentEdit> edits;
  for (const auto &[key, expected] : fresh->sources) {
    auto replacement = expected;
    if (flag_bit) {
      const auto current = Read64(expected.data, field.offset);
      Encode(field,
             std::uint64_t{std::get<std::uint64_t>(value) ? current | *flag_bit
                                                          : current & ~*flag_bit},
             replacement.data);
    } else
      Encode(field, value, replacement.data);
    edits.push_back({key, expected, std::move(replacement)});
  }
  return scene.ApplyOpaqueComponents(edits);
}
std::optional<ReflectedInspector> ReflectedInspector::LoadProject(const std::filesystem::path &root,
                                                                  std::string *error) {
  const auto fail = [&]() -> std::optional<ReflectedInspector> {
    if (error)
      *error = "Invalid or over-budget Inspector reflection metadata; component bytes preserved.";
    return {};
  };
  std::error_code ec;
  const auto path = root / ".nexora/inspector.reflection";
  const auto status = std::filesystem::symlink_status(path, ec);
  if ((!ec && status.type() == std::filesystem::file_type::not_found) ||
      ec == std::errc::no_such_file_or_directory)
    return ReflectedInspector{};
  if (ec || !std::filesystem::is_regular_file(status) ||
      std::filesystem::hard_link_count(path, ec) != 1 || ec)
    return fail();
  std::ifstream file(path, std::ios::binary);
  std::string bytes(kMaximumSchemaBytes + 1, '\0');
  if (!file || !file.read(bytes.data(), static_cast<std::streamsize>(bytes.size())).eof() ||
      file.bad() || static_cast<std::size_t>(file.gcount()) > kMaximumSchemaBytes)
    return fail();
  bytes.resize(static_cast<std::size_t>(file.gcount()));
  if (!foundation::IsValidUtf8(bytes) || bytes.find('\0') != std::string::npos)
    return fail();
  std::istringstream input(bytes);
  input.imbue(std::locale::classic());
  std::string line;
  if (!std::getline(input, line) || line != "NXEDITORREFLECTION 1")
    return fail();
  std::vector<ReflectedComponent> components;
  while (std::getline(input, line)) {
    if (components.size() == kMaximumTypes)
      return fail();
    std::istringstream parser(line);
    parser.imbue(std::locale::classic());
    std::string tag;
    ReflectedComponent component;
    std::uint64_t size{}, fields{};
    if (!(parser >> tag) || tag != "type" || !Unsigned(parser, component.type) ||
        !(parser >> std::quoted(component.name)) || !Unsigned(parser, size) ||
        size > UnknownComponentStore::kMaximumComponentBytes || !Unsigned(parser, fields) ||
        !fields || fields > kMaximumFields || !End(parser))
      return fail();
    component.bytes = static_cast<std::size_t>(size);
    for (std::uint64_t i = 0; i < fields; ++i) {
      if (!std::getline(input, line))
        return fail();
      std::istringstream field_line(line);
      field_line.imbue(std::locale::classic());
      ReflectedField field;
      std::string kind;
      std::uint64_t offset{}, count{}, choices{};
      if (!(field_line >> tag >> std::quoted(field.path) >> kind) || tag != "field" ||
          !Unsigned(field_line, offset) || offset > component.bytes ||
          !Unsigned(field_line, count) || count > kMaximumElements ||
          !Unsigned(field_line, choices) || choices > 32 || !End(field_line))
        return fail();
      const auto found = std::ranges::find(names, kind);
      if (found == names.end())
        return fail();
      field.kind = static_cast<ReflectedKind>(std::distance(names.begin(), found));
      field.offset = static_cast<std::size_t>(offset);
      field.elements = static_cast<std::size_t>(count);
      for (std::uint64_t c = 0; c < choices; ++c) {
        if (!std::getline(input, line))
          return fail();
        std::istringstream choice_line(line);
        choice_line.imbue(std::locale::classic());
        ReflectedChoice choice;
        if (!(choice_line >> tag) || tag != "choice" || !Unsigned(choice_line, choice.value) ||
            !(choice_line >> std::quoted(choice.label)) || !End(choice_line))
          return fail();
        field.choices.push_back(std::move(choice));
      }
      component.fields.push_back(std::move(field));
    }
    components.push_back(std::move(component));
  }
  ReflectedInspector catalog;
  if (!catalog.SetComponents(std::move(components)))
    return fail();
  return catalog;
}
} // namespace nexora::editor
