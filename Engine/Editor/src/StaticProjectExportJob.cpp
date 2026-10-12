#include "Nexora/Editor/StaticProjectExportJob.h"

#include "AtomicFile.h"
#include "Nexora/Runtime/ProjectPackage.h"

#include <algorithm>
#include <array>
#include <fstream>
#include <iomanip>
#include <limits>
#include <locale>
#include <mutex>
#include <sstream>
#include <stdexcept>

namespace nexora::editor {
namespace {
constexpr std::string_view kRelative = ".nexora/exports/static-view.nxproject";
constexpr std::string_view kVerify =
    "NexoraProjectPlayer --verify-package \".nexora/exports/static-view.nxproject\"";
bool Active(StaticExportPhase phase) {
  return phase >= StaticExportPhase::Queued && phase <= StaticExportPhase::ReadyToPublish;
}
std::string Diagnostic(std::string message) {
  if (!foundation::IsValidUtf8(message))
    return "StaticView export failed with an invalid UTF-8 diagnostic.";
  if (message.size() > 1024) {
    message.resize(1024);
    while (!foundation::IsValidUtf8(message))
      message.pop_back();
  }
  return message;
}
bool Missing(const std::filesystem::path &path) {
  std::error_code ec;
  const auto status = std::filesystem::symlink_status(path, ec);
  return ec == std::errc::no_such_file_or_directory ||
         (!ec && status.type() == std::filesystem::file_type::not_found);
}
// Same project-writer threat model as other workspace publishers. Reject existing aliases at
// every fixed output component. This is not isolation from hostile concurrent filesystem actors.
bool SafeParents(const std::filesystem::path &root, bool create) {
  std::error_code ec;
  for (const auto &path : {root / ".nexora", root / ".nexora/exports"}) {
    auto status = std::filesystem::symlink_status(path, ec);
    if ((ec == std::errc::no_such_file_or_directory || (!ec && !exists(status))) && create) {
      ec.clear();
      std::filesystem::create_directory(path, ec);
      if (ec)
        return false;
      status = std::filesystem::symlink_status(path, ec);
    }
    if (ec || !std::filesystem::is_directory(status))
      return false;
  }
  return true;
}
bool SafeOutput(const std::filesystem::path &root, bool create) {
  if (!SafeParents(root, create))
    return false;
  std::error_code ec;
  const auto status = std::filesystem::symlink_status(root / kRelative, ec);
  return ec == std::errc::no_such_file_or_directory ||
         (!ec && (status.type() == std::filesystem::file_type::not_found ||
                  std::filesystem::is_regular_file(status)));
}
bool Allowed(const ProjectWorkspace &workspace, const ProjectContentSession &content) {
  return workspace.Writable() && !workspace.Project().id.IsNil() &&
         !workspace.HasRecoveryJournal() && !workspace.HasExternalChange() &&
         workspace.Root() == content.Root() && content.Writable() && !content.ReimportBusy() &&
         content.Browser().ProjectGeneration() &&
         std::ranges::none_of(content.Conflicts().Conflicts(), [](const auto &conflict) {
           return conflict.choice == DirtyConflictChoice::Pending;
         });
}
std::string Checksum(std::span<const std::byte> bytes) {
  std::uint64_t value = 14695981039346656037ULL;
  for (const auto byte : bytes) {
    value ^= std::to_integer<unsigned char>(byte);
    value *= 1099511628211ULL;
  }
  std::ostringstream stream;
  stream.imbue(std::locale::classic());
  stream << std::hex << std::setfill('0') << std::setw(16) << value;
  return stream.str();
}
} // namespace

struct StaticProjectExportJob::Implementation final {
  struct Operation final {
    mutable std::mutex mutex;
    StaticExportSnapshot snapshot;
    core::CancellationSource cancellation;
    core::JobHandle job;
    std::filesystem::path root, stage;
    foundation::Uuid project;
    std::uint64_t generation{}, revision{};
    StaticProjectExportInput input;
    std::optional<SceneDocument::PreparedSave> prepared;
    std::vector<MeshAssetSnapshot> meshes;
    std::vector<MaterialAssetSnapshot> materials;
    // Worker writes this, owner consumes only after JobHandle becomes terminal and Wait returns.
    bool owns_stage{};
    bool consumed{};
    bool stage_stamped{};
    std::filesystem::file_time_type stage_time{};
    std::uintmax_t stage_size{};
    bool StageCurrent() const {
      if (!SafeParents(root, false))
        return false;
      std::error_code ec;
      const auto status = std::filesystem::symlink_status(stage, ec);
      if (ec || !std::filesystem::is_regular_file(status))
        return false;
      if (!stage_stamped)
        return true;
      const auto size = std::filesystem::file_size(stage, ec);
      if (ec || size != stage_size)
        return false;
      const auto time = std::filesystem::last_write_time(stage, ec);
      return !ec && time == stage_time;
    }
    void Set(StaticExportPhase phase, std::string message) {
      std::lock_guard lock{mutex};
      snapshot.phase = phase;
      snapshot.message = Diagnostic(std::move(message));
    }
    void Cleanup() noexcept {
      try {
        if (!owns_stage || !StageCurrent())
          return;
        std::error_code ec;
        std::filesystem::remove(stage, ec);
        if (!ec)
          owns_stage = false;
      } catch (...) {
      }
    }
  };
  explicit Implementation(core::JobSystem &value) : jobs(value) {}
  core::JobSystem &jobs;
  std::shared_ptr<Operation> operation;
  std::uint64_t next{1};
  bool stopped{};
  static void Run(const std::shared_ptr<Operation> &op, const core::CancellationToken &token) {
    const auto cancelled = [&] {
      if (!token.IsCancellationRequested())
        return false;
      op->Cleanup();
      op->Set(StaticExportPhase::Cancelled,
              "StaticView export cancelled; previous package retained.");
      return true;
    };
    try {
      if (cancelled())
        return;
      op->Set(StaticExportPhase::Cooking, "Cooking captured StaticView scene and imported assets.");
      std::string error;
      auto bytes = CookStaticProject(op->input, &error);
      if (cancelled())
        return;
      if (!bytes) {
        op->Set(StaticExportPhase::Failed, std::move(error));
        return;
      }
      op->Set(StaticExportPhase::Verifying,
              "Verifying the cooked package with the Runtime loader.");
      const auto loaded = runtime::DecodeStaticProjectPackage(*bytes, &error);
      if (cancelled())
        return;
      if (!loaded || loaded->ProjectId() != op->input.project ||
          loaded->SceneAssetId() != op->input.scene_asset) {
        op->Set(StaticExportPhase::Failed, "Runtime package verification failed: " + error);
        return;
      }
      if (!SafeOutput(op->root, false) || !Missing(op->stage)) {
        op->Set(StaticExportPhase::Failed,
                "Unsafe output or occupied staging path; nothing replaced.");
        return;
      }
      op->Set(StaticExportPhase::Staging, "Writing verified package staging.");
      std::ofstream output(op->stage, std::ios::binary | std::ios::trunc);
      if (!output) {
        op->Set(StaticExportPhase::Failed, "Could not open package staging.");
        return;
      }
      op->owns_stage = true;
      constexpr std::size_t chunk = 8192;
      for (std::size_t offset = 0; offset < bytes->size(); offset += chunk) {
        if (token.IsCancellationRequested()) {
          output.close();
          static_cast<void>(cancelled());
          return;
        }
        const auto size = std::min(chunk, bytes->size() - offset);
        output.write(reinterpret_cast<const char *>(bytes->data() + offset),
                     static_cast<std::streamsize>(size));
        if (!output)
          break;
      }
      output.close();
      if (output.fail()) {
        op->Cleanup();
        op->Set(StaticExportPhase::Failed,
                "Could not flush package staging; previous package retained.");
        return;
      }
      // Read the actual staged bytes, not only the in-memory candidate, before reporting readiness.
      std::ifstream input(op->stage, std::ios::binary);
      std::array<std::byte, chunk> buffer{};
      std::size_t offset{};
      while (input) {
        if (token.IsCancellationRequested()) {
          input.close();
          static_cast<void>(cancelled());
          return;
        }
        input.read(reinterpret_cast<char *>(buffer.data()),
                   static_cast<std::streamsize>(buffer.size()));
        const auto count = static_cast<std::size_t>(input.gcount());
        if (count > bytes->size() - offset ||
            !std::equal(buffer.begin(), buffer.begin() + static_cast<std::ptrdiff_t>(count),
                        bytes->begin() + static_cast<std::ptrdiff_t>(offset))) {
          input.close();
          op->Cleanup();
          op->Set(StaticExportPhase::Failed,
                  "Package staging verification differs from cooked bytes.");
          return;
        }
        offset += count;
      }
      if (!input.eof() || offset != bytes->size()) {
        input.close();
        op->Cleanup();
        op->Set(StaticExportPhase::Failed, "Could not read complete package staging.");
        return;
      }
      input.close();
      if (cancelled())
        return;
      std::error_code stamp_error;
      op->stage_size = std::filesystem::file_size(op->stage, stamp_error);
      if (stamp_error || op->stage_size != bytes->size())
        throw std::runtime_error("Package staging size could not be confirmed.");
      op->stage_time = std::filesystem::last_write_time(op->stage, stamp_error);
      if (stamp_error)
        throw std::runtime_error("Package staging revision could not be confirmed.");
      op->stage_stamped = true;
      {
        std::lock_guard lock{op->mutex};
        op->snapshot.bytes = static_cast<std::uint64_t>(bytes->size());
        op->snapshot.checksum = Checksum(*bytes);
      }
      op->Set(StaticExportPhase::ReadyToPublish,
              "Verified staging awaits current Editor state checks.");
    } catch (const std::exception &exception) {
      op->Cleanup();
      op->Set(StaticExportPhase::Failed, exception.what());
    } catch (...) {
      op->Cleanup();
      op->Set(StaticExportPhase::Failed, "StaticView export worker failed.");
    }
  }
};

StaticProjectExportJob::StaticProjectExportJob(core::JobSystem &jobs)
    : implementation_(std::make_unique<Implementation>(jobs)) {}
StaticProjectExportJob::~StaticProjectExportJob() { Shutdown(); }

bool StaticProjectExportJob::Start(const ProjectWorkspace &workspace, const SceneDocument &document,
                                   const ProjectContentSession &content,
                                   const MeshAssetCatalog &meshes,
                                   const MaterialAssetCatalog &materials,
                                   runtime::AssetUuid scene_asset, std::string *error) {
  if (error)
    error->clear();
  auto &impl = *implementation_;
  const auto reject = [&](const char *message) {
    if (error)
      *error = message;
    if (!Busy()) {
      auto observation = std::make_shared<Implementation::Operation>();
      observation->consumed = true;
      observation->snapshot.phase = StaticExportPhase::Failed;
      observation->snapshot.message = message;
      impl.operation = std::move(observation);
    }
    return false;
  };
  if (impl.stopped || Busy() || impl.next == std::numeric_limits<std::uint64_t>::max())
    return reject("StaticView export is busy, stopped or has exhausted operation identities.");
  if (!Allowed(workspace, content))
    return reject(
        "StaticView export requires the current writable project without recovery or conflicts.");
  const auto *scene = content.Browser().Find(scene_asset);
  if (!scene || scene->type != ".scene" || scene_asset == runtime::AssetUuid{})
    return reject("Save the current scene as a managed Content scene before exporting.");
  auto prepared = document.PrepareSave();
  auto capture = document.CaptureRuntimeScene(error);
  if (!prepared || !capture)
    return reject("Could not capture bounded current authoring scene content.");
  auto op = std::make_shared<Implementation::Operation>();
  op->root = workspace.Root();
  op->project = workspace.Project().id;
  op->generation = content.Browser().ProjectGeneration();
  op->revision = content.Browser().Revision();
  op->input = {{op->project.high, op->project.low}, scene_asset, std::move(*capture), {}, {}};
  op->prepared = std::move(prepared);
  std::size_t geometry_bytes{};
  for (const auto &item : content.Browser().Items()) {
    if (!item.mesh && !item.material)
      continue;
    if (op->input.meshes.size() + op->input.materials.size() ==
        runtime::kMaximumProjectPackageAssets - 1)
      return reject("StaticView export exceeds 4095 supplied imported assets.");
    if (item.mesh) {
      const auto current = meshes.ResolveAsset(item.id, op->generation);
      if (!current || current->geometry != item.mesh || item.mesh->vertices.empty() ||
          item.mesh->vertices.size() > runtime::kCookedMeshMaximumVertices ||
          item.mesh->indices.empty() ||
          item.mesh->indices.size() > runtime::kCookedMeshMaximumIndices)
        return reject("StaticView export requires bounded current imported mesh geometry.");
      const auto bytes = item.mesh->vertices.size() * 48 + item.mesh->indices.size() * 2;
      if (bytes > runtime::kMaximumProjectGeometryBytes - geometry_bytes)
        return reject("StaticView export exceeds the 128 MiB supplied geometry budget.");
      geometry_bytes += bytes;
      op->meshes.push_back(*current);
      op->input.meshes.push_back({item.id, *item.mesh});
    }
    if (item.material) {
      if (op->input.meshes.size() + op->input.materials.size() ==
          runtime::kMaximumProjectPackageAssets - 1)
        return reject("StaticView export exceeds 4095 supplied imported assets.");
      const auto current = materials.ResolveAsset(item.id, op->generation);
      if (!current || current->material != item.material)
        return reject("StaticView export requires current imported scalar materials.");
      op->materials.push_back(*current);
      op->input.materials.push_back({item.id, *item.material});
    }
  }
  // IO starts only after authoring capture/admission. Occupied staging is never owned or removed.
  if (!SafeOutput(op->root, true))
    return reject("StaticView export output directories or destination are unsafe.");
  op->stage = op->root / kRelative;
  op->stage += ".tmp";
  if (!Missing(op->stage))
    return reject("StaticView export staging is already occupied; previous files retained.");
  op->snapshot = {impl.next++,
                  StaticExportPhase::Queued,
                  "StaticView export queued.",
                  std::string(kRelative),
                  std::string(kVerify),
                  {},
                  0,
                  op->project,
                  op->input.capture.document_generation};
  try {
    op->job = impl.jobs.Submit(
        {[op](const core::CancellationToken &token) { Implementation::Run(op, token); },
         core::JobPriority::Low, op->cancellation.Token(), "StaticView package export"});
    if (!op->job.IsValid())
      return reject("StaticView export could not submit its worker.");
  } catch (...) {
    return reject("StaticView export could not submit its worker.");
  }
  impl.operation = std::move(op);
  return true;
}

bool StaticProjectExportJob::Poll(const ProjectWorkspace &workspace, const SceneDocument &document,
                                  const ProjectContentSession &content,
                                  const MeshAssetCatalog &meshes,
                                  const MaterialAssetCatalog &materials) {
  auto &impl = *implementation_;
  const auto op = impl.operation;
  if (!op || op->consumed)
    return false;
  const auto job_status = op->job.Status();
  if (job_status == core::JobStatus::Queued || job_status == core::JobStatus::Running)
    return false;
  try {
    impl.jobs.Wait(op->job);
  } catch (...) {
    op->Cleanup();
    op->Set(StaticExportPhase::Failed, "StaticView export job failed.");
  }
  const auto finish = [&] {
    op->Cleanup();
    if (op->owns_stage) {
      std::lock_guard lock{op->mutex};
      op->snapshot.message = Diagnostic(
          op->snapshot.message +
          " Staging was changed or could not be removed safely; inspect the package .tmp path.");
    }
    op->input = {};
    op->prepared.reset();
    op->meshes.clear();
    op->materials.clear();
    op->consumed = true;
    return true;
  };
  if (op->cancellation.Token().IsCancellationRequested() ||
      job_status == core::JobStatus::Cancelled) {
    op->Set(StaticExportPhase::Cancelled,
            "StaticView export cancelled; previous package retained.");
    return finish();
  }
  if (Snapshot().phase != StaticExportPhase::ReadyToPublish) {
    if (Active(Snapshot().phase))
      op->Set(StaticExportPhase::Failed, "StaticView export completed without verified staging.");
    return finish();
  }
  bool current = Allowed(workspace, content) && workspace.Root() == op->root &&
                 workspace.Project().id == op->project &&
                 content.Browser().ProjectGeneration() == op->generation &&
                 content.Browser().Revision() == op->revision && op->prepared &&
                 document.MatchesPreparedSave(*op->prepared);
  if (current) {
    const auto capture = document.CaptureRuntimeScene();
    current = capture && *capture == op->input.capture;
  }
  for (const auto &expected : op->meshes) {
    const auto value = meshes.ResolveAsset(expected.asset, op->generation);
    current = current && value && value->geometry == expected.geometry;
  }
  for (const auto &expected : op->materials) {
    const auto value = materials.ResolveAsset(expected.asset, op->generation);
    current = current && value && value->material == expected.material;
  }
  if (!current) {
    op->Set(StaticExportPhase::Stale,
            "Editor content or access changed; previous package retained.");
    return finish();
  }
  if (!op->owns_stage || !op->StageCurrent() || !SafeOutput(op->root, false)) {
    op->Set(StaticExportPhase::Failed,
            "Unsafe publication destination; previous package retained.");
    return finish();
  }
  // Cancellation, freshness and publication execute serially on the owner thread. No callbacks
  // can mutate authoring state between this final check and the single native replacement.
  std::error_code ec;
#if defined(_WIN32)
  if (!MoveFileExW(op->stage.c_str(), (op->root / kRelative).c_str(),
                   MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
    ec = std::error_code(static_cast<int>(GetLastError()), std::system_category());
#else
  std::filesystem::rename(op->stage, op->root / kRelative, ec);
#endif
  if (ec) {
    op->Set(StaticExportPhase::Failed, "Package publication failed; previous package retained.");
    return finish();
  }
  op->owns_stage = false;
  op->Set(StaticExportPhase::Published,
          "StaticView package ready. Run verification from the project root.");
  return finish();
}

bool StaticProjectExportJob::Cancel() noexcept {
  if (!implementation_->operation)
    return false;
  try {
    if (!Busy())
      return false;
    implementation_->operation->cancellation.Cancel();
    return true;
  } catch (...) {
    return false;
  }
}
bool StaticProjectExportJob::Busy() const {
  return implementation_->operation && !implementation_->operation->consumed;
}
StaticExportSnapshot StaticProjectExportJob::Snapshot() const {
  const auto op = implementation_->operation;
  if (!op)
    return {};
  std::lock_guard lock{op->mutex};
  return op->snapshot;
}
void StaticProjectExportJob::Shutdown() noexcept {
  auto &impl = *implementation_;
  impl.stopped = true;
  const auto op = impl.operation;
  if (!op)
    return;
  op->cancellation.Cancel();
  try {
    impl.jobs.Wait(op->job);
  } catch (...) {
  }
  op->Cleanup();
  try {
    if (Active(Snapshot().phase))
      op->Set(StaticExportPhase::Cancelled,
              "StaticView export stopped; previous package retained.");
    op->input = {};
    op->prepared.reset();
    op->meshes.clear();
    op->materials.clear();
    op->consumed = true;
  } catch (...) {
  }
}
} // namespace nexora::editor
