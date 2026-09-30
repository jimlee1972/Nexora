#pragma once

#include "Nexora/AI/Api.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

namespace nexora::ai {

using AgentId = std::uint64_t;
using NavigationRequestId = std::uint64_t;
using NavNodeId = std::uint32_t;
using NavRegionId = std::uint32_t;

struct Vec3 final {
  double x{}, y{}, z{};
  friend bool operator==(const Vec3 &, const Vec3 &) = default;
};

enum class CharacterMovementMode : std::uint8_t { Grounded, Airborne, Disabled };

struct CharacterIntent final {
  Vec3 desired_move_direction{};
  double desired_speed{};
  Vec3 desired_facing{};
  CharacterMovementMode movement_mode{CharacterMovementMode::Grounded};
  std::uint32_t flags{};
  friend bool operator==(const CharacterIntent &, const CharacterIntent &) = default;
};

struct AIAction final {
  std::uint32_t action_id{};
  CharacterIntent character{};
  friend bool operator==(const AIAction &, const AIAction &) = default;
};

class NEXORA_AI_API INavigationCostProvider {
public:
  virtual ~INavigationCostProvider() = default;
  [[nodiscard]] virtual double Cost(Vec3 position) const = 0;
};

class NEXORA_AI_API GridCostField final : public INavigationCostProvider {
public:
  GridCostField(Vec3 origin, double cell_size, std::size_t width, std::size_t height,
                double default_cost = 0.0);
  bool Set(std::size_t x, std::size_t z, double cost);
  [[nodiscard]] double Cost(Vec3 position) const override;
  [[nodiscard]] std::uint64_t Generation() const noexcept { return generation_; }

private:
  Vec3 origin_{};
  double cell_size_{};
  std::size_t width_{}, height_{};
  std::vector<double> costs_;
  std::uint64_t generation_{1};
};

struct NavRegion final {
  NavRegionId id{};
  std::vector<NavRegionId> neighbours;
};

struct NavNode final {
  NavNodeId id{};
  NavRegionId region{};
  Vec3 position{};
  std::vector<NavNodeId> neighbours;
};

struct NavigationPath final {
  std::vector<NavNodeId> nodes;
  std::uint64_t generation{};
};

class NEXORA_AI_API HierarchicalNavigationWorld final {
public:
  bool AddRegion(NavRegion region);
  bool AddNode(NavNode node);
  bool RemoveRegion(NavRegionId region);
  [[nodiscard]] std::optional<NavigationPath>
  FindPath(NavNodeId start, NavNodeId goal, const INavigationCostProvider *cost = nullptr) const;
  [[nodiscard]] std::uint64_t Generation() const noexcept { return generation_; }
  [[nodiscard]] const NavNode *FindNode(NavNodeId node) const;

private:
  std::unordered_map<NavRegionId, NavRegion> regions_;
  std::unordered_map<NavNodeId, NavNode> nodes_;
  std::uint64_t generation_{1};
};

enum class NavigationResultStatus : std::uint8_t {
  Completed,
  Unreachable,
  Cancelled,
  Stale
};

struct NavigationRequest final {
  NavigationRequestId id{};
  AgentId agent{};
  NavNodeId start{};
  NavNodeId goal{};
  std::int32_t priority{};
  std::uint64_t submitted_tick{};
  std::uint64_t navigation_generation{};
};

struct NavigationResult final {
  NavigationRequestId id{};
  AgentId agent{};
  NavigationResultStatus status{NavigationResultStatus::Unreachable};
  std::optional<NavigationPath> path;
};

struct NavigationSchedulerStats final {
  std::uint64_t submitted{};
  std::uint64_t completed{};
  std::uint64_t cancelled{};
  std::uint64_t stale{};
  std::uint64_t deferred{};
  std::size_t max_processed_per_tick{};
};

class NEXORA_AI_API NavigationQueryScheduler final {
public:
  explicit NavigationQueryScheduler(std::size_t budget_per_tick);
  [[nodiscard]] NavigationRequestId Submit(AgentId agent, NavNodeId start, NavNodeId goal,
                                           std::int32_t priority, std::uint64_t tick,
                                           std::uint64_t navigation_generation);
  bool Cancel(NavigationRequestId id);
  std::size_t Process(const HierarchicalNavigationWorld &world, std::uint64_t tick,
                      const INavigationCostProvider *cost = nullptr);
  [[nodiscard]] std::vector<NavigationResult> DrainResults();
  [[nodiscard]] std::size_t PendingCount() const noexcept;
  [[nodiscard]] const NavigationSchedulerStats &Stats() const noexcept { return stats_; }

private:
  struct Pending final {
    NavigationRequest request;
    bool cancelled{};
  };

