#include "Nexora/AI/AI.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <queue>
#include <unordered_set>

namespace nexora::ai {
namespace {
constexpr double kEpsilon = 1e-9;

bool Finite(Vec3 value) {
  return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

Vec3 Add(Vec3 a, Vec3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
Vec3 Sub(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
Vec3 Mul(Vec3 value, double scale) { return {value.x * scale, value.y * scale, value.z * scale}; }
double LengthSquared(Vec3 value) {
  return value.x * value.x + value.y * value.y + value.z * value.z;
}
double Length(Vec3 value) { return std::sqrt(LengthSquared(value)); }
Vec3 Normalize(Vec3 value) {
  const auto length = Length(value);
  return length > kEpsilon ? Mul(value, 1.0 / length) : Vec3{};
}

double Distance(Vec3 a, Vec3 b) { return Length(Sub(a, b)); }

std::int64_t CrowdCell(double x, double size) {
  return static_cast<std::int64_t>(std::floor(x / size));
}

std::uint64_t CrowdKey(std::int64_t x, std::int64_t z) {
  const auto ux = static_cast<std::uint64_t>(static_cast<std::uint32_t>(x));
  const auto uz = static_cast<std::uint64_t>(static_cast<std::uint32_t>(z));
  return (ux << 32U) | uz;
}

double CurveValue(const UtilityConsideration &consideration) {
  const auto input = std::clamp(consideration.input, 0.0, 1.0);
  switch (consideration.curve) {
  case UtilityCurve::Linear:
    return input;
  case UtilityCurve::Inverse:
    return 1.0 - input;
  case UtilityCurve::Quadratic:
    return input * input;
  case UtilityCurve::Step:
    return input >= consideration.threshold ? 1.0 : 0.0;
  }
  return 0.0;
}
} // namespace

GridCostField::GridCostField(Vec3 origin, double cell_size, std::size_t width, std::size_t height,
                             double default_cost)
    : origin_(origin), cell_size_(cell_size), width_(width), height_(height),
      costs_(width * height, std::max(0.0, default_cost)) {
  if (!Finite(origin_) || !std::isfinite(cell_size_) || cell_size_ <= 0.0 || width_ == 0 ||
      height_ == 0) {
    cell_size_ = 0.0;
    width_ = 0;
    height_ = 0;
    costs_.clear();
  }
}

bool GridCostField::Set(std::size_t x, std::size_t z, double cost) {
  if (x >= width_ || z >= height_ || !std::isfinite(cost) || cost < 0.0)
    return false;
  costs_[z * width_ + x] = cost;
  ++generation_;
  return true;
}

double GridCostField::Cost(Vec3 position) const {
  if (cell_size_ <= 0.0 || !Finite(position))
    return 0.0;
  const auto x = static_cast<std::int64_t>(std::floor((position.x - origin_.x) / cell_size_));
  const auto z = static_cast<std::int64_t>(std::floor((position.z - origin_.z) / cell_size_));
  if (x < 0 || z < 0 || static_cast<std::size_t>(x) >= width_ ||
      static_cast<std::size_t>(z) >= height_)
    return 0.0;
  return costs_[static_cast<std::size_t>(z) * width_ + static_cast<std::size_t>(x)];
}

bool HierarchicalNavigationWorld::AddRegion(NavRegion region) {
  if (region.id == 0 || regions_.contains(region.id))
    return false;
  for (const auto neighbour : region.neighbours)
    if (neighbour == 0 || neighbour == region.id)
      return false;
  regions_.emplace(region.id, std::move(region));
  ++generation_;
  return true;
}

bool HierarchicalNavigationWorld::AddNode(NavNode node) {
  if (node.id == 0 || node.region == 0 || !regions_.contains(node.region) ||
      nodes_.contains(node.id) || !Finite(node.position))
    return false;
  for (const auto neighbour : node.neighbours)
    if (neighbour == 0 || neighbour == node.id)
      return false;
  nodes_.emplace(node.id, std::move(node));
  ++generation_;
  return true;
}

bool HierarchicalNavigationWorld::RemoveRegion(NavRegionId region) {
  if (!regions_.erase(region))
    return false;
  for (auto it = nodes_.begin(); it != nodes_.end();) {
    if (it->second.region == region)
      it = nodes_.erase(it);
    else
      ++it;
  }
  ++generation_;
  return true;
}

const NavNode *HierarchicalNavigationWorld::FindNode(NavNodeId node) const {
  const auto found = nodes_.find(node);
  return found == nodes_.end() ? nullptr : &found->second;
}

std::optional<NavigationPath>
HierarchicalNavigationWorld::FindPath(NavNodeId start, NavNodeId goal,
                                      const INavigationCostProvider *cost) const {
  const auto start_it = nodes_.find(start);
  const auto goal_it = nodes_.find(goal);
  if (start_it == nodes_.end() || goal_it == nodes_.end())
    return std::nullopt;

  std::unordered_set<NavRegionId> allowed_regions;
  if (start_it->second.region == goal_it->second.region) {
    allowed_regions.insert(start_it->second.region);
  } else {
    std::queue<NavRegionId> open_regions;
    std::unordered_map<NavRegionId, NavRegionId> parent_regions;
    open_regions.push(start_it->second.region);
    parent_regions[start_it->second.region] = 0;
    while (!open_regions.empty() && !parent_regions.contains(goal_it->second.region)) {
      const auto current = open_regions.front();
      open_regions.pop();
      const auto region = regions_.find(current);
      if (region == regions_.end())
        continue;
      for (const auto next : region->second.neighbours) {
        if (!regions_.contains(next) || parent_regions.contains(next))
          continue;
        parent_regions[next] = current;
        open_regions.push(next);
      }
    }
    if (!parent_regions.contains(goal_it->second.region))
      return std::nullopt;
    for (auto at = goal_it->second.region; at != 0; at = parent_regions.at(at))
      allowed_regions.insert(at);
  }

  struct Candidate final {
    double total{};
    NavNodeId node{};
    bool operator>(const Candidate &other) const {
      if (total != other.total)
        return total > other.total;
      return node > other.node;
    }
  };

  std::priority_queue<Candidate, std::vector<Candidate>, std::greater<>> open;
  std::unordered_map<NavNodeId, double> distance{{start, 0.0}};
  std::unordered_map<NavNodeId, NavNodeId> parent{{start, 0}};
  open.push({0.0, start});

  while (!open.empty()) {
    const auto current = open.top();
    open.pop();
    if (current.node == goal)
      break;
    const auto current_it = nodes_.find(current.node);
    if (current_it == nodes_.end() || !allowed_regions.contains(current_it->second.region))
      continue;
    const auto known = distance.find(current.node);
    if (known == distance.end() || current.total > known->second + kEpsilon)
      continue;
    for (const auto next_id : current_it->second.neighbours) {
      const auto next_it = nodes_.find(next_id);
      if (next_it == nodes_.end() || !allowed_regions.contains(next_it->second.region))
        continue;
      auto edge = Distance(current_it->second.position, next_it->second.position);
      if (cost)
        edge += std::max(0.0, cost->Cost(next_it->second.position));
      const auto next_distance = current.total + edge;
      const auto existing = distance.find(next_id);
      if (existing == distance.end() || next_distance + kEpsilon < existing->second) {
        distance[next_id] = next_distance;
        parent[next_id] = current.node;
        open.push({next_distance, next_id});
      }
    }
  }

  if (!parent.contains(goal))
    return std::nullopt;
  std::vector<NavNodeId> path;
  for (auto at = goal; at != 0; at = parent.at(at))
    path.push_back(at);
  std::ranges::reverse(path);
  return NavigationPath{std::move(path), generation_};
}

NavigationQueryScheduler::NavigationQueryScheduler(std::size_t budget_per_tick)
    : budget_per_tick_(budget_per_tick) {}

NavigationRequestId NavigationQueryScheduler::Submit(AgentId agent, NavNodeId start, NavNodeId goal,
                                                     std::int32_t priority, std::uint64_t tick,
                                                     std::uint64_t navigation_generation) {
  if (agent == 0 || start == 0 || goal == 0)
    return 0;
  if (const auto old = latest_by_agent_.find(agent); old != latest_by_agent_.end())
    Cancel(old->second);
  const auto id = next_id_++;
  pending_.push_back({{id, agent, start, goal, priority, tick, navigation_generation}, false});
  latest_by_agent_[agent] = id;
  ++stats_.submitted;
  return id;
}

bool NavigationQueryScheduler::Cancel(NavigationRequestId id) {
  for (auto &pending : pending_) {
    if (pending.request.id != id || pending.cancelled)
      continue;
    pending.cancelled = true;
    ++stats_.cancelled;
    return true;
  }
  return false;
}

std::size_t NavigationQueryScheduler::Process(const HierarchicalNavigationWorld &world,
                                              std::uint64_t tick,
                                              const INavigationCostProvider *cost) {
  std::ranges::stable_sort(pending_, [tick](const Pending &a, const Pending &b) {
    if (a.cancelled != b.cancelled)
      return !a.cancelled;
    if (a.request.priority != b.request.priority)
      return a.request.priority > b.request.priority;
    const auto age_a = tick >= a.request.submitted_tick ? tick - a.request.submitted_tick : 0;
    const auto age_b = tick >= b.request.submitted_tick ? tick - b.request.submitted_tick : 0;
    if (age_a != age_b)
      return age_a > age_b;
    return a.request.id < b.request.id;
  });

  std::size_t processed = 0;
  std::vector<Pending> deferred;
  deferred.reserve(pending_.size());
  for (auto &pending : pending_) {
    if (pending.cancelled) {
      results_.push_back({pending.request.id, pending.request.agent,
                          NavigationResultStatus::Cancelled, std::nullopt});
      const auto latest = latest_by_agent_.find(pending.request.agent);
      if (latest != latest_by_agent_.end() && latest->second == pending.request.id)
        latest_by_agent_.erase(latest);
      continue;
    }
    if (processed >= budget_per_tick_) {
      deferred.push_back(std::move(pending));
      continue;
    }
    ++processed;
    if (pending.request.navigation_generation != world.Generation()) {
      results_.push_back({pending.request.id, pending.request.agent, NavigationResultStatus::Stale,
                          std::nullopt});
      ++stats_.stale;
    } else {
      auto path = world.FindPath(pending.request.start, pending.request.goal, cost);
      const auto status = path ? NavigationResultStatus::Completed
                               : NavigationResultStatus::Unreachable;
      results_.push_back({pending.request.id, pending.request.agent, status, std::move(path)});
      ++stats_.completed;
    }
    const auto latest = latest_by_agent_.find(pending.request.agent);
    if (latest != latest_by_agent_.end() && latest->second == pending.request.id)
      latest_by_agent_.erase(latest);
  }
  pending_ = std::move(deferred);
  stats_.deferred += pending_.size();
  stats_.max_processed_per_tick = std::max(stats_.max_processed_per_tick, processed);
  return processed;
}

std::vector<NavigationResult> NavigationQueryScheduler::DrainResults() {
  auto out = std::move(results_);
  results_.clear();
  return out;
}

std::size_t NavigationQueryScheduler::PendingCount() const noexcept {
  return static_cast<std::size_t>(std::ranges::count_if(
      pending_, [](const Pending &pending) { return !pending.cancelled; }));
}

std::vector<CrowdResult> CrowdSystem::Solve(std::span<const CrowdAgentInput> agents) const {
  std::vector<CrowdResult> out;
  out.reserve(agents.size());
  if (agents.empty())
    return out;

  double max_radius = 0.5;
  for (const auto &agent : agents)
    if (std::isfinite(agent.radius) && agent.radius > 0.0)
      max_radius = std::max(max_radius, agent.radius);
  const auto cell_size = std::max(1.0, max_radius * 4.0);

  std::unordered_map<std::uint64_t, std::vector<std::size_t>> cells;
  cells.reserve(agents.size());
  for (std::size_t i = 0; i < agents.size(); ++i) {
    if (!Finite(agents[i].position))
      continue;
    cells[CrowdKey(CrowdCell(agents[i].position.x, cell_size),
                   CrowdCell(agents[i].position.z, cell_size))]
        .push_back(i);
  }

  for (std::size_t i = 0; i < agents.size(); ++i) {
    const auto &agent = agents[i];
    Vec3 velocity = Finite(agent.preferred_velocity) ? agent.preferred_velocity : Vec3{};
    Vec3 separation{};
    if (agent.agent != 0 && Finite(agent.position) && std::isfinite(agent.radius) &&
        agent.radius > 0.0) {
      const auto cx = CrowdCell(agent.position.x, cell_size);
      const auto cz = CrowdCell(agent.position.z, cell_size);
      for (std::int64_t dz = -1; dz <= 1; ++dz) {
        for (std::int64_t dx = -1; dx <= 1; ++dx) {
          const auto found = cells.find(CrowdKey(cx + dx, cz + dz));
          if (found == cells.end())
            continue;
          for (const auto j : found->second) {
            if (j == i)
              continue;
            const auto &other = agents[j];
            if (!Finite(other.position) || !std::isfinite(other.radius) || other.radius <= 0.0)
              continue;
            const auto away = Sub(agent.position, other.position);
            const auto distance = Length(away);
            const auto desired = agent.radius + other.radius;
            if (distance > kEpsilon && distance < desired)
              separation = Add(separation, Mul(away, (desired - distance) / (distance * desired)));
          }
        }
      }
    }
    velocity = Add(velocity, separation);
    const auto speed_limit = std::isfinite(agent.max_speed) ? std::max(0.0, agent.max_speed) : 0.0;
    const auto speed = std::min(Length(velocity), speed_limit);
    const auto direction = Normalize(velocity);
    out.push_back(
        {agent.agent, {direction, speed, direction, CharacterMovementMode::Grounded, 0}});
  }
  return out;
}

std::optional<UtilityDecision> UtilityAI::Select(std::span<const UtilityAction> actions) const {
  std::optional<UtilityDecision> best;
  for (const auto &candidate : actions) {
    if (!std::isfinite(candidate.bias) || candidate.bias < 0.0)
      continue;
    double score = candidate.bias;
    bool valid = true;
    for (const auto &consideration : candidate.considerations) {
      if (!std::isfinite(consideration.input) || !std::isfinite(consideration.weight) ||
          consideration.weight < 0.0 || !std::isfinite(consideration.threshold)) {
        valid = false;
        break;
      }
      score *= std::pow(CurveValue(consideration), consideration.weight);
    }
    if (!valid || !std::isfinite(score))
      continue;
    if (!best || score > best->score + kEpsilon ||
        (std::abs(score - best->score) <= kEpsilon &&
         candidate.action.action_id < best->action.action_id))
      best = UtilityDecision{candidate.action, score};
  }
  return best;
}

AISchedule SimulationLODPolicy::Classify(const AgentRelevance &relevance) const {
  if (!std::isfinite(relevance.distance) || relevance.distance < 0.0)
    return {};
  if (relevance.important || relevance.in_combat || relevance.distance <= 30.0)
    return {PerceptionLOD::Full, 1, 1, 1, false};
  if (relevance.visible || relevance.recently_interacted || relevance.distance <= 100.0)
    return {PerceptionLOD::Reduced, 4, 4, 4, false};
  if (relevance.distance <= 300.0)
    return {PerceptionLOD::Far, 16, 16, 32, false};
  return {PerceptionLOD::Dormant, 0, 0, 0, true};
}

bool SimulationLODPolicy::ShouldRun(AgentId agent, std::uint64_t tick,
                                    std::uint32_t interval_ticks) noexcept {
  if (agent == 0 || interval_ticks == 0)
    return false;
  return tick % interval_ticks == agent % interval_ticks;
}

SelfPlayFrame SelfPlayBridge::Capture() const {
  return {step_, environment_.Observe(), environment_.Rewards(), environment_.Done()};
}

std::optional<SelfPlayFrame> SelfPlayBridge::Reset(std::uint64_t seed) {
  if (!environment_.Reset(seed))
    return std::nullopt;
  step_ = 0;
  return Capture();
}

std::optional<SelfPlayFrame> SelfPlayBridge::Step(std::span<const AIAction> actions) {
  if (environment_.Done() || !environment_.ApplyActions(actions) ||
      !environment_.StepSimulation())
    return std::nullopt;
  ++step_;
  return Capture();
}

} // namespace nexora::ai
