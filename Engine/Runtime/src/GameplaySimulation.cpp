#include "Nexora/Runtime/GameplaySimulation.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <queue>
#include <unordered_set>

namespace nexora::runtime {
namespace {
SimulationVector Add(SimulationVector a, SimulationVector b) {
  return {a.x + b.x, a.y + b.y, a.z + b.z};
}
SimulationVector Sub(SimulationVector a, SimulationVector b) {
  return {a.x - b.x, a.y - b.y, a.z - b.z};
}
SimulationVector Mul(SimulationVector a, double b) { return {a.x * b, a.y * b, a.z * b}; }
double Length(SimulationVector a) { return std::sqrt(a.x * a.x + a.y * a.y + a.z * a.z); }
bool Finite(SimulationVector a) {
  return std::isfinite(a.x) && std::isfinite(a.y) && std::isfinite(a.z);
}
SimulationVector Normalize(SimulationVector a) {
  const auto n = Length(a);
  return n > 0 ? Mul(a, 1 / n) : SimulationVector{};
}
} // namespace

bool PhysicsWorld::AddBody(PhysicsBody body) {
  if (body.id == 0 || bodies_.contains(body.id) || !Finite(body.minimum) ||
      !Finite(body.maximum) || !Finite(body.velocity) || body.minimum.x > body.maximum.x ||
      body.minimum.y > body.maximum.y || body.minimum.z > body.maximum.z)
    return false;
  return bodies_.emplace(body.id, body).second;
}
bool PhysicsWorld::RemoveBody(SimulationId body) { return bodies_.erase(body) != 0; }
void PhysicsWorld::Step(double seconds) {
  if (!std::isfinite(seconds) || seconds <= 0)
    return;
  for (auto &[id, body] : bodies_) {
    (void)id;
    if (!body.dynamic)
      continue;
    const auto delta = Mul(body.velocity, seconds);
    body.minimum = Add(body.minimum, delta);
    body.maximum = Add(body.maximum, delta);
  }
}
std::optional<PhysicsHit> PhysicsWorld::Raycast(const RaycastRequest &request) const {
  if (!Finite(request.origin) || !Finite(request.direction) || request.distance < 0 ||
      !std::isfinite(request.distance))
    return std::nullopt;
  const auto direction = Normalize(request.direction);
  if (Length(direction) == 0)
    return std::nullopt;
  std::optional<PhysicsHit> closest;
  for (const auto &[id, body] : bodies_) {
    if (body.trigger)
      continue;
    double low = 0, high = request.distance;
    SimulationVector normal{};
    const double origins[3]{request.origin.x, request.origin.y, request.origin.z};
    const double dirs[3]{direction.x, direction.y, direction.z};
    const double mins[3]{body.minimum.x, body.minimum.y, body.minimum.z};
    const double maxs[3]{body.maximum.x, body.maximum.y, body.maximum.z};
    bool hit = true;
    for (int axis = 0; axis < 3; ++axis) {
      if (std::abs(dirs[axis]) < 1e-12) {
        if (origins[axis] < mins[axis] || origins[axis] > maxs[axis])
          hit = false;
        continue;
      }
      auto enter = (mins[axis] - origins[axis]) / dirs[axis],
           leave = (maxs[axis] - origins[axis]) / dirs[axis];
      double sign = -1;
      if (enter > leave) {
        std::swap(enter, leave);
        sign = 1;
      }
      if (enter > low) {
        low = enter;
        normal = {};
        if (axis == 0)
          normal.x = sign;
        else if (axis == 1)
          normal.y = sign;
        else
          normal.z = sign;
      }
      high = std::min(high, leave);
      if (low > high)
        hit = false;
    }
    if (hit && low >= 0 && (!closest || low < closest->distance))
      closest = PhysicsHit{id, low, Add(request.origin, Mul(direction, low)), normal};
  }
  return closest;
}
std::vector<std::optional<PhysicsHit>>
PhysicsWorld::RaycastBatch(std::span<const RaycastRequest> requests) const {
  std::vector<std::optional<PhysicsHit>> result;
  result.reserve(requests.size());
  for (const auto &request : requests)
    result.push_back(Raycast(request));
  return result;
}

CharacterMoveResult CharacterController::Move(CharacterState &state, SimulationVector request,
                                              const PhysicsWorld &physics) const {
  CharacterMoveResult out{};
  out.requested_motion = request;
  auto actual = request;
  const auto horizontal = SimulationVector{request.x, 0, request.z};
  const auto distance = Length(horizontal);
  if (distance > 0) {
    const auto hit = physics.Raycast({state.position, horizontal, distance + config_.radius});
    if (hit && hit->distance <= distance + config_.radius) {
      // Try a bounded step before treating the obstacle as a wall.
      const auto raised = Add(state.position, {0, config_.step_height, 0});
      const auto step_hit = physics.Raycast({raised, horizontal, distance + config_.radius});
      if (!step_hit)
        actual.y += config_.step_height;
      else {
        actual.x = 0;
        actual.z = 0;
      }
    }
  }
  auto candidate = Add(state.position, actual);
  const auto down =
      physics.Raycast({candidate, {0, -1, 0}, config_.ground_snap_distance + config_.step_height});
  if (down && request.y <= 0) {
    actual.y -= down->distance;
    candidate.y -= down->distance;
    state.ground = CharacterGroundState::OnGround;
    state.ground_body = down->body;
    out.ground_normal = down->normal;
  } else {
    state.ground = CharacterGroundState::InAir;
    state.ground_body = 0;
  }
  state.position = candidate;
  out.actual_motion = actual;
  out.motion_error = Sub(request, actual);
  out.ground = state.ground;
  return out;
}
bool CharacterController::SetCrouched(CharacterState &state, bool crouched,
                                      const PhysicsWorld &physics) const {
  if (!crouched && state.crouched) {
    const auto clearance = config_.standing_height - config_.crouching_height;
    if (physics.Raycast({state.position, {0, 1, 0}, clearance}))
      return false;
  }
  state.crouched = crouched;
  return true;
}
void CharacterController::Teleport(CharacterState &state, SimulationVector position,
                                   bool preserve_velocity) const {
  state.position = position;
  state.ground = CharacterGroundState::Unsupported;
  state.ground_body = 0;
  if (!preserve_velocity)
    state.velocity = {};
}
CharacterMoveResult StandardCharacterMotor::Tick(CharacterState &state, const CharacterInput &input,
                                                 double seconds, const PhysicsWorld &physics,
                                                 const CharacterController &controller) {
  if (!std::isfinite(seconds) || seconds <= 0 || !Finite(state.position) ||
      !Finite(state.velocity) || !Finite(input.root_motion) ||
      !Finite(input.external_velocity) || !std::isfinite(input.move_x) ||
      !std::isfinite(input.move_z))
    return {};
  auto planar = SimulationVector{input.move_x, 0, input.move_z};
  const auto max_speed = (input.flags & CharacterSprint) != 0 ? 8.0 : 5.0;
  if (Length(planar) > 1)
    planar = Normalize(planar);
  planar = Mul(planar, max_speed);
  if ((input.flags & CharacterJump) != 0 && state.ground == CharacterGroundState::OnGround)
    state.velocity.y = 6;
  else
    state.velocity.y += -9.81 * seconds;
  external_velocity_ = input.external_velocity;
  auto desired =
      Add(Add(Mul(planar, seconds), Mul(Add(state.velocity, external_velocity_), seconds)),
          input.root_motion);
  auto result = controller.Move(state, desired, physics);
  result.desired_velocity = Add(Add(planar, state.velocity), external_velocity_);
  result.resolved_velocity = Mul(result.actual_motion, 1 / seconds);
  state.velocity = result.resolved_velocity;
  if (result.ground == CharacterGroundState::OnGround && state.velocity.y < 0)
    state.velocity.y = 0;
  return result;
}

bool NavigationWorld::LoadTile(NavTileId tile, std::vector<NavPoint> points) {
  if (tile == 0 || tiles_.contains(tile) || points.empty())
    return false;
  std::unordered_set<SimulationId> ids;
  for (const auto &point : points)
    if (point.id == 0 || point.tile != tile || points_.contains(point.id) ||
        !ids.insert(point.id).second)
      return false;
  auto &stored = tiles_[tile];
  for (auto &point : points) {
    stored.push_back(point.id);
    points_.emplace(point.id, std::move(point));
  }
  ++generation_;
  return true;
}
bool NavigationWorld::UnloadTile(NavTileId tile) {
  const auto found = tiles_.find(tile);
  if (found == tiles_.end())
    return false;
  for (const auto id : found->second)
    points_.erase(id);
  tiles_.erase(found);
  ++generation_;
  return true;
}
std::optional<NavigationPath> NavigationWorld::FindPath(SimulationId start,
                                                        SimulationId goal) const {
  if (!points_.contains(start) || !points_.contains(goal))
    return std::nullopt;
  std::queue<SimulationId> open;
  std::unordered_map<SimulationId, SimulationId> parent{{start, 0}};
  open.push(start);
  while (!open.empty() && !parent.contains(goal)) {
    auto current = open.front();
    open.pop();
    for (auto next : points_.at(current).neighbours)
      if (points_.contains(next) && !parent.contains(next)) {
        parent[next] = current;
        open.push(next);
      }
  }
  if (!parent.contains(goal))
    return std::nullopt;
  std::vector<SimulationVector> path;
  for (auto at = goal; at != 0; at = parent.at(at))
    path.push_back(points_.at(at).position);
  std::ranges::reverse(path);
  return NavigationPath{std::move(path), generation_};
}
SimulationVector NavigationWorld::DesiredVelocity(const NavigationPath &path,
                                                  SimulationVector position,
                                                  double max_speed) const {
  if (path.generation != generation_ || path.points.empty() || !Finite(position) ||
      !std::isfinite(max_speed) || max_speed <= 0)
    return {};
  auto target = path.points.back();
  for (const auto &point : path.points)
    if (Length(Sub(point, position)) > 0.05) {
      target = point;
      break;
    }
  return Mul(Normalize(Sub(target, position)), max_speed);
}

bool Blackboard::Set(std::size_t slot, BlackboardValue value) {
  if (slot >= slots_.size() || slots_[slot].index() != value.index())
    return false;
  slots_[slot] = std::move(value);
  return true;
}
const BlackboardValue *Blackboard::Get(std::size_t slot) const {
  return slot < slots_.size() ? &slots_[slot] : nullptr;
}
bool BehaviorProgram::Evaluate(std::uint32_t index, const Blackboard &blackboard,
                               BehaviorTrace &trace,
                               std::vector<std::uint8_t> &active) const {
  if (index >= nodes_.size() || active[index] != 0)
    return false;
  active[index] = 1;
  trace.visited.push_back(index);
  const auto &node = nodes_[index];
  bool result = false;
  if (node.op == BehaviorOp::Succeed)
    result = true;
  else if (node.op == BehaviorOp::Fail)
    result = false;
  else if (node.op == BehaviorOp::BlackboardBool) {
    const auto *value = blackboard.Get(node.slot);
    result = value && std::holds_alternative<bool>(*value) && std::get<bool>(*value);
  } else if (node.first_child <= nodes_.size() &&
             node.child_count <= nodes_.size() - node.first_child) {
    if (node.op == BehaviorOp::Sequence) {
      result = true;
      for (std::uint32_t i = 0; i < node.child_count; ++i)
        if (!Evaluate(node.first_child + i, blackboard, trace, active)) {
          result = false;
          break;
        }
    } else if (node.op == BehaviorOp::Selector) {
      for (std::uint32_t i = 0; i < node.child_count; ++i)
        if (Evaluate(node.first_child + i, blackboard, trace, active)) {
          result = true;
          break;
        }
    }
  }
  active[index] = 0;
  return result;
}
BehaviorTrace BehaviorProgram::Tick(const Blackboard &blackboard) const {
  BehaviorTrace trace;
  std::vector<std::uint8_t> active(nodes_.size());
  trace.succeeded = !nodes_.empty() && Evaluate(0, blackboard, trace, active);
  return trace;
}
void PerceptionSystem::Publish(Stimulus stimulus) {
  if (stimulus.source != 0 && Finite(stimulus.position) &&
      std::isfinite(stimulus.strength) && stimulus.strength >= 0)
    stimuli_.push_back(stimulus);
}
std::vector<Stimulus> PerceptionSystem::Query(SimulationVector observer, double range,
                                              std::size_t budget) const {
  std::vector<Stimulus> result;
  if (!Finite(observer) || !std::isfinite(range) || range < 0.0)
    return result;
  for (const auto &stimulus : stimuli_) {
    if (result.size() >= budget)
      break;
    if (Length(Sub(stimulus.position, observer)) <= range * stimulus.strength)
      result.push_back(stimulus);
  }
  return result;
}
} // namespace nexora::runtime
