#include "Nexora/Runtime/Runtime.h"
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
using namespace nexora::runtime;
void Require(bool condition, const char *message) {
  if (!condition)
    throw std::runtime_error(message);
}
std::string Snapshot(Id entity) {
  return "NEXORA_SCENE 3 \"Identity\" 0 1\n" + std::to_string(entity) +
         " 0 0 0 0 0 0 0 1 1 1 1 0 0 0 60 .1 1000 1 0 0\n";
}
template <class Action> void Exhausted(Action action) {
  try {
    action();
    throw std::runtime_error("exhausted identity admitted");
  } catch (const std::overflow_error &) {
  }
}
} // namespace
int main() {
  try {
    constexpr auto maximum = std::numeric_limits<Id>::max();
    World rejected;
    Require(!rejected.LoadSceneSnapshot(Snapshot(maximum)) && rejected.LoadScene("First") == 1,
            "maximum imported ID changed allocation");
    World entity_last;
    const auto scene = entity_last.LoadSceneSnapshot(Snapshot(maximum - 2));
    Require(scene && entity_last.Activate(*scene), "near-limit scene rejected");
    Require(entity_last.CreateEntity(*scene).id == maximum - 1, "last entity ID was not allocated");
    const auto original = *entity_last.SaveScene(*scene);
    for (int repeat = 0; repeat < 8; ++repeat) {
      Exhausted([&] { static_cast<void>(entity_last.CreateEntity(*scene)); });
      Exhausted([&] { static_cast<void>(entity_last.LoadScene("Exhausted")); });
      Require(!entity_last.LoadSceneSnapshot("NEXORA_SCENE 3 \"Empty\" 0 0\n") &&
                  entity_last.SaveScene(*scene) == original && !entity_last.FindEntity(0) &&
                  !entity_last.FindEntity(maximum),
              "failed creation wrapped IDs or published a partial object");
    }
    auto clone = entity_last.CloneForPlay();
    Exhausted([&] { static_cast<void>(clone.CreateEntity(*scene)); });
    Require(clone.SaveScene(*scene) == original, "clone reset exhausted identity");
    World scene_last;
    const auto retained = scene_last.LoadSceneSnapshot(Snapshot(maximum - 2));
    Require(retained && scene_last.LoadScene("Last scene") == maximum - 1,
            "last scene ID was not allocated");
    Exhausted([&] { static_cast<void>(scene_last.CreateEntity(*retained)); });
    Require(scene_last.FindScene(maximum - 1) && !scene_last.FindScene(0),
            "last scene did not retain identity");
    World imported_last;
    const auto final = imported_last.LoadSceneSnapshot(Snapshot(maximum - 1));
    Require(final && imported_last.FindEntity(maximum - 1), "last imported entity rejected");
    Exhausted([&] { static_cast<void>(imported_last.LoadScene("Overflow")); });
    Require(imported_last.ReplaceSceneSnapshot(*final, Snapshot(maximum - 1)),
            "exhaustion blocked replacement of existing objects");
    Exhausted([&] { static_cast<void>(imported_last.CreateEntity(*final)); });
    std::cout << "World identity exhaustion and owning clone contracts passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
