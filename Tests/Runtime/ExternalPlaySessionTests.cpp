#include "Nexora/Runtime/EditorSdk.h"
#include <iostream>
#include <stdexcept>

namespace {
using namespace nexora::runtime;
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
void Run() {
  World authoring;
  const auto source_scene = authoring.LoadScene("Authoring");
  const auto source = authoring.CreateEntity(source_scene).id;
  Require(authoring.Activate(source_scene), "source activation");
  const auto original = authoring.SaveScene(source_scene);
  World game(WorldKind::Play);
  const auto scene = game.LoadScene("Configured startup");
  const auto entity = game.CreateEntity(scene).id;
  Require(game.Activate(scene) && entity == source, "overlapping authored IDs");
  PlaySession play(authoring);
  const auto update = [entity](World &world, double seconds) {
    const auto *current = world.FindEntity(entity);
    if (!current)
      return false;
    auto transform = current->transform;
    transform.x += seconds;
    WorldCommandBuffer edits;
    edits.SetTransform(entity, transform);
    return edits.Apply(world);
  };
  Require(!play.StartExternal(authoring, 0.5, update) && !play.StartExternal(game, 0, update) &&
              !play.StartExternal(game, 0.5, {}),
          "invalid borrowed Play admitted");
  Require(play.StartExternal(game, 0.5, update) && play.PlayWorld() == &game &&
              !play.CanApplyTransforms() && !play.AcceptsInput() &&
              !play.StartExternal(game, 0.5, update) && !play.Start(0.5, update),
          "external World ownership/start admission");
  const auto generation = play.Generation();
  play.SetInputFocus(true);
  Require(play.AcceptsInput() && play.Tick() && game.FindEntity(entity)->transform.x == 0.5 &&
              authoring.SaveScene(source_scene) == original,
          "external tick touched authoring");
  Require(play.Pause() && !play.AcceptsInput() && !play.Tick() && play.Step() &&
              play.Stats().fixed_ticks == 2 && game.FindEntity(entity)->transform.x == 1,
          "external pause/step");
  const auto snapshot = play.Inspect();
  Require(snapshot.entities.size() == 1 && snapshot.entities[0].transform.x == 1 &&
              play.PreviewTransformApplyBack().empty(),
          "remapped IDs entered apply-back");
  Require(!play.Stop(ApplyBackPolicy::Transforms) && play.State() == PlayState::Stopped &&
              play.LastApplyBackStatus() == ApplyBackStatus::Failed && !play.PlayWorld() &&
              game.FindEntity(entity)->transform.x == 1 &&
              authoring.SaveScene(source_scene) == original,
          "external Stop copied back or destroyed borrowed World");
  Require(play.StartExternal(game, 0.5, [](World &, double) { return false; }) &&
              play.Generation() != generation && !play.Tick() &&
              play.State() == PlayState::Paused && !play.AcceptsInput() &&
              play.LastPauseReason() == PauseReason::RuntimeFailure && play.PlayWorld() == &game &&
              play.Stop(),
          "external failure/teardown");
  Require(snapshot.entities[0].transform.x == 1 && play.Inspect().entities.empty(),
          "inspection retained World borrows");
  Require(play.Start(0.5, update) && play.CanApplyTransforms() && play.PlayWorld() != &game &&
              play.Tick() && play.Stop() && authoring.SaveScene(source_scene) == original,
          "normal isolated-clone compatibility after borrowed Play");
}
} // namespace
int main() {
  try {
    Run();
    std::cout << "Host-owned Play World borrowing, pause/failure/lifetime and apply-back rejection "
                 "passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
