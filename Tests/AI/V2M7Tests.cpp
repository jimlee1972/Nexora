#include "Nexora/AI/AI.h"

#include <stdexcept>
#include <vector>

using namespace nexora::ai;

namespace {
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}

class FakePolicy final : public IPolicyRuntime {
public:
  const char *PolicyId() const noexcept override { return "fake-policy"; }
  std::uint64_t PolicyVersion() const noexcept override { return 7; }
  PolicyEvaluationStatus Evaluate(std::span<const PolicyObservation> observations,
                                  std::vector<AIAction> &actions) override {
    actions.clear();
    for (const auto &observation : observations)
      actions.push_back({static_cast<std::uint32_t>(observation.agent),
                         {{1, 0, 0}, 2.0, {1, 0, 0}, CharacterMovementMode::Grounded, 0}});
    return PolicyEvaluationStatus::Ready;
  }
};

class SequencedPolicy final : public IPolicyRuntime {
public:
  const char *PolicyId() const noexcept override { return "sequenced-policy"; }
  std::uint64_t PolicyVersion() const noexcept override { return 1; }

  PolicyEvaluationStatus Evaluate(std::span<const PolicyObservation> observations,
                                  std::vector<AIAction> &actions) override {
    actions.clear();
    if (calls_++ == 0) {
      for (const auto &observation : observations)
        actions.push_back({static_cast<std::uint32_t>(observation.agent),
                           {{0, 0, 1}, 3.0, {0, 0, 1}, CharacterMovementMode::Grounded, 0}});
      return PolicyEvaluationStatus::Ready;
    }
    return PolicyEvaluationStatus::Deferred;
  }

private:
  std::uint32_t calls_{};
};

class FakeEnvironment final : public ISelfPlayEnvironment {
public:
  bool Reset(std::uint64_t seed) override {
    seed_ = seed;
    step_ = 0;
    done_ = false;
    return true;
  }
  std::vector<PolicyObservation> Observe() const override {
    return {{1, {static_cast<float>(seed_), static_cast<float>(step_)}}};
  }
  bool ApplyActions(std::span<const AIAction> actions) override {
    if (actions.size() != 1)
      return false;
    last_ = actions.front();
    return true;
  }
  bool StepSimulation() override {
    ++step_;
    done_ = step_ >= 2;
    return true;
  }
  std::vector<double> Rewards() const override { return {last_.character.desired_speed}; }
  bool Done() const override { return done_; }

private:
  std::uint64_t seed_{}, step_{};
  bool done_{};
  AIAction last_{};
};
} // namespace

