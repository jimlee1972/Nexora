#include "Nexora/Runtime/ShaderRuntime.h"

#include <algorithm>
#include <array>
#include <fstream>
#include <limits>
#include <utility>

namespace nexora::runtime {
namespace {
constexpr std::array<char, 8> kCookedMagic{'N', 'X', 'S', 'H', 'D', 'R', '\0', '\1'};
constexpr std::uint32_t kCookedVersion = 1;
constexpr std::size_t kCookedHeaderSize = 52;
constexpr std::uint32_t kMaxCookedString = 1U << 20U;
constexpr std::uint32_t kMaxCookedBinary = 256U << 20U;
constexpr std::uint32_t kMaxCookedBindings = 4096;

void AppendU32(std::vector<std::byte> &bytes, std::uint32_t value) {
  for (std::uint32_t shift = 0; shift < 32; shift += 8)
    bytes.push_back(static_cast<std::byte>((value >> shift) & 0xffU));
}
void AppendU64(std::vector<std::byte> &bytes, std::uint64_t value) {
  for (std::uint32_t shift = 0; shift < 64; shift += 8)
    bytes.push_back(static_cast<std::byte>((value >> shift) & 0xffU));
}
bool ReadU32(std::span<const std::byte> bytes, std::size_t &offset, std::uint32_t &value) {
  if (offset > bytes.size() || bytes.size() - offset < 4)
    return false;
  value = 0;
  for (std::uint32_t shift = 0; shift < 32; shift += 8)
    value |= static_cast<std::uint32_t>(std::to_integer<unsigned char>(bytes[offset++])) << shift;
  return true;
}
bool ReadU64(std::span<const std::byte> bytes, std::size_t &offset, std::uint64_t &value) {
  if (offset > bytes.size() || bytes.size() - offset < 8)
    return false;
  value = 0;
  for (std::uint32_t shift = 0; shift < 64; shift += 8)
    value |= static_cast<std::uint64_t>(std::to_integer<unsigned char>(bytes[offset++])) << shift;
  return true;
}
std::uint64_t HashBytes(std::span<const std::byte> bytes) {
  constexpr std::uint64_t kOffset = 1469598103934665603ULL;
  constexpr std::uint64_t kPrime = 1099511628211ULL;
  auto hash = kOffset;
  for (const auto byte : bytes) {
    hash ^= std::to_integer<unsigned char>(byte);
    hash *= kPrime;
  }
  return hash;
}
bool ValidArtifactShape(const rhi::ShaderModuleArtifact &artifact, std::string &error) {
  if (artifact.shader_id.empty() || artifact.entry_point.empty() || artifact.binary.empty() ||
      artifact.format < rhi::ShaderBinaryFormat::Dxil ||
      artifact.format > rhi::ShaderBinaryFormat::MetalSource) {
    error = "cooked shader artifact has an empty identity, entry point, or payload";
    return false;
  }
  if (artifact.shader_id.size() > kMaxCookedString ||
      artifact.entry_point.size() > kMaxCookedString || artifact.binary.size() > kMaxCookedBinary ||
      artifact.reflection.bindings.size() > kMaxCookedBindings ||
      artifact.reflection.schema_version != 1 || artifact.reflection.layout_hash == 0 ||
      rhi::ComputeLayoutHash(artifact.reflection.bindings) != artifact.reflection.layout_hash) {
    error = "cooked shader artifact shape or canonical reflection is invalid";
    return false;
  }
  return true;
}
} // namespace

bool SerializeCookedShaderArtifact(const rhi::ShaderModuleArtifact &artifact,
                                   std::vector<std::byte> &output, std::string &error) {
  if (!ValidArtifactShape(artifact, error))
    return false;
  output.clear();
  output.reserve(kCookedHeaderSize + artifact.shader_id.size() + artifact.entry_point.size() +
                 artifact.binary.size() + artifact.reflection.bindings.size() * 20);
  for (const auto byte : kCookedMagic)
    output.push_back(static_cast<std::byte>(byte));
  AppendU32(output, kCookedVersion);
  output.push_back(static_cast<std::byte>(artifact.format));
  output.insert(output.end(), 3, std::byte{0});
  AppendU32(output, static_cast<std::uint32_t>(artifact.shader_id.size()));
  AppendU32(output, static_cast<std::uint32_t>(artifact.entry_point.size()));
  AppendU32(output, static_cast<std::uint32_t>(artifact.binary.size()));
  AppendU32(output, static_cast<std::uint32_t>(artifact.reflection.bindings.size()));
  AppendU32(output, artifact.reflection.schema_version);
  AppendU64(output, artifact.reflection.layout_hash);
  AppendU64(output, 0);
  const auto append_string = [&output](const std::string &value) {
    const auto *begin = reinterpret_cast<const std::byte *>(value.data());
    output.insert(output.end(), begin, begin + value.size());
  };
  append_string(artifact.shader_id);
  append_string(artifact.entry_point);
  output.insert(output.end(), artifact.binary.begin(), artifact.binary.end());
  for (const auto &binding : artifact.reflection.bindings) {
    AppendU64(output, binding.resource_id);
    AppendU32(output, binding.binding);
    output.push_back(static_cast<std::byte>(binding.type));
    output.push_back(static_cast<std::byte>(binding.stages));
    output.insert(output.end(), 2, std::byte{0});
    AppendU32(output, binding.byte_size);
  }
  const auto payload_hash =
      HashBytes(std::span<const std::byte>(output).subspan(kCookedHeaderSize));
  for (std::uint32_t index = 0; index < 8; ++index)
    output[44 + index] = static_cast<std::byte>((payload_hash >> (index * 8U)) & 0xffU);
  error.clear();
  return true;
}

bool DeserializeCookedShaderArtifact(std::span<const std::byte> input,
                                     rhi::ShaderModuleArtifact &artifact, std::string &error) {
  if (input.size() < kCookedHeaderSize ||
      !std::equal(kCookedMagic.begin(), kCookedMagic.end(), input.begin(),
                  [](char expected, std::byte actual) {
                    return static_cast<unsigned char>(expected) ==
                           std::to_integer<unsigned char>(actual);
                  })) {
    error = "cooked shader artifact magic/version is invalid";
    return false;
  }
  std::size_t offset = 8;
  std::uint32_t version = 0;
  if (!ReadU32(input, offset, version) || version != kCookedVersion) {
    error = "cooked shader artifact version is unsupported";
    return false;
  }
  const auto format = std::to_integer<unsigned char>(input[offset++]);
  if (format > static_cast<unsigned char>(rhi::ShaderBinaryFormat::MetalSource)) {
    error = "cooked shader artifact format is invalid";
    return false;
  }
  offset += 3;
  std::uint32_t shader_size = 0, entry_size = 0, binary_size = 0, binding_count = 0, schema = 0;
  std::uint64_t layout_hash = 0, payload_hash = 0;
  if (!ReadU32(input, offset, shader_size) || !ReadU32(input, offset, entry_size) ||
      !ReadU32(input, offset, binary_size) || !ReadU32(input, offset, binding_count) ||
      !ReadU32(input, offset, schema) || !ReadU64(input, offset, layout_hash) ||
      !ReadU64(input, offset, payload_hash) || shader_size > kMaxCookedString ||
      entry_size > kMaxCookedString || binary_size > kMaxCookedBinary ||
      binding_count > kMaxCookedBindings) {
    error = "cooked shader artifact header is invalid";
    return false;
  }
  if (HashBytes(input.subspan(kCookedHeaderSize)) != payload_hash) {
    error = "cooked shader artifact payload checksum mismatch";
    return false;
  }
  const auto binding_bytes = static_cast<std::size_t>(binding_count) * 20U;
  if (offset > input.size() || input.size() - offset < static_cast<std::size_t>(shader_size) +
                                                           entry_size + binary_size +
                                                           binding_bytes) {
    error = "cooked shader artifact is truncated";
    return false;
  }
  artifact = {};
  artifact.format = static_cast<rhi::ShaderBinaryFormat>(format);
  artifact.shader_id.assign(reinterpret_cast<const char *>(input.data() + offset), shader_size);
  offset += shader_size;
  artifact.entry_point.assign(reinterpret_cast<const char *>(input.data() + offset), entry_size);
  offset += entry_size;
  artifact.binary.assign(input.begin() + static_cast<std::ptrdiff_t>(offset),
                         input.begin() + static_cast<std::ptrdiff_t>(offset + binary_size));
  offset += binary_size;
  artifact.reflection.schema_version = schema;
  artifact.reflection.layout_hash = layout_hash;
  artifact.reflection.bindings.reserve(binding_count);
  for (std::uint32_t index = 0; index < binding_count; ++index) {
    rhi::ShaderBinding binding;
    if (!ReadU64(input, offset, binding.resource_id) || !ReadU32(input, offset, binding.binding) ||
        offset + 4 > input.size()) {
      error = "cooked shader artifact binding table is truncated";
      return false;
    }
    binding.type = static_cast<rhi::BindingType>(std::to_integer<unsigned char>(input[offset++]));
    binding.stages = std::to_integer<unsigned char>(input[offset++]);
    offset += 2;
    if (!ReadU32(input, offset, binding.byte_size) ||
        binding.type > rhi::BindingType::StorageBuffer || binding.stages == 0 ||
        (binding.stages & ~static_cast<std::uint8_t>(7)) != 0) {
      error = "cooked shader artifact binding metadata is invalid";
      return false;
    }
    artifact.reflection.bindings.push_back(binding);
  }
  if (offset != input.size()) {
    error = "cooked shader artifact has trailing bytes after the binding table";
    return false;
  }
  if (!ValidArtifactShape(artifact, error))
    return false;
  error.clear();
  return true;
}

bool LoadCookedShaderArtifact(const std::filesystem::path &path,
                              rhi::ShaderModuleArtifact &artifact, std::string &error) {
  std::ifstream input(path, std::ios::binary | std::ios::ate);
  if (!input) {
    error = "unable to open cooked shader artifact: " + path.string();
    return false;
  }
  const auto size = input.tellg();
  if (size <= 0 || static_cast<std::uint64_t>(size) > kMaxCookedBinary + (8U << 20U)) {
    error = "cooked shader artifact file size is invalid";
    return false;
  }
  std::vector<std::byte> bytes(static_cast<std::size_t>(size));
  input.seekg(0);
  input.read(reinterpret_cast<char *>(bytes.data()), size);
  if (!input) {
    error = "unable to read cooked shader artifact: " + path.string();
    return false;
  }
  return DeserializeCookedShaderArtifact(bytes, artifact, error);
}
ShaderArtifactSlot::ShaderArtifactSlot(rhi::Backend backend, std::uint64_t expected_layout_hash,
                                       NativeShaderModuleCallbacks native)
    : backend_(backend),
#if defined(NEXORA_SHIPPING_ENABLED) && NEXORA_SHIPPING_ENABLED
      mode_(ShaderBuildMode::Shipping),
#else
      mode_(ShaderBuildMode::Development),
#endif
      expected_layout_hash_(expected_layout_hash), native_(std::move(native)) {
}

ShaderArtifactSlot::~ShaderArtifactSlot() {
  if (!native_.destroy)
    return;
  if (native_module_ != 0)
    native_.destroy(native_module_);
  for (const auto &retired : retired_) {
    if (retired.native_module != 0)
      native_.destroy(retired.native_module);
  }
}

bool ShaderArtifactSlot::Stage(rhi::ShaderModuleArtifact artifact, ShaderArtifactSource source,
                               std::string &error) {
  if (source == ShaderArtifactSource::DynamicCompile && mode_ == ShaderBuildMode::Shipping) {
    error = "Shipping Runtime accepts cooked shader artifacts only";
    return false;
  }
  if (!rhi::IsArtifactCompatible(artifact, backend_, expected_layout_hash_)) {
    error = "shader artifact target, payload, or canonical reflection is incompatible";
    return false;
  }
  if (active_ && active_->shader_id != artifact.shader_id) {
    error = "shader reload cannot change the active shader identity";
    return false;
  }
  staged_ = std::move(artifact);
  error.clear();
  return true;
}

bool ShaderArtifactSlot::StageCookedFile(const std::filesystem::path &path, std::string &error) {
  rhi::ShaderModuleArtifact artifact;
  if (!LoadCookedShaderArtifact(path, artifact, error))
    return false;
  return Stage(std::move(artifact), ShaderArtifactSource::Cooked, error);
}

bool ShaderArtifactSlot::Commit(std::string &error) { return Commit(0, error); }

bool ShaderArtifactSlot::Commit(std::uint64_t retire_fence, std::string &error) {
  if (!staged_) {
    error = "no validated shader artifact is staged";
    return false;
  }
  if (generation_ == std::numeric_limits<std::uint64_t>::max()) {
    error = "shader artifact generation is exhausted";
    return false;
  }
  NativeShaderModuleHandle candidate_module = 0;
  if (native_.create) {
    candidate_module = native_.create(*staged_, error);
    if (candidate_module == 0) {
      if (error.empty())
        error = "native shader-module creation failed";
      // Roll the transaction back: a candidate the backend rejected must not stay staged, or the
      // next Commit() would silently retry the same rejected artifact.
      staged_.reset();
      return false;
    }
  }
  if (active_)
    retired_.push_back({retire_fence, std::move(*active_), native_module_});
  active_ = std::move(staged_);
  native_module_ = candidate_module;
  staged_.reset();
  ++generation_;
  error.clear();
  return true;
}

void ShaderArtifactSlot::DiscardStaged() noexcept { staged_.reset(); }

std::size_t ShaderArtifactSlot::CollectRetired(std::uint64_t completed_fence) noexcept {
  const auto before = retired_.size();
  retired_.erase(std::remove_if(retired_.begin(), retired_.end(),
                                [this, completed_fence](const RetiredArtifact &artifact) {
                                  if (artifact.retire_fence > completed_fence)
                                    return false;
                                  if (artifact.native_module != 0 && native_.destroy)
                                    native_.destroy(artifact.native_module);
                                  return true;
                                }),
                 retired_.end());
  return before - retired_.size();
}

const rhi::ShaderModuleArtifact *ShaderArtifactSlot::Active() const noexcept {
  return active_ ? &*active_ : nullptr;
}
} // namespace nexora::runtime
