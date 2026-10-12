#include "../EditorImGui/TemporaryDirectoryCleanup.h"
#include "Nexora/Editor/ChromeTrace.h"
#include "Nexora/Editor/EditorWorkspace.h"
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
using namespace nexora::editor;
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
std::string Event(std::uint64_t timestamp, std::string_view name = "Frame") {
  return "{\"name\":\"" + std::string(name) +
         "\",\"ph\":\"X\",\"pid\":42,\"tid\":7,\"ts\":" + std::to_string(timestamp) +
         ",\"dur\":1250.5,\"args\":{\"unknown\":[true,null,{\"escaped\":\"\\n\\u0041\"}]}}";
}
void Run() {
  const ChromeTraceSelection selection{42, 7, "Frame"};
  const auto event = Event(100);
  const auto trace = "{\"displayTimeUnit\":\"ns\",\"traceEvents\":[" + event + "," +
                     Event(200, "Other") +
                     ",{\"name\":\"thread_name\",\"ph\":\"M\",\"pid\":42,\"tid\":7,"
                     "\"args\":{\"name\":\"main\"}}," +
                     Event(300) + "]}";
  std::string error = "stale";
  auto imported = ChromeTraceImporter::Import(trace, selection, &error);
  Require(
      imported && ChromeTraceImporter::Validate(*imported) && error.empty() &&
          imported->samples.size() == 2 && imported->selection.process == 42 &&
          imported->selection.thread == 7 && imported->selection.event_name == "Frame" &&
          imported->samples[0].sequence == 1 && imported->samples[0].start_microseconds == 100 &&
          imported->samples[0].duration_milliseconds == 1.2505 &&
          imported->samples[1].start_microseconds == 300 && imported->older_samples_dropped == 0,
      "Real wrapped Chrome trace lost explicit external clock/unit/scope");
  Require(ChromeTraceImporter::Import("[" + event + "]", selection).has_value(),
          "Bare Chrome event array rejected");
  const auto reject = [&](const std::string &bytes) {
    error = "stale";
    const auto copy = bytes;
    Require(!ChromeTraceImporter::Import(bytes, selection, &error) && !error.empty() &&
                error != "stale" && bytes == copy && imported->samples.size() == 2 &&
                imported->samples[0].duration_milliseconds == 1.2505,
            "Invalid import mutated input/prior capture or failed diagnostic contract");
  };
  for (std::size_t length = 0; length < trace.size(); ++length)
    reject(trace.substr(0, length));
  const auto mutate = [&](std::string_view from, std::string_view to) {
    auto bytes = trace;
    const auto index = bytes.find(from);
    Require(index != std::string::npos, "Trace mutation fixture missing");
    bytes.replace(index, from.size(), to);
    reject(bytes);
  };
  mutate("\"dur\":1250.5", "\"dur\":-1");
  mutate("\"dur\":1250.5", "\"dur\":1e999");
  mutate("\"dur\":1250.5", "\"dur\":null");
  mutate("\"dur\":1250.5", "\"dur\":1250.5,\"dur\":2");
  mutate("\"dur\":1250.5", "\"du\\u0072\":1250.5,\"dur\":2");
  mutate("\"pid\":42", "\"pid\":9007199254740992");
  mutate("\"pid\":42", "\"pid\":4.2");
  mutate("\"pid\":42", "\"pid\":042");
  mutate("\"pid\":42", "\"pid\":\"+42\"");
  mutate("\"ts\":100", "\"ts\":9007199254740991");
  mutate("\"ts\":300", "\"ts\":99");
  mutate("\"ph\":\"X\"", "\"missing_phase\":\"X\"");
  mutate("\"dur\":1250.5,", "");
  mutate("\"traceEvents\":", "\"traceEvents\":[],\"traceEvents\":");
  mutate("\\u0041", "\\u+d41");
  mutate("\\u0041", "\\ud800");
  mutate("\\u0041", "\\udc00");
  mutate("\\u0041", "\\ud800\\u0041");
  reject(trace + "{}");
  reject(trace + std::string(1, '\0'));
  reject("{}");
  reject("[]");
  reject("[" + Event(1, "Unknown") + "]");
  reject("[" + event + ",]");
  reject("[" + event + "]" + std::string(ChromeTraceImporter::kMaximumBytes, ' '));
  auto exact = "[" + event + "]";
  exact.resize(ChromeTraceImporter::kMaximumBytes, ' ');
  Require(ChromeTraceImporter::Import(exact, selection).has_value(), "Exact byte budget rejected");
  auto numeric_strings = "[" + event + "]";
  for (const auto &field : {std::string("\"pid\":42"), std::string("\"tid\":7")}) {
    const auto colon = field.find(':');
    numeric_strings.replace(numeric_strings.find(field), field.size(),
                            field.substr(0, colon + 1) + "\"" + field.substr(colon + 1) + "\"");
  }
  Require(ChromeTraceImporter::Import(numeric_strings, selection).has_value(),
          "Exact Chrome string process/thread IDs rejected");
  const ChromeTraceSelection unicode{42, 7, "時間😀"};
  Require(
      ChromeTraceImporter::Import("[" + Event(1, "時間😀") + "]", unicode).has_value() &&
          ChromeTraceImporter::Import("[" + Event(1, "\\u6642\\u9593\\ud83d\\ude00") + "]", unicode)
              .has_value(),
      "Raw UTF-8 or JSON surrogate-pair event identity rejected");
  reject("[" + Event(1, std::string(1, static_cast<char>(0xff))) + "]");
  for (const auto &bad : {ChromeTraceSelection{42, 7, ""}, ChromeTraceSelection{42, 7, "bad\n"},
                          ChromeTraceSelection{42, 7, std::string(257, 'x')},
                          ChromeTraceSelection{9007199254740992ULL, 7, "Frame"}})
    Require(!ChromeTraceImporter::Import(trace, bad), "Invalid selector accepted");
  std::string many = "[";
  for (std::uint64_t i = 1; i <= 605; ++i) {
    if (i > 1)
      many += ',';
    many += Event(i);
  }
  many += ']';
  auto ring = ChromeTraceImporter::Import(many, selection);
  Require(ring && ChromeTraceImporter::Validate(*ring) && ring->samples.size() == 600 &&
              ring->older_samples_dropped == 5 && ring->samples.front().sequence == 6 &&
              ring->samples.back().sequence == 605 &&
              ring->samples.front().start_microseconds == 6 &&
              ring->samples.back().start_microseconds == 605,
          "Bounded newest-sample retention/order/dropped count failed");
  std::string too_many = "[" + event;
  for (std::size_t i = 1; i <= ChromeTraceImporter::kMaximumEvents; ++i)
    too_many += ",{\"ph\":\"M\"}";
  reject(too_many + "]");
  reject("{\"traceEvents\":[" + event + "],\"metadata\":" + std::string(33, '[') + "0" +
         std::string(33, ']') + "}");
  reject("{\"traceEvents\":[" + event + "],\"metadata\":\"" +
         std::string(ChromeTraceImporter::kMaximumStringBytes + 1, 'a') + "\"}");
  std::string keys = "{";
  for (std::size_t i = 0; i < 64; ++i) {
    if (i)
      keys += ',';
    keys += "\"key" + std::to_string(i) + "\":null";
  }
  reject(keys + ",\"traceEvents\":[" + event + "]}");
  Require(ChromeTraceImporter::Import("[" + Event(100) + "," + Event(100) + "]", selection)
                  ->samples.size() == 2,
          "Simultaneous ordered Chrome intervals rejected");
}

