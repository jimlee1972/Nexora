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
    ProjectIo();
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
