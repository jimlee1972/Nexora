#pragma once
// Opt-in test observation only; no input injection, document mutation, or retained pointers.
#include "EditorImGuiTestAccess.h"
#include <iomanip>
#include <sstream>

namespace nexora::editor::test {
inline std::string PrefabWorkflowEvidence(const imgui::EditorImGuiHost &ui,
                                          const SceneDocument *document, float dpi) {
  using Access = imgui::EditorImGuiTestAccess;
  const auto input = Access::PrefabInput(ui);
  std::ostringstream out;
  out << std::setprecision(17) << "prefab ui pointer=" << input.pointer[0] * dpi << ','
      << input.pointer[1] * dpi << " down=" << input.down << " active=" << input.active
      << " hovered=" << input.hovered << " undo=" << input.undo_attempts << ','
      << input.undo_applied << " redo=" << input.redo_attempts << ',' << input.redo_applied;
  out << " focused=" << input.focused << " keys=";
  for (const bool key : input.keys)
    out << key;
  for (std::size_t index = 0; index < input.enabled.size(); ++index)
    if (const auto point = Access::PrefabControlPosition(ui, index))
      out << " c" << index << '=' << (*point)[0] * dpi << ',' << (*point)[1] * dpi << ','
          << input.enabled[index];
  for (std::size_t row = 0; row < Access::PrefabReviewRowCount(ui); ++row) {
    if (const auto point = Access::PrefabPropertyPosition(ui, row))
      out << " p" << row << '=' << (*point)[0] * dpi << ',' << (*point)[1] * dpi;
    for (std::size_t option = 0; option < 3; ++option)
      if (const auto point = Access::PrefabRebaseChoicePosition(ui, row, option))
        out << " choice" << row << '_' << option << '=' << (*point)[0] * dpi << ','
            << (*point)[1] * dpi;
  }
  if (document) {
    out << " dirty=" << document->Dirty();
    for (const auto &node : document->Nodes()) {
      if (node.parent)
        continue;
      out << " node=" << node.id << " name_hex=";
      // Names are bounded by the document contract; hex keeps whitespace unambiguous.
      for (const unsigned char byte : document->Name(node.id))
        out << "0123456789abcdef"[byte >> 4] << "0123456789abcdef"[byte & 15];
      if (const auto transform = document->Transform(node.id))
        out << " x=" << transform->x;
      break;
    }
  }
  return out.str();
}
} // namespace nexora::editor::test
