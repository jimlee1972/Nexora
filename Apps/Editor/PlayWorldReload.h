#pragma once
#include "Nexora/Runtime/Runtime.h"
#include "nexora/gameplay_scene.h"

namespace nexora::twod {
class GameSpriteSession;
}
namespace nexora::editor::preview {
// Optional owner-thread transaction for an embedding with more state than a bare World.
// begin owns unpublished/retired storage before returning. validate may prepare CPU/native
// presentation and reject before old code retires; it catches all preparation failures.
// retire, commit and rollback do not allocate or throw.
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
  // Optional main Game metadata paired with the unpublished World returned by begin.
  // Kept alive through candidate callbacks/finalizers, then adopted at commit or discarded.
  twod::GameSpriteSession *(*sprites)(void *) noexcept {};
  [[nodiscard]] bool Present() const noexcept {
    return begin || validate || retire || commit || rollback || sprites;
  }
  [[nodiscard]] bool Complete() const noexcept {
    return begin && validate && retire && commit && rollback;
  }
};
} // namespace nexora::editor::preview
