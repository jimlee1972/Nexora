#pragma once
#include "Nexora/Editor/MaterialToolDocument.h"
#include "Nexora/Editor/ProjectContent.h"

namespace nexora::editor {
// Serialized construction-thread owner. Workspace/content outlive this session. Owning
// snapshots remain inspectable after scope loss; all authoring and IO recheck live bindings.
class NEXORA_EDITOR_API MaterialToolSourceSession final {
public:
  MaterialToolSourceSession(const ProjectWorkspace &, ProjectContentSession &);
  MaterialToolSourceSession(const MaterialToolSourceSession &) = delete;
  MaterialToolSourceSession &operator=(const MaterialToolSourceSession &) = delete;
  bool Open(runtime::AssetUuid, bool discard_dirty = false, std::string *error = nullptr);
  [[nodiscard]] std::optional<MaterialToolSnapshot> Snapshot() const;
  bool Apply(MaterialToolScope, std::uint64_t serial, std::span<const std::byte>,
             bool authoring_allowed, std::string *error = nullptr);
  bool Undo(MaterialToolScope, std::uint64_t serial, bool authoring_allowed,
            std::string *error = nullptr);
  bool Redo(MaterialToolScope, std::uint64_t serial, bool authoring_allowed,
            std::string *error = nullptr);
  // False after a confirmed write explicitly reports publication without acknowledgement;
  // callers retain the dirty owner and inspect/reopen rather than retrying an old baseline.
  bool Save(MaterialToolScope, std::uint64_t serial, bool authoring_allowed,
            std::string *error = nullptr);
  bool Close(MaterialToolScope, std::uint64_t serial, bool discard_dirty = false,
             std::string *error = nullptr);

private:
  static std::uint64_t AllocateInstance() noexcept;
  bool Owner(std::string *) const;
  bool Workspace(std::string *) const;
  bool Matches(MaterialToolScope, std::uint64_t, bool, std::string *) const;
  std::optional<std::string> CurrentSource(MaterialToolScope, std::string *) const;
  const std::thread::id owner_{std::this_thread::get_id()};
  const std::uint64_t instance_{AllocateInstance()};
  const ProjectWorkspace &workspace_;
  ProjectContentSession &content_;
  MaterialToolDocument document_;
  std::filesystem::path root_, path_;
  std::uint64_t content_generation_{};
};
} // namespace nexora::editor