  std::size_t budget_per_tick_{};
  NavigationRequestId next_id_{1};
  std::vector<Pending> pending_;
  std::vector<NavigationResult> results_;
  std::unordered_map<AgentId, NavigationRequestId> latest_by_agent_;
  NavigationSchedulerStats stats_{};
};

struct CrowdAgentInput final {
  AgentId agent{};
  Vec3 position{};
  Vec3 preferred_velocity{};
  double radius{0.4};
  double max_speed{5.0};
};

struct CrowdResult final {
  AgentId agent{};
  CharacterIntent intent{};
};

class NEXORA_AI_API CrowdSystem final {
public:
  [[nodiscard]] std::vector<CrowdResult> Solve(std::span<const CrowdAgentInput> agents) const;
};

enum class UtilityCurve : std::uint8_t { Linear, Inverse, Quadratic, Step };

struct UtilityConsideration final {
  double input{};
  double weight{1.0};
  UtilityCurve curve{UtilityCurve::Linear};
  double threshold{0.5};
};

struct UtilityAction final {
  AIAction action{};
  double bias{1.0};
  std::vector<UtilityConsideration> considerations;
  std::uint64_t cooldown_until_tick{};
};

struct UtilityDecision final {
  AIAction action{};
  double score{};
};

class NEXORA_AI_API UtilityAI final {
public:
  [[nodiscard]] std::optional<UtilityDecision>
  Select(std::span<const UtilityAction> actions, std::uint64_t tick = 0,
         std::uint32_t current_action_id = 0, double hysteresis = 0.0) const;
};

enum class PerceptionLOD : std::uint8_t { Full, Reduced, Far, Dormant };

struct AgentRelevance final {
  double distance{};
  bool in_combat{};
  bool visible{};
  bool important{};
  bool recently_interacted{};
};

struct AISchedule final {
  PerceptionLOD lod{PerceptionLOD::Dormant};
  std::uint32_t perception_interval_ticks{};
  std::uint32_t decision_interval_ticks{};
  std::uint32_t navigation_interval_ticks{};
  bool dormant{true};
};

class NEXORA_AI_API SimulationLODPolicy final {
public:
  [[nodiscard]] AISchedule Classify(const AgentRelevance &relevance) const;
  [[nodiscard]] static bool ShouldRun(AgentId agent, std::uint64_t tick,
                                      std::uint32_t interval_ticks) noexcept;
};

struct AIAgentScheduleInput final {
  AgentId agent{};
  AgentRelevance relevance{};
};

struct AIWorkItem final {
  AgentId agent{};
  PerceptionLOD lod{PerceptionLOD::Dormant};
  bool run_perception{};
  bool run_decision{};
  bool run_navigation{};
};

struct AISchedulerStats final {
  std::uint64_t total_agents{};
  std::uint64_t active_agents{};
  std::uint64_t dormant_agents{};
  std::uint64_t perception_updates{};
  std::uint64_t decision_updates{};
  std::uint64_t navigation_updates{};
};

class NEXORA_AI_API AISimulationScheduler final {
public:
  [[nodiscard]] std::vector<AIWorkItem> Build(std::span<const AIAgentScheduleInput> agents,
                                              std::uint64_t tick);
  [[nodiscard]] const AISchedulerStats &Stats() const noexcept { return stats_; }

private:
  SimulationLODPolicy policy_;
  AISchedulerStats stats_{};
};

struct PolicyObservation final {
  AgentId agent{};
  std::vector<float> values;
};

enum class PolicyEvaluationStatus : std::uint8_t {
  Ready,
  Deferred,
  Unavailable,
  Invalid
};

class NEXORA_AI_API IPolicyRuntime {
public:
  virtual ~IPolicyRuntime() = default;
  [[nodiscard]] virtual const char *PolicyId() const noexcept = 0;
  [[nodiscard]] virtual std::uint64_t PolicyVersion() const noexcept = 0;
  virtual PolicyEvaluationStatus Evaluate(std::span<const PolicyObservation> observations,
                                          std::vector<AIAction> &actions) = 0;
};

struct PolicyBatchResult final {
  PolicyEvaluationStatus status{PolicyEvaluationStatus::Unavailable};
  std::vector<AIAction> actions;
  bool used_fallback{};
  bool reused_cached{};
};

class NEXORA_AI_API PolicyRuntimeDriver final {
public:
  PolicyRuntimeDriver(IPolicyRuntime &runtime, AIAction fallback_action,
                      std::uint64_t max_stale_ticks)
      : runtime_(runtime), fallback_action_(fallback_action), max_stale_ticks_(max_stale_ticks) {}

