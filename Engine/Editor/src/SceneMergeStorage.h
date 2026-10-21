#pragma once
#include "Nexora/Editor/SceneMergePublication.h"

namespace nexora::editor::merge_storage {
using Path = std::filesystem::path;
[[nodiscard]] bool Missing(const Path &);
[[nodiscard]] bool PlainDirectory(const Path &);
[[nodiscard]] bool PlainFile(const Path &);
[[nodiscard]] bool SafeRelativeScene(const Path &);
[[nodiscard]] bool SafeParents(const Path &root, const Path &relative);
[[nodiscard]] std::optional<std::string> Read(const Path &, std::size_t limit);
[[nodiscard]] bool WriteNew(const Path &, std::string_view, const core::CancellationToken &);
[[nodiscard]] bool SyncDirectory(const Path &) noexcept;
[[nodiscard]] bool Replace(const Path &, const Path &) noexcept;
[[nodiscard]] std::optional<std::string> Digest(std::string_view);
[[nodiscard]] std::optional<std::string> Encode(const SceneMergeArchive &, std::string *);
[[nodiscard]] std::optional<SceneMergeArchive> Decode(std::string_view, std::string *);
[[nodiscard]] std::optional<SceneMergeArchive> ReadArchive(const Path &, foundation::Uuid,
                                                           const Path &relative, std::string *);
} // namespace nexora::editor::merge_storage
