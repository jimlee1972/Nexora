#include "Nexora/Editor/GitStatusProvider.h"
#include "GitStatusProviderTestAccess.h"

#include <algorithm>
#include <limits>
#include <new>
#include <unordered_set>

namespace nexora::editor {
namespace {
thread_local std::size_t fail_decode_after = std::numeric_limits<std::size_t>::max();
bool Path(std::string_view path, bool directory) {
  if (path.empty() || path.front() == '/' || (!directory && path.back() == '/'))
    return false;
  for (std::size_t start = 0; start < path.size();) {
    const auto slash = path.find('/', start);
    const auto component = path.substr(start, slash == path.npos ? slash : slash - start);
    if (component.empty() || component == "." || component == "..")
      return false;
    if (slash == path.npos)
      break;
    start = slash + 1;
  }
  return true;
}
std::optional<GitStatusKind> Kind(std::string_view xy) {
  if (xy == "??")
    return GitStatusKind::Untracked;
  if (xy == "!!")
    return GitStatusKind::Ignored;
  constexpr std::string_view allowed = " MADRCUT";
  if (xy == "  " || allowed.find(xy[0]) == allowed.npos || allowed.find(xy[1]) == allowed.npos)
    return {};
  constexpr std::string_view conflicts[]{"DD", "AU", "UD", "UA", "DU", "AA", "UU"};
  if (std::ranges::find(conflicts, xy) != std::end(conflicts))
    return GitStatusKind::Unmerged;
  if (xy.find('U') != xy.npos)
    return {};
  if (xy.find('R') != xy.npos)
    return GitStatusKind::Renamed;
  if (xy.find('C') != xy.npos)
    return GitStatusKind::Copied;
  return GitStatusKind::Changed;
}
} // namespace
std::optional<std::vector<GitStatusEntry>>
GitStatusProvider::DecodePorcelainV1(std::string_view bytes, GitStatusError *error) {
  const auto reject = [&](GitStatusError value) -> std::optional<std::vector<GitStatusEntry>> {
    if (error)
      *error = value;
    return {};
  };
  if (bytes.size() > kMaximumOutputBytes)
    return reject(GitStatusError::OutputBudgetExceeded);
  try {
    std::vector<GitStatusEntry> entries;
    std::unordered_set<std::string_view> destinations;
    std::size_t cursor{};
    while (cursor < bytes.size()) {
      if (bytes.size() - cursor < 4 || bytes[cursor + 2] != ' ')
        return reject(GitStatusError::MalformedOutput);
      const auto xy = bytes.substr(cursor, 2);
      const auto kind = Kind(xy);
      if (!kind)
        return reject(GitStatusError::MalformedOutput);
      cursor += 3;
      const auto path_end = bytes.find('\0', cursor);
      if (path_end == bytes.npos)
        return reject(GitStatusError::MalformedOutput);
      const auto path = bytes.substr(cursor, path_end - cursor);
      if (path.size() > kMaximumPathBytes || entries.size() == kMaximumEntries)
        return reject(GitStatusError::OutputBudgetExceeded);
      if (!Path(path, *kind == GitStatusKind::Untracked || *kind == GitStatusKind::Ignored))
        return reject(GitStatusError::MalformedOutput);
      if (entries.size() == fail_decode_after) {
        fail_decode_after = std::numeric_limits<std::size_t>::max();
        throw std::bad_alloc{};
      }
      if (!destinations.insert(path).second)
        return reject(GitStatusError::MalformedOutput);
      cursor = path_end + 1;
      std::string_view original;
      if (*kind == GitStatusKind::Renamed || *kind == GitStatusKind::Copied) {
        const auto original_end = bytes.find('\0', cursor);
        if (original_end == bytes.npos)
          return reject(GitStatusError::MalformedOutput);
        original = bytes.substr(cursor, original_end - cursor);
        if (original.size() > kMaximumPathBytes)
          return reject(GitStatusError::OutputBudgetExceeded);
        if (!Path(original, false) || original == path)
          return reject(GitStatusError::MalformedOutput);
        cursor = original_end + 1;
      }
      entries.push_back({*kind, xy[0], xy[1], std::string(path), std::string(original)});
    }
    if (error)
      *error = GitStatusError::None;
    return entries;
  } catch (const std::bad_alloc &) {
    return reject(GitStatusError::AllocationFailed);
  }
}
struct GitStatusProvider::Implementation final {
  BuildProcess process;
  GitStatusQuery query;
  std::filesystem::path root;
  std::shared_ptr<const GitStatusCapture> capture;
  bool invalidated{};
  std::uint64_t next{1};
  GitStatusAllocationPoint failure{GitStatusAllocationPoint::None};
  void Allocate(GitStatusAllocationPoint point) {
    if (failure == point) {
      failure = GitStatusAllocationPoint::None;
      throw std::bad_alloc{};
    }
  }
  explicit Implementation(core::JobSystem &jobs) : process(jobs) {}
};
GitStatusProvider::GitStatusProvider(core::JobSystem &jobs)
    : implementation_(std::make_unique<Implementation>(jobs)) {}
GitStatusProvider::~GitStatusProvider() { Shutdown(); }
bool GitStatusProvider::Refresh(std::filesystem::path git, std::filesystem::path repository_root,
                                std::uint64_t scope, GitStatusError *error) {
  auto &impl = *implementation_;
  const auto reject = [&](GitStatusError value) {
    if (error)
      *error = value;
    return false;
  };
  if (impl.query.phase == GitStatusPhase::Stopped)
    return reject(GitStatusError::Stopped);
  if (!scope)
    return reject(GitStatusError::InvalidRequest);
  if (impl.process.Busy())
    return reject(GitStatusError::Busy);
  if (impl.next == std::numeric_limits<std::uint64_t>::max())
    return reject(GitStatusError::OperationExhausted);
  try {
    impl.Allocate(GitStatusAllocationPoint::Refresh);
    std::error_code ec;
    if (!git.is_absolute() || !std::filesystem::is_regular_file(git, ec) || ec)
      return reject(GitStatusError::ToolUnavailable);
    if (!repository_root.is_absolute())
      return reject(GitStatusError::RepositoryUnavailable);
    auto root = std::filesystem::canonical(repository_root, ec);
    if (ec || !std::filesystem::is_directory(root, ec) || ec)
      return reject(GitStatusError::RepositoryUnavailable);
    const auto metadata = std::filesystem::status(root / ".git", ec);
    if (ec ||
        (!std::filesystem::is_directory(metadata) && !std::filesystem::is_regular_file(metadata)))
      return reject(GitStatusError::RepositoryUnavailable);
    if (impl.query.scope == scope && !impl.root.empty() && impl.root != root)
      return reject(GitStatusError::InvalidRequest);
    BuildProcessRequest request{scope,
                                std::move(git),
                                root,
                                {"--git-dir=.git", "--work-tree=.", "--no-optional-locks", "-c",
                                 "core.fsmonitor=false", "-c", "core.untrackedCache=false", "-c",
                                 "status.renames=true", "status", "--porcelain=v1", "-z",
                                 "--untracked-files=normal", "--ignore-submodules=all"},
                                kMaximumOutputBytes};
    if (!impl.process.Start(std::move(request)))
      return reject(GitStatusError::ProcessRejected);
    // No allocating work follows accepted process intake: owner identity publication is atomic
    // even if a worker finishes immediately with a large output buffer.
    impl.query = {scope, impl.next++, GitStatusPhase::Refreshing};
    impl.root = std::move(root);
    impl.invalidated = false;
    if (impl.capture && impl.capture->scope != scope)
      impl.capture.reset();
    if (error)
      *error = GitStatusError::None;
    return true;
  } catch (const std::bad_alloc &) {
    return reject(GitStatusError::AllocationFailed);
  }
}
bool GitStatusProvider::Poll(std::uint64_t current_scope) {
  auto &impl = *implementation_;
  if (impl.query.phase == GitStatusPhase::Stopped)
    return false;
  if (impl.query.scope && current_scope != impl.query.scope) {
    impl.invalidated = true;
    impl.capture.reset();
    static_cast<void>(impl.process.Cancel());
    if (!impl.process.Busy()) {
      impl.query.phase = GitStatusPhase::Stale;
      impl.query.error = GitStatusError::Stale;
    }
  }
  if (!impl.process.Poll(current_scope))
    return false;
  try {
    impl.Allocate(GitStatusAllocationPoint::Outcome);
    const auto outcome = impl.process.Snapshot();
    impl.query.exit_code = outcome.exit_code;
    impl.query.dropped_output_bytes = outcome.dropped_output_bytes;
    if (impl.invalidated || outcome.phase == BuildProcessPhase::Stale) {
      impl.query.phase = GitStatusPhase::Stale;
      impl.query.error = GitStatusError::Stale;
      impl.capture.reset();
    } else if (outcome.phase == BuildProcessPhase::Cancelled) {
      impl.query.phase = GitStatusPhase::Cancelled;
      impl.query.error = GitStatusError::Cancelled;
    } else if (outcome.phase != BuildProcessPhase::Exited) {
      impl.query.phase = GitStatusPhase::Failed;
      impl.query.error = GitStatusError::ProcessFailed;
    } else {
      impl.query.phase = GitStatusPhase::Failed;
      if (outcome.dropped_output_bytes) {
        impl.query.error = GitStatusError::OutputBudgetExceeded;
        return true;
      }
      auto entries = DecodePorcelainV1(outcome.output, &impl.query.error);
      if (!entries)
        return true;
      try {
        impl.Allocate(GitStatusAllocationPoint::Capture);
        auto candidate = std::make_shared<GitStatusCapture>(GitStatusCapture{
            impl.query.scope, impl.query.operation, impl.root, std::move(*entries)});
        impl.capture = std::move(candidate);
        impl.query.phase = GitStatusPhase::Ready;
        impl.query.error = GitStatusError::None;
      } catch (const std::bad_alloc &) {
        impl.query.error = GitStatusError::AllocationFailed;
      }
    }
    return true;
  } catch (const std::bad_alloc &) {
    impl.query.phase = GitStatusPhase::Failed;
    impl.query.error = GitStatusError::AllocationFailed;
    return true;
  }
}
void GitStatusProviderTestAccess::FailNext(GitStatusProvider &provider,
                                           GitStatusAllocationPoint point) {
  provider.implementation_->failure = point;
}
void GitStatusProviderTestAccess::FailDecodeAllocationAfter(std::size_t records) {
  fail_decode_after = records;
}
bool GitStatusProvider::Cancel() noexcept { return implementation_->process.Cancel(); }
bool GitStatusProvider::Busy() const { return implementation_->process.Busy(); }
GitStatusQuery GitStatusProvider::Query() const noexcept { return implementation_->query; }
std::shared_ptr<const GitStatusCapture>
GitStatusProvider::Capture(std::uint64_t current_scope) const {
  const auto &capture = implementation_->capture;
  return capture && capture->scope == current_scope ? capture : nullptr;
}
void GitStatusProvider::Shutdown() noexcept {
  auto &impl = *implementation_;
  impl.process.Shutdown();
  impl.capture.reset();
  impl.root.clear();
  impl.query = {};
  impl.query.phase = GitStatusPhase::Stopped;
}
} // namespace nexora::editor
