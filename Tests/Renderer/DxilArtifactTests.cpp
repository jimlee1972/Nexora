#include "Nexora/RHI/Device.h"
#include "Nexora/RHI/DxilArtifact.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using namespace nexora::rhi;
void Require(bool condition, const char *message) {
  if (!condition)
    throw std::runtime_error(message);
}
template <typename Function> void Reject(Function function, const char *message) {
  try {
    function();
  } catch (const std::invalid_argument &error) {
    Require(std::string_view{error.what()} == message, "wrong artifact rejection reason");
    return;
  }
  throw std::runtime_error("incompatible artifact was admitted");
}
void Put(std::vector<std::byte> &bytes, std::size_t offset, std::uint32_t value) {
  for (std::size_t index = 0; index < 4; ++index)
    bytes.at(offset + index) = static_cast<std::byte>((value >> (index * 8)) & 0xffU);
}
std::vector<std::byte> Metadata(std::uint32_t kind = 5, std::uint32_t minor = 0) {
  // Synthetic container/program metadata with LLVM magic, NOT executable/validated bitcode.
  std::vector<std::byte> bytes(72);
  Put(bytes, 0, 0x43425844); // DXBC
  Put(bytes, 20, 1);         // Container version 1.0
  Put(bytes, 24, static_cast<std::uint32_t>(bytes.size()));
  Put(bytes, 28, 1);
  Put(bytes, 32, 36);
  Put(bytes, 36, 0x4c495844); // DXIL part
  Put(bytes, 40, 28);
  Put(bytes, 44, (kind << 16U) | 0x60U | minor);
  Put(bytes, 48, 7);
  Put(bytes, 52, 0x4c495844); // Embedded bitcode header
  Put(bytes, 56, 0x100);
  Put(bytes, 60, 16);
  Put(bytes, 64, 4);
  Put(bytes, 68, 0xdec04342);
  return bytes;
}
void TestMetadataAdmission() {
  for (const auto &[kind, stage] :
       std::array{std::pair{0U, ShaderStage::Fragment}, std::pair{1U, ShaderStage::Vertex},
                  std::pair{5U, ShaderStage::Compute}}) {
    for (const std::uint32_t minor : {0U, 6U, 9U, 10U}) {
      const auto bytes = Metadata(kind, minor);
      const auto info = RequireDxilArtifact(bytes, stage, {6, static_cast<std::uint16_t>(minor)});
      Require(info.stage == stage &&
                  info.shader_model == FeatureVersion{6, static_cast<std::uint16_t>(minor)},
              "DXIL entry metadata decoded incorrectly");
    }
  }
  (void)RequireDxilArtifact(Metadata(5, 10), ShaderStage::Compute, {6, 9});
  Reject([&] { (void)RequireDxilArtifact(Metadata(5, 9), ShaderStage::Compute, {6, 10}); },
         "DXIL artifact shader model is too low");
  Reject([&] { (void)RequireDxilArtifact(Metadata(1), ShaderStage::Compute); },
         "DXIL artifact stage does not match the pipeline stage");
  std::vector<std::byte> unaligned{std::byte{0xff}};
  const auto valid = Metadata();
  unaligned.insert(unaligned.end(), valid.begin(), valid.end());
  Require(InspectDxilProgram(std::span{unaligned}.subspan(1)).has_value(),
          "unaligned input was not decoded safely");
  for (std::size_t size = 0; size < valid.size(); ++size)
    Require(!InspectDxilProgram(std::span{valid}.first(size)), "truncated container admitted");
  for (const auto &[offset, value] : std::array{
           std::pair{0U, 0U},           std::pair{20U, 2U},          std::pair{24U, 0xffffffffU},
           std::pair{28U, 0U},          std::pair{28U, 0xffffffffU}, std::pair{32U, 0U},
           std::pair{32U, 37U},         std::pair{32U, 0xfffffffcU}, std::pair{36U, 0U},
           std::pair{40U, 0xffffffffU}, std::pair{44U, 0x50050U},    std::pair{44U, 0x60060U},
           std::pair{44U, 0xd0060U},    std::pair{44U, 0x50160U},    std::pair{48U, 0U},
           std::pair{48U, 0xffffffffU}, std::pair{52U, 0U},          std::pair{60U, 0U},
           std::pair{60U, 0xffffffffU}, std::pair{64U, 0U},          std::pair{64U, 0xffffffffU},
           std::pair{68U, 0U}}) {
    auto broken = valid;
    Put(broken, offset, value);
    Require(!InspectDxilProgram(broken), "malformed or unsupported program metadata admitted");
  }
  std::vector<std::byte> parts(88);
  std::copy(valid.begin(), valid.begin() + 32, parts.begin());
  Put(parts, 24, 88);
  Put(parts, 28, 2);
  Put(parts, 32, 76); // Offset table may be in a different order than physical parts.
  Put(parts, 36, 40);
  std::copy(valid.begin() + 36, valid.end(), parts.begin() + 40);
  Put(parts, 76, 0x42444c49); // Unrelated part
  Put(parts, 80, 4);
  Require(InspectDxilProgram(parts).has_value(), "valid unordered part table rejected");
  auto overlap = parts;
  Put(overlap, 32, 48);
  Require(!InspectDxilProgram(overlap), "overlapping part ranges admitted");
  parts.resize(112);
  Put(parts, 24, 112);
  std::copy(valid.begin() + 36, valid.end(), parts.begin() + 76);
  Require(!InspectDxilProgram(parts), "duplicate executable DXIL parts admitted");
  Reject([&] { (void)RequireDxilArtifact({}, ShaderStage::Compute); },
         "DXIL artifact has invalid or unsupported program metadata");
  std::cout << "Portable DXIL metadata: stages, numeric 6.9/6.10 profiles, truncation, invalid "
               "ranges, overlap, unordered parts and duplicate rejection passed\n";
}
#if defined(_WIN32)
std::vector<std::byte> Read(const char *path) {
  std::ifstream stream{path, std::ios::binary | std::ios::ate};
  Require(stream.good() && stream.tellg() > 0, "native DXIL artifact is unreadable");
  std::vector<std::byte> bytes(static_cast<std::size_t>(stream.tellg()));
  stream.seekg(0);
  Require(static_cast<bool>(stream.read(reinterpret_cast<char *>(bytes.data()),
                                        static_cast<std::streamsize>(bytes.size()))),
          "native DXIL artifact read failed");
  return bytes;
}
void TestNativeArtifacts() {
  const auto *vertex = std::getenv("NEXORA_SLANG_DXIL_VERTEX_PATH");
  const auto *fragment = std::getenv("NEXORA_SLANG_DXIL_FRAGMENT_PATH");
  const auto *compute = std::getenv("NEXORA_SLANG_COMPUTE_DXIL_PATH");
  if (!vertex || !*vertex || !fragment || !*fragment || !compute || !*compute ||
      !IsBackendAvailable(Backend::Direct3D12)) {
    const auto *required = std::getenv("NEXORA_REQUIRE_NATIVE_BACKENDS");
    Require(!(required && *required == '1'), "native DXIL artifacts/device are required");
    std::cout << "Optional native DXIL admission unavailable; portable checks completed\n";
    return;
  }
  const auto vs = RequireDxilArtifact(Read(vertex), ShaderStage::Vertex, {6, 6});
  const auto ps = RequireDxilArtifact(Read(fragment), ShaderStage::Fragment, {6, 6});
  const auto cs = RequireDxilArtifact(Read(compute), ShaderStage::Compute, {6, 0});
  Require(vs.shader_model == FeatureVersion{6, 6} && ps.shader_model == FeatureVersion{6, 6} &&
              cs.shader_model == FeatureVersion{6, 0},
          "actual Slang DXIL profiles differ from configured baselines");
  auto device = CreateDevice(Backend::Direct3D12);
  for (const auto type : {PipelineType::Graphics, PipelineType::Compute}) {
    PipelineDescriptor descriptor{1, 1, TextureFormat::Rgba8Unorm, "artifact profile admission"};
    descriptor.type = type;
    const auto baseline = type == PipelineType::Compute ? cs.shader_model : vs.shader_model;
    descriptor.graphics_requirements.minimum_dxil_shader_model = baseline;
    const auto pipeline = device->CreatePipeline(descriptor);
    device->DestroyPipeline(pipeline);
    const FeatureVersion newer{baseline.major, static_cast<std::uint16_t>(baseline.minor + 1)};
    if (device->Capabilities().highest_dxil_shader_model >= newer) {
      descriptor.graphics_requirements.minimum_dxil_shader_model = newer;
      Reject([&] { (void)device->CreatePipeline(descriptor); },
             "DXIL artifact shader model is too low");
    }
  }
  Require(device->Diagnostics().submitted_command_lists == 0,
          "artifact admission unexpectedly submitted native work");
  std::cout << "Native Slang DXIL: vertex/fragment SM6.6, compute SM6.0; baseline PSOs and "
               "device-supported newer artifact requirement rejection passed\n";
}
#endif
} // namespace
int main() {
  try {
    TestMetadataAdmission();
#if defined(_WIN32)
    TestNativeArtifacts();
#endif
  } catch (const std::exception &error) {
    std::cerr << "FAIL: " << error.what() << '\n';
    return 1;
  }
}
