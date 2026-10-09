#include "Nexora/Runtime/Runtime.h"

#include <iostream>
#include <limits>
#include <locale>
#include <stdexcept>

namespace {
using namespace nexora::runtime;

void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}

class GroupedComma final : public std::numpunct<char> {
protected:
  char do_decimal_point() const override { return ','; }
  char do_thousands_sep() const override { return '_'; }
  std::string do_grouping() const override { return "\3"; }
};

class LocaleGuard final {
public:
  LocaleGuard()
      : previous_(std::locale::global(std::locale(std::locale::classic(), new GroupedComma))) {}
  ~LocaleGuard() { std::locale::global(previous_); }

private:
  std::locale previous_;
};

void HistoricWireAndBoundaries() {
  World world;
  const auto scene = world.LoadScene("Scene\"\\\n界", true);
  auto &entity = world.CreateEntity(scene);
  entity.id = std::numeric_limits<Id>::max() - 1;
  entity.transform = {1.25, -2.5, 0.125};
  entity.transform.qz = 1;
  entity.transform.qw = 0;
  entity.transform.sx = 2;
  entity.transform.sy = -3;
  entity.transform.sz = 0.5;
  entity.camera = entity.light = entity.mesh_renderer = true;
  entity.camera_data = {45, 0.5, 64};
  entity.light_data.intensity = 1.5F;
  entity.mesh_data.mesh = std::numeric_limits<Id>::max();
  entity.mesh_data.material.shader = 0xfedcba9876543210ULL;

  // Fixed schema-3 output from the existing serializer: quoted escapes, UTF-8 bytes, component
  // order, classic decimal formatting and complete unsigned 64-bit identities remain unchanged.
  const std::string expected =
      "NEXORA_SCENE 3 \"Scene\\\"\\\\\n界\" 1 1\n"
      "18446744073709551614 0 1.25 -2.5 0.125 0 0 1 0 2 -3 0.5 1 1 1 45 0.5 64 1.5 "
      "18446744073709551615 18364758544493064720\n";
  const auto legacy = world.SaveScene(scene);
  Require(legacy && *legacy == expected, "historical schema-3 serializer bytes changed");
  for (std::size_t cap = 0; cap < expected.size(); ++cap)
    Require(!world.SaveScene(scene, cap),
            "under-budget save returned a partial/oversized snapshot");
  Require(world.SaveScene(scene, expected.size()) == legacy &&
              world.SaveScene(scene, expected.size() + 1) == legacy &&
              world.SaveScene(scene, std::numeric_limits<std::size_t>::max()) == legacy,
          "exact or unlimited output budget changed bytes");
  {
    LocaleGuard locale;
    Require(world.SaveScene(scene) == legacy && world.SaveScene(scene, expected.size()) == legacy,
            "global comma/grouping locale changed canonical scene bytes");
  }
  World restored;
  const auto loaded = restored.LoadSceneSnapshot(expected);
  Require(loaded && restored.SaveScene(*loaded, expected.size()) == legacy,
          "bounded output did not load/roundtrip through actual World");
  const auto *restored_entity = restored.FindEntity(entity.id);
  Require(restored_entity && restored_entity->mesh_data.material.shader == 0xfedcba9876543210ULL &&
              restored_entity->mesh_data.mesh == std::numeric_limits<Id>::max(),
          "full resource/shader IDs truncated during bounded roundtrip");
}

void PrecisionHierarchyAndLifetime() {
  World world;
  const auto scene = world.LoadScene("Precision hierarchy");
  auto &root = world.CreateEntity(scene);
  root.transform = {0.1, -0.2, 0.3};
  root.transform.sy = 2.5;
  const auto parent = root.id;
  auto &child = world.CreateEntity(scene);
  child.parent = parent;
  child.transform.x = 1.0 / 7.0;
  child.camera = true;
  child.camera_data = {60, 0.1, 1000};
  const auto child_id = child.id;
  Require(world.Activate(scene), "fixture scene activation failed");
  const auto *scene_borrow = world.FindScene(scene);
  const auto *entity_borrow = world.FindEntity(child_id);
  const auto before = world.SaveScene(scene);
  Require(before && before->find("0.10000000000000001") != std::string::npos &&
              before->find("0.14285714285714285") != std::string::npos,
          "setprecision(17) was lost");
  const auto pose = world.WorldTransform(child_id);
  const auto snapshot = world.SaveScene(scene, before->size());
  Require(snapshot == before && !world.SaveScene(scene, before->size() - 1) &&
              world.SaveScene(scene) == before && world.FindScene(scene) == scene_borrow &&
              world.FindEntity(child_id) == entity_borrow &&
              world.FindScene(scene)->state == SceneState::Active &&
              world.WorldTransform(child_id) == pose,
          "bounded success/rejection mutated World state or invalidated borrows");
  World same = world;
  Require(!world.SaveScene(scene, 1), "tiny budget did not reject");
  Require(world.CreateEntity(scene).id == same.CreateEntity(scene).id,
          "failed serialization changed identity allocation");
  // The returned snapshot owns its bytes through later mutation, unloading and source destruction.
  Require(world.RequestUnload(scene), "fixture unload failed");
  Require(!world.SaveScene(scene) && !world.SaveScene(scene, before->size()),
          "unloading scene accepted by an overload");
  world.EndFrame();
  Require(!world.SaveScene(scene) && !world.SaveScene(scene, before->size()) && snapshot == before,
          "unloaded scene accepted or owning snapshot invalidated");
  world = World{};
  Require(snapshot == before && !world.SaveScene(scene) && !world.SaveScene(scene, before->size()),
          "destroyed/missing scene retained a borrow or produced output");
}