std::string Scope(std::string_view phase, std::uint64_t time, std::string_view name = {},
                  std::uint64_t thread = 7) {
  return "{\"ph\":\"" + std::string(phase) + "\",\"pid\":42,\"tid\":" + std::to_string(thread) +
         ",\"ts\":" + std::to_string(time) +
         (name.empty() ? "" : ",\"name\":\"" + std::string(name) + "\"") + "}";
}
void DurationPairs() {
  const ChromeTraceSelection selection{42, 7, "Frame"};
  const auto begin = Scope("B", 100, "Frame");
  const auto end = Scope("E", 1350);
  const auto trace = "[" + begin + "," + Scope("B", 200, "Other") + "," + Scope("E", 300) + "," +
                     Event(400) + "," + end + "]";
  auto capture = ChromeTraceImporter::Import(trace, selection);
  Require(capture && ChromeTraceImporter::Validate(*capture) && capture->samples.size() == 2 &&
              capture->samples[0].start_microseconds == 100 &&
              capture->samples[0].duration_milliseconds == 1.25 &&
              capture->samples[1].start_microseconds == 400 &&
              capture->samples[1].duration_milliseconds == 1.2505,
          "Mixed X/B/E with nested nonmatching scope lost begin order or external duration");
  auto nested = ChromeTraceImporter::Import("[" + begin + "," + Scope("B", 200, "Frame") + "," +
                                                Scope("E", 300, "Frame") + "," + end + "]",
                                            selection);
  Require(nested && ChromeTraceImporter::Validate(*nested) && nested->samples.size() == 2 &&
              nested->samples[0].sequence == 1 && nested->samples[1].sequence == 2 &&
              nested->samples[0].duration_milliseconds == 1.25 &&
              nested->samples[1].duration_milliseconds == .1,
          "Nested same-name scopes lost LIFO pairing or were ordered by completion");
  auto zero = ChromeTraceImporter::Import("[" + begin + "," + Scope("E", 100) + "]", selection);
  Require(zero && zero->samples[0].duration_milliseconds == 0,
          "Valid zero-duration synchronous scope rejected");
  Require(ChromeTraceImporter::Import("[" + Scope("E", 5, {}, 8) + "," + begin + "," + end + "]",
                                      selection)
              .has_value(),
          "Unrelated thread end interfered with the selected synchronous stack");
  const auto reject = [&](const std::string &bytes) {
    std::string error = "stale";
    const auto source = bytes;
    Require(!ChromeTraceImporter::Import(bytes, selection, &error) && !error.empty() &&
                error != "stale" && source == bytes &&
                capture->samples[0].duration_milliseconds == 1.25,
            "Malformed duration scope changed source/prior capture or lost diagnostic");
  };
  reject("[" + begin + "]");
  reject("[" + Event(50) + "," + begin + "]");
  reject("[" + end + "," + begin + "]");
  reject("[" + begin + "," + Scope("E", 99) + "]");
  reject("[" + begin + "," + Scope("E", 200, "Other") + "]");
  reject("[" + begin + "," + Scope("E", 200, {}, 8) + "]");
  reject("[" + begin + "," + Scope("E", 200) + "," + end + "]");
  reject("[" + begin + "," + Scope("B", 99, "Other") + "," + end + "]");
  reject("[" + begin + "," + Scope("B", 200, "Other") + "," + Scope("E", 300) + "," +
         Scope("E", 250) + "]");
  reject("[" + Scope("B", 100) + "," + end + "]");
  reject("[{\"ph\":\"B\",\"name\":\"Frame\",\"pid\":42,\"tid\":7}," + end + "]");
  reject("[" + begin + ",{\"ph\":\"E\",\"pid\":42,\"ts\":1350}]");
  reject("[" + begin + ",{\"ph\":\"E\",\"pid\":42,\"tid\":7,\"ts\":1350,\"dur\":0}]");
  // The outer selected begin is evicted while still open. Its eventual end must not write
  // a duration into the newer X interval that occupies the same ring slot.
  std::string eviction = "[" + Scope("B", 0, "Frame");
  for (std::uint64_t i = 1; i <= 605; ++i)
    eviction += ',' + Event(i);
  auto ring = ChromeTraceImporter::Import(eviction + ',' + Scope("E", 10000) + "]", selection);
  Require(ring && ChromeTraceImporter::Validate(*ring) && ring->samples.size() == 600 &&
              ring->older_samples_dropped == 6 && ring->samples.front().sequence == 7 &&
              ring->samples.front().start_microseconds == 6 && ring->samples.back().sequence == 606,
          "Pending-begin eviction lost newest-by-start sequence or dropped count");
  for (const auto &sample : ring->samples)
    Require(sample.duration_milliseconds == 1.2505, "Evicted end corrupted a newer ring occupant");
  std::string completed = "[";
  for (std::uint64_t i = 0; i < 605; ++i)
    completed += (i ? "," : "") + Scope("B", i * 10, "Frame") + ',' + Scope("E", i * 10 + 5);
  auto completed_ring = ChromeTraceImporter::Import(completed + "]", selection);
  Require(completed_ring && ChromeTraceImporter::Validate(*completed_ring) &&
              completed_ring->older_samples_dropped == 5 &&
              completed_ring->samples.front().sequence == 6 &&
              completed_ring->samples.front().start_microseconds == 50 &&
              completed_ring->samples.back().duration_milliseconds == .005,
          "Completed synchronous scopes lost ring wrap, duration or loss counts");
  auto fractional =
      ChromeTraceImporter::Import(R"([{"ph":"B","name":"Frame","pid":42,"tid":7,"ts":100.25},)"
                                  R"({"ph":"E","pid":42,"tid":7,"ts":1350.75}])",
                                  selection);
  Require(fractional && fractional->samples[0].start_microseconds == 100.25 &&
              fractional->samples[0].duration_milliseconds == 1.2505,
          "Fractional synchronous microseconds were rounded or treated as milliseconds");
  const ChromeTraceSelection unicode{42, 7, "時間😀"};
  Require(ChromeTraceImporter::Import("[" + Scope("B", 100, "時間😀") + ',' +
                                          Scope("E", 200, "\\u6642\\u9593\\ud83d\\ude00") + "]",
                                      unicode)
              .has_value(),
          "Synchronous begin/end Unicode names did not share decoded identity");
  std::string depth = "[";
  for (std::size_t i = 0; i < ChromeTraceImporter::kMaximumOpenScopes; ++i)
    depth += (i ? "," : "") + Scope("B", i, "Frame");
  auto exact = depth;
  for (std::size_t i = 0; i < ChromeTraceImporter::kMaximumOpenScopes; ++i)
    exact += ',' + Scope("E", 1000 + i);
  auto maximum = ChromeTraceImporter::Import(exact + "]", selection);
  Require(maximum && ChromeTraceImporter::Validate(*maximum) &&
              maximum->samples.size() == ChromeTraceImporter::kMaximumOpenScopes,
          "Exact open-scope capacity rejected");
  reject(depth + ',' + Scope("B", 500, "Other") + "]");
}

