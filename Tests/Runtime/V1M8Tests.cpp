#include "Nexora/Runtime/GameplaySimulation.h"
#include <cmath>
#include <stdexcept>
using namespace nexora::runtime;
static void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
int main() {
  PhysicsWorld physics;
  Require(physics.AddBody({1, {-100, -1, -100}, {100, 0, 100}, false, false, {}}),
          "ground rejected");
  Require(physics.AddBody({2, {2, 0, -1}, {3, 3, 1}, false, false, {}}), "wall rejected");
  const RaycastRequest rays[]{{{0, 2, 0}, {0, -1, 0}, 10}, {{0, 2, 0}, {1, 0, 0}, 1}};
  const auto hits = physics.RaycastBatch(rays);
  Require(hits[0] && hits[0]->body == 1 && !hits[1], "batch query failed");

  CharacterState state{{0, 0.1, 0}, {}, CharacterGroundState::OnGround};
  CharacterController controller;
  StandardCharacterMotor motor;
  auto motion = motor.Tick(state, {1, 0, 0, {}, {}}, 0.1, physics, controller);
  Require(motion.requested_motion.x > 0 && motion.actual_motion.x > 0 &&
              motion.ground == CharacterGroundState::OnGround,
          "ground motor failed");
  state.position = {1.8, 0, 0};
  motion = motor.Tick(state, {1, 0, 0, {}, {}}, 0.1, physics, controller);
  Require(motion.actual_motion.x == 0 && motion.motion_error.x > 0,
          "wall did not separate requested and actual motion");
  controller.Teleport(state, {0, 4, 0});
  Require(state.ground == CharacterGroundState::Unsupported && state.velocity == SimulationVector{},
          "teleport contract failed");

  NavigationWorld nav;
  Require(nav.LoadTile(1, {{10, 1, {0, 0, 0}, {20}}}) &&
              nav.LoadTile(2, {{20, 2, {4, 0, 0}, {30}}, {30, 2, {8, 0, 0}, {}}}),
          "nav tiles rejected");
  const auto path = nav.FindPath(10, 30);
  Require(path && path->points.size() == 3, "cross-tile path failed");
  const auto desired = nav.DesiredVelocity(*path, {0, 0, 0}, 3);
  Require(std::abs(desired.x - 3) < 1e-9, "nav desired velocity failed");
  Require(nav.UnloadTile(2) && !nav.FindPath(10, 30) &&
              nav.DesiredVelocity(*path, {0, 0, 0}, 3) == SimulationVector{},
          "stale nav reference survived unload");

  Blackboard blackboard({false, std::int64_t{0}, SimulationVector{}});
  Require(blackboard.Set(0, true) && !blackboard.Set(0, 1.0), "typed blackboard failed");
  BehaviorProgram tree({{BehaviorOp::Sequence, 1, 2, 0},
                        {BehaviorOp::BlackboardBool, 0, 0, 0},
                        {BehaviorOp::Succeed, 0, 0, 0}});
  const auto trace = tree.Tick(blackboard);
  Require(trace.succeeded && trace.visited == std::vector<std::uint32_t>({0, 1, 2}),
          "behavior trace failed");
  PerceptionSystem perception;
  perception.Publish({7, StimulusKind::Hearing, {2, 0, 0}, 1});
  perception.Publish({8, StimulusKind::Sight, {3, 0, 0}, 1});
  Require(perception.Query({0, 0, 0}, 10, 1).size() == 1, "perception budget failed");
  return 0;
}
