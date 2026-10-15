#pragma once
#include "Nexora/EditorImGui/EditorImGui.h"
#include "PlayGameplayModule.h"

namespace nexora::editor::preview {
// Application-thread bridge. Bind the new project scope before accepting its Play commands.
// A scope change hides the provider's older teardown; widgets receive only owning values.
class PlayHostMemoryPresenter final {
public:
  std::optional<imgui::PlayHostMemoryObservation> Observe(const PlayGameplayModule &module,
                                                          const ProjectWorkspace *workspace,
                                                          std::uint64_t generation,
                                                          runtime::PlaySession &play) {
    if (!workspace || !generation) {
      scope_.reset();
      return {};
    }
    const auto project = workspace->Project().id;
    const auto &root = workspace->Root();
    const auto serial = module.AllocationTeardownSequence();
    if (!scope_ || scope_->project != project || scope_->root != root ||
        scope_->project_generation != generation) {
      scope_ =
          imgui::PlayHostMemoryObservation{project, root, generation, play.Generation(), {}, {}};
      baseline_ = serial;
    }
    auto result = *scope_;
    result.play_generation = play.Generation();
    if (module.IsLoaded() && module.IsBoundTo(play.PlayWorld()))
      result.current = Copy(module.Allocations());
    // Exhausted serials cannot prove freshness; never wrap or relabel an old teardown.
    if (serial > baseline_ && serial != std::numeric_limits<std::uint64_t>::max())
      result.last_teardown = Copy(module.LastTeardownAllocations());
    return result;
  }

private:
  static imgui::PlayHostAllocationCounters Copy(PlayGameplayModule::AllocationSnapshot value) {
    return {value.live_allocations,      value.live_bytes,
            value.peak_allocations,      value.peak_bytes,
            value.rejected_allocations,  value.rejected_deallocations,
            value.reclaimed_allocations, value.reclaimed_bytes};
  }
  std::optional<imgui::PlayHostMemoryObservation> scope_;
  std::uint64_t baseline_{};
};
} // namespace nexora::editor::preview
