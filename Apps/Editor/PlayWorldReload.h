#pragma once
#include "Nexora/Runtime/Runtime.h"
#include "nexora/gameplay_scene.h"

namespace nexora::editor::preview {
// Optional owner-thread transaction for an embedding with more state than a bare World.
// begin owns unpublished/retired storage before returning; all remaining hooks do not allocate.
// retire moves identical staged storage into the original Play World address and returns the
// quarantined old World. Keep both scene providers alive through module callbacks/finalizers.
// commit runs only after old code retires; rollback runs after rejected candidate code retires.
struct PlayWorldReload final {
  void *context{};
  bool (*begin)(void *, runtime::World *&, NexoraGameplaySceneApiV1 &) noexcept {};
  bool (*validate)(void *) noexcept {};
  void (*retire)(void *, runtime::World *&) noexcept {};
  void (*commit)(void *) noexcept {};
  void (*rollback)(void *) noexcept {};
  [[nodiscard]] bool Present() const noexcept {
    return begin || validate || retire || commit || rollback;
  }
  [[nodiscard]] bool Complete() const noexcept {
    return begin && validate && retire && commit && rollback;
  }
};
} // namespace nexora::editor::preview
