#include "Nexora/Core/TaskGraph.h"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace nexora::core {
namespace {
bool Intersects(const std::vector<TaskResource> &left, const std::vector<TaskResource> &right) {
  return std::ranges::any_of(left, [&right](TaskResource resource) {
    return std::ranges::find(right, resource) != right.end();
  });
}
bool Conflicts(const TaskDescriptor &earlier, const TaskDescriptor &later) {
  return Intersects(earlier.writes, later.reads) || Intersects(earlier.writes, later.writes) ||
         Intersects(earlier.reads, later.writes);
}
} // namespace

std::size_t TaskGraph::Add(TaskDescriptor descriptor) {
  if (!descriptor.function || descriptor.name.empty()) {
    throw std::invalid_argument("task name and function are required");
  }
  compiled_ = false;
  tasks_.push_back(std::move(descriptor));
  return tasks_.size() - 1;
}

void TaskGraph::Compile() {
  dependencies_.assign(tasks_.size(), {});
  for (std::size_t later = 0; later < tasks_.size(); ++later) {
    for (std::size_t earlier = 0; earlier < later; ++earlier) {
      if (Conflicts(tasks_[earlier], tasks_[later]))
        dependencies_[later].push_back(earlier);
    }
  }
  compiled_ = true;
}

std::vector<JobHandle> TaskGraph::Execute(JobSystem &jobs,
                                          const CancellationToken &cancellation) const {
  if (!compiled_)
    throw std::logic_error("task graph must be compiled before execution");
  std::vector<JobHandle> handles;
  handles.reserve(tasks_.size());
  for (std::size_t index = 0; index < tasks_.size(); ++index) {
    std::vector<JobHandle> prerequisites;
    prerequisites.reserve(dependencies_[index].size());
    for (const auto dependency : dependencies_[index])
      prerequisites.push_back(handles[dependency]);
    const auto &task = tasks_[index];
    handles.push_back(
        jobs.Submit({task.function, task.priority, cancellation, task.name}, prerequisites));
  }
  return handles;
}

std::span<const std::size_t> TaskGraph::Dependencies(std::size_t task) const {
  if (!compiled_ || task >= dependencies_.size())
    throw std::out_of_range("invalid compiled task");
  return dependencies_[task];
}
} // namespace nexora::core
