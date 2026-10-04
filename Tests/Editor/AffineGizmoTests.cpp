#include "Nexora/Editor/EditorWorkspace.h"
#include "Nexora/Editor/ViewportMath.h"

#include <array>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <limits>
#include <numbers>
#include <stdexcept>

namespace {
using namespace nexora;
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
bool Near(double a, double b) { return std::abs(a - b) < 1e-9; }
void SameMatrix(const runtime::TransformMatrix &a, const runtime::TransformMatrix &b) {
  for (std::size_t i = 0; i < a.size(); ++i)
    Require(Near(a[i], b[i]), "prospective affine matrix differs from committed matrix");
}
void Run() {
  runtime::World world;
  const auto scene = world.LoadScene("Exact gizmo positions");
  Require(world.Activate(scene), "scene activation failed");
  editor::SceneDocument document(world, scene);
  const auto root = document.Create("Mirrored stretch");
  const auto parent = document.Create("Rotated parent", root);
  const auto leaf = document.Create("Selected leaf", parent);
  const auto descendant = document.Create("Following descendant", leaf);
  runtime::Transform stretched{10, -3, 4};
  stretched.sx = -2;
  stretched.sy = 3;
  stretched.sz = 0.5;
  runtime::Transform turned{};
  turned.qz = std::sin(std::numbers::pi / 8);
  turned.qw = std::cos(std::numbers::pi / 8);
  Require(document.SetTransform(root, stretched) && document.SetTransform(parent, turned) &&
              document.SetTransform(leaf, {1, 0, 2}) &&
              document.SetTransform(descendant, {2, -1, 1}) &&
              document.Select(std::array{leaf, descendant}),
          "gizmo fixture failed");
  const auto path =
      std::filesystem::temp_directory_path() /
      ("nexora-affine-gizmo-" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".scene");
  struct Cleanup {
    std::filesystem::path path;
    ~Cleanup() {
      std::error_code error;
      std::filesystem::remove(path, error);
    }
  } cleanup{path};
  Require(document.Save(path), "baseline save failed");
  const auto baseline = *world.SaveScene(scene);
  const auto before = *document.WorldMatrix(leaf);
  const auto leaf_local = *document.Transform(leaf);
  const auto descendant_local = *document.Transform(descendant);
  const double c = std::sqrt(0.5);
  Require(Near(before[12], 10 - 2 * c) && Near(before[13], -3 + 3 * c) && Near(before[14], 5),
          "closed-form world origin differs from affine matrix");
  const auto frame = document.SelectionGizmoFrame(editor::GizmoPivot::Center);
  Require(frame && Near(frame->x, before[12]) && Near(frame->y, before[13]),
          "selected descendants must not change the exact gizmo center");
  Require(!document.WorldMatrix(999'999), "unknown document node exposed a matrix");
  const auto foreign_scene = world.LoadScene("Foreign document");
  const auto foreign = world.CreateEntity(foreign_scene).id;
  Require(!document.WorldMatrix(foreign), "foreign scene matrix escaped document ownership");
  const std::array keys{*document.Key(leaf), *document.Key(descendant)};
  std::array<editor::GizmoOperation, 5> operations;
  operations[0].translation = {2, -3, 4};
  operations[1].kind = editor::GizmoOperation::Kind::Rotate;
  operations[1].pivot = editor::GizmoPivot::Center;
  operations[1].center = {1, 2, 3};
  operations[1].axis = {0, 0, 1};
  operations[1].angle = std::numbers::pi / 2;
  operations[2].kind = editor::GizmoOperation::Kind::Scale;
  operations[2].pivot = editor::GizmoPivot::Center;
  operations[2].center = {1, 2, 3};
  operations[2].factors = {2, 3, 0.5};
  operations[3] = operations[1];
  operations[3].pivot = editor::GizmoPivot::Pivot;
  operations[4] = operations[2];
  operations[4].pivot = editor::GizmoPivot::Pivot;
  const auto captured = editor::GizmoTargets(world, std::array{leaf});
  Require(captured && captured->front().parent_chain && captured->front().parent_chain->size() == 2,
          "gizmo targets did not capture the complete owning ancestry");
  const auto captured_move = editor::ApplyGizmo(*captured, operations[0]);
  auto moved_root = stretched;
  moved_root.x += 100;
  runtime::WorldCommandBuffer move_root, restore_root;
  move_root.SetTransform(root, moved_root);
  restore_root.SetTransform(root, stretched);
  Require(move_root.Apply(world), "captured ancestry mutation failed");
  const auto retained_move = editor::ApplyGizmo(*captured, operations[0]);
  Require(restore_root.Apply(world), "captured ancestry restore failed");
  Require(captured_move && retained_move && *captured_move == *retained_move,
          "captured gizmo ancestry borrowed mutable Runtime transforms");
  auto malformed = *captured;
  malformed.front().parent_chain->front().sx = 0;
  Require(!editor::ApplyGizmo(malformed, operations[0]),
          "a malformed captured ancestor was accepted");
  for (const auto &operation : operations) {
    const auto poses = document.PreviewSelectionGizmo(keys, operation);
    const auto matrices = document.PreviewSelectionGizmoMatrices(keys, operation);
    Require(poses && matrices && matrices->size() == 4 && !document.Dirty() &&
                world.SaveScene(scene) == baseline && document.Selection().size() == 2,
            "preview mutated the document or omitted descendants");
    const auto &predicted = matrices->at(leaf);
    if (operation.kind == editor::GizmoOperation::Kind::Translate) {
      Require(Near(predicted[12], before[12] + 2) && Near(predicted[13], before[13] - 3) &&
                  Near(predicted[14], before[14] + 4),
              "world translation was converted through a lossy parent transform");
    } else if (operation.pivot == editor::GizmoPivot::Center) {
      if (operation.kind == editor::GizmoOperation::Kind::Rotate)
        Require(Near(predicted[12], 1 - (before[13] - 2)) &&
                    Near(predicted[13], 2 + (before[12] - 1)) && Near(predicted[14], before[14]),
                "center rotation did not swing the exact origin");
      else
        Require(Near(predicted[12], 1 + 2 * (before[12] - 1)) &&
                    Near(predicted[13], 2 + 3 * (before[13] - 2)) &&
                    Near(predicted[14], 3 + 0.5 * (before[14] - 3)),
                "center scale did not scale the exact origin offset");
    } else {
      Require(Near(predicted[12], before[12]) && Near(predicted[13], before[13]) &&
                  Near(predicted[14], before[14]),
              "pivot rotate/scale drifted an entity origin");
    }
    Require(document.ApplySelectionGizmo(keys, operation), "gesture commit failed");
    Require(*document.Transform(descendant) == descendant_local,
            "selected descendant was edited twice");
    for (const auto &[id, matrix] : *matrices) {
      SameMatrix(matrix, *document.WorldMatrix(id));
      const auto actual = *document.WorldTransform(id);
      const auto preview = poses->at(id);
      Require(Near(actual.x, matrix[12]) && Near(actual.y, matrix[13]) &&
                  Near(actual.z, matrix[14]) && Near(actual.x, preview.x) &&
                  Near(actual.y, preview.y) && Near(actual.z, preview.z),
              "world pose origin and preview matrix disagree");
    }
    Require(document.Undo() && !document.Dirty() && world.SaveScene(scene) == baseline &&
                *document.Transform(leaf) == leaf_local,
            "gesture was not one atomic undo");
    Require(document.PreviewSelectionGizmoMatrices(keys, operation) && document.Redo(),
            "preview discarded the redo branch");
    SameMatrix(predicted, *document.WorldMatrix(leaf));
    Require(document.Undo() && !document.Dirty(), "redo did not undo to the clean baseline");
  }
  auto stale = keys;
  ++stale[0].document_generation;
  auto invalid = operations[0];
  invalid.translation.x = std::numeric_limits<double>::infinity();
  Require(
      !document.PreviewSelectionGizmoMatrices(stale, operations[0]) &&
          !document.PreviewSelectionGizmoMatrices(std::array{keys[0], keys[0]}, operations[0]) &&
          !document.PreviewSelectionGizmoMatrices({}, operations[0]) &&
          !document.PreviewSelectionGizmoMatrices(keys, invalid) && !document.Dirty() &&
          document.Redo() && document.Undo(),
      "rejected preview mutated history");
  Require(document.TranslateSelection(keys, 2, -3, 4) && document.Save(path),
          "exact position save failed");
  const auto saved_matrix = *document.WorldMatrix(descendant);
  Require(document.Reload(path), "exact position reload failed");
  SameMatrix(saved_matrix, *document.WorldMatrix(descendant));
  Require(Near(before[12], 10 - 2 * c), "borrowed matrix changed across mutation");
}
} // namespace
int main() {
  try {
    Run();
    std::cout << "Affine gizmo contracts passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
