#pragma once

#include "Nexora/Runtime/Api.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace nexora::runtime {

using SimulationId = std::uint64_t;

struct SimulationVector final {
  double x{}, y{}, z{};
  friend bool operator==(const SimulationVector &, const SimulationVector &) = default;
};

struct PhysicsBody final {
  SimulationId id{};
  SimulationVector minimum{}, maximum{};
  bool dynamic{};
  bool trigger{};
  SimulationVector velocity{};
};
struct RaycastRequest final {
  SimulationVector origin{}, direction{};
  double distance{};
};
struct PhysicsHit final {
  SimulationId body{};
  double distance{};
  SimulationVector point{}, normal{};
};

// Platform-neutral CPU-authoritative query boundary. A production Jolt adapter can
// implement the same batched contract without exposing backend types publicly.
class NEXORA_RUNTIME_API PhysicsWorld final {
public:
  bool AddBody(PhysicsBody body);
  bool RemoveBody(SimulationId body);
  void Step(double seconds);
  [[nodiscard]] std::optional<PhysicsHit> Raycast(const RaycastRequest &request) const;
  [[nodiscard]] std::vector<std::optional<PhysicsHit>>
  RaycastBatch(std::span<const RaycastRequest> requests) const;

private:
  std::unordered_map<SimulationId, PhysicsBody> bodies_;
};

enum class CharacterGroundState {
  InAir,
  OnGround,
  OnSteepGround,
  Sliding,
  Unsupported,
  StreamingPending
};
enum CharacterIntentFlags : std::uint32_t {
  CharacterJump = 1U << 0U,
  CharacterCrouch = 1U << 1U,
  CharacterSprint = 1U << 2U
};
struct CharacterControllerConfig final {
  double radius{0.4}, standing_height{1.8}, crouching_height{1.0};
  double max_walkable_slope_degrees{50.0}, step_height{0.4}, ground_snap_distance{0.25};
};
struct CharacterInput final {
  double move_x{}, move_z{};
  std::uint32_t flags{};
  SimulationVector root_motion{}, external_velocity{};
};
struct CharacterState final {
  SimulationVector position{}, velocity{};
  CharacterGroundState ground{CharacterGroundState::InAir};
  bool crouched{};
  SimulationId ground_body{};
};
struct CharacterMoveResult final {
  SimulationVector requested_motion{}, actual_motion{}, motion_error{};
  SimulationVector desired_velocity{}, resolved_velocity{}, ground_normal{0, 1, 0};
  CharacterGroundState ground{CharacterGroundState::InAir};
};
class NEXORA_RUNTIME_API CharacterController final {
public:
  explicit CharacterController(CharacterControllerConfig config = {}) : config_(config) {}
  [[nodiscard]] CharacterMoveResult Move(CharacterState &state, SimulationVector requested_motion,
                                         const PhysicsWorld &physics) const;
  bool SetCrouched(CharacterState &state, bool crouched, const PhysicsWorld &physics) const;
  void Teleport(CharacterState &state, SimulationVector position,
                bool preserve_velocity = false) const;

private:
  CharacterControllerConfig config_;
};
class NEXORA_RUNTIME_API StandardCharacterMotor final {
public:
  [[nodiscard]] CharacterMoveResult Tick(CharacterState &state, const CharacterInput &input,
                                         double seconds, const PhysicsWorld &physics,
                                         const CharacterController &controller);

private:
  SimulationVector external_velocity_{};
};

using NavTileId = std::uint32_t;
struct NavPoint final {
  SimulationId id{};
  NavTileId tile{};
  SimulationVector position{};
  std::vector<SimulationId> neighbours;
};
struct NavigationPath final {
  std::vector<SimulationVector> points;
  std::uint64_t generation{};
};
class NEXORA_RUNTIME_API NavigationWorld final {
public:
  bool LoadTile(NavTileId tile, std::vector<NavPoint> points);
  bool UnloadTile(NavTileId tile);
  [[nodiscard]] std::optional<NavigationPath> FindPath(SimulationId start, SimulationId goal) const;
  [[nodiscard]] SimulationVector DesiredVelocity(const NavigationPath &path,
                                                 SimulationVector position, double max_speed) const;
  [[nodiscard]] std::uint64_t Generation() const noexcept { return generation_; }

private:
  std::unordered_map<SimulationId, NavPoint> points_;
  std::unordered_map<NavTileId, std::vector<SimulationId>> tiles_;
  std::uint64_t generation_{1};
};

using BlackboardValue = std::variant<bool, std::int64_t, double, SimulationVector, SimulationId>;
class NEXORA_RUNTIME_API Blackboard final {
public:
  explicit Blackboard(std::vector<BlackboardValue> layout) : slots_(std::move(layout)) {}
  bool Set(std::size_t slot, BlackboardValue value);
  [[nodiscard]] const BlackboardValue *Get(std::size_t slot) const;

private:
  std::vector<BlackboardValue> slots_;
};
enum class BehaviorOp { Succeed, Fail, BlackboardBool, Sequence, Selector };
struct BehaviorNode final {
  BehaviorOp op{};
  std::uint32_t first_child{}, child_count{}, slot{};
};
struct BehaviorTrace final {
  std::vector<std::uint32_t> visited;
  bool succeeded{};
};
class NEXORA_RUNTIME_API BehaviorProgram final {
public:
  explicit BehaviorProgram(std::vector<BehaviorNode> nodes) : nodes_(std::move(nodes)) {}
  [[nodiscard]] BehaviorTrace Tick(const Blackboard &blackboard) const;

private:
  bool Evaluate(std::uint32_t node, const Blackboard &, BehaviorTrace &,
                  std::vector<std::uint8_t> &active) const;
  std::vector<BehaviorNode> nodes_;
};
enum class StimulusKind { Sight, Hearing };
struct Stimulus final {
  SimulationId source{};
  StimulusKind kind{};
  SimulationVector position{};
  double strength{};
};
class NEXORA_RUNTIME_API PerceptionSystem final {
public:
  void Publish(Stimulus stimulus);
  [[nodiscard]] std::vector<Stimulus> Query(SimulationVector observer, double range,
                                            std::size_t budget) const;
  void Clear() { stimuli_.clear(); }

private:
  std::vector<Stimulus> stimuli_;
};

} // namespace nexora::runtime
