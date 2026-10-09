#include "Nexora/Editor/AdditiveSceneSession.h"

#include <array>
#include <chrono>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
using namespace nexora;
void Require(bool condition, const char *message) {
  if (!condition)
    throw std::runtime_error(message);
}
std::string Read(const std::filesystem::path &path) {
  std::ifstream input(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(input), {}};
}
void Write(const std::filesystem::path &path, std::string_view bytes) {
  std::ofstream output(path, std::ios::binary);
  output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
  Require(static_cast<bool>(output), "Fixture write failed");
}
void Editing(const std::filesystem::path &root) {
  editor::ProjectWorkspace workspace;
  Require(workspace.Create(root, "Session"), "Create project failed");
  runtime::World world;
  const auto primary_id = world.LoadScene("Primary");
  Require(world.Activate(primary_id), "Primary activation failed");
  editor::SceneDocument primary(world, primary_id);
  const auto camera = primary.CreateCamera("Primary camera");
  editor::SceneFileSession primary_files(workspace, primary);
  Require(camera && primary_files.SaveAs(primary_files.Token(), "Content/Primary.scene").Applied(),
          "Primary scene fixture failed");
  const auto original = Read(root / "Content/Primary.scene");
  std::filesystem::path second_path = std::filesystem::path(u8"Content/第二.scene");
  {
    editor::AdditiveSceneSession session(workspace, world);
    const auto attached = session.Attach(primary, primary_files);
    const auto second = session.New("Second");
    Require(attached == primary_id && second && session.Active() == second &&
                world.ActiveSceneCount() == 2,
            "Primary attachment or additive admission failed");
    auto *document = session.EditableDocument(*second);
    auto *files = session.WritableFiles(*second);
    const auto light = document->CreateLight("Second light");
    Require(light &&
                document->SetOpaqueComponent(*document->Key(light),
                                             {19, "Missing.Session", {0, 17, 255}}) &&
                session.SaveAs(*second, files->Token(), second_path).Applied(),
            "Second file fixture failed");
    const auto saved_second = Read(root / second_path);
    Require(session.SaveAs(*second, files->Token(), "Content/Primary.scene", true).status ==
                    editor::SceneFileStatus::Rejected &&
                Read(root / "Content/Primary.scene") == original &&
                files->CurrentPath() == second_path,
            "Save As replaced another open document or changed its managed association");
    const auto first_token = primary_files.Token(), second_token = files->Token();
    const auto first_generation = primary.Generation(), second_generation = document->Generation();
    Require(primary.Rename(*primary.Key(camera), "Primary edited") &&
                document->Rename(*document->Key(light), "Second edited") &&
                session.Select(*attached, first_token) && session.Active() == attached &&
                session.Select(*second, second_token) && session.Active() == second &&
                session.Document(*second) == document && primary.Generation() == first_generation &&
                document->Generation() == second_generation && primary.Dirty() && document->Dirty(),
            "Switching lost isolated dirty state, identity or stable document borrow");
    Require(!session.Select(*second, first_token) && !session.Remove(*second, second_token) &&
                session.Snapshot().size() == 2 &&
                Read(root / "Content/Primary.scene") == original &&
                Read(root / second_path) == saved_second,
            "Stale selection or unconfirmed removal changed documents or files");
    Require(session.SaveAll().Published() && !primary.Dirty() && !document->Dirty() &&
                primary_files.Token() == first_token && files->Token() == second_token &&
                primary.Undo() && primary.Dirty() && document->Undo() && document->Dirty() &&
                primary.Redo() && !primary.Dirty() && document->Redo() && !document->Dirty(),
            "Coordinated owner save lost baselines or per-document Undo/Redo");
    Require(session.SetDependencies(*second, second_token, {*attached, *attached}) &&
                session.Snapshot()[1].dependencies ==
                    std::vector<editor::SceneDocumentId>{*attached} &&
                !session.SetDependencies(*attached, first_token, {*second}) &&
                !session.Remove(*attached, first_token),
            "Dependency normalization, cycle rejection or required-scene removal failed");
    Require(!session.Open(second_path) && !session.Open("Content/primary.scene") &&
                session.Snapshot().size() == 2,
            "Duplicate or ASCII case-colliding path admission changed composition");
    Require(session.Remove(*second, second_token) && !world.FindScene(*second) &&
                session.Active() == attached && world.ActiveSceneCount() == 1,
            "Closing an owned document leaked a World scene or lost deterministic selection");
    const auto probe = world.LoadScene("Admission probe");
    const auto before_dependency_admission = Read(root / second_path);
    Require(world.RemoveEditorScene(probe) && !session.Open(second_path, true, {999999}) &&
                !world.FindScene(probe + 1) && session.Snapshot().size() == 1 &&
                session.Active() == attached &&
                Read(root / second_path) == before_dependency_admission,
            "Rejected dependency admission leaked an activated scene or changed composition");
    Require(session.Remove(*attached, first_token) && !session.Active() &&
                world.FindScene(primary_id) && world.ActiveSceneCount() == 1,
            "Detaching the borrowed primary destroyed its external owner");
  }
  Require(primary.Name(camera) == "Primary edited",
          "Session destruction expired the primary owner");
  runtime::World reopened;
  editor::AdditiveSceneSession reload(workspace, reopened);
  const auto first = reload.Open("Content/Primary.scene");
  Require(first.has_value(), "Primary independent reopen failed");
  const auto second = reload.Open(second_path, false, {*first});
  Require(first && second && reload.Document(*first)->Name(camera) == "Primary edited" &&
              reload.Document(*second)->Nodes().size() == 1 && !reload.EditableDocument(*second) &&
              !reload.WritableFiles(*second) &&
              reload.Document(*second)->OpaqueComponents(
                  *reload.Document(*second)->Key(reload.Document(*second)->Nodes()[0].id)) ==
                  std::vector<editor::OpaqueComponent>{{19, "Missing.Session", {0, 17, 255}}},
          "Independent reopen or inspection-only reference admission lost opaque data");
  const auto reference_bytes = Read(root / second_path);
  auto *first_document = reload.EditableDocument(*first);
  Require(first_document->Rename(*first_document->Key(camera), "Owned after reopen") &&
              reload.SaveAll().Published() && Read(root / second_path) == reference_bytes,
          "Save All wrote an inspection-only reference");
  auto stale = reload.Files(*first)->Token();
  Require(reload.WritableFiles(*first)->New(stale, true).Applied() &&
              !reload.Select(*first, stale) && !reload.Remove(*first, stale, true),
          "A replaced document revived a stale selection or removal request");
}
void FailureAndCapacity(const std::filesystem::path &root) {
  editor::ProjectWorkspace workspace;
  Require(workspace.Create(root, "Failures"), "Failure project create failed");
  runtime::World source;
  const auto source_scene = source.LoadScene("Colliding source");
  editor::SceneDocument document(source, source_scene);
  const auto camera = document.CreateCamera("Camera");
  Require(camera && document.Save(root / "Content/Collision.scene"),
          "Collision fixture save failed");
  Write(root / "Content/Broken.scene", "not a scene");
  runtime::World world;
  const auto sentinel = world.LoadScene("Sentinel");
  auto &entity = world.CreateEntity(sentinel);
  const auto occupied = entity.id;
  Require(occupied == camera, "Collision fixture identities differ");
  const auto sentinel_before = world.SaveScene(sentinel);
  editor::AdditiveSceneSession session(workspace, world);
  for (std::size_t i = 0; i != 64; ++i) {
    const auto before = world.LoadScene("Probe");
    Require(world.RemoveEditorScene(before) && !session.Open("Content/Collision.scene") &&
                !session.Open("Content/Broken.scene") && !world.FindScene(before + 1) &&
                !world.FindScene(before + 2) && world.SaveScene(sentinel) == sentinel_before &&
                session.Snapshot().empty(),
            "Rejected admission retained scene records, rewrote IDs or changed the existing scene");
  }
  std::vector<editor::SceneDocumentId> admitted;
  for (std::size_t i = 0; i != editor::AdditiveSceneSession::kMaximumDocuments; ++i) {
    const auto id = session.New("Bounded " + std::to_string(i));
    Require(id.has_value(), "Exact document capacity was rejected");
    admitted.push_back(*id);
  }
  const auto before = session.Active();
  Require(!session.New("Overflow") && session.Snapshot().size() == admitted.size() &&
              session.Active() == before && !session.SaveAll().Published(),
          "Capacity overflow or unnamed Save All changed admitted documents");
  for (const auto id : admitted)
    Require(session.Remove(id, session.Files(id)->Token(), true) && !world.FindScene(id),
            "Explicit dirty close leaked its owned scene record");
  Require(!session.Active() && session.Snapshot().empty() && world.FindScene(sentinel),
          "Composition removal changed a foreign scene");
  Write(root / ".nexora/workspace.recovery", "invalid retained recovery");
  Require(!session.New("Blocked") && !session.Open("Content/Collision.scene") &&
              !session.SaveAll().Published(),
          "Pending recovery admitted or published additive scenes");
}
void ReadOnlyAndTeardown(const std::filesystem::path &root) {
  editor::ProjectWorkspace writer;
  Require(writer.Create(root, "Read only"), "Reader project create failed");
  runtime::World source;
  const auto scene = source.LoadScene("Saved");
  editor::SceneDocument document(source, scene);
  Require(document.CreateLight("Light") && document.Save(root / "Content/Read.scene"),
          "Reader source fixture failed");
  editor::ProjectWorkspace reader;
  Require(reader.Open(root, editor::ProjectAccess::ReadOnly), "Read-only project open failed");
  runtime::World world;
  {
    editor::AdditiveSceneSession session(reader, world);
    const auto id = session.Open("Content/Read.scene");
    Require(id && !reader.Writable() && session.Document(*id) && !session.EditableDocument(*id) &&
                !session.WritableFiles(*id) && !session.New("Forbidden") &&
                !session.SaveAll().Published(),
            "Read-only observer received authoring or publication permissions");
  }
  Require(world.ActiveSceneCount() == 0 && !world.FindEntity(2),
          "Session destruction retained owned scene entities");
  runtime::World play(runtime::WorldKind::Play);
  const auto play_scene = play.LoadScene("Playing", true);
  Require(play.Activate(play_scene) && !play.RemoveEditorScene(play_scene) &&
              play.FindScene(play_scene) && !world.RemoveEditorScene(0),
          "Editor release bypassed Play lifecycle or admitted a missing scene");
  const auto persistent = world.LoadScene("Persistent editor", true);
  const auto persistent_entity = world.CreateEntity(persistent).id;
  Require(world.RemoveEditorScene(persistent) && !world.FindEntity(persistent_entity) &&
              world.LoadScene("Later") > persistent_entity,
          "Editor teardown retained persistent records or recycled object identities");
  auto near_limit = *source.SaveScene(scene);
  const auto start = near_limit.find('\n') + 1;
  const auto end = near_limit.find(' ', start);
  near_limit.replace(start, end - start,
                     std::to_string(std::numeric_limits<runtime::Id>::max() - 1));
  runtime::World exhausted;
  const auto loaded = exhausted.LoadSceneSnapshot(near_limit);
  Require(loaded && exhausted.RemoveEditorScene(*loaded),
          "Near-exhaustion scene could not be released");
  bool overflow = false;
  try {
    static_cast<void>(exhausted.LoadScene("No recycled identities"));
  } catch (const std::overflow_error &) {
    overflow = true;
  }
  Require(overflow && !exhausted.FindScene(*loaded),
          "Scene release rewound the exhausted allocation watermark");
  runtime::World unloading;
  const auto pending = unloading.LoadScene("Pending unload");
  editor::SceneDocument pending_document(unloading, pending);
  editor::SceneFileSession pending_files(writer, pending_document);
  editor::AdditiveSceneSession observer(writer, unloading);
  Require(unloading.Activate(pending) && unloading.RequestUnload(pending) &&
              !observer.Attach(pending_document, pending_files) && observer.Snapshot().empty() &&
              unloading.FindScene(pending)->state == runtime::SceneState::Unloading,
          "Attachment revived or removed an unloading foreign scene");
  unloading.EndFrame();
  Require(!observer.Attach(pending_document, pending_files) && unloading.FindScene(pending) &&
              unloading.FindScene(pending)->state == runtime::SceneState::Unloaded,
          "Attachment admitted an unloaded authoring scene");
}
void LiveComposition(const std::filesystem::path &root) {
  editor::ProjectWorkspace workspace;
  Require(workspace.Create(root, "Live composition"), "Live project create failed");
  runtime::World world;
  editor::AdditiveSceneSession session(workspace, world);
  const auto first = session.New("First"), second = session.New("Second");
  Require(first && second, "Live composition admission failed");
  const auto first_token = session.Files(*first)->Token();
  const auto second_token = session.Files(*second)->Token();
  Require(session.SaveAs(*first, first_token, "Content/First.scene").Applied() &&
              session.SaveAs(*second, second_token, "Content/Second.scene").Applied() &&
              session.SetDependencies(*second, second_token, {*first}),
          "Live composition save/dependency fixture failed");
  const auto first_bytes = Read(root / "Content/First.scene");
  const auto second_bytes = Read(root / "Content/Second.scene");
  Require(world.RequestUnload(*first) && !session.Document(*first) &&
              !session.SetDependencies(*second, second_token, {*first}) &&
              !session.SaveAll().Published() && Read(root / "Content/First.scene") == first_bytes &&
              Read(root / "Content/Second.scene") == second_bytes,
          "Unloading composition revived dependencies or published source files");
  world.EndFrame();
  Require(!session.SaveAll().Published() && session.Snapshot()[0].save_blocked &&
              Read(root / "Content/First.scene") == first_bytes &&
              Read(root / "Content/Second.scene") == second_bytes,
          "Unloaded composition was accepted for Save All");
}
void ProjectScope(const std::filesystem::path &root) {
  editor::ProjectWorkspace workspace;
  Require(workspace.Create(root / "original", "Original"), "Scope project create failed");
  runtime::World world;
  runtime::Id original{};
  {
    editor::AdditiveSceneSession session(workspace, world);
    const auto admitted = session.New("Before project switch");
    Require(admitted.has_value(), "Scope scene admission failed");
    original = *admitted;
    const auto token = session.Files(original)->Token();
    Require(workspace.Create(root / "replacement", "Replacement") && !session.Active() &&
                session.Snapshot().empty() && !session.Document(original) &&
                !session.Files(original) && !session.EditableDocument(original) &&
                !session.WritableFiles(original) && !session.Select(original, token) &&
                !session.Remove(original, token, true) && !session.New("Stale") &&
                !session.SaveAll().Published(),
            "Project switching revived authoring, membership or publication from an old scope");
    Require(world.FindScene(original), "Rejected old-scope operation destroyed live owners");
  }
  Require(!world.FindScene(original),
          "Old project scope prevented owner teardown from releasing its World record");
}
} // namespace
int main() {
  const auto root = std::filesystem::temp_directory_path() /
                    ("nexora-additive-session-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  try {
    Editing(root / "editing");
    FailureAndCapacity(root / "failure");
    ReadOnlyAndTeardown(root / "read-only");
    ProjectScope(root / "scope");
    LiveComposition(root / "live");
    std::filesystem::remove_all(root);
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << "\nFixture retained at " << root << '\n';
    return 1;
  }
}
