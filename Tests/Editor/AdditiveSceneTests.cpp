#include "Nexora/Editor/SceneAuthoring.h"

#include <array>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
using nexora::editor::AdditiveScene;
using nexora::editor::AdditiveSceneGraph;
using nexora::editor::SceneDocumentId;

void Require(bool condition, const char *message) {
  if (!condition)
    throw std::runtime_error(message);
}

void AdmissionPreservesGraph() {
  AdditiveSceneGraph graph;
  Require(graph.Add({10, "Content/Base.scene", true, {}}) &&
              graph.Add({20, "Content/Lighting.scene", false, {10}}),
          "initial owned/reference scenes were rejected");
  const std::vector<SceneDocumentId> expected{10, 20};
  const auto rejected = std::array{
      AdditiveScene{0, "Content/Zero.scene", true, {}},
      AdditiveScene{30, {}, true, {}},
      AdditiveScene{30, "Content/Self.scene", true, {30}},
      AdditiveScene{30, "Content/Missing.scene", true, {999}},
      AdditiveScene{30, "Content/ZeroDependency.scene", true, {0}},
      AdditiveScene{30, "Content/Partial.scene", true, {10, 999}},
      AdditiveScene{10, "Content/Replacement.scene", false, {20}},
  };
  for (const auto &scene : rejected) {
    Require(!graph.Add(scene), "invalid initial scene admission succeeded");
    const auto *base = graph.Find(10);
    const auto *reference = graph.Find(20);
    Require(graph.LoadOrder() == expected && !graph.Find(0) && !graph.Find(30) &&
                !graph.Find(999) && base && base->path == "Content/Base.scene" && base->owned &&
                base->dependencies.empty() && reference &&
                reference->path == "Content/Lighting.scene" && !reference->owned &&
                reference->dependencies == std::vector<SceneDocumentId>{10},
            "rejected initial edges changed the retained graph");
  }
  // Rejected incoming edges must not prevent subsequent addition or safe reverse-order removal.
  Require(graph.Add({999, "Content/Later.scene", true, {20}}) &&
              graph.LoadOrder() == std::vector<SceneDocumentId>{10, 20, 999} && !graph.Remove(10) &&
              !graph.Remove(20) && graph.Remove(999) && graph.Remove(20) && graph.Remove(10) &&
              graph.LoadOrder().empty(),
          "rejected additions retained dangling dependencies or broke removal");
}

void DependencyNormalizationAndRollback() {
  AdditiveSceneGraph graph;
  // Deliberately admit independent roots in descending order: ready nodes order by stable ID.
  Require(graph.Add({40, "Content/Forty.scene", false, {}}) &&
              graph.Add({10, "Content/Ten.scene", true, {}}) &&
              graph.Add({30, "Content/Thirty.scene", false, {40, 10, 40, 10}}) &&
              graph.Find(30)->dependencies == std::vector<SceneDocumentId>{10, 40} &&
              graph.Add({50, "Content/Fifty.scene", true, {30, 30}}) &&
              graph.Find(50)->dependencies == std::vector<SceneDocumentId>{30} &&
              graph.LoadOrder() == std::vector<SceneDocumentId>{10, 40, 30, 50},
          "initial edges were not normalized or deterministic");
  Require(graph.SetDependencies(30, {10, 40, 10}) &&
              graph.Find(30)->dependencies == std::vector<SceneDocumentId>{10, 40},
          "initial and replacement edges use different normalization");
  const auto expected = graph.LoadOrder();
  Require(!graph.SetDependencies(10, {50}) && graph.Find(10)->dependencies.empty() &&
              !graph.SetDependencies(40, {40}) && graph.Find(40)->dependencies.empty() &&
              !graph.SetDependencies(30, {999}) &&
              graph.Find(30)->dependencies == std::vector<SceneDocumentId>{10, 40} &&
              graph.LoadOrder() == expected,
          "cyclic or invalid replacement edges changed the retained graph");
  Require(!graph.Remove(30) && graph.Remove(50) && graph.Remove(30) && graph.Remove(40) &&
              graph.Remove(10),
          "normalized edges or rollback broke safe removal");
}
} // namespace

int main() {
  try {
    AdmissionPreservesGraph();
    DependencyNormalizationAndRollback();
    std::cout << "Additive scene dependency contracts passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
