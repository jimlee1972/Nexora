#pragma once
#include "Nexora/Editor/SceneMergePublication.h"
#include <atomic>
namespace nexora::editor {
// Private test-build-only scheduling seam; never a retained native callback or production override.
struct SceneMergePublicationGate final {
  std::atomic_bool entered{false}, release{false};
};
class NEXORA_EDITOR_API SceneMergePublicationTestAccess final {
public:
  static void Gate(SceneMergePublication &, std::shared_ptr<SceneMergePublicationGate>);
};
} // namespace nexora::editor
