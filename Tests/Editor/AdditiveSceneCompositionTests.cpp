#include "../../Engine/Editor/src/SceneCompositionTestAccess.h"
#include "Nexora/Editor/AdditiveSceneComposition.h"

#include <array>
#include <chrono>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>

namespace {
using namespace nexora;
using Status = editor::SceneCompositionStatus;
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
std::string Read(const std::filesystem::path &path) {
  std::ifstream input(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(input), {}};
}
void Write(const std::filesystem::path &path, std::string_view bytes) {
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
  Require(static_cast<bool>(output), "Fixture write failed");
}
struct Project final {
  std::filesystem::path root;
  editor::ProjectWorkspace workspace;
  const std::filesystem::path first_path = "Content/Primary.scene";
  const std::filesystem::path second_path = std::filesystem::path(u8"Content/第二.scene");
  std::array<std::string, 2> source;
  std::string metadata;
  explicit Project(const std::filesystem::path &path) : root(path) {
    Require(workspace.Create(root, "Composition"), "Project create failed");
    runtime::World world;
    const auto first_id = world.LoadScene("Primary");
    Require(world.Activate(first_id), "Primary activation failed");
    editor::SceneDocument first(world, first_id);
    editor::SceneFileSession first_files(workspace, first);
    const auto camera = first.CreateCamera("Primary camera");
    Require(camera && first.SetEulerField(std::array{*first.Key(camera)}, 1, 720) &&
                first_files.SaveAs(first_files.Token(), first_path).Applied(),
            "Primary source fixture failed");
    editor::AdditiveSceneSession session(workspace, world);
    Require(session.Attach(first, first_files) == first_id, "Primary attachment failed");
    const auto second = session.New("Second");
    Require(second.has_value(), "Second admission failed");
    auto *document = session.EditableDocument(*second);
    const auto light = document->CreateLight("Reference light");
    Require(light &&
                document->SetOpaqueComponent(*document->Key(light),
                                             {41, "Missing.Composition", {0, 7, 255}}) &&
                session.SaveAs(*second, session.Files(*second)->Token(), second_path).Applied() &&
                session.Remove(*second, session.Files(*second)->Token()),
            "Second source/release fixture failed");
    const auto reference = session.Open(second_path, false, {first_id});
    Require(reference.has_value(), "Reference admission failed");
    editor::AdditiveSceneComposition composition(session);
    Require(composition.Restore() == Status::Missing && composition.Save(),
            "First composition publication failed");
    source = {Read(root / first_path), Read(root / second_path)};
    metadata = Read(Metadata());
  }
  std::filesystem::path Metadata() const { return root / ".nexora/scene-composition.ini"; }
  std::string Header(std::string_view rows, std::size_t count = 2, std::size_t active = 1) const {
    return "NEXORA_SCENE_COMPOSITION 1\n" + workspace.Project().id.ToString() + "\n" +
           std::to_string(count) + " " + std::to_string(active) + "\n" + std::string(rows);
  }
  void Unchanged() const {
    Require(Read(root / first_path) == source[0] && Read(root / second_path) == source[1],
            "Composition metadata workflow changed a scene/opaque source");
  }
};
struct Reopened final {
  Project &project;
  const editor::ProjectWorkspace &workspace;
  runtime::World world;
  runtime::Id id = world.LoadScene("Reopened primary");
  editor::SceneDocument primary{world, id};
  editor::SceneFileSession files;
  editor::AdditiveSceneSession session;
  editor::AdditiveSceneComposition composition;
  explicit Reopened(Project &owner, const editor::ProjectWorkspace *access = nullptr)
      : project(owner), workspace(access ? *access : owner.workspace), files(workspace, primary),
        session(workspace, world), composition(session) {
    Require(world.Activate(id) && files.Open(files.Token(), project.first_path, true).Applied() &&
                session.Attach(primary, files) == id,
            "Clean bootstrap primary failed");
  }
};
void ReopenAndPolicies(const std::filesystem::path &root) {
  Project project(root);
  Require(editor::AdditiveSceneComposition::BootstrapScene(project.workspace) == project.first_path,
          "Saved load-order bootstrap was replaced by active reference selection");
  Reopened reopened(project);
  const auto token = reopened.files.Token();
  const auto primary_before = reopened.primary.CaptureRuntimeScene();
  Require(reopened.composition.Restore() == Status::Restored,
          "Complete composition restore failed");
  const auto rows = reopened.session.Snapshot();
  Require(rows.size() == 2 && rows[0].path == project.first_path && rows[0].owned &&
              rows[1].path == project.second_path && !rows[1].owned &&
              rows[1].dependencies == std::vector<editor::SceneDocumentId>{rows[0].id} &&
              reopened.session.Active() == rows[1].id && reopened.files.Token() == token &&
              reopened.primary.CaptureRuntimeScene() == primary_before &&
              reopened.world.ActiveSceneCount() == 2 &&
              !reopened.session.EditableDocument(rows[1].id) &&
              !reopened.session.WritableFiles(rows[1].id),
          "Restore lost load order, active role, stable primary or reference policy");
  const auto *reference = reopened.session.Document(rows[1].id);
  Require(reference->OpaqueComponents(*reference->Key(reference->Nodes()[0].id)) ==
              std::vector<editor::OpaqueComponent>{{41, "Missing.Composition", {0, 7, 255}}},
          "Composition reopen lost opaque payloads");
  Require(reopened.composition.Save() && Read(project.Metadata()) == project.metadata,
          "Unchanged composition did not round-trip deterministically");
  project.Unchanged();
  editor::ProjectWorkspace reader;
  Require(reader.Open(root, editor::ProjectAccess::ReadOnly), "Read-only observer open failed");
  Reopened observation(project, &reader);
  Require(observation.composition.Restore() == Status::Restored &&
              !observation.composition.Save() && !observation.session.SaveAll().Published() &&
              Read(project.Metadata()) == project.metadata,
          "Read-only restore wrote metadata or scene sources");
  project.Unchanged();
}
void RejectedMetadata(const std::filesystem::path &root) {
  Project project(root);
  Reopened reopened(project);
  const auto before = reopened.primary.CaptureRuntimeScene();
  const auto token = reopened.files.Token();
  const auto good_rows = "\"Content/Primary.scene\" 1 0\n\"Content/第二.scene\" 0 1 0\n";
  auto foreign = project.metadata;
  const auto uuid = project.workspace.Project().id.ToString();
  foreign.replace(foreign.find(uuid), uuid.size(), "00000000-0000-0000-0000-000000000000");
  const std::vector invalid{
      foreign,
      std::string(32 * 1024 + 1, 'x'),
      project.metadata + "unexpected\n",
      project.Header(good_rows, 17),
      project.Header(good_rows, 0),
      project.Header(good_rows, 2, 2),
      project.Header("\"Content/Primary.scene\" 1 1 0\n\"Content/第二.scene\" 0 0\n"),
      project.Header("\"Content/Primary.scene\" 1 0\n\"Content/第二.scene\" 0 1 1\n"),
      project.Header("\"Content/Primary.scene\" 1 0\n\"Content/primary.scene\" 0 1 0\n"),
      project.Header("\"Content/Primary.scene\" 1 0\n\"../outside.scene\" 0 1 0\n"),
      project.Header("\"Content/Primary.scene\" 1 0\n\"/absolute.scene\" 0 1 0\n"),
      project.Header("\"Content/Primary.scene\" 1 0\n\"Content/x.txt\" 0 1 0\n"),
      project.Header("\"Content/Primary.scene\" 1 0\n\"Content/第二.scene\" 2 1 0\n"),
      project.Header("\"Content/Primary.scene\" 1 0\n\"Content/第二.scene\" 0 2 0 0\n"),
      std::string("invalid\xff", 8),
      project.Header("\"Content/Primary.scene\" 1 0\n\"a\nb.scene\" 0 1 0\n")};
  for (const auto &bytes : invalid) {
    Write(project.Metadata(), bytes);
    std::string error;
    Require(!editor::AdditiveSceneComposition::BootstrapScene(project.workspace, &error) &&
                !error.empty() && reopened.composition.Restore() == Status::Rejected &&
                !reopened.composition.Save() && Read(project.Metadata()) == bytes &&
                reopened.session.Snapshot().size() == 1 &&
                reopened.session.Active() == reopened.id &&
                reopened.primary.CaptureRuntimeScene() == before && reopened.files.Token() == token,
            "Rejected composition modified metadata, primary membership, content or identity");
    project.Unchanged();
  }
  Write(project.Metadata(), project.metadata);
  Require(reopened.composition.Restore() == Status::Restored, "Valid metadata retry failed");
}
void SourceFailures(const std::filesystem::path &root) {
  Project project(root);
  Reopened reopened(project);
  const auto before = reopened.primary.CaptureRuntimeScene();
  Write(project.Metadata(),
        project.Header("\"Content/Primary.scene\" 1 0\n\"Content/Missing.scene\" 0 1 0\n"));
  for (unsigned attempt = 0; attempt < 64; ++attempt) {
    const auto probe = reopened.world.LoadScene("Probe");
    Require(reopened.world.RemoveEditorScene(probe) &&
                reopened.composition.Restore() == Status::Rejected &&
                !reopened.world.FindScene(probe + 1) && reopened.world.ActiveSceneCount() == 1 &&
                reopened.session.Snapshot().size() == 1 &&
                reopened.primary.CaptureRuntimeScene() == before,
            "Rejected source restore leaked candidate records or changed primary contents");
  }
  Write(root / "Content/Collision.scene", project.source[0]);
  Write(project.Metadata(),
        project.Header("\"Content/Primary.scene\" 1 0\n\"Content/Collision.scene\" 0 1 0\n"));
  Require(reopened.composition.Restore() == Status::Rejected &&
              Read(root / "Content/Collision.scene") == project.source[0] &&
              reopened.session.Snapshot().size() == 1 && reopened.world.ActiveSceneCount() == 1,
          "Source identity collision rewrote IDs, originals or membership");
  Write(project.Metadata(), project.metadata);
  Write(root / project.second_path, "corrupt source");
  Require(reopened.composition.Restore() == Status::Rejected &&
              Read(root / project.second_path) == "corrupt source" &&
              reopened.primary.CaptureRuntimeScene() == before,
          "Malformed source restore destroyed the primary or replaced its foreign source");
  Write(root / project.second_path, project.source[1]);
  Require(reopened.composition.Restore() == Status::Restored, "Source repair did not permit retry");
  project.Unchanged();
}
void PublicationAndGates(const std::filesystem::path &root) {
  Project project(root);
  Reopened reopened(project);
  Require(reopened.composition.Restore() == Status::Restored, "Publication restore failed");
  const auto primary_key = *reopened.primary.Key(reopened.primary.Nodes()[0].id);
  Require(reopened.primary.Rename(primary_key, "Unsaved authoring") &&
              reopened.session.Select(reopened.id, reopened.files.Token()),
          "Dirty active selection fixture failed");
  auto temporary = project.Metadata();
  temporary += ".tmp";
  Write(temporary, "foreign staging");
  Require(!reopened.composition.Save() && Read(temporary) == "foreign staging" &&
              Read(project.Metadata()) == project.metadata && reopened.primary.Dirty(),
          "Occupied metadata staging changed originals or acknowledged dirty authoring");
  std::filesystem::remove(temporary);
  Write(root / project.second_path, project.source[1] + "\nexternal");
  Require(!reopened.composition.Save() && Read(project.Metadata()) == project.metadata &&
              Read(root / project.second_path) == project.source[1] + "\nexternal",
          "Unresolved source revision admitted composition persistence or erased foreign bytes");
  Write(root / project.second_path, project.source[1]);
  Write(project.Metadata(), project.metadata + "\n");
  Require(!reopened.composition.Save() && Read(project.Metadata()) == project.metadata + "\n",
          "Exact-byte metadata external change was silently overwritten");
  Write(project.Metadata(), project.metadata);
  Require(reopened.composition.Save() && reopened.primary.Dirty() &&
              Read(project.Metadata()) != project.metadata,
          "Metadata save changed scene dirty baseline or failed to persist active selection");
  project.Unchanged();
  const auto last_good = Read(project.Metadata());
  const auto unnamed = reopened.session.New("Unnamed");
  Require(unnamed && !reopened.composition.Save() && Read(project.Metadata()) == last_good &&
              reopened.session.Remove(*unnamed, reopened.session.Files(*unnamed)->Token(), true),
          "Unnamed composition overwrote its previous metadata");
  Write(root / ".nexora/workspace.recovery", "pending recovery");
  Require(!reopened.composition.Save() && reopened.composition.Restore() == Status::Rejected &&
              Read(project.Metadata()) == last_good,
          "Recovery gate admitted composition writes or replacement");
  std::filesystem::remove(root / ".nexora/workspace.recovery");
  Require(project.workspace.Create(root / "new-project", "Replacement") &&
              !reopened.composition.Save() && reopened.composition.Restore() == Status::Rejected &&
              Read(root / ".nexora/scene-composition.ini") == last_good,
          "Old project scope revived composition IO");
}
void LateChangesAndPartialAdmission(const std::filesystem::path &root) {
  Project project(root);
  Reopened reopened(project);
  const auto before = reopened.primary.CaptureRuntimeScene();
  const auto token = reopened.files.Token();
  editor::SceneCompositionTestAccess::BeforeRestorePublish(
      reopened.composition, [&] { Write(project.Metadata(), project.metadata + "\n"); });
  Require(reopened.composition.Restore() == Status::Rejected &&
              Read(project.Metadata()) == project.metadata + "\n" &&
              reopened.session.Snapshot().size() == 1 && reopened.world.ActiveSceneCount() == 1 &&
              reopened.primary.CaptureRuntimeScene() == before && reopened.files.Token() == token,
          "Late metadata replacement published stale candidate membership");
  Write(project.Metadata(), project.metadata);
  editor::SceneCompositionTestAccess::BeforeRestorePublish(
      reopened.composition, [&] { Write(root / project.second_path, project.source[1] + "\n"); });
  Require(reopened.composition.Restore() == Status::Rejected &&
              Read(root / project.second_path) == project.source[1] + "\n" &&
              reopened.session.Snapshot().size() == 1 && reopened.world.ActiveSceneCount() == 1 &&
              reopened.primary.CaptureRuntimeScene() == before,
          "Late source replacement was overwritten or admitted a stale composition");
  Write(root / project.second_path, project.source[1]);
  editor::SceneCompositionTestAccess::BeforeRestorePublish(
      reopened.composition, [] { throw std::runtime_error("Interrupted before publication"); });
  Require(reopened.composition.Restore() == Status::Rejected &&
              reopened.session.Snapshot().size() == 1 && reopened.world.ActiveSceneCount() == 1,
          "Interrupted staged composition retained candidate owners");
  editor::SceneCompositionTestAccess::BeforeRestorePublish(reopened.composition, {});
  Write(root / "Content/Last.scene", "invalid last source");
  Write(project.Metadata(), project.Header("\"Content/Primary.scene\" 1 0\n"
                                           "\"Content/第二.scene\" 0 1 0\n"
                                           "\"Content/Last.scene\" 1 2 0 1\n",
                                           3, 2));
  Require(reopened.composition.Restore() == Status::Rejected &&
              reopened.session.Snapshot().size() == 1 && reopened.world.ActiveSceneCount() == 1 &&
              Read(root / "Content/Last.scene") == "invalid last source" &&
              reopened.primary.CaptureRuntimeScene() == before,
          "Last-source failure left an earlier admitted candidate live or changed its source");
  Write(project.Metadata(), project.metadata);
  Require(reopened.composition.Restore() == Status::Restored,
          "Late/partial failures prevented a complete restore retry");
  project.Unchanged();
}
void CapacityAndPayload(const std::filesystem::path &root) {
  editor::ProjectWorkspace workspace;
  Require(workspace.Create(root, "Sixteen scenes"), "Capacity workspace create failed");
  std::string metadata;
  {
    runtime::World world;
    const auto primary_id = world.LoadScene("Primary");
    Require(world.Activate(primary_id), "Capacity primary activation failed");
    editor::SceneDocument primary(world, primary_id);
    editor::SceneFileSession primary_files(workspace, primary);
    Require(primary.CreateCamera("Primary camera") &&
                primary_files.SaveAs(primary_files.Token(), "Content/Primary.scene").Applied(),
            "Capacity primary source failed");
    editor::AdditiveSceneSession session(workspace, world);
    Require(session.Attach(primary, primary_files) == primary_id, "Capacity attachment failed");
    auto previous = primary_id;
    for (std::size_t index = 1; index < 16; ++index) {
      const auto added = session.New("Scene " + std::to_string(index));
      Require(added.has_value(), "Genuine sixteen-document admission failed");
      const auto path = std::filesystem::path("Content/Scene" + std::to_string(index) + ".scene");
      auto *document = session.EditableDocument(*added);
      Require(document->CreateLight("Unique light") &&
                  session.SaveAs(*added, session.Files(*added)->Token(), path).Applied() &&
                  session.SetDependencies(*added, session.Files(*added)->Token(), {previous}),
              "Capacity source/dependency failed");
      previous = *added;
    }
    editor::AdditiveSceneComposition composition(session);
    Require(composition.Restore() == Status::Missing && composition.Save(),
            "Genuine sixteen-document metadata publication failed");
    metadata = Read(root / ".nexora/scene-composition.ini");
  }
  runtime::World world;
  const auto primary_id = world.LoadScene("Reopened primary");
  Require(world.Activate(primary_id), "Capacity reopen activation failed");
  editor::SceneDocument primary(world, primary_id);
  editor::SceneFileSession files(workspace, primary);
  Require(files.Open(files.Token(), "Content/Primary.scene", true).Applied(),
          "Capacity bootstrap failed");
  editor::AdditiveSceneSession session(workspace, world);
  Require(session.Attach(primary, files) == primary_id, "Capacity reopen attachment failed");
  editor::AdditiveSceneComposition composition(session);
  Require(composition.Restore() == Status::Restored && session.Snapshot().size() == 16 &&
              world.ActiveSceneCount() == 16 && session.Active() == session.Snapshot().back().id &&
              composition.Save() && Read(root / ".nexora/scene-composition.ini") == metadata,
          "Sixteen-document restore lost deterministic order, dependencies or active selection");
  const auto rows = session.Snapshot();
  for (std::size_t index = 1; index < rows.size(); ++index)
    Require(rows[index].dependencies == std::vector<editor::SceneDocumentId>{rows[index - 1].id},
            "Restored sixteen-document chain changed a dependency");
  // Real sparse sources stay below the per-file cap but exceed the aggregate cap before parsing.
  for (std::size_t index = 1; index < 16; ++index)
    std::filesystem::resize_file(root / ("Content/Scene" + std::to_string(index) + ".scene"),
                                 9 * 1024 * 1024);
  runtime::World probe_world;
  const auto probe_id = probe_world.LoadScene("Probe primary");
  Require(probe_world.Activate(probe_id), "Payload probe activation failed");
  editor::SceneDocument probe(probe_world, probe_id);
  editor::SceneFileSession probe_files(workspace, probe);
  Require(probe_files.Open(probe_files.Token(), "Content/Primary.scene", true).Applied(),
          "Payload bootstrap failed");
  editor::AdditiveSceneSession probe_session(workspace, probe_world);
  Require(probe_session.Attach(probe, probe_files) == probe_id, "Payload primary attach failed");
  editor::AdditiveSceneComposition probe_composition(probe_session);
  std::string error;
  Require(probe_composition.Restore(&error) == Status::Rejected &&
              error.find("aggregate payload budget") != std::string::npos &&
              probe_session.Snapshot().size() == 1 && probe_world.ActiveSceneCount() == 1 &&
              Read(root / ".nexora/scene-composition.ini") == metadata,
          "Aggregate source budget did not reject before candidate membership or metadata changes");
}
} // namespace
int main() {
  const auto root = std::filesystem::temp_directory_path() /
                    ("nexora-scene-composition-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  try {
    ReopenAndPolicies(root / "reopen");
    RejectedMetadata(root / "invalid");
    SourceFailures(root / "sources");
    PublicationAndGates(root / "publication");
    LateChangesAndPartialAdmission(root / "late");
    CapacityAndPayload(root / "capacity");
    std::filesystem::remove_all(root);
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << "\nFixture retained at " << root << '\n';
    return 1;
  }
}
