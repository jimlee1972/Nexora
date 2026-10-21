#include "../../Engine/Editor/src/GitDiffProviderTestAccess.h"
#include "../EditorImGui/TemporaryDirectoryCleanup.h"
#include "GitTestSupport.h"
#include <fstream>
#include <future>

namespace {
using namespace nexora;
using Provider = editor::GitDiffProvider;
using Error = editor::GitDiffError;
using Phase = editor::GitDiffPhase;
using Target = editor::GitDiffTarget;
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
void Write(const std::filesystem::path &path, std::string_view bytes) {
  std::ofstream output(path, std::ios::binary);
  output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
  output.close();
  Require(!output.fail(), "Diff fixture write failed");
}
std::string Read(const std::filesystem::path &path) {
  std::ifstream input(path, std::ios::binary);
  Require(static_cast<bool>(input), "Diff fixture read failed");
  return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}
std::string Utf8(const std::filesystem::path &path) {
  const auto value = path.generic_u8string();
  return {value.begin(), value.end()};
}
void Finish(Provider &provider, std::uint64_t scope) {
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
  while (!provider.Poll(scope)) {
    Require(std::chrono::steady_clock::now() < deadline, "Diff operation did not drain");
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
}
void AwaitMarker(const std::filesystem::path &root) {
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
  while (!std::filesystem::exists(root / ".fixture-ready")) {
    Require(std::chrono::steady_clock::now() < deadline, "Diff child did not actually start");
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
}
void Integration(const std::filesystem::path &git, const std::filesystem::path &fixture) {
  const auto root = std::filesystem::temp_directory_path() /
                    ("nexora-git-diff-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  editor::test::TemporaryDirectoryCleanup cleanup{root};
  const auto repo = root / std::filesystem::path(u8"專案 µ");
  std::filesystem::create_directories(repo);
  core::JobSystem jobs(1);
  jobs.Start();
  const auto run = [&](std::vector<std::string> args, std::uint32_t expected = 0) {
    return editor::test::RunGit(jobs, git, repo, std::move(args), expected);
  };
  const auto templates = root / "empty-template";
  std::filesystem::create_directories(templates);
  run({"init", "-q", "--template=" + Utf8(templates)});
  run({"symbolic-ref", "HEAD", "refs/heads/main"});
  run({"config", "core.autocrlf", "false"});
  Write(repo / "tracked.txt", "base\n");
  Write(repo / "rename source.txt", "rename\n");
  Write(repo / "conflict.txt", "base\n");
  Write(repo / "binary.dat", std::string("base\0bytes", 10));
  Write(repo / "[literal].txt", "literal base\n");
  Write(repo / "l.txt", "glob decoy base\n");
  Write(repo / "--option.txt", "option base\n");
  const auto unicode = std::filesystem::path(u8"新增 空白.txt");
  Write(repo / unicode, "unicode base\n");
#if !defined(_WIN32)
  Write(repo / ":(glob)*", "magic base\n");
  Write(repo / "line\nname.txt", "newline base\n");
#endif
  run({"add", "--", "."});
  run({"commit", "-qm", "base"});
  Provider provider(jobs);
  const editor::GitDiffRequest working{Target::WorkingTree, {"tracked.txt"}};
  const editor::GitDiffRequest staged{Target::Index, {"tracked.txt"}};
  const auto index_before = Read(repo / ".git/index");
  const auto query = [&](editor::GitDiffRequest request) {
    Require(provider.Compare(git, repo, 41, std::move(request)), "Real diff not admitted");
    Finish(provider, 41);
    const auto capture = provider.Capture(41);
    Require(capture && provider.Query().phase == Phase::Ready &&
                capture->operation == provider.Query().operation && !provider.Capture(42),
            "Diff success/scope/operation identity incorrect");
    return capture;
  };
  const auto clean = query(working);
  Require(clean->output.empty() && Read(repo / ".git/index") == index_before,
          "Clean diff unavailable or index changed");
  Write(repo / "tracked.txt", "staged value\n");
  run({"add", "--", "tracked.txt"});
  Write(repo / "tracked.txt", "external working value\n");
  const auto index_staged = Read(repo / ".git/index");
  const auto index = query(staged);
  Require(index->request == staged && index->output.find("-base") != std::string::npos &&
              index->output.find("+staged value") != std::string::npos &&
              index->output.find("external working value") == std::string::npos,
          "Staged diff included working-tree values");
  const auto changed = query(working);
  Require(changed->request == working &&
              changed->output.find("-staged value") != std::string::npos &&
              changed->output.find("+external working value") != std::string::npos &&
              index->output.find("+staged value") != std::string::npos && clean->output.empty() &&
              Read(repo / ".git/index") == index_staged &&
              Read(repo / "tracked.txt") == "external working value\n",
          "Working diff, owning snapshots or no-write contract failed");
  Error error{};
  const auto operation = provider.Query().operation;
  for (const auto &paths :
       std::vector<std::vector<std::string>>{{},
                                             {""},
                                             {"a", "b", "c"},
                                             {"tracked.txt", "tracked.txt"},
                                             {"/outside"},
                                             {"../outside"},
                                             {"a/../outside"},
                                             {"a//b"},
                                             {"a/./b"},
                                             {"dir/"},
                                             {std::string("bad\0path", 8)},
                                             {std::string(1, static_cast<char>(0xff))},
                                             {std::string(Provider::kMaximumPathBytes + 1, 'x')}}) {
    Require(!provider.Compare(git, repo, 41, {Target::WorkingTree, paths}, &error) &&
                error == Error::InvalidRequest && provider.Query().operation == operation &&
                provider.Capture(41) == changed,
            "Invalid path changed intake/last-good");
  }
  Require(
      !provider.Compare(git, repo, 0, working, &error) && error == Error::InvalidRequest &&
          !provider.Compare(git, repo, 41, {static_cast<Target>(255), {"tracked.txt"}}, &error) &&
          error == Error::InvalidRequest,
      "Invalid scope/target admitted");
  Write(repo / "[literal].txt", "literal changed\n");
  Write(repo / "l.txt", "glob decoy changed\n");
  const auto literal = query({Target::WorkingTree, {"[literal].txt"}});
  Require(literal->output.find("+literal changed") != std::string::npos &&
              literal->output.find("glob decoy") == std::string::npos,
          "Literal bracket path expanded to a glob");
  Write(repo / "--option.txt", "option changed\n");
  Require(query({Target::WorkingTree, {"--option.txt"}})->output.find("+option changed") !=
              std::string::npos,
          "Dash path interpreted as an option");
  Write(repo / unicode, "unicode changed\n");
  Require(query({Target::WorkingTree, {Utf8(unicode)}})->output.find("+unicode changed") !=
              std::string::npos,
          "Unicode path lost");
#if !defined(_WIN32)
  Write(repo / ":(glob)*", "magic changed\n");
  const auto magic = query({Target::WorkingTree, {":(glob)*"}});
  Require(magic->output.find("+magic changed") != std::string::npos &&
              magic->output.find("external working value") == std::string::npos,
          "Pathspec magic interpreted");
  Write(repo / "line\nname.txt", "newline changed\n");
  Require(query({Target::WorkingTree, {"line\nname.txt"}})->output.find("+newline changed") !=
              std::string::npos,
          "Control-bearing path was split");
#endif
  Write(repo / "binary.dat", std::string("new\0bytes", 9));
  Require(query({Target::WorkingTree, {"binary.dat"}})->output.find("Binary files") !=
              std::string::npos,
          "Binary difference fabricated text/empty success");
  run({"mv", "--", "rename source.txt", "rename destination.txt"});
  const auto rename = query({Target::Index, {"rename source.txt", "rename destination.txt"}});
  Require(rename->output.find("-rename") != std::string::npos &&
              rename->output.find("+rename") != std::string::npos,
          "Rename pair lost a side under deterministic no-renames output");
  const auto copy_fixture = [&](const char *name) {
    auto path = root / name;
    path += fixture.extension();
    std::filesystem::copy_file(fixture, path);
    return path;
  };
  const auto sleeper = copy_fixture("sleep");
  const auto overflow = copy_fixture("overflow");
  const auto maximum = copy_fixture("maximum");
  const auto raw = copy_fixture("raw");
  {
    Provider raw_provider(jobs);
    Require(raw_provider.Compare(maximum, repo, 71, working), "Exact output limit not admitted");
    Finish(raw_provider, 71);
    const auto full = raw_provider.Capture(71);
    Require(full && full->output == std::string(Provider::kMaximumOutputBytes, 'x') &&
                !raw_provider.Query().dropped_output_bytes,
            "Complete maximum-sized output was truncated/rejected");
    Require(raw_provider.Compare(raw, repo, 71, staged), "Raw-output fixture not admitted");
    Finish(raw_provider, 71);
    const auto bytes = raw_provider.Capture(71);
    raw_provider.Shutdown();
    Require(bytes && bytes->request == staged &&
                bytes->output == std::string("warning\nraw\0bytes", 17) &&
                full->output.size() == Provider::kMaximumOutputBytes,
            "Raw diagnostics/NUL bytes or retained capture ownership changed");
  }
  run({"config", "core.fsmonitor", "must-not-run-nexora-fsmonitor"});
  run({"config", "diff.external", Utf8(sleeper)});
  run({"config", "diff.blocked.textconv", Utf8(sleeper)});
  Write(repo / ".gitattributes", "tracked.txt diff=blocked\n");
  const auto config = Read(repo / ".git/config");
  const auto safe_index = Read(repo / ".git/index");
  const auto safe = query(working);
  Require(!std::filesystem::exists(repo / ".fixture-ready") && safe->output == changed->output &&
              Read(repo / ".git/config") == config && Read(repo / ".git/index") == safe_index,
          "Diff invoked configured helpers or wrote source/index/config");
  using Access = editor::GitDiffProviderTestAccess;
  using Point = editor::GitDiffAllocationPoint;
  Access::FailNext(provider, Point::Intake);
  Require(!provider.Compare(git, repo, 41, staged, &error) && error == Error::AllocationFailed &&
              provider.Capture(41) == safe && !provider.Busy(),
          "Intake allocation changed last-good");
  for (const auto point : {Point::Outcome, Point::Capture}) {
    Access::FailNext(provider, point);
    Require(provider.Compare(git, repo, 41, staged), "Allocation query not admitted");
    Finish(provider, 41);
    Require(provider.Query().error == Error::AllocationFailed && !provider.Busy() &&
                provider.Capture(41) == safe && provider.Capture(41)->request == working,
            "Allocation failure published a partial/wrongly labeled capture");
  }
  Require(!provider.Compare(root / "missing", repo, 41, working, &error) &&
              error == Error::ToolUnavailable && provider.Capture(41) == safe &&
              !provider.Compare(git, root, 41, working, &error) &&
              error == Error::RepositoryUnavailable,
          "Unavailable tool/repository replaced last-good");
  const auto head = Read(repo / ".git/HEAD");
  Write(repo / ".git/HEAD", "invalid HEAD\n");
  Require(provider.Compare(git, repo, 41, staged), "Broken Git query not admitted");
  Finish(provider, 41);
  Require(provider.Query().phase == Phase::Failed &&
              provider.Query().error == Error::ProcessFailed && provider.Capture(41) == safe,
          "Nonzero Git published success");
  Write(repo / ".git/HEAD", head);
  Require(provider.Compare(overflow, repo, 41, staged), "Overflow fixture not admitted");
  Finish(provider, 41);
  Require(provider.Query().error == Error::OutputBudgetExceeded &&
              provider.Query().dropped_output_bytes && provider.Capture(41) == safe,
          "Truncated patch tail published as complete");
  std::promise<void> release;
  const auto blocked = release.get_future().share();
  auto entered = std::make_shared<std::promise<void>>();
  auto started = entered->get_future();
  const auto blocker = jobs.Submit({[blocked, entered](const core::CancellationToken &) {
                                      entered->set_value();
                                      blocked.wait();
                                    },
                                    core::JobPriority::Normal,
                                    {},
                                    "Blocked diff intake"});
  Require(started.wait_for(std::chrono::seconds(30)) == std::future_status::ready,
          "Blocking worker did not start");
  const bool accepted = provider.Compare(git, repo, 41, staged);
  const bool cancelled = provider.Cancel();
  release.set_value();
  jobs.Wait(blocker);
  Require(accepted && cancelled, "Queued diff cancellation not accepted");
  Finish(provider, 41);
  Require(provider.Query().phase == Phase::Cancelled && provider.Capture(41) == safe,
          "Queued cancellation erased last-good");
  Require(provider.Compare(sleeper, repo, 41, staged), "Running fixture not admitted");
  AwaitMarker(repo);
  Require(!provider.Compare(git, repo, 41, working, &error) && error == Error::Busy &&
              provider.Cancel(),
          "Busy query replaced work or running cancellation failed");
  Finish(provider, 41);
  Require(provider.Query().phase == Phase::Cancelled && provider.Capture(41) == safe,
          "Running cancellation did not drain/preserve last-good");
  std::filesystem::remove(repo / ".fixture-ready");
  Require(provider.Compare(sleeper, repo, 41, staged), "Stale fixture not admitted");
  AwaitMarker(repo);
  if (!provider.Poll(42))
    Finish(provider, 41);
  Require(provider.Query().phase == Phase::Stale && !provider.Capture(41) && !provider.Capture(42),
          "Switch-back resurrected stale diff");
  std::filesystem::remove(repo / ".fixture-ready");
  // Fixture mutations below are setup, never provider commands.
  run({"config", "--unset", "diff.external"});
  run({"config", "--unset", "diff.blocked.textconv"});
  run({"add", "--", "."});
  run({"commit", "-qm", "changes"});
  run({"checkout", "-qb", "side"});
  Write(repo / "conflict.txt", "side\n");
  run({"commit", "-qam", "side"});
  run({"checkout", "-q", "main"});
  Write(repo / "conflict.txt", "main\n");
  run({"commit", "-qam", "main"});
  run({"merge", "--no-edit", "side"}, 1);
  const auto conflict = query({Target::WorkingTree, {"conflict.txt"}});
  Require(!conflict->output.empty() && conflict->output.find("conflict.txt") != std::string::npos,
          "Real unmerged difference lost");
  run({"merge", "--abort"});
  const auto linked = root / "linked worktree";
  run({"worktree", "add", "-q", "--detach", Utf8(linked), "HEAD"});
  Write(linked / "tracked.txt", "linked value\n");
  Require(!provider.Compare(git, linked, 41, working, &error) && error == Error::InvalidRequest &&
              provider.Compare(git, linked, 43, working),
          "Root replacement reused scope or gitfile rejected");
  Finish(provider, 43);
  const auto retained = provider.Capture(43);
  Require(retained && retained->output.find("+linked value") != std::string::npos,
          "Linked worktree diff failed");
  Require(provider.Compare(sleeper, linked, 43, working), "Shutdown fixture not admitted");
  AwaitMarker(linked);
  provider.Shutdown();
  Require(!provider.Busy() && provider.Query().phase == Phase::Stopped && !provider.Capture(43) &&
              !provider.Compare(git, linked, 44, working, &error) && error == Error::Stopped &&
              retained->output.find("+linked value") != std::string::npos,
          "Shutdown retained work or invalidated owning capture");
  jobs.Stop();
}
} // namespace
int main(int argc, char **argv) {
  try {
    if (argc != 3 || std::string_view(argv[1]) == "--unavailable") {
      std::cout << "Actual Git unavailable; native diff acceptance not run\n";
      return 77;
    }
    Integration(std::filesystem::absolute(argv[1]), std::filesystem::absolute(argv[2]));
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