int main() {
  HierarchicalNavigationWorld world;
  Require(world.AddRegion({1, {2}}) && world.AddRegion({2, {1, 3}}) &&
              world.AddRegion({3, {2}}),
          "regions rejected");
  Require(world.AddNode({10, 1, {0, 0, 0}, {11}}) &&
              world.AddNode({11, 1, {5, 0, 0}, {10, 20}}) &&
              world.AddNode({20, 2, {10, 0, 0}, {11, 21}}) &&
              world.AddNode({21, 2, {15, 0, 0}, {20, 30}}) &&
              world.AddNode({30, 3, {20, 0, 0}, {21}}),
          "nodes rejected");

  GridCostField field({0, 0, 0}, 5.0, 8, 2);
  Require(field.Set(3, 0, 25.0), "cost field rejected update");
  const auto direct = world.FindPath(10, 30, &field);
  Require(direct && direct->nodes.front() == 10 && direct->nodes.back() == 30,
          "hierarchical path failed");

  HierarchicalNavigationWorld cost_world;
  Require(cost_world.AddRegion({1, {}}) &&
              cost_world.AddNode({1, 1, {0, 0, 0}, {2, 3}}) &&
              cost_world.AddNode({2, 1, {1, 0, 0}, {1, 4}}) &&
              cost_world.AddNode({3, 1, {0, 0, 1}, {1, 4}}) &&
              cost_world.AddNode({4, 1, {1, 0, 1}, {2, 3}}),
          "cost-field navigation setup failed");
  GridCostField reroute_cost({0, 0, 0}, 1.0, 3, 3);
  Require(reroute_cost.Set(1, 0, 100.0), "reroute cost rejected");
  const auto rerouted = cost_world.FindPath(1, 4, &reroute_cost);
  Require(rerouted && rerouted->nodes.size() == 3 && rerouted->nodes[1] == 3,
          "influence/cost field did not affect route selection");

  NavigationQueryScheduler priority_scheduler(1);
  const auto low_priority =
      priority_scheduler.Submit(7001, 10, 30, 0, 10, world.Generation());
  const auto high_priority =
      priority_scheduler.Submit(7002, 10, 30, 100, 10, world.Generation());
  Require(low_priority != 0 && high_priority != 0 &&
              priority_scheduler.Process(world, 10, &field) == 1,
          "priority scheduler setup failed");
  const auto priority_results = priority_scheduler.DrainResults();
  Require(priority_results.size() == 1 && priority_results[0].id == high_priority,
          "navigation priority was not respected");

  const auto replaced =
      priority_scheduler.Submit(7003, 10, 30, 0, 11, world.Generation());
  const auto replacement =
      priority_scheduler.Submit(7003, 10, 30, 1, 11, world.Generation());
  const auto cancelled =
      priority_scheduler.Submit(7004, 10, 30, 2, 11, world.Generation());
  Require(replaced != 0 && replacement != 0 && cancelled != 0 &&
              priority_scheduler.Cancel(cancelled),
          "navigation cancellation/coalescing setup failed");
  priority_scheduler.Process(world, 11, &field);
  const auto coalesced_results = priority_scheduler.DrainResults();
  bool replaced_cancelled = false;
  bool explicit_cancelled = false;
  for (const auto &result : coalesced_results) {
    replaced_cancelled |=
        result.id == replaced && result.status == NavigationResultStatus::Cancelled;
    explicit_cancelled |=
        result.id == cancelled && result.status == NavigationResultStatus::Cancelled;
  }
  Require(replaced_cancelled && explicit_cancelled,
          "navigation cancellation/coalescing did not produce cancelled results");

  NavigationQueryScheduler scheduler(32);
  for (AgentId agent = 1; agent <= 5000; ++agent)
    Require(scheduler.Submit(agent, 10, 30, static_cast<std::int32_t>(agent % 4), 100,
                             world.Generation()) != 0,
            "navigation request rejected");
  const auto processed = scheduler.Process(world, 100, &field);
  Require(processed == 32 && scheduler.PendingCount() == 4968 &&
              scheduler.Stats().max_processed_per_tick == 32,
          "navigation scheduler exceeded per-tick budget");
  Require(scheduler.DrainResults().size() == 32, "navigation results missing");

  const auto stale_id = scheduler.Submit(6000, 10, 30, 100, 101, world.Generation());
  Require(stale_id != 0 && world.RemoveRegion(3), "stale setup failed");
  scheduler.Process(world, 101, &field);
  const auto stale_results = scheduler.DrainResults();
  bool found_stale = false;
  for (const auto &result : stale_results)
    if (result.id == stale_id)
      found_stale = result.status == NavigationResultStatus::Stale;
  Require(found_stale, "stale navigation request was not rejected");

  CrowdSystem crowd;
  const CrowdAgentInput crowd_inputs[]{{1, {0, 0, 0}, {1, 0, 0}, 0.6, 3.0},
                                       {2, {0.5, 0, 0}, {-1, 0, 0}, 0.6, 3.0}};
  const auto crowd_results = crowd.Solve(crowd_inputs);
  Require(crowd_results.size() == 2 && crowd_results[0].agent == 1 &&
              crowd_results[0].intent.desired_speed <= 3.0 &&
              crowd_inputs[0].position == Vec3{0, 0, 0},
          "crowd did not produce intent-only output");

  UtilityAI utility;
  const UtilityAction utility_actions[]{{{20, {{0, 0, 1}, 1.0, {0, 0, 1},
                                                   CharacterMovementMode::Grounded, 0}},
                                          1.0,
                                          {{0.8, 1.0, UtilityCurve::Linear, 0.5}}},
                                         {{10, {{1, 0, 0}, 2.0, {1, 0, 0},
                                                   CharacterMovementMode::Grounded, 0}},
                                          1.0,
                                          {{0.8, 1.0, UtilityCurve::Linear, 0.5}}}};
  const auto decision = utility.Select(utility_actions);
  Require(decision && decision->action.action_id == 10,
          "utility tie-break was not deterministic");
  const auto hysteresis_decision = utility.Select(utility_actions, 0, 20, 0.1);
  Require(hysteresis_decision && hysteresis_decision->action.action_id == 20,
          "utility hysteresis did not retain the current action");
  auto cooldown_actions = std::vector<UtilityAction>(std::begin(utility_actions),
                                                     std::end(utility_actions));
  cooldown_actions[1].cooldown_until_tick = 5;
  const auto cooldown_decision = utility.Select(cooldown_actions, 1);
  Require(cooldown_decision && cooldown_decision->action.action_id == 20,
          "utility cooldown did not suppress an unavailable action");

  SimulationLODPolicy lod;
  const auto far_schedule = lod.Classify({200.0, false, false, false, false});
  const auto dormant_schedule = lod.Classify({1000.0, false, false, false, false});
  Require(far_schedule.lod == PerceptionLOD::Far && far_schedule.perception_interval_ticks == 16 &&
              dormant_schedule.dormant,
          "far AI throttle/dormancy failed");
  std::vector<AIAgentScheduleInput> scheduled_agents;
  scheduled_agents.reserve(6000);
  for (AgentId agent = 1; agent <= 5000; ++agent)
    scheduled_agents.push_back({agent, {200.0, false, false, false, false}});
  for (AgentId agent = 5001; agent <= 6000; ++agent)
    scheduled_agents.push_back({agent, {1000.0, false, false, false, false}});

  AISimulationScheduler ai_scheduler;
  const auto work = ai_scheduler.Build(scheduled_agents, 200);
  const auto &ai_stats = ai_scheduler.Stats();
  Require(ai_stats.total_agents == 6000 && ai_stats.active_agents == 5000 &&
              ai_stats.dormant_agents == 1000 && ai_stats.perception_updates <= 313 &&
              ai_stats.decision_updates <= 313 && ai_stats.navigation_updates <= 157 &&
              work.size() <= 313,
          "far/dormant AI scheduling exceeded phase-staggered work");

  FakePolicy policy;
  const std::vector<PolicyObservation> observations{{42, {1.0F, 2.0F}}};
  std::vector<AIAction> policy_actions;
  Require(policy.Evaluate(observations, policy_actions) == PolicyEvaluationStatus::Ready &&
              policy_actions.size() == 1 &&
              policy_actions.front().character.desired_speed == 2.0,
          "learned policy runtime did not emit common AIAction");

  SequencedPolicy sequenced_policy;
  const AIAction fallback_action{
      999, {{0, 0, 0}, 0.5, {0, 0, 1}, CharacterMovementMode::Grounded, 0}};
  PolicyRuntimeDriver policy_driver(sequenced_policy, fallback_action, 2);
  const auto ready_policy = policy_driver.Evaluate(observations, 10);
  const auto delayed_policy = policy_driver.Evaluate(observations, 11);
  const auto stale_policy = policy_driver.Evaluate(observations, 20);
  Require(ready_policy.status == PolicyEvaluationStatus::Ready &&
              ready_policy.actions[0].character.desired_speed == 3.0 &&
              delayed_policy.status == PolicyEvaluationStatus::Deferred &&
              delayed_policy.reused_cached && !delayed_policy.used_fallback &&
              delayed_policy.actions[0].character.desired_speed == 3.0 &&
              stale_policy.used_fallback && !stale_policy.reused_cached &&
              stale_policy.actions[0].action_id == 999,
          "policy delayed-result/fallback handling failed");

  FakeEnvironment environment;
  SelfPlayBridge bridge(environment);
  const auto reset = bridge.Reset(123);
  Require(reset && reset->step == 0 && !reset->done && reset->observations.size() == 1,
          "self-play reset failed");
  const auto step1 = bridge.Step(policy_actions);
  Require(step1 && step1->step == 1 && !step1->done && step1->rewards[0] == 2.0,
          "self-play first step failed");
  const auto step2 = bridge.Step(policy_actions);
  Require(step2 && step2->done && step2->step == 2, "self-play termination failed");
  Require(!bridge.Step(policy_actions), "self-play accepted actions after termination");

  return 0;
}
