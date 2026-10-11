#include "Nexora/Editor/MaterialToolDocument.h"
#include <limits>

namespace nexora::editor {
namespace {
bool Fail(std::string *error, const char *message) {
  if (error)
    *error = message;
  return false;
}
void Clear(std::string *error) {
  if (error)
    error->clear();
}
} // namespace
bool MaterialToolDocument::Owner(std::string *error) const {
  return owner_ == std::this_thread::get_id() ||
         Fail(error, "Material tool document requires its construction thread");
}
bool MaterialToolDocument::Matches(MaterialToolScope scope, std::uint64_t serial,
                                   std::string *error) const {
  if (!Owner(error))
    return false;
  return (current_ && scope == scope_ && serial == serial_ &&
          serial_ != std::numeric_limits<std::uint64_t>::max()) ||
         Fail(error, "Material tool document scope or serial is stale");
}
bool MaterialToolDocument::Dirty() const noexcept {
  return current_ && current_->source != saved_canonical_;
}
std::optional<MaterialToolDocument::State> MaterialToolDocument::Parse(std::string_view source,
                                                                       std::string *error) {
  auto parsed = ImportMaterial(source);
  if (!parsed.material) {
    if (error)
      *error = parsed.error;
    return {};
  }
  auto canonical = ExportMaterial(*parsed.material);
  if (!canonical.source) {
    if (error)
      *error = canonical.error;
    return {};
  }
  return State{std::move(*canonical.source), std::move(*parsed.material)};
}
bool MaterialToolDocument::Open(MaterialToolScope scope, std::string_view source,
                                bool discard_dirty, std::string *error) {
  if (!Owner(error))
    return false;
  if (scope.project == foundation::Uuid{} || scope.asset == runtime::AssetUuid{} ||
      !scope.generation || serial_ == std::numeric_limits<std::uint64_t>::max())
    return Fail(error, "Material tool document requires a valid scope and available serial");
  if (Dirty() && !discard_dirty)
    return Fail(error, "Replacing a dirty material document requires explicit discard");
  auto next = Parse(source, error);
  if (!next)
    return false;
  std::string saved(source), canonical(next->source);
  current_ = std::move(next);
  saved_source_ = std::move(saved);
  saved_canonical_ = std::move(canonical);
  scope_ = scope;
  undo_.clear();
  redo_.clear();
  ++serial_;
  Clear(error);
  return true;
}
std::optional<MaterialToolSnapshot> MaterialToolDocument::Snapshot() const {
  if (!Owner(nullptr) || !current_)
    return {};
  return MaterialToolSnapshot{
      scope_,  serial_,        current_->source, saved_source_, current_->material,
      Dirty(), !undo_.empty(), !redo_.empty()};
}
bool MaterialToolDocument::Apply(MaterialToolScope scope, std::uint64_t serial,
                                 std::span<const std::byte> output, bool editable,
                                 std::string *error) {
  if (!Matches(scope, serial, error))
    return false;
  if (!editable)
    return Fail(error, "Material tool document is read-only");
  if (output.empty() || output.size() > kMaximumMaterialSourceBytes)
    return Fail(error, "Material tool output is empty or exceeds the source budget");
  const std::string_view source{reinterpret_cast<const char *>(output.data()), output.size()};
  auto next = Parse(source, error);
  if (!next)
    return false;
  if (next->source == current_->source) {
    Clear(error);
    return true;
  }
  auto history = undo_;
  if (history.size() == kMaximumHistory)
    history.erase(history.begin());
  history.push_back(*current_);
  current_ = std::move(next);
  undo_.swap(history);
  redo_.clear();
  ++serial_;
  Clear(error);
  return true;
}
bool MaterialToolDocument::Undo(MaterialToolScope scope, std::uint64_t serial, bool editable,
                                std::string *error) {
  if (!Matches(scope, serial, error))
    return false;
  if (!editable || undo_.empty())
    return Fail(error, "Material tool Undo is unavailable");
  State next = undo_.back();
  auto history = redo_;
  history.push_back(*current_);
  current_ = std::move(next);
  redo_.swap(history);
  undo_.pop_back();
  ++serial_;
  Clear(error);
  return true;
}
bool MaterialToolDocument::Redo(MaterialToolScope scope, std::uint64_t serial, bool editable,
                                std::string *error) {
  if (!Matches(scope, serial, error))
    return false;
  if (!editable || redo_.empty())
    return Fail(error, "Material tool Redo is unavailable");
  State next = redo_.back();
  auto history = undo_;
  history.push_back(*current_);
  current_ = std::move(next);
  undo_.swap(history);
  redo_.pop_back();
  ++serial_;
  Clear(error);
  return true;
}
bool MaterialToolDocument::AcknowledgeSave(MaterialToolScope scope, std::uint64_t serial,
                                           std::string_view previous, std::string_view published,
                                           std::string *error) {
  if (!Matches(scope, serial, error))
    return false;
  if (previous != saved_source_)
    return Fail(error, "Material save acknowledgement has a stale source baseline");
  auto parsed = Parse(published, error);
  if (!parsed || parsed->source != current_->source)
    return Fail(error, "Published material does not match the current document");
  std::string saved(published), canonical(parsed->source);
  saved_source_ = std::move(saved);
  saved_canonical_ = std::move(canonical);
  ++serial_;
  Clear(error);
  return true;
}
bool MaterialToolDocument::Close(MaterialToolScope scope, std::uint64_t serial, bool discard_dirty,
                                 std::string *error) {
  if (!Matches(scope, serial, error))
    return false;
  if (Dirty() && !discard_dirty)
    return Fail(error, "Closing a dirty material document requires explicit discard");
  current_.reset();
  scope_ = {};
  saved_source_.clear();
  saved_canonical_.clear();
  undo_.clear();
  redo_.clear();
  ++serial_;
  Clear(error);
  return true;
}
} // namespace nexora::editor