  [[nodiscard]] PolicyBatchResult Evaluate(std::span<const PolicyObservation> observations,
                                           std::uint64_t tick);

private:
  IPolicyRuntime &runtime_;
  AIAction fallback_action_{};
  std::uint64_t max_stale_ticks_{};
  std::vector<AIAction> cached_actions_;
  std::uint64_t cached_tick_{};
  bool has_cache_{};
};

struct SelfPlayFrame final {
  std::uint64_t step{};
  std::vector<PolicyObservation> observations;
  std::vector<double> rewards;
  bool done{};
};

class NEXORA_AI_API ISelfPlayEnvironment {
public:
  virtual ~ISelfPlayEnvironment() = default;
  virtual bool Reset(std::uint64_t seed) = 0;
  [[nodiscard]] virtual std::vector<PolicyObservation> Observe() const = 0;
  virtual bool ApplyActions(std::span<const AIAction> actions) = 0;
  virtual bool StepSimulation() = 0;
  [[nodiscard]] virtual std::vector<double> Rewards() const = 0;
  [[nodiscard]] virtual bool Done() const = 0;
};

class NEXORA_AI_API SelfPlayBridge final {
public:
  explicit SelfPlayBridge(ISelfPlayEnvironment &environment) : environment_(environment) {}
  [[nodiscard]] std::optional<SelfPlayFrame> Reset(std::uint64_t seed);
  [[nodiscard]] std::optional<SelfPlayFrame> Step(std::span<const AIAction> actions);

private:
  [[nodiscard]] SelfPlayFrame Capture() const;
  ISelfPlayEnvironment &environment_;
  std::uint64_t step_{};
};

using SelfPlayWorldId = std::uint64_t;

enum class SelfPlayWorldState : std::uint8_t { Pending, Active, Completed, Failed };

struct SelfPlayWorldSnapshot final {
  SelfPlayWorldId id{};
  std::uint64_t seed{};
  SelfPlayWorldState state{SelfPlayWorldState::Pending};
  std::optional<SelfPlayFrame> frame;
  std::string failure;
  bool used_fallback{};
  bool reused_cached_action{};
};

struct SelfPlayOrchestratorTick final {
  std::size_t processed_worlds{};
  std::size_t deferred_worlds{};
  std::vector<SelfPlayWorldSnapshot> updates;
};

struct SelfPlayOrchestratorStats final {
  std::size_t registered_worlds{};
  std::size_t pending_worlds{};
  std::size_t active_worlds{};
  std::size_t completed_worlds{};
  std::size_t failed_worlds{};
  std::uint64_t simulation_steps{};
  std::size_t max_processed_per_tick{};
};

// Budgeted synchronous orchestration across caller-owned independent self-play environments.
class NEXORA_AI_API SelfPlayBatchOrchestrator final {
public:
  SelfPlayBatchOrchestrator(IPolicyRuntime &runtime, AIAction fallback_action,
                            std::uint64_t max_stale_ticks, std::uint64_t base_seed,
                            std::size_t max_worlds, std::size_t worlds_per_tick);
  ~SelfPlayBatchOrchestrator();
  SelfPlayBatchOrchestrator(const SelfPlayBatchOrchestrator &) = delete;
  SelfPlayBatchOrchestrator &operator=(const SelfPlayBatchOrchestrator &) = delete;

  [[nodiscard]] std::optional<SelfPlayWorldId> AddWorld(ISelfPlayEnvironment &environment);
  bool Retire(SelfPlayWorldId world);
  [[nodiscard]] SelfPlayOrchestratorTick Tick(std::uint64_t tick);
  [[nodiscard]] std::vector<SelfPlayWorldSnapshot> Snapshot() const;
  [[nodiscard]] const SelfPlayOrchestratorStats &Stats() const noexcept { return stats_; }

private:
  struct Slot;
  void RefreshStats() noexcept;

  IPolicyRuntime &runtime_;
  AIAction fallback_action_{};
  std::uint64_t max_stale_ticks_{};
  std::uint64_t base_seed_{};
  std::size_t max_worlds_{};
  std::size_t worlds_per_tick_{};
  SelfPlayWorldId next_world_id_{1};
  std::size_t next_slot_{};
  std::vector<std::unique_ptr<Slot>> slots_;
  SelfPlayOrchestratorStats stats_{};
};

} // namespace nexora::ai
