#include "Nexora/Runtime/GameplaySimulation.h"
#include <chrono>
#include <cmath>
#include <limits>
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

  CharacterState pending_state{{5, 0.1, 0}, {1, -2, 0}, CharacterGroundState::OnGround};
  auto pending_result =
      motor.Tick(pending_state, {1, 0, 0, {}, {}}, 0.1, physics, controller, false);
  Require(pending_state.ground == CharacterGroundState::StreamingPending &&
              pending_state.position == SimulationVector{5, 0.1, 0} &&
              pending_state.velocity == SimulationVector{} &&
              pending_result.actual_motion == SimulationVector{},
          "unready ground did not suspend locomotion");
  auto resumed_result = motor.Tick(pending_state, {1, 0, 0, {}, {}}, 0.1, physics, controller);
  Require(resumed_result.ground == CharacterGroundState::OnGround && pending_state.position.x > 5,
          "streaming-pending motor did not resume once ground became ready");

  CharacterState blocked_teleport{{9, 4, 0}, {0, -3, 0}, CharacterGroundState::OnGround};
  controller.Teleport(blocked_teleport, {0, 4, 0}, false, false);
  Require(blocked_teleport.ground == CharacterGroundState::StreamingPending &&
              blocked_teleport.position == SimulationVector{9, 4, 0},
          "teleport into an unready destination placed the character without collision");
  controller.Teleport(blocked_teleport, {0, 4, 0}, false, true);
  Require(blocked_teleport.ground == CharacterGroundState::Unsupported &&
              blocked_teleport.position == SimulationVector{0, 4, 0},
          "teleport did not complete once the destination became ready");

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
  perception.Publish({9, StimulusKind::Sight,
                      {std::numeric_limits<double>::quiet_NaN(), 0, 0}, 1});
  Require(perception.Query({0, 0, 0}, std::numeric_limits<double>::quiet_NaN(), 1).empty(),
          "invalid perception input was accepted");

  BehaviorProgram cycle({{BehaviorOp::Sequence, 1, 1, 0},
                         {BehaviorOp::Sequence, 0, 1, 0}});
  const auto cycle_trace = cycle.Tick(blackboard);
  Require(!cycle_trace.succeeded && cycle_trace.visited == std::vector<std::uint32_t>({0, 1}),
          "cyclic behavior program was not bounded");

  std::vector<RaycastRequest> baseline_requests(10000, RaycastRequest{{0, 2, 0}, {0, -1, 0}, 10});
  const auto begin = std::chrono::steady_clock::now();
  const auto baseline_hits = physics.RaycastBatch(baseline_requests);
  Require(baseline_hits.size() == baseline_requests.size() &&
              std::chrono::steady_clock::now() - begin < std::chrono::seconds(2),
          "physics batch performance baseline failed");
  return 0;
}
