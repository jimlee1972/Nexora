#include "Nexora/Editor/GitDiffProvider.h"
#include "GitDiffProviderTestAccess.h"
#include "Nexora/Foundation/Types.h"
#include <limits>
#include <new>

namespace nexora::editor {
namespace {
bool LiteralFile(std::string_view path) {
  if (path.empty() || path.size() > GitDiffProvider::kMaximumPathBytes || path.front() == '/' ||
      path.back() == '/' || path.find('\0') != path.npos || !foundation::IsValidUtf8(path))
    return false;
  for (std::size_t start = 0; start < path.size();) {
    const auto slash = path.find('/', start);
    const auto part = path.substr(start, slash == path.npos ? slash : slash - start);
    if (part.empty() || part == "." || part == "..")
      return false;
    if (slash == path.npos)
      break;
    start = slash + 1;
  }
  return true;
}
} // namespace
struct GitDiffProvider::Implementation final {
  BuildProcess process;
  GitDiffQuery query;
  std::filesystem::path root;
  GitDiffRequest request;
  std::shared_ptr<const GitDiffCapture> capture;
  bool invalidated{};
  std::uint64_t next{1};
  GitDiffAllocationPoint failure{GitDiffAllocationPoint::None};
  explicit Implementation(core::JobSystem &jobs) : process(jobs) {}
  void Allocate(GitDiffAllocationPoint point) {
    if (failure == point) {
      failure = GitDiffAllocationPoint::None;
      throw std::bad_alloc{};
    }
  }
};
GitDiffProvider::GitDiffProvider(core::JobSystem &jobs)
    : implementation_(std::make_unique<Implementation>(jobs)) {}
GitDiffProvider::~GitDiffProvider() { Shutdown(); }
bool GitDiffProvider::Compare(std::filesystem::path git, std::filesystem::path repository_root,
                              std::uint64_t scope, GitDiffRequest request, GitDiffError *error) {
  auto &impl = *implementation_;
  const auto reject = [&](GitDiffError value) {
    if (error)
      *error = value;
    return false;
  };
  if (impl.query.phase == GitDiffPhase::Stopped)
    return reject(GitDiffError::Stopped);
  if (!scope ||
      (request.target != GitDiffTarget::WorkingTree && request.target != GitDiffTarget::Index) ||
      request.paths.empty() || request.paths.size() > kMaximumPaths)
    return reject(GitDiffError::InvalidRequest);
  for (std::size_t i = 0; i < request.paths.size(); ++i)
    if (!LiteralFile(request.paths[i]) || (i && request.paths[i] == request.paths[0]))
      return reject(GitDiffError::InvalidRequest);
  if (impl.process.Busy())
    return reject(GitDiffError::Busy);
  if (impl.next == std::numeric_limits<std::uint64_t>::max())
    return reject(GitDiffError::OperationExhausted);
  try {
    impl.Allocate(GitDiffAllocationPoint::Intake);
    std::error_code ec;
    if (!git.is_absolute() || !std::filesystem::is_regular_file(git, ec) || ec)
      return reject(GitDiffError::ToolUnavailable);
    if (!repository_root.is_absolute())
      return reject(GitDiffError::RepositoryUnavailable);
    auto root = std::filesystem::canonical(repository_root, ec);
    if (ec || !std::filesystem::is_directory(root, ec) || ec)
      return reject(GitDiffError::RepositoryUnavailable);
    const auto metadata = std::filesystem::status(root / ".git", ec);
    if (ec ||
        (!std::filesystem::is_directory(metadata) && !std::filesystem::is_regular_file(metadata)))
      return reject(GitDiffError::RepositoryUnavailable);
    if (impl.query.scope == scope && !impl.root.empty() && impl.root != root)
      return reject(GitDiffError::InvalidRequest);
    BuildProcessRequest process{scope,
                                std::move(git),
                                root,
                                {"--git-dir=.git", "--work-tree=.", "--no-pager",
                                 "--literal-pathspecs", "--no-optional-locks", "-c",
                                 "core.fsmonitor=false", "-c", "core.untrackedCache=false", "diff",
                                 "--no-ext-diff", "--no-textconv", "--no-color", "--no-renames",
                                 "--ignore-submodules=all", "--unified=3"},
                                kMaximumOutputBytes};
    if (request.target == GitDiffTarget::Index)
      process.arguments.emplace_back("--cached");
    process.arguments.emplace_back("--");
    process.arguments.insert(process.arguments.end(), request.paths.begin(), request.paths.end());
    if (!impl.process.Start(std::move(process)))
      return reject(GitDiffError::ProcessRejected);
    // Every potentially allocating owner input is staged before accepted process intake.
    impl.root = std::move(root);
    impl.request = std::move(request);
    impl.query = {scope, impl.next++, GitDiffPhase::Comparing};
    impl.invalidated = false;
    if (impl.capture && impl.capture->scope != scope)
      impl.capture.reset();
    if (error)
      *error = GitDiffError::None;
    return true;
  } catch (const std::bad_alloc &) {
    return reject(GitDiffError::AllocationFailed);
  }
}
bool GitDiffProvider::Poll(std::uint64_t current_scope) {
  auto &impl = *implementation_;
  if (impl.query.phase == GitDiffPhase::Stopped)
    return false;
  if (impl.query.scope && current_scope != impl.query.scope) {
    impl.invalidated = true;
    impl.capture.reset();
    static_cast<void>(impl.process.Cancel());
    if (!impl.process.Busy()) {
      impl.query.phase = GitDiffPhase::Stale;
      impl.query.error = GitDiffError::Stale;
    }
  }
  if (!impl.process.Poll(current_scope))
    return false;
  try {
    impl.Allocate(GitDiffAllocationPoint::Outcome);
    auto result = impl.process.Snapshot();
    impl.query.exit_code = result.exit_code;
    impl.query.dropped_output_bytes = result.dropped_output_bytes;
    if (impl.invalidated || result.phase == BuildProcessPhase::Stale) {
      impl.query.phase = GitDiffPhase::Stale;
      impl.query.error = GitDiffError::Stale;
      impl.capture.reset();
    } else if (result.phase == BuildProcessPhase::Cancelled) {
      impl.query.phase = GitDiffPhase::Cancelled;
      impl.query.error = GitDiffError::Cancelled;
    } else if (result.phase != BuildProcessPhase::Exited) {
      impl.query.phase = GitDiffPhase::Failed;
      impl.query.error = GitDiffError::ProcessFailed;
    } else if (result.dropped_output_bytes) {
      impl.query.phase = GitDiffPhase::Failed;
      impl.query.error = GitDiffError::OutputBudgetExceeded;
    } else {
      impl.Allocate(GitDiffAllocationPoint::Capture);
      auto capture = std::make_shared<GitDiffCapture>(
          GitDiffCapture{impl.query.scope, impl.query.operation, impl.root, impl.request,
                         std::move(result.output)});
      impl.capture = std::move(capture);
      impl.query.phase = GitDiffPhase::Ready;
      impl.query.error = GitDiffError::None;
    }
  } catch (const std::bad_alloc &) {
    impl.query.phase = GitDiffPhase::Failed;
    impl.query.error = GitDiffError::AllocationFailed;
  }
  return true;
}
void GitDiffProviderTestAccess::FailNext(GitDiffProvider &provider, GitDiffAllocationPoint point) {
  provider.implementation_->failure = point;
}
bool GitDiffProvider::Cancel() noexcept { return implementation_->process.Cancel(); }
bool GitDiffProvider::Busy() const { return implementation_->process.Busy(); }
GitDiffQuery GitDiffProvider::Query() const noexcept { return implementation_->query; }
std::shared_ptr<const GitDiffCapture> GitDiffProvider::Capture(std::uint64_t scope) const {
  const auto &capture = implementation_->capture;
  return capture && capture->scope == scope ? capture : nullptr;
}
void GitDiffProvider::Shutdown() noexcept {
  auto &impl = *implementation_;
  impl.process.Shutdown();
  impl.capture.reset();
  impl.root.clear();
  impl.request = {};
  impl.query = {};
  impl.query.phase = GitDiffPhase::Stopped;
}
} // namespace nexora::editor