void LargeQuotedNames() {
  World world;
  std::string escaped(64 * 1024, '"');
  for (std::size_t index = 1; index < escaped.size(); index += 2)
    escaped[index] = '\\';
  const auto escaped_id = world.LoadScene(escaped);
  const auto quoted_bytes = escaped.size() * 2 + 2;
  const auto entire_bytes =
      quoted_bytes + std::string_view("NEXORA_SCENE 3 ").size() + std::string_view(" 0 0\n").size();
  Require(!world.SaveScene(escaped_id, 32) && !world.SaveScene(escaped_id, escaped.size() + 2) &&
              !world.SaveScene(escaped_id, quoted_bytes - 1),
          "large escaped name accepted below its quoted representation budget");
  const auto historic = world.SaveScene(escaped_id);
  Require(historic && historic->size() == entire_bytes &&
              world.SaveScene(escaped_id, entire_bytes) == historic &&
              !world.SaveScene(escaped_id, entire_bytes - 1),
          "large escaped name exact snapshot boundary/bytes changed");

  std::string unicode = "\"\\";
  for (std::size_t index = 0; index < 8192; ++index)
    unicode += "界";
  unicode += "\\\"";
  const auto unicode_id = world.LoadScene(unicode);
  const auto expected_quoted_bytes = unicode.size() + 4 + 2;
  const auto expected_bytes = expected_quoted_bytes + std::string_view("NEXORA_SCENE 3 ").size() +
                              std::string_view(" 0 0\n").size();
  const auto full = world.SaveScene(unicode_id);
  Require(full && full->size() == expected_bytes &&
              !world.SaveScene(unicode_id, unicode.size() - 1) &&
              world.SaveScene(unicode_id, expected_bytes) == full &&
              !world.SaveScene(unicode_id, expected_bytes - 1),
          "UTF-8 name was escaped/counted as code points or changed exact boundary");
  World restored;
  const auto loaded = restored.LoadSceneSnapshot(*full);
  Require(loaded && restored.FindScene(*loaded)->name == unicode &&
              restored.SaveScene(*loaded, expected_bytes) == full,
          "large UTF-8 quoted name failed actual World roundtrip");
}

void EmptyAndLargeWorlds() {
  World world;
  const auto empty = world.LoadScene("Empty");
  const std::string expected = "NEXORA_SCENE 3 \"Empty\" 0 0\n";
  Require(!world.SaveScene(empty, 0) && !world.SaveScene(empty, expected.size() - 1) &&
              world.SaveScene(empty, expected.size()) == expected &&
              world.SaveScene(empty) == expected,
          "empty-scene exact boundary failed");
  Require(!world.SaveScene(999) && !world.SaveScene(999, std::numeric_limits<std::size_t>::max()),
          "missing scene accepted");

  const auto large = world.LoadScene("Large owning scene");
  constexpr std::size_t count = 20000;
  Id previous{};
  for (std::size_t index = 0; index < count; ++index) {
    auto &entity = world.CreateEntity(large);
    entity.parent = previous;
    entity.transform.x = static_cast<double>(index) / 8;
    previous = entity.id;
  }
  const auto before = world.SaveScene(large);
  Require(before && before->size() > 1024 * 1024, "large-world fixture is not substantial");
  Require(!world.SaveScene(large, 128) && !world.SaveScene(large, before->size() - 1) &&
              world.SaveScene(large, before->size()) == before &&
              world.FindScene(large)->entities.size() == count,
          "large-world bounded save failed exact limits or changed entities");
  World restored;
  const auto loaded = restored.LoadSceneSnapshot(*before);
  Require(loaded && restored.SaveScene(*loaded, before->size()) == before &&
              restored.FindEntity(previous)->parent != 0,
          "large-world hierarchy/snapshot byte compatibility failed");
}
} // namespace

int main() {
  try {
    HistoricWireAndBoundaries();
    PrecisionHierarchyAndLifetime();
    LargeQuotedNames();
    EmptyAndLargeWorlds();
    std::cout << "Bounded World scene save passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