void ProjectIo() {
  const auto root = std::filesystem::temp_directory_path() /
                    ("nexora-chrome-io-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  std::filesystem::create_directory(root);
  nexora::editor::test::TemporaryDirectoryCleanup cleanup{root};
  ProjectWorkspace workspace, observer;
  std::string error;
  Require(workspace.Create(root / "Project", "Chrome trace", &error), "Project fixture failed");
  const auto path = workspace.Root() / ".nexora/chrome-trace.json";
  const auto bytes = "[" + Event(100) + "]";
  const auto write = [&](const std::string &value) {
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    stream.write(value.data(), static_cast<std::streamsize>(value.size()));
  };
  const auto read = [&] {
    std::ifstream stream(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(stream), {});
  };
  const ChromeTraceSelection selection{42, 7, "Frame"};
  Require(!workspace.ImportChromeTraceJson(selection, &error) && !error.empty(),
          "Missing project trace fabricated a capture");
  write(bytes);
  auto capture = workspace.ImportChromeTraceJson(selection, &error);
  Require(capture && ChromeTraceImporter::Validate(*capture) && error.empty() && read() == bytes &&
              observer.Open(workspace.Root(), ProjectAccess::ReadOnly, &error) &&
              observer.ImportChromeTraceJson(selection, &error) && read() == bytes,
          "Writer/read-only import changed trace or failed external provenance");
  const auto paired_bytes = "[" + Scope("B", 100, "Frame") + "," + Scope("E", 1350) + "]";
  write(paired_bytes);
  auto paired_capture = observer.ImportChromeTraceJson(selection, &error);
  Require(paired_capture && ChromeTraceImporter::Validate(*paired_capture) && error.empty() &&
              paired_capture->samples.front().duration_milliseconds == 1.25 &&
              read() == paired_bytes,
          "Read-only project duration import altered bytes or lost external duration");
  write("[" + Scope("B", 100, "Frame") + "]");
  Require(!observer.ImportChromeTraceJson(selection, &error) && !error.empty() &&
              paired_capture->samples.front().duration_milliseconds == 1.25,
          "Unclosed project duration erased prior owning capture");
  write("corrupt");
  Require(!workspace.ImportChromeTraceJson(selection, &error) && read() == "corrupt" &&
              capture->samples.front().duration_milliseconds == 1.2505,
          "Malformed import changed file or prior capture");
  write(std::string(ChromeTraceImporter::kMaximumBytes + 1, ' '));
  Require(!workspace.ImportChromeTraceJson(selection, &error) &&
              std::filesystem::file_size(path) == ChromeTraceImporter::kMaximumBytes + 1,
          "Oversized project read exceeded bounds or changed file");
  write(bytes);
  const auto recovery = workspace.Root() / ".nexora/workspace.recovery";
  std::filesystem::create_directory(recovery);
  Require(!workspace.ImportChromeTraceJson(selection, &error) &&
              !observer.ImportChromeTraceJson(selection, &error) && read() == bytes,
          "Pending recovery admitted external import");
  std::filesystem::remove(recovery);
  const auto metadata = workspace.Root() / ".nexora/workspace";
  const auto stamp = std::filesystem::last_write_time(metadata);
  std::filesystem::last_write_time(metadata, stamp + std::chrono::seconds(5));
  Require(!workspace.ImportChromeTraceJson(selection, &error) && read() == bytes,
          "External workspace change admitted import");
  std::filesystem::last_write_time(metadata, stamp);
  const auto backup = root / "backup.json";
  std::filesystem::rename(path, backup);
  std::filesystem::create_directory(path);
  Require(!workspace.ImportChromeTraceJson(selection, &error) &&
              std::filesystem::is_directory(path),
          "Nonregular import path accepted or altered");
  std::filesystem::remove(path);
  std::error_code ec;
  std::filesystem::create_symlink(backup, path, ec);
  if (!ec) {
    Require(!workspace.ImportChromeTraceJson(selection, &error) && read() == bytes,
            "Symlink import accepted or altered target");
    std::filesystem::remove(path);
  }
  ec.clear();
  std::filesystem::create_hard_link(backup, path, ec);
  if (!ec) {
    Require(!workspace.ImportChromeTraceJson(selection, &error) && read() == bytes,
            "Hard-linked import accepted or altered target");
    std::filesystem::remove(path);
  }
  std::filesystem::rename(backup, path);
  observer = ProjectWorkspace{};
  workspace = ProjectWorkspace{};
  Require(!workspace.ImportChromeTraceJson(selection, &error) && !error.empty(),
          "Closed project admitted external import");
}
} // namespace
int main() {
  try {
    Run();
    DurationPairs();
    ProjectIo();
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
