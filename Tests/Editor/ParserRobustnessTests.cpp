// Deterministic mutation test for the parsers that read persisted or external data.
//
// Each target starts from a valid encoding, applies seeded random mutations (bit flips, truncation,
// huge-length windows, numeric tokens that overflow), and requires the parser to return normally:
// no crash, no hang, no uncaught exception. Under the ASan/UBSan presets it also turns memory and
// undefined-behavior errors into failures. Seeds and iteration counts are fixed, so a failure
// reproduces; raise NEXORA_PARSER_ROBUSTNESS_ITERATIONS locally for a longer soak.

#include "Nexora/Core/RemoteDiagnostics.h"
#include "Nexora/Editor/EditorWorkspace.h"
#include "Nexora/Editor/SceneAuthoring.h"
#include "Nexora/Editor/ShaderAuthoring.h"
#include "Nexora/RHI/ShaderReflection.h"
#include "Nexora/Runtime/AssetPipeline.h"
#include "Nexora/Runtime/Runtime.h"
#include "Nexora/Runtime/ShaderRuntime.h"
#if defined(NEXORA_ROBUSTNESS_NETWORK)
#include "Nexora/Network/Prediction.h"
#endif

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {
using namespace nexora;
namespace fs = std::filesystem;
using Bytes = std::vector<std::byte>;

Bytes ToBytes(std::string_view text) {
  Bytes bytes(text.size());
  if (!text.empty())
    std::memcpy(bytes.data(), text.data(), text.size());
  return bytes;
}

std::string_view AsText(const Bytes &bytes) {
  return {reinterpret_cast<const char *>(bytes.data()), bytes.size()};
}

void WriteFile(const fs::path &path, const Bytes &bytes) {
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  out.write(reinterpret_cast<const char *>(bytes.data()),
            static_cast<std::streamsize>(bytes.size()));
}

Bytes ReadFile(const fs::path &path) {
  std::ifstream in(path, std::ios::binary);
  Bytes bytes;
  for (char c; in.get(c);)
    bytes.push_back(static_cast<std::byte>(c));
  return bytes;
}

void Mutate(Bytes &bytes, std::mt19937_64 &rng) {
  const auto pick = [&](std::size_t n) { return n == 0 ? std::size_t{0} : std::size_t(rng() % n); };
  const auto rounds = 1 + rng() % 4;
  for (std::uint64_t round = 0; round < rounds; ++round) {
    switch (rng() % 9) {
    case 0:
      if (!bytes.empty())
        bytes[pick(bytes.size())] ^= static_cast<std::byte>(1U << (rng() % 8));
      break;
    case 1:
      if (!bytes.empty())
        bytes[pick(bytes.size())] = static_cast<std::byte>(rng());
      break;
    case 2:
      if (!bytes.empty())
        bytes.resize(pick(bytes.size()));
      break;
    case 3:
      for (auto n = 1 + rng() % 16; n > 0; --n)
        bytes.push_back(static_cast<std::byte>(rng()));
      break;
    case 4: // a run of 0xFF turns any length/count field into a huge value
    case 5: // a run of zeros exercises empty and zero-length paths
      if (bytes.size() >= 2) {
        const auto at = pick(bytes.size());
        const auto fill = (rng() % 2 == 0) ? std::byte{0xFF} : std::byte{0};
        for (std::size_t i = at; i < bytes.size() && i < at + 1 + rng() % 8; ++i)
          bytes[i] = fill;
      }
      break;
    case 6:
      if (!bytes.empty()) {
        const auto at = pick(bytes.size());
        const auto n = std::min<std::size_t>(1 + rng() % 32, bytes.size() - at);
        const Bytes chunk(bytes.begin() + static_cast<std::ptrdiff_t>(at),
                          bytes.begin() + static_cast<std::ptrdiff_t>(at + n));
        bytes.insert(bytes.begin() + static_cast<std::ptrdiff_t>(pick(bytes.size())), chunk.begin(),
                     chunk.end());
      }
      break;
    case 7: {
      static constexpr std::string_view tokens[] = {"99999999999999999999",
                                                    "-1",
                                                    "18446744073709551616",
                                                    "0",
                                                    "4294967296",
                                                    "1e999",
                                                    "nan",
                                                    "\n",
                                                    " ",
                                                    "-",
                                                    "9223372036854775808"};
      const auto token = ToBytes(tokens[rng() % std::size(tokens)]);
      bytes.insert(bytes.begin() + static_cast<std::ptrdiff_t>(pick(bytes.size() + 1)),
                   token.begin(), token.end());
      break;
    }
    default:
      bytes.assign(rng() % 64, std::byte{});
      for (auto &value : bytes)
        value = static_cast<std::byte>(rng());
      break;
    }
  }
}

struct Target {
  std::string name;
  std::vector<Bytes> seeds;
  std::function<void(const Bytes &)> run;
};

std::uint64_t Iterations() {
  if (const auto *value = std::getenv("NEXORA_PARSER_ROBUSTNESS_ITERATIONS"); value && *value)
    return std::strtoull(value, nullptr, 10);
  return 2000;
}

int Run() {
  const auto root = fs::temp_directory_path() /
                    ("nexora-parser-robustness-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  fs::create_directories(root);
  struct Cleanup final {
    fs::path root;
    ~Cleanup() {
      std::error_code ec;
      fs::remove_all(root, ec);
    }
  } cleanup{root};

  std::vector<Target> targets;
  std::string error;

  {
    core::diagnostics::TraceEvent trace;
    trace.span_id = 7;
    trace.plugin_id = "plugin.alpha";
    trace.resource = "tex/a.png";
    trace.bytes = 4096;
    core::diagnostics::TraceEvent long_trace;
    long_trace.plugin_id.assign(40, 'x');
    long_trace.resource.assign(300, 'y');
    targets.push_back(
        {"trace_event",
         {core::diagnostics::EncodeTraceEvent(trace),
          core::diagnostics::EncodeTraceEvent(long_trace),
          {}},
         [](const Bytes &b) { static_cast<void>(core::diagnostics::DecodeTraceEvent(b)); }});
    core::diagnostics::MetricSample metric;
    metric.device_id = "dev0";
    metric.plugin_id = "p";
    metric.resident_memory_bytes = 1U << 20;
    targets.push_back(
        {"metric_sample", {core::diagnostics::EncodeMetricSample(metric), {}}, [](const Bytes &b) {
           static_cast<void>(core::diagnostics::DecodeMetricSample(b));
         }});
  }
#if defined(NEXORA_ROBUSTNESS_NETWORK)
  {
    network::ReplayLog log;
    log.RecordInput(1, {1, 2});
    log.RecordInput(2, {2, -3});
    network::AuthoritativeCorrection correction;
    correction.acknowledged_sequence = 1;
    log.RecordCorrection(3, correction);
    targets.push_back({"replay_log", {log.Encode(), {}}, [](const Bytes &b) {
                         network::ReplayLog decoded;
                         if (network::ReplayLog::Decode(b, decoded))
                           static_cast<void>(decoded.Replay());
                       }});
  }
#endif
  {
    editor::UnknownComponentStore store;
    store.Set(1, {42, "Mystery", {1, 2, 3, 4}});
    store.Set(2, {43, "Other", {}});
    targets.push_back({"unknown_components", {ToBytes(store.Serialize()), {}}, [](const Bytes &b) {
                         editor::UnknownComponentStore parsed;
                         static_cast<void>(parsed.Deserialize(AsText(b)));
                       }});
  }
  {
    // Seeds: a version 3 scene whose entities carry rotation, scale, and a parent chain, plus
    // hand-written version 2 and version 1 scenes, so every reader is mutated.
    runtime::World world;
    const auto scene = world.LoadScene("Main");
    static_cast<void>(world.Activate(scene));
    runtime::WorldCommandBuffer hierarchy;
    runtime::Id previous{};
    for (int index = 0; index < 3; ++index) {
      auto &entity = world.CreateEntity(scene);
      entity.transform = {1.0 * index, 2.0, 3.0};
      entity.transform.qy = entity.transform.qw = 0.70710678118654752440;
      entity.transform.sx = 1.5;
      entity.transform.sy = -2.0;
      if (previous != 0)
        hierarchy.SetParent(entity.id, previous, false);
      previous = entity.id;
    }
    if (!hierarchy.Apply(world))
      throw std::runtime_error("could not build the scene hierarchy seed");
    const auto version2 = ToBytes(
        "NEXORA_SCENE 2 \"rotated\" 0 1\n"
        "1 5 6 7 0 0.70710678118654752 0 0.70710678118654752 1 2 3 0 0 0 60 0.1 1000 1 0 0\n");
    const auto legacy = ToBytes("NEXORA_SCENE 1 \"legacy\" 0 2\n"
                                "1 5 6 7 0 0 0 60 0.1 1000 1 0 0\n"
                                "2 -1 0 1 1 0 1 72 0.25 750 2 3 4\n");
    targets.push_back({"scene_snapshot",
                       {ToBytes(world.SaveScene(scene).value_or("")), version2, legacy, {}},
                       [](const Bytes &b) {
                         runtime::World fresh;
                         static_cast<void>(fresh.LoadSceneSnapshot(AsText(b)));
                       }});
  }
  {
    runtime::RuntimeBlob blob;
    blob.type = "mesh";
    blob.payload = {std::byte{1}, std::byte{2}, std::byte{3}};
    blob.content_hash = "abc";
    targets.push_back(
        {"runtime_blob", {runtime::AssetCooker::Serialize(blob), {}}, [](const Bytes &b) {
           static_cast<void>(runtime::AssetCooker::Deserialize(b));
         }});
  }
  {
    rhi::ShaderModuleArtifact artifact;
    artifact.shader_id = "triangle";
    artifact.format = rhi::ShaderBinaryFormat::SpirV;
    artifact.entry_point = "vertexMain";
    artifact.binary = {std::byte{1}, std::byte{2}, std::byte{3}, std::byte{4}};
    artifact.reflection = rhi::TrianglePipelineLayout();
    Bytes cooked;
    if (!runtime::SerializeCookedShaderArtifact(artifact, cooked, error))
      throw std::runtime_error("could not build the cooked shader seed: " + error);
    targets.push_back({"nxshdr", {cooked, {}}, [](const Bytes &b) {
                         rhi::ShaderModuleArtifact parsed;
                         std::string message;
                         static_cast<void>(
                             runtime::DeserializeCookedShaderArtifact(b, parsed, message));
                       }});
  }
  targets.push_back(
      {"shader_diagnostics",
       {ToBytes(
            "error[E30015]: undefined identifier\n --> Shaders/A.slang:5:15\n  |\n5 | x\n  | ^\n"
            "--'\nwarning[E30081]: implicit\n --> a.slang:6:22\n"),
        ToBytes("a.slang:3:1: warning: unused\nb.slang:99:2: error: bad\n"),
        {}},
       [](const Bytes &b) {
         static_cast<void>(editor::ParseShaderDiagnostics(AsText(b), rhi::Backend::Vulkan, "V"));
       }});
  targets.push_back(
      {"asset_uuid",
       {ToBytes("123e4567-e89b-12d3-a456-426614174000"),
        ToBytes("00000000-0000-0000-0000-000000000000"),
        {}},
       [](const Bytes &b) { static_cast<void>(runtime::AssetUuid::Parse(AsText(b))); }});
  {
    const auto path = root / "autosave";
    editor::AutosaveJournal::Write(path, 9, "recoverable scene payload", &error);
    targets.push_back({"autosave", {ReadFile(path), {}}, [path](const Bytes &b) {
                         WriteFile(path, b);
                         std::uint64_t revision{};
                         static_cast<void>(
                             editor::AutosaveJournal::Recover(path, &revision, nullptr));
                       }});
  }
  {
    const auto path = root / "camera";
    editor::SceneCameraState camera;
    camera.pitch = 1.5;
    camera.yaw = -2.0;
    editor::CameraPersistence::Save(path, camera, &error);
    targets.push_back({"camera", {ReadFile(path), {}}, [path](const Bytes &b) {
                         WriteFile(path, b);
                         static_cast<void>(editor::CameraPersistence::Load(path, nullptr));
                       }});
  }
  {
    const auto project = root / "project";
    editor::ProjectWorkspace workspace;
    if (!workspace.Create(project, "Robustness", &error))
      throw std::runtime_error("could not create the seed project: " + error);
    const std::string documents[] = {"scenes/a.scene", "scenes/b.scene"};
    static_cast<void>(workspace.SaveWorkspace(documents, &error));
    static_cast<void>(
        workspace.SaveEditorLayout("[Window][Hierarchy]\nPos=0,0\nSize=100,100\n", &error));
    const auto file_target = [&](const char *name, fs::path relative, bool recover) {
      const auto file = project / relative;
      targets.push_back({name, {ReadFile(file), {}}, [project, file, recover](const Bytes &b) {
                           WriteFile(file, b);
                           editor::ProjectWorkspace opened;
                           std::string message;
                           static_cast<void>(opened.Open(project, &message));
                           static_cast<void>(opened.HasExternalChange());
                           if (recover)
                             static_cast<void>(opened.RecoverWorkspace(&message));
                           static_cast<void>(opened.LoadEditorLayout(&message));
                         }});
    };
    file_target("project_descriptor", "project.nexora", false);
    file_target("workspace_file", ".nexora/workspace", false);
    file_target("workspace_recovery", ".nexora/workspace.recovery", true);
    file_target("editor_layout", ".nexora/editor-layout.ini", false);
  }

  const auto iterations = Iterations();
  for (auto &target : targets) {
    // A different, fixed seed per target keeps failures reproducible by name and iteration.
    std::mt19937_64 rng(std::hash<std::string>{}(target.name) ^ 0x4E455830ULL);
    for (std::uint64_t index = 0; index < iterations; ++index) {
      Bytes input = target.seeds[rng() % target.seeds.size()];
      Mutate(input, rng);
      try {
        target.run(input);
      } catch (const std::exception &exception) {
        std::cerr << "parser '" << target.name << "' threw on mutation " << index << " ("
                  << input.size() << " bytes): " << exception.what() << '\n';
        return 1;
      }
    }
  }
  std::cout << "Parser robustness: " << targets.size() << " parsers x " << iterations
            << " mutations passed\n";
  return 0;
}
} // namespace

int main() {
  try {
    return Run();
  } catch (const std::exception &exception) {
    std::cerr << "parser robustness setup failed: " << exception.what() << '\n';
    return 1;
  }
}
