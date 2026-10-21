#include "../../Engine/Editor/src/GitStatusProviderTestAccess.h"
#include "../EditorImGui/TemporaryDirectoryCleanup.h"
#include "Nexora/Editor/GitStatusProvider.h"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <future>
#include <iostream>
#include <stdexcept>
#include <thread>

namespace {
using namespace nexora;
using Provider = editor::GitStatusProvider;
using Error = editor::GitStatusError;
using Phase = editor::GitStatusPhase;
using Kind = editor::GitStatusKind;
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
std::string Record(std::string text) { return text + '\0'; }
void Parser() {
  Error error{Error::ProcessFailed};
  auto empty = Provider::DecodePorcelainV1({}, &error);
  Require(empty && empty->empty() && error == Error::None,
          "Clean is not an available empty result");
  std::string raw = Record(" M changed with spaces") + Record("R  new\nname") + Record("old name") +
                    Record("C  copied") + Record("copy source") + Record("UU conflict") +
                    Record("?? directory/") + Record("!! ignored") + Record(" A intent-to-add") +
                    Record(" T type-change");
  raw += Record("?? raw-" + std::string(1, static_cast<char>(0xff)));
  auto decoded = Provider::DecodePorcelainV1(raw, &error);
  Require(decoded && decoded->size() == 9 && error == Error::None,
          "Complete porcelain records lost");
  Require((*decoded)[0].index == ' ' && (*decoded)[0].working_tree == 'M' &&
              (*decoded)[1].kind == Kind::Renamed && (*decoded)[1].path == "new\nname" &&
              (*decoded)[1].original_path == "old name" && (*decoded)[2].kind == Kind::Copied &&
              (*decoded)[3].kind == Kind::Unmerged && (*decoded)[4].kind == Kind::Untracked &&
              (*decoded)[5].kind == Kind::Ignored &&
              (*decoded)[8].path.back() == static_cast<char>(0xff),
          "Status, rename order, raw bytes or directory names were reinterpreted");
  std::fill(raw.begin(), raw.end(), 'x');
  Require((*decoded)[1].path == "new\nname", "Decoded paths borrowed caller storage");
  for (const auto &input :
       {std::string(" M no terminator"), Record(" M "), Record("XX file"), Record(" M /absolute"),
        Record(" M ../outside"), Record(" M a/../b"), Record(" M a//b"), Record(" M ./file"),
        Record(" M folder/"), Record("  unchanged"), Record("?M file"), Record("UM file"),
        Record("R  missing-source"), Record("R  same") + Record("same"),
        Record(" M duplicate") + Record("A  duplicate"),
        Record(" M good") + std::string("?? partial")}) {
    Require(!Provider::DecodePorcelainV1(input, &error) && error == Error::MalformedOutput,
            "Malformed/truncated/ambiguous status was partially accepted");
  }
  Require(
      !Provider::DecodePorcelainV1(std::string(Provider::kMaximumOutputBytes + 1, 'x'), &error) &&
          error == Error::OutputBudgetExceeded,
      "Output byte limit ignored");
  Require(!Provider::DecodePorcelainV1(Record("?? " + std::string(4097, 'x')), &error) &&
              error == Error::OutputBudgetExceeded,
          "Path byte limit ignored");
  Require(Provider::DecodePorcelainV1(Record("?? " + std::string(4096, 'x'))).has_value(),
          "Maximum admitted path rejected");
  std::string many;
  for (std::size_t i = 0; i < Provider::kMaximumEntries; ++i)
    many += Record("?? path-" + std::to_string(i));
  const auto maximum = Provider::DecodePorcelainV1(many);
  Require(maximum && maximum->size() == Provider::kMaximumEntries, "Maximum entry count rejected");
  editor::GitStatusProviderTestAccess::FailDecodeAllocationAfter(1);
  Require(!Provider::DecodePorcelainV1(Record("?? first") + Record("?? second"), &error) &&
              error == Error::AllocationFailed,
          "Partial decoded records escaped allocation failure");
  Require(Provider::DecodePorcelainV1(Record("?? recovered")).has_value(),
          "Decode allocation failure was not one-shot/recoverable");
  many += Record("?? overflow");
  Require(!Provider::DecodePorcelainV1(many, &error) && error == Error::OutputBudgetExceeded,
          "Entry count limit ignored");
}
void Write(const std::filesystem::path &path, std::string_view text) {
  std::ofstream output(path, std::ios::binary);
  output << text;
  output.close();
  Require(!output.fail(), "Fixture write failed");
}
std::string Read(const std::filesystem::path &path) {
  std::ifstream input(path, std::ios::binary);
  Require(static_cast<bool>(input), "Fixture read failed");
  return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}
void Finish(Provider &provider, std::uint64_t scope) {
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
  while (!provider.Poll(scope)) {
    Require(std::chrono::steady_clock::now() < deadline, "Git query did not drain");
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
}
std::string Utf8(const std::filesystem::path &path) {
  const auto value = path.generic_u8string();
  return {value.begin(), value.end()};
}
std::string Git(core::JobSystem &jobs, const std::filesystem::path &tool,
                const std::filesystem::path &root, std::vector<std::string> arguments,
                std::uint32_t expected = 0) {
  editor::BuildProcess process(jobs);
  std::vector<std::string> command{"-c", "user.name=Nexora Tests",
                                   "-c", "user.email=tests@example.invalid",
                                   "-c", "commit.gpgsign=false"};
  command.insert(command.end(), arguments.begin(), arguments.end());
  Require(process.Start({9000, tool, root, std::move(command), 64 * 1024}),
          "Git fixture launch failed");
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
  while (!process.Poll(9000)) {
    Require(std::chrono::steady_clock::now() < deadline, "Git fixture setup timed out");
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  const auto result = process.Snapshot();
  if (result.exit_code != expected)
    std::cerr << "Fixture Git output: " << result.output << '\n';
  Require(result.exit_code == expected && !result.dropped_output_bytes,
          "Git fixture command failed");
  return result.output;
}
bool Has(const editor::GitStatusCapture &capture, std::string_view path, Kind kind) {
  return std::ranges::any_of(
      capture.entries, [&](const auto &entry) { return entry.path == path && entry.kind == kind; });
}
void AwaitMarker(const std::filesystem::path &root) {
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
  while (!std::filesystem::exists(root / ".fixture-ready")) {
    Require(std::chrono::steady_clock::now() < deadline, "Managed fixture did not actually launch");
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
}
void Integration(const std::filesystem::path &git, const std::filesystem::path &fixture) {
  const auto root = std::filesystem::temp_directory_path() /
                    ("nexora-git-status-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  editor::test::TemporaryDirectoryCleanup cleanup{root};
  const auto repo = root / std::filesystem::path(u8"專案 µ");
  std::filesystem::create_directories(repo);
  core::JobSystem jobs(1);
  jobs.Start();
  const auto templates = root / "empty-template";
  std::filesystem::create_directories(templates);
  Git(jobs, git, repo, {"init", "-q", "--template=" + Utf8(templates)});
  const auto excludes = root / "empty-excludes";
  Write(excludes, "");
  Git(jobs, git, repo, {"config", "core.excludesFile", Utf8(excludes)});
  Git(jobs, git, repo, {"symbolic-ref", "HEAD", "refs/heads/main"});
  Write(repo / "tracked.txt", "base\n");
  Write(repo / "rename source.txt", "rename content\n");
  Write(repo / "conflict.txt", "base\n");
  Write(repo / "refresh-only.txt", "unchanged\n");
  Git(jobs, git, repo, {"add", "--", "."});
  Git(jobs, git, repo, {"commit", "-qm", "base"});
  Git(jobs, git, repo, {"config", "core.fsmonitor", "must-not-run-nexora-fsmonitor"});
  // Stat refresh would ordinarily update index metadata despite identical file contents.
  const auto refreshed = repo / "refresh-only.txt";
  std::filesystem::last_write_time(refreshed, std::filesystem::last_write_time(refreshed) +
                                                  std::chrono::seconds(2));
  const auto index = Read(repo / ".git/index");
  Provider provider(jobs);
  Require(provider.Refresh(git, repo, 41) && provider.Busy(), "Real Git refresh not admitted");
  Finish(provider, 41);
  auto clean = provider.Capture(41);
  Require(clean && clean->entries.empty() && provider.Query().phase == Phase::Ready &&
              clean->operation == provider.Query().operation && !provider.Capture(42) &&
              Read(repo / ".git/index") == index && Read(refreshed) == "unchanged\n",
          "Clean query mutated index/source or confused unavailable/scope identity");
  Write(repo / "tracked.txt", "external change\n");
  Write(repo / std::filesystem::path(u8"新增 空白.txt"), "new asset\n");
  Git(jobs, git, repo, {"mv", "--", "rename source.txt", "rename destination.txt"});
  const auto changed_index = Read(repo / ".git/index");
  Require(provider.Refresh(git, repo, 41), "Changed query not admitted");
  Error error{};
  Require(!provider.Refresh(git, repo, 41, &error) && error == Error::Busy,
          "Busy refresh replaced its in-flight operation");
  Finish(provider, 41);
  auto changed = provider.Capture(41);
  Require(changed && Has(*changed, "tracked.txt", Kind::Changed) &&
              Has(*changed, "新增 空白.txt", Kind::Untracked) &&
              Has(*changed, "rename destination.txt", Kind::Renamed) && clean->entries.empty() &&
              changed->operation > clean->operation && Read(repo / ".git/index") == changed_index &&
              Read(repo / "tracked.txt") == "external change\n",
          "Real changes/Unicode/rename, immutable retention or no-index-write contract failed");
  const auto renamed = std::ranges::find_if(
      changed->entries, [](const auto &entry) { return entry.kind == Kind::Renamed; });
  Require(renamed->original_path == "rename source.txt", "Actual -z rename source order lost");
  using Access = editor::GitStatusProviderTestAccess;
  using Point = editor::GitStatusAllocationPoint;
  const auto before_allocation = provider.Query();
  Access::FailNext(provider, Point::Refresh);
  Require(!provider.Refresh(git, repo, 41, &error) && error == Error::AllocationFailed &&
              !provider.Busy() && provider.Query().operation == before_allocation.operation &&
              provider.Capture(41) == changed,
          "Intake allocation failure admitted work or changed last-good");
  for (const auto point : {Point::Outcome, Point::Capture}) {
    Access::FailNext(provider, point);
    Require(provider.Refresh(git, repo, 41), "Allocation-failure query not admitted");
    Finish(provider, 41);
    Require(!provider.Busy() && provider.Query().phase == Phase::Failed &&
                provider.Query().error == Error::AllocationFailed &&
                provider.Capture(41) == changed,
            "Outcome/capture allocation failure retained work or published partial state");
  }
  Access::FailDecodeAllocationAfter(1);
  Require(provider.Refresh(git, repo, 41), "Decode-allocation query not admitted");
  Finish(provider, 41);
  Require(provider.Query().error == Error::AllocationFailed && provider.Capture(41) == changed,
          "Partial actual-Git decode replaced last-good after allocation failure");
  Require(provider.Refresh(git, repo, 41), "Post-allocation recovery did not accept work");
  Finish(provider, 41);
  auto recovered = provider.Capture(41);
  Require(recovered && recovered->entries == changed->entries && recovered != changed &&
              provider.Query().phase == Phase::Ready,
          "Allocation failure was not recoverable with a new complete observation");
  changed = std::move(recovered);
  Require(!provider.Refresh(root / "missing-git", repo, 41, &error) &&
              error == Error::ToolUnavailable && provider.Capture(41) == changed,
          "Missing tool erased last-good or was represented as a clean tree");
  Require(!provider.Refresh(git, root, 41, &error) && error == Error::RepositoryUnavailable &&
              provider.Capture(41) == changed,
          "Non-repository intake changed last-good");
  const auto head = Read(repo / ".git/HEAD");
  Write(repo / ".git/HEAD", "not a valid HEAD\n");
  Require(provider.Refresh(git, repo, 41), "Failure query not admitted");
  Finish(provider, 41);
  Require(provider.Query().phase == Phase::Failed &&
              provider.Query().error == Error::ProcessFailed && provider.Capture(41) == changed,
          "Actual Git failure published clean/partial state");
  Write(repo / ".git/HEAD", head);
  Git(jobs, git, repo, {"add", "--", "."});
  Git(jobs, git, repo, {"commit", "-qm", "changes"});
  Git(jobs, git, repo, {"checkout", "-qb", "side"});
  Write(repo / "conflict.txt", "side\n");
  Git(jobs, git, repo, {"commit", "-qam", "side"});
  Git(jobs, git, repo, {"checkout", "-q", "main"});
  Write(repo / "conflict.txt", "main\n");
  Git(jobs, git, repo, {"commit", "-qam", "main"});
  Git(jobs, git, repo, {"merge", "--no-edit", "side"}, 1);
  Require(provider.Refresh(git, repo, 41), "Conflict query not admitted");
  Finish(provider, 41);
  const auto conflicted = provider.Capture(41);
  Require(conflicted && Has(*conflicted, "conflict.txt", Kind::Unmerged),
          "Real unmerged state lost");
  const auto suffix = fixture.extension();
  const auto copied = [&](const char *name) {
    auto destination = root / name;
    destination += suffix;
    std::filesystem::copy_file(fixture, destination);
    return destination;
  };
  const auto malformed = copied("malformed");
  const auto overflow = copied("overflow");
  const auto sleeper = copied("sleep");
  for (const auto &[tool, expected] : {std::pair{malformed, Error::MalformedOutput},
                                       std::pair{overflow, Error::OutputBudgetExceeded}}) {
    Require(provider.Refresh(tool, repo, 41), "Fault-injection process did not launch");
    Finish(provider, 41);
    Require(provider.Query().phase == Phase::Failed && provider.Query().error == expected &&
                provider.Capture(41) == conflicted,
            "Malformed/overflow success partially replaced last-good");
  }
  std::promise<void> release;
  auto blocked = release.get_future().share();
  auto entered = std::make_shared<std::promise<void>>();
  auto started = entered->get_future();
  auto blocker = jobs.Submit({[blocked, entered](const core::CancellationToken &) {
                                entered->set_value();
                                blocked.wait();
                              },
                              core::JobPriority::Normal,
                              {},
                              "Blocked Git intake"});
  Require(started.wait_for(std::chrono::seconds(30)) == std::future_status::ready,
          "Blocking worker did not start");
  Require(provider.Refresh(git, repo, 41) && provider.Cancel(), "Queued cancellation not accepted");
  release.set_value();
  jobs.Wait(blocker);
  Finish(provider, 41);
  Require(provider.Query().phase == Phase::Cancelled && provider.Capture(41) == conflicted,
          "Queued cancellation erased last-good");
  Require(provider.Refresh(sleeper, repo, 41), "Running cancellation fixture did not launch");
  AwaitMarker(repo);
  Require(provider.Cancel(), "Actually running cancellation not accepted");
  Finish(provider, 41);
  Require(provider.Query().phase == Phase::Cancelled && provider.Capture(41) == conflicted,
          "Running cancellation did not drain/preserve last-good");
  std::filesystem::remove(repo / ".fixture-ready");
  Require(provider.Refresh(sleeper, repo, 41), "Stale fixture did not launch");
  AwaitMarker(repo);
  static_cast<void>(provider.Poll(42));
  const auto stale_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
  while (provider.Busy()) {
    Require(std::chrono::steady_clock::now() < stale_deadline, "Stale process did not drain");
    if (provider.Poll(41))
      break;
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  Require(provider.Query().phase == Phase::Stale && !provider.Capture(41) && !provider.Capture(42),
          "Scope switch-back resurrected a stale result");
  std::filesystem::remove(repo / ".fixture-ready");
  const auto linked = root / "linked worktree";
  Git(jobs, git, repo, {"merge", "--abort"});
  Git(jobs, git, repo, {"worktree", "add", "-q", "--detach", Utf8(linked), "HEAD"});
  Require(!provider.Refresh(git, linked, 41, &error) && error == Error::InvalidRequest,
          "One scope was reused for another repository root");
  Require(provider.Refresh(git, linked, 43), "Linked gitfile working tree not admitted");
  Finish(provider, 43);
  auto retained = provider.Capture(43);
  Require(retained && retained->entries.empty() && provider.Query().phase == Phase::Ready,
          "Actual gitfile working tree failed");
  Require(provider.Refresh(sleeper, linked, 43), "Teardown fixture did not launch");
  AwaitMarker(linked);
  provider.Shutdown();
  Require(!provider.Busy() && provider.Query().phase == Phase::Stopped && !provider.Capture(43) &&
              retained->entries.empty() && !provider.Refresh(git, linked, 44, &error) &&
              error == Error::Stopped,
          "Teardown retained a live worker/borrow or accepted new work");
  jobs.Stop();
  std::filesystem::remove_all(repo);
  Require(changed->entries.size() >= 3 && Has(*changed, "新增 空白.txt", Kind::Untracked) &&
              !retained->repository_root.empty(),
          "Owning observations borrowed filesystem/provider storage");
}
} // namespace
int main(int argc, char **argv) {
  try {
    if (argc == 2 && std::string_view(argv[1]) == "--parser") {
      Parser();
      return 0;
    }
    if (argc != 3 || std::string_view(argv[1]) == "--unavailable") {
      std::cout << "Actual Git executable unavailable; native provider acceptance not run\n";
      return 77;
    }
    Integration(std::filesystem::absolute(argv[1]), std::filesystem::absolute(argv[2]));
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
