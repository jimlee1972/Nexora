#include "Nexora/Editor/ContentBrowser.h"

#include <array>
#include <iostream>
#include <stdexcept>

namespace {
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
} // namespace
int main() {
  try {
    using namespace nexora;
    editor::ContentItem item{
        {1, 2}, "Content/Broken.obj", ".obj", "", editor::ThumbnailState::Failed};
    // Exact byte limit with multibyte text, plus literal formatting/ImGui marker characters.
    item.import_diagnostic = std::string(1015, 'a') + "失敗%##";
    editor::ContentBrowserModel model;
    Require(item.import_diagnostic.size() == 1024 && model.Reset(std::array{item}, 7) &&
                model.Select(item.id),
            "exact-limit diagnostic was not admitted");
    const auto revision = model.Revision();
    for (const auto &invalid : {item.import_diagnostic + "!", std::string("bad\0text", 8),
                                std::string("bad\xff", 4), std::string("bad\xe4\xb8", 5)}) {
      auto candidate = item;
      candidate.import_diagnostic = invalid;
      Require(!model.Reset(std::array{candidate}, 8) && model.Revision() == revision &&
                  model.ProjectGeneration() == 7 && model.IsSelected(item.id) &&
                  model.Find(item.id)->import_diagnostic == item.import_diagnostic,
              "invalid diagnostic replaced live model or selection");
      candidate.id = {3, 4};
      candidate.path = "Content/New.obj";
      Require(!model.Discover(candidate) && model.Revision() == revision,
              "Discover admitted invalid diagnostic");
    }
    Require(model.Rename(item.id, "Moved.obj") &&
                model.Find(item.id)->import_diagnostic == item.import_diagnostic,
            "rename lost owning diagnostic");
    const auto renamed_revision = model.Revision();
    Require(!model.PublishArtifact(item.id, "", editor::ThumbnailState::Ready) &&
                model.Revision() == renamed_revision &&
                model.Find(item.id)->import_diagnostic == item.import_diagnostic,
            "failed publication cleared diagnostic");
    Require(model.Undo() && model.Find(item.id)->path == item.path &&
                model.Find(item.id)->import_diagnostic == item.import_diagnostic,
            "Undo lost failure diagnostic");
    Require(model.Rename(item.id, "Repaired.obj") &&
                model.PublishArtifact(item.id, "repaired", editor::ThumbnailState::Ready) &&
                model.Find(item.id)->import_diagnostic.empty() && model.Undo() &&
                model.Find(item.id)->import_diagnostic.empty() &&
                model.Find(item.id)->artifact_hash == "repaired" &&
                model.Find(item.id)->thumbnail == editor::ThumbnailState::Ready,
            "successful reimport or content Undo resurrected failure diagnostic");
    item.id = {5, 6};
    item.path = "Content/Discovered.obj";
    item.import_diagnostic = "new failure";
    Require(model.Discover(item) && model.Find(item.id)->import_diagnostic == "new failure",
            "Discover discarded valid owning diagnostic");
    std::cout << "Content import diagnostic admission and history contracts passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
