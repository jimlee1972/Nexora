#include "../EditorImGui/TemporaryDirectoryCleanup.h"
#include "Nexora/Editor/ChromeTrace.h"
#include "Nexora/Editor/EditorWorkspace.h"
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <limits>
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

std::string Async(std::string_view phase, std::uint64_t time, std::string_view id = "1",
                  std::uint64_t thread = 7, std::string_view name = "Frame",
                  std::string_view category = "work", std::string_view scope = "session") {
  return "{\"ph\":\"" + std::string(phase) + "\",\"pid\":42,\"tid\":" + std::to_string(thread) +
         ",\"name\":\"" + std::string(name) + "\",\"ts\":" + std::to_string(time) +
         ",\"id2\":{\"local\":" + std::string(id) + "},\"cat\":\"" + std::string(category) +
         "\",\"scope\":\"" + std::string(scope) + "\"}";
}
void AsyncPairs() {
  const ChromeTraceSelection selection{42, 7, "Frame"};
  const auto begin = Async("b", 100);
  const auto end = Async("e", 1350, "1", 8);
  const auto bytes = "[" + begin + ',' + Async("b", 200, "2") + ',' + Async("e", 300, "1", 9) +
                     ',' + Event(400) + ',' + Async("e", 500, "2", 8) + "]";
  auto capture = ChromeTraceImporter::Import(bytes, selection);
  Require(capture && ChromeTraceImporter::Validate(*capture) && capture->samples.size() == 3 &&
              capture->samples[0].duration_milliseconds == .2 &&
              capture->samples[1].duration_milliseconds == .3 &&
              capture->samples[2].duration_milliseconds == 1.2505,
          "Cross-thread async IDs lost independent completion or mixed begin order");
  const auto reject = [&](const std::string &source) {
    std::string error;
    const auto copy = source;
    Require(!ChromeTraceImporter::Import(source, selection, &error) && !error.empty() &&
                source == copy && capture->samples.front().duration_milliseconds == .2,
            "Invalid async import changed source/prior capture or omitted diagnostic");
  };
  auto nested =
      ChromeTraceImporter::Import("[" + begin + ',' + Async("b", 200, "1", 8, "Other") + ',' +
                                      Async("e", 300, "1", 9, "Other") + ',' + end + "]",
                                  selection);
  Require(nested && nested->samples.size() == 1 && nested->samples[0].duration_milliseconds == 1.25,
          "Nonselected begin on another thread broke identity LIFO or begin-thread selection");
  auto same_name = ChromeTraceImporter::Import("[" + begin + ',' + Async("b", 200) + ',' +
                                                   Async("e", 300, "1", 8) + ',' + end + "]",
                                               selection);
  Require(same_name && same_name->samples.size() == 2 &&
              same_name->samples[0].duration_milliseconds == 1.25 &&
              same_name->samples[1].duration_milliseconds == .1,
          "Same-ID nested async intervals lost inclusive duration or begin order");
  // Numeric and string IDs, category and scope are independent identity lanes.
  for (int lane = 0; lane < 3; ++lane) {
    const auto id = lane == 0 ? "\"1\"" : "1";
    const auto cat = lane == 1 ? "other" : "work";
    const auto scope = lane == 2 ? "other" : "session";
    auto separated = ChromeTraceImporter::Import(
        "[" + begin + ',' + Async("b", 200, id, 7, "Frame", cat, scope) + ',' + end + ',' +
            Async("e", 1400, id, 8, "Frame", cat, scope) + "]",
        selection);
    Require(separated && separated->samples.size() == 2 &&
                separated->samples[0].duration_milliseconds == 1.25 &&
                separated->samples[1].duration_milliseconds == 1.2,
            "Async identity lanes collided");
  }
  Require(ChromeTraceImporter::Import("[" + Async("b", 100, "\"時間\\ud83d\\ude00\"") + ',' +
                                          Async("e", 100, "\"時間😀\"", 8) + "]",
                                      selection)
                  ->samples[0]
                  .duration_milliseconds == 0,
          "Unicode ID or zero async duration rejected");
  Require(ChromeTraceImporter::Import(
              R"([{"ph":"b","name":"Frame","pid":42,"tid":7,"ts":100.25,"id2":{"local":1}},)"
              R"({"ph":"e","name":"Frame","pid":42,"tid":8,"ts":1350.75,"id2":{"local":1}}])",
              selection)
                  ->samples[0]
                  .duration_milliseconds == 1.2505,
          "Fractional async microseconds or omitted category/scope rejected");
  reject("[" + begin + "]");
  reject("[" + end + ',' + begin + "]");
  reject("[" + begin + ',' + Async("e", 99) + "]");
  reject("[" + begin + ',' + Async("e", 200, "2") + "]");
  reject("[" + begin + ',' + Async("e", 200, "1", 8, "Other") + "]");
  reject("[" + begin + ',' + Async("b", 99) + ',' + Async("e", 200) + ',' + end + "]");
  reject("[" + begin + ',' + Async("b", 200) + ',' + Async("e", 400) + ',' + Async("e", 300) + "]");
  for (const auto id : {"null", "true", "-1", "1.5", "9007199254740992", "\"\""})
    reject("[" + Async("b", 100, id) + ',' + Async("e", 200, id) + "]");
  reject("[" + Async("b", 100, "\"" + std::string(257, 'x') + "\"") + ',' + end + "]");
  const auto mutate = [&](std::string_view from, std::string_view to) {
    auto value = "[" + begin + ',' + end + "]";
    const auto index = value.find(from);
    Require(index != std::string::npos, "Async mutation fixture missing");
    value.replace(index, from.size(), to);
    reject(value);
  };
  mutate("\"id2\":{\"local\":1},", "");
  mutate("\"id2\":{\"local\":1},", "\"id\":1,");
  mutate("\"local\":1", "\"global\":1");
  mutate("\"local\":1", "\"local\":1,\"global\":1");
  mutate("\"id2\":{\"local\":1}", "\"id2\":{\"local\":1},\"id\":1");
  mutate("\"cat\":\"work\"", "\"cat\":42");
  mutate("\"scope\":\"session\"", "\"scope\":null");
  mutate("\"ts\":100", "\"ts\":100,\"dur\":1");
  mutate("\"tid\":7,", "");
  mutate("\"name\":\"Frame\",", "");
  mutate("\"pid\":42", "\"pid\":43");
  // Recognized metadata on existing X events stays opaque, even if it is not an async identity.
  Require(ChromeTraceImporter::Import("[" + Event(1).substr(0, Event(1).size() - 1) +
                                          ",\"id\":null,\"cat\":{},\"scope\":[]}]",
                                      selection)
              .has_value(),
          "Async decoding changed existing X metadata compatibility");
  std::string pending = "[" + Async("b", 0);
  for (std::uint64_t i = 1; i <= 605; ++i)
    pending += ',' + Event(i);
  auto ring =
      ChromeTraceImporter::Import(pending + ',' + Async("e", 10000, "1", 8) + "]", selection);
  Require(ring && ChromeTraceImporter::Validate(*ring) && ring->older_samples_dropped == 6 &&
              ring->samples.front().sequence == 7 && ring->samples.back().sequence == 606,
          "Async pending-begin eviction lost sequence/drop count");
  for (const auto &sample : ring->samples)
    Require(sample.duration_milliseconds == 1.2505, "Evicted async end overwrote newer interval");
  std::string depth = "[";
  for (std::size_t i = 0; i < ChromeTraceImporter::kMaximumOpenScopes; ++i)
    depth += (i ? "," : "") + Async("b", i, std::to_string(i));
  auto exact = depth;
  for (std::size_t i = 0; i < ChromeTraceImporter::kMaximumOpenScopes; ++i)
    exact += ',' + Async("e", 1000 + i, std::to_string(i), 8);
  Require(ChromeTraceImporter::Import(exact + "]", selection)->samples.size() == 128,
          "Exact async open-scope capacity rejected");
  reject(depth + ',' + Scope("B", 500, "Other") + "]");
  reject(depth + ',' + Async("b", 500, "999") + "]");
}

std::string Global(std::string_view phase, std::uint64_t time, std::uint64_t process = 42,
                   bool legacy = false, std::string_view id = "1", std::string_view name = "Frame",
                   std::string_view category = "work", std::string_view scope = "session") {
  auto event = Async(phase, time, id, 7, name, category, scope);
  event.replace(event.find("\"pid\":42"), 8, "\"pid\":" + std::to_string(process));
  const auto identity = "\"id2\":{\"local\":" + std::string(id) + "}";
  event.replace(event.find(identity), identity.size(),
                legacy ? "\"id\":" + std::string(id)
                       : "\"id2\":{\"global\":" + std::string(id) + "}");
  return event;
}
void GlobalPairs() {
  const ChromeTraceSelection selection{42, 7, "Frame"};
  const auto begin = Global("b", 100);
  const auto end = Global("e", 1350, 43);
  const auto bytes = "[" + begin + ',' + Global("b", 200, 44, true, "1", "Other") + ',' +
                     Global("e", 300, 45, false, "1", "Other") + ',' + end + "]";
  auto capture = ChromeTraceImporter::Import(bytes, selection);
  Require(capture && ChromeTraceImporter::Validate(*capture) && capture->samples.size() == 1 &&
              capture->samples[0].duration_milliseconds == 1.25,
          "Cross-process global nesting lost inclusive latency or begin-process selection");
  for (const bool legacy : {false, true}) {
    auto alias = ChromeTraceImporter::Import(
        "[" + Global("b", 100, 42, legacy) + ',' + Global("e", 1350, 43, !legacy) + "]", selection);
    Require(alias && alias->samples[0].duration_milliseconds == 1.25,
            "Legacy id and explicit global id2 failed to share the global namespace");
  }
  auto namespace_split = ChromeTraceImporter::Import(
      "[" + Async("b", 100) + ',' + Global("b", 200) + ',' + Async("e", 300) + ',' + end + "]",
      selection);
  Require(namespace_split && namespace_split->samples.size() == 2 &&
              namespace_split->samples[0].duration_milliseconds == .2 &&
              namespace_split->samples[1].duration_milliseconds == 1.15,
          "Local/global IDs collided or local end closed a global begin");
  auto same =
      "[" + begin + ',' + Global("b", 200, 43) + ',' + Global("e", 300, 44) + ',' + end + "]";
  auto selected_other = ChromeTraceImporter::Import(same, {43, 7, "Frame"});
  Require(selected_other && selected_other->samples.size() == 1 &&
              selected_other->samples[0].start_microseconds == 200 &&
              selected_other->samples[0].duration_milliseconds == .1,
          "Global samples selected the end process instead of the begin process");
  auto ordered = ChromeTraceImporter::Import(
      "[" + begin + ',' + Global("b", 200, 42, true, "2") + ',' + Global("e", 300, 44) + ',' +
          Scope("B", 400, "Frame") + ',' + Event(500) + ',' + Scope("E", 600) + ',' +
          Global("e", 700, 43, false, "2") + "]",
      selection);
  Require(ordered && ChromeTraceImporter::Validate(*ordered) && ordered->samples.size() == 4 &&
              ordered->samples[0].duration_milliseconds == .2 &&
              ordered->samples[1].duration_milliseconds == .5 &&
              ordered->samples[2].duration_milliseconds == .2,
          "Global independent completion lost mixed X/B/E/b/e begin ordering");
  for (int lane = 0; lane < 3; ++lane) {
    const auto id = lane == 0 ? "\"1\"" : "1";
    const auto cat = lane == 1 ? "other" : "work";
    const auto scope = lane == 2 ? "other" : "session";
    auto separated = ChromeTraceImporter::Import(
        "[" + begin + ',' + Global("b", 200, 42, true, id, "Frame", cat, scope) + ',' + end + ',' +
            Global("e", 1400, 44, false, id, "Frame", cat, scope) + "]",
        selection);
    Require(separated && separated->samples.size() == 2 &&
                separated->samples[0].duration_milliseconds == 1.25 &&
                separated->samples[1].duration_milliseconds == 1.2,
            "Typed global ID/category/scope identity lanes collided");
  }
  auto unicode =
      ChromeTraceImporter::Import("[" + Global("b", 100, 42, true, "\"時間\\ud83d\\ude00\"") + ',' +
                                      Global("e", 100, 43, false, "\"時間😀\"") + "]",
                                  selection);
  Require(unicode && unicode->samples[0].duration_milliseconds == 0,
          "Global escaped Unicode ID alias or zero duration rejected");
  auto unrelated_local = Async("b", 200, "null", 7, "Other");
  unrelated_local.replace(unrelated_local.find("\"pid\":42"), 8, "\"pid\":44");
  Require(
      ChromeTraceImporter::Import("[" + begin + ',' + unrelated_local + ',' + end + "]", selection)
          .has_value(),
      "Unrelated process-local metadata interfered with global pairing");
  const auto reject = [&](const std::string &source) {
    const auto copy = source;
    std::string error;
    Require(!ChromeTraceImporter::Import(source, selection, &error) && !error.empty() &&
                source == copy && capture->samples[0].duration_milliseconds == 1.25,
            "Invalid global trace lost source/prior capture or diagnostic preservation");
  };
  reject("[" + begin + "]");
  reject("[" + end + ',' + begin + "]");
  reject("[" + begin + ',' + Global("e", 99, 43) + "]");
  reject("[" + begin + ',' + Global("e", 200, 43, false, "2") + "]");
  reject("[" + begin + ',' + Global("e", 200, 43, false, "1", "Other") + "]");
  reject("[" + begin + ',' + Global("b", 200, 43) + ',' + Global("e", 400, 44) + ',' +
         Global("e", 300) + "]");
  reject("[" + Event(1) + ',' + Global("b", 200, 44, true, "2", "Other") + "]");
  reject("[" + begin + ',' + Global("b", 99, 43) + ',' + Global("e", 300, 44) + ',' + end + "]");
  for (const auto id : {"null", "true", "-1", "1.5", "9007199254740992", "\"\""})
    for (bool legacy : {false, true})
      reject("[" + Global("b", 100, 42, legacy, id) + ',' + end + "]");
  auto ambiguous = begin;
  ambiguous.replace(ambiguous.find("\"global\":1"), 10, "\"global\":1,\"local\":1");
  reject("[" + ambiguous + ',' + end + "]");
  auto dual = begin;
  dual.insert(dual.size() - 1, ",\"id\":1");
  reject("[" + dual + ',' + end + "]");
  std::string pending = "[" + begin;
  for (std::uint64_t i = 1; i <= 605; ++i)
    pending += ',' + Event(100 + i);
  auto evicted =
      ChromeTraceImporter::Import(pending + ',' + Global("e", 10000, 43) + "]", selection);
  Require(evicted && ChromeTraceImporter::Validate(*evicted) &&
              evicted->older_samples_dropped == 6 && evicted->samples.front().sequence == 7 &&
              evicted->samples.back().sequence == 606,
          "Global pending begin eviction lost newest600/drop accounting");
  for (const auto &sample : evicted->samples)
    Require(sample.duration_milliseconds == 1.2505,
            "Evicted global end overwrote a newer ring slot");
  std::string depth = "[" + Event(1);
  for (std::size_t i = 0; i < ChromeTraceImporter::kMaximumOpenScopes; ++i)
    depth += ',' + Global("b", 100 + i, 43, true, std::to_string(i), "Other");
  auto exact = depth;
  for (std::size_t i = 0; i < ChromeTraceImporter::kMaximumOpenScopes; ++i)
    exact += ',' + Global("e", 1000 + i, 44, false, std::to_string(i), "Other");
  Require(ChromeTraceImporter::Import(exact + "]", selection).has_value(),
          "Exact global open capacity rejected");
  reject(depth + ',' + Async("b", 500, "999") + "]");
  reject(depth + ',' + Scope("B", 500, "Other") + "]");
}

void LegacyAsyncPairs() {
  const ChromeTraceSelection selection{42, 7, "Frame"};
  const auto begin = Global("S", 100, 42, true);
  const auto end = Global("F", 1350, 43);
  const auto bytes = "[" + begin + ',' + Global("S", 200, 42, false, "2") + ',' +
                     Global("F", 300, 44, true) + ',' + Event(400) + ',' +
                     Global("F", 500, 45, true, "2") + "]";
  const auto capture = ChromeTraceImporter::Import(bytes, selection);
  Require(capture && ChromeTraceImporter::Validate(*capture) && capture->samples.size() == 3 &&
              capture->samples[0].start_microseconds == 100 &&
              capture->samples[0].duration_milliseconds == .2 &&
              capture->samples[1].duration_milliseconds == .3 &&
              capture->samples[2].duration_milliseconds == 1.2505,
          "Legacy S/F lost cross-process ID aliases, independent completion or begin order");
  const auto reject = [&](const std::string &source) {
    const auto original = source;
    std::string error = "stale";
    Require(!ChromeTraceImporter::Import(source, selection, &error) && !error.empty() &&
                error != "stale" && source == original &&
                capture->samples[0].duration_milliseconds == .2,
            "Invalid legacy trace lost input/prior capture preservation or diagnostics");
  };
  // Legacy S/F is not nestable; same typed ID can be live under different names.
  auto names = ChromeTraceImporter::Import(
      "[" + begin + ',' + Global("S", 200, 44, false, "1", "Other") + ',' + end + ',' +
          Global("F", 1400, 45, true, "1", "Other") + "]",
      selection);
  Require(names && names->samples.size() == 1 && names->samples[0].duration_milliseconds == 1.25,
          "Independent legacy event names collided or selected an end process/thread");
  auto other_begin = ChromeTraceImporter::Import(
      "[" + Global("S", 100, 43) + ',' + Global("F", 200) + ',' + Event(300) + "]", selection);
  Require(other_begin && other_begin->samples.size() == 1 &&
              other_begin->samples[0].start_microseconds == 300,
          "Legacy selection used finish process rather than begin process");
  auto mixed = ChromeTraceImporter::Import(
      "[" + begin + ',' + Global("b", 200) + ',' + Async("S", 300) + ',' +
          Scope("B", 400, "Frame") + ',' + Event(500) + ',' + Scope("E", 600) + ',' +
          Global("e", 700, 43) + ',' + Async("F", 800, "1", 8) + ',' + end + "]",
      selection);
  Require(mixed && ChromeTraceImporter::Validate(*mixed) && mixed->samples.size() == 5 &&
              mixed->samples[0].duration_milliseconds == 1.25 &&
              mixed->samples[1].duration_milliseconds == .5 &&
              mixed->samples[2].duration_milliseconds == .5 &&
              mixed->samples[3].duration_milliseconds == .2 &&
              mixed->samples[4].duration_milliseconds == 1.2505,
          "Legacy, nestable, local and synchronous pairing domains interfered");
  for (int lane = 0; lane < 3; ++lane) {
    const auto id = lane == 0 ? "\"1\"" : "1";
    const auto cat = lane == 1 ? "other" : "work";
    const auto scope = lane == 2 ? "other" : "session";
    const auto separated = ChromeTraceImporter::Import(
        "[" + begin + ',' + Global("S", 200, 42, false, id, "Frame", cat, scope) + ',' + end + ',' +
            Global("F", 1400, 44, true, id, "Frame", cat, scope) + "]",
        selection);
    Require(separated && separated->samples.size() == 2 &&
                separated->samples[0].duration_milliseconds == 1.25 &&
                separated->samples[1].duration_milliseconds == 1.2,
            "Legacy numeric/string ID, category or scope namespaces collided");
  }
  const auto unicode =
      ChromeTraceImporter::Import("[" + Global("S", 100, 42, true, "\"時間\\ud83d\\ude00\"") + ',' +
                                      Global("F", 100, 43, false, "\"時間😀\"") + "]",
                                  selection);
  Require(unicode && unicode->samples[0].duration_milliseconds == 0,
          "Legacy Unicode ID alias or zero duration rejected");
  const auto fractional = ChromeTraceImporter::Import(
      R"([{"ph":"S","name":"Frame","pid":42,"tid":7,"ts":100.25,"id":1},)"
      R"({"ph":"F","name":"Frame","pid":43,"tid":9,"ts":1350.75,"id2":{"global":1}}])",
      selection);
  Require(fractional && fractional->samples[0].duration_milliseconds == 1.2505,
          "Legacy fractional external timestamps lost precision");
  const auto metadata = ChromeTraceImporter::Import(
      "[" + begin + R"(,{"ph":"T","args":{"step":"opaque"}},{"ph":"p"},)" + end + "]", selection);
  Require(metadata && metadata->samples.size() == 1 &&
              metadata->samples[0].duration_milliseconds == 1.25,
          "Legacy steps changed whole-interval duration or fabricated samples");
  reject("[" + begin + "]");
  reject("[" + end + ',' + begin + "]");
  reject("[" + begin + ',' + Global("F", 99, 43) + "]");
  reject("[" + begin + ',' + Global("S", 200, 43) + ',' + end + "]");
  reject("[" + begin + ',' + Global("F", 200, 43, false, "2") + "]");
  reject("[" + begin + ',' + Global("F", 200, 43, false, "1", "Other") + "]");
  reject("[" + begin + ',' + Global("e", 200, 43) + "]");
  reject("[" + Global("b", 100) + ',' + end + "]");
  reject("[" + begin + ',' + Async("F", 200) + "]");
  reject("[" + Event(1) + ',' + Global("S", 200, 43, true, "2", "Other") + "]");
  for (const auto id : {"null", "true", "-1", "1.5", "9007199254740992", "\"\""})
    reject("[" + Global("S", 100, 42, true, id) + ',' + end + "]");
  const auto mutate = [&](std::string_view from, std::string_view to) {
    auto source = "[" + begin + ',' + end + "]";
    const auto at = source.find(from);
    Require(at != std::string::npos, "Legacy mutation fixture missing");
    source.replace(at, from.size(), to);
    reject(source);
  };
  mutate("\"id\":1,", "");
  mutate("\"id\":1", "\"id\":1,\"id2\":{\"global\":1}");
  mutate("\"id\":1", "\"id2\":{\"global\":1,\"local\":1}");
  mutate("\"cat\":\"work\"", "\"cat\":null");
  mutate("\"scope\":\"session\"", "\"scope\":[]");
  mutate("\"ts\":100", "\"ts\":100,\"dur\":1");
  mutate("\"tid\":7,", "");
  mutate("\"name\":\"Frame\",", "");
  for (std::size_t n = 0; n < bytes.size(); ++n)
    reject(bytes.substr(0, n));
  auto local_other = Async("S", 100, "null");
  local_other.replace(local_other.find("\"pid\":42"), 8, "\"pid\":43");
  Require(ChromeTraceImporter::Import("[" + local_other + ',' + Event(200) + "]", selection)
              .has_value(),
          "Unrelated process-local legacy metadata interfered with selection");
  auto reused = ChromeTraceImporter::Import(
      "[" + begin + ',' + end + ',' + Global("S", 1400) + ',' + Global("F", 1500) + "]", selection);
  Require(reused && reused->samples.size() == 2 && reused->samples[1].duration_milliseconds == .1,
          "Completed legacy identity could not be reused");
  std::string pending = "[" + begin;
  for (std::uint64_t i = 1; i <= 605; ++i)
    pending += ',' + Event(100 + i);
  const auto ring =
      ChromeTraceImporter::Import(pending + ',' + Global("F", 10000) + "]", selection);
  Require(ring && ChromeTraceImporter::Validate(*ring) && ring->older_samples_dropped == 6 &&
              ring->samples.front().sequence == 7 && ring->samples.back().sequence == 606,
          "Legacy pending-begin eviction lost retention or sequence accounting");
  for (const auto &sample : ring->samples)
    Require(sample.duration_milliseconds == 1.2505,
            "Evicted legacy finish overwrote a newer sample slot");
  std::string depth = "[";
  for (std::size_t i = 0; i < ChromeTraceImporter::kMaximumOpenScopes; ++i)
    depth += (i ? "," : "") + Global("S", i, 42, true, std::to_string(i));
  auto exact = depth;
  for (std::size_t i = 0; i < ChromeTraceImporter::kMaximumOpenScopes; ++i)
    exact += ',' + Global("F", 1000 + i, 43, false, std::to_string(i));
  const auto full = ChromeTraceImporter::Import(exact + "]", selection);
  Require(full && full->samples.size() == 128, "Exact legacy open-scope capacity rejected");
  // Balance every overflow candidate so rejection cannot be explained by missing ends.
  const auto finishes = exact.substr(depth.size());
  reject(depth + ',' + Global("S", 500, 42, true, "999") + ',' +
         Global("F", 900, 43, false, "999") + finishes + "]");
  reject(depth + ',' + Global("b", 500, 42, true, "999") + ',' +
         Global("e", 900, 43, false, "999") + finishes + "]");
  reject(depth + ',' + Scope("B", 500, "Other") + ',' + Scope("E", 900) + finishes + "]");
}

std::string CompleteEvent(std::string_view start, std::string_view duration,
                          std::string_view name = "Frame", unsigned thread = 7,
                          unsigned process = 42) {
  return "{\"ph\":\"X\",\"name\":\"" + std::string(name) + "\",\"pid\":" + std::to_string(process) +
         ",\"tid\":" + std::to_string(thread) + ",\"ts\":" + std::string(start) +
         ",\"dur\":" + std::string(duration) + "}";
}
void SelfElapsed() {
  const ChromeTraceSelection selection{42, 7, "Frame"};
  const auto parse = [&](const std::string &source) {
    const auto original = source;
    auto result = ChromeTraceImporter::Import(source, selection);
    Require(result && ChromeTraceImporter::Validate(*result) && source == original,
            "Self analysis rejected a valid inclusive trace or violated capture bounds");
    return *result;
  };
  const auto equal = [](const std::optional<double> &value, double expected) {
    return value && std::abs(*value - expected) < 1e-12;
  };
  const auto single =
      parse("[" + CompleteEvent("100", "1000") + ',' + CompleteEvent("200", "200", "Other") + ',' +
            CompleteEvent("500", "100", "Other") + "]");
  Require(single.samples.size() == 1 && single.samples[0].duration_milliseconds == 1 &&
              equal(single.samples[0].self_duration_milliseconds, .7),
          "Unselected synchronous children did not reduce parent self elapsed time");
  const auto mixed = parse("[" + Scope("B", 0, "Frame") + ',' + Scope("B", 100, "Other") + ',' +
                           CompleteEvent("150", "50", "Other") + ',' + Scope("E", 300) + ',' +
                           CompleteEvent("400", "100") + ',' + Scope("E", 1000) + "]");
  Require(mixed.samples.size() == 2 && equal(mixed.samples[0].self_duration_milliseconds, .7) &&
              equal(mixed.samples[1].self_duration_milliseconds, .1),
          "Mixed B/E/X self time double-counted grandchildren or lost selected begin order");
  const auto fractional = parse("[" + CompleteEvent("100.25", "1250.5") + ',' +
                                CompleteEvent("100.5", "250.25", "Other") + "]");
  Require(equal(fractional.samples[0].self_duration_milliseconds, 1.00025),
          "Fractional external microseconds lost self-time precision");
  const auto large = parse("[" + CompleteEvent("9007199254740000", "0.25") + ',' +
                           CompleteEvent("9007199254740000", "0.125", "Other") + "]");
  Require(equal(large.samples[0].self_duration_milliseconds, .000125) &&
              large.samples[0].duration_milliseconds == .00025,
          "Large absolute clocks rounded away raw relative durations");
  const auto crossing_large = parse("[" + CompleteEvent("1000000000000000", "0.2") + ',' +
                                    CompleteEvent("1000000000000000.125", "0.1", "Other") + "]");
  Require(!crossing_large.samples[0].self_duration_milliseconds &&
              crossing_large.samples[0].duration_milliseconds == .0002,
          "Rounded absolute endpoints fabricated a contained child or changed inclusive latency");
  const auto unordered =
      parse("[" + CompleteEvent("200", "100", "Other") + ',' + CompleteEvent("100", "1000") + "]");
  Require(equal(unordered.samples[0].self_duration_milliseconds, .9),
          "Out-of-order nonselected context changed selected input acceptance or self time");
  const auto threads =
      parse("[" + CompleteEvent("100", "1000") + ',' + CompleteEvent("200", "100", "Other", 8) +
            ',' + CompleteEvent("300", "100", "Other", 7, 43) + "]");
  Require(equal(threads.samples[0].self_duration_milliseconds, 1),
          "Unrelated process/thread intervals reduced selected-thread self time");
  const auto coincident =
      parse("[" + CompleteEvent("100", "100") + ',' + CompleteEvent("100", "100", "Other") + "]");
  Require(!coincident.samples[0].self_duration_milliseconds &&
              coincident.samples[0].duration_milliseconds == .1,
          "Ambiguous coincident X intervals fabricated self time or lost inclusive duration");
  for (bool selected_parent : {false, true}) {
    const auto paired = parse("[" + Scope("B", 100, selected_parent ? "Frame" : "Other") + ',' +
                              Scope("B", 100, selected_parent ? "Other" : "Frame") + ',' +
                              Scope("E", 200) + ',' + Scope("E", 200) + "]");
    Require(paired.samples.size() == 1 &&
                equal(paired.samples[0].self_duration_milliseconds, selected_parent ? 0 : .1),
            "Known coincident B/E hierarchy lost its original begin order");
  }
  const auto crossing =
      parse("[" + CompleteEvent("0", "200") + ',' + CompleteEvent("50", "100", "Other") + ',' +
            CompleteEvent("100", "100", "Other") + ',' + CompleteEvent("300", "50") + "]");
  Require(crossing.samples.size() == 2 && !crossing.samples[0].self_duration_milliseconds &&
              equal(crossing.samples[1].self_duration_milliseconds, .05),
          "Crossing component fabricated self time or poisoned an independent valid component");
  const auto async =
      parse("[" + Scope("B", 0, "Frame") + ',' + Global("b", 100) + ',' +
            Global("S", 300, 42, true) + ',' + CompleteEvent("400", "100", "Other") + ',' +
            Global("e", 900, 43) + ',' + Global("F", 950, 43) + ',' + Scope("E", 1000) + "]");
  Require(async.samples.size() == 3 && equal(async.samples[0].self_duration_milliseconds, .9) &&
              !async.samples[1].self_duration_milliseconds &&
              !async.samples[2].self_duration_milliseconds,
          "Async intervals claimed self time or interfered with synchronous child accounting");
  const auto zero = parse("[" + CompleteEvent("0", "0") + ',' + CompleteEvent("0", "0") + "]");
  Require(equal(zero.samples[0].self_duration_milliseconds, 0) &&
              equal(zero.samples[1].self_duration_milliseconds, 0),
          "Valid zero-duration intervals lost available zero self time");
  // Independent tick-coverage oracle: subtract the union of all contained intervals, not a
  // reconstruction stack or sum of every descendant. All times are integral microseconds.
  const std::array spans{std::pair{0, 100},   std::pair{10, 80},   std::pair{20, 30},
                         std::pair{40, 50},   std::pair{60, 70},   std::pair{90, 100},
                         std::pair{200, 250}, std::pair{210, 230}, std::pair{220, 220},
                         std::pair{230, 240}};
  const auto check_coverage = [&](const auto &intervals) {
    std::string source = "[";
    for (const auto &[begin, end] : intervals) {
      if (source.size() > 1)
        source += ',';
      source += CompleteEvent(std::to_string(begin), std::to_string(end - begin));
    }
    const auto oracle = parse(source + "]");
    Require(oracle.samples.size() == intervals.size(), "Coverage oracle lost selected intervals");
    for (std::size_t i = 0; i < intervals.size(); ++i) {
      const auto [begin, end] = intervals[i];
      unsigned uncovered{};
      for (int tick = begin; tick < end; ++tick) {
        bool covered = false;
        for (std::size_t j = 0; j < intervals.size(); ++j)
          if (j != i && intervals[j].first >= begin && intervals[j].second <= end &&
              tick >= intervals[j].first && tick < intervals[j].second)
            covered = true;
        uncovered += !covered;
      }
      Require(equal(oracle.samples[i].self_duration_milliseconds, uncovered / 1000.0),
              "Self elapsed disagrees with independent integer tick-union coverage");
    }
  };
  check_coverage(spans);
  for (std::uint32_t seed = 1; seed <= 40; ++seed) {
    std::vector<std::pair<int, int>> generated;
    auto state = seed;
    const auto add = [&](auto &&recurse, int begin, int end, unsigned depth) -> void {
      generated.emplace_back(begin, end);
      if (!depth || end - begin < 6)
        return;
      state = state * 1664525U + 1013904223U;
      const auto children = (state >> 16) & 3U;
      const auto middle = begin + (end - begin) / 2;
      if (children & 1)
        recurse(recurse, begin + 1, middle - 1, depth - 1);
      if (children & 2)
        recurse(recurse, middle + 1, end - 1, depth - 1);
    };
    add(add, 0, 128, 5);
    check_coverage(generated);
  }
  std::string deep = "[";
  for (unsigned i = 0; i < 4096; ++i)
    deep += (i ? "," : "") + CompleteEvent(std::to_string(i), std::to_string(8192 - 2 * i));
  const auto deep_capture = parse(deep + "]");
  Require(deep_capture.older_samples_dropped == 3496 && deep_capture.samples.size() == 600,
          "Deep synchronous context changed newest600 retention");
  for (const auto &sample : deep_capture.samples)
    Require(equal(sample.self_duration_milliseconds, .002),
            "Iterative deep self-time reconstruction lost nested coverage");
  // Equal starts reverse context order relative to retention order. The evicted innermost
  // interval has100us self time; it must not overwrite retained sequence601's3us ring slot.
  std::string eviction = "[";
  for (unsigned i = 0; i < 606; ++i)
    eviction += (i ? "," : "") + CompleteEvent("0", std::to_string(100 + 3 * i));
  const auto retained = parse(eviction + "]");
  Require(retained.older_samples_dropped == 6 && retained.samples.front().sequence == 7 &&
              retained.samples.back().sequence == 606,
          "Self-time context sorting changed sample sequence/loss order");
  for (const auto &sample : retained.samples)
    Require(equal(sample.self_duration_milliseconds, .003),
            "Evicted sorted context overwrote a newer ring occupant's self time");
  for (const auto invalid : {-1., 1.01, std::numeric_limits<double>::infinity(),
                             std::numeric_limits<double>::quiet_NaN()}) {
    auto copy = single;
    copy.samples[0].self_duration_milliseconds = invalid;
    Require(!ChromeTraceImporter::Validate(copy), "Invalid optional self time admitted");
  }
  auto unavailable = single;
  unavailable.samples[0].self_duration_milliseconds.reset();
  Require(ChromeTraceImporter::Validate(unavailable), "Unavailable self time rejected");
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
              paired_capture->samples.front().self_duration_milliseconds == 1.25 &&
              read() == paired_bytes,
          "Read-only project duration import altered bytes or lost external duration");
  const auto async_bytes = "[" + Async("b", 100) + ',' + Async("e", 1350, "1", 8) + "]";
  write(async_bytes);
  auto async_capture = observer.ImportChromeTraceJson(selection, &error);
  Require(async_capture && async_capture->samples.front().duration_milliseconds == 1.25 &&
              !async_capture->samples.front().self_duration_milliseconds && read() == async_bytes,
          "Read-only cross-thread async import changed source or duration");
  const auto global_bytes = "[" + Global("b", 100, 42, true) + ',' + Global("e", 1350, 43) + "]";
  write(global_bytes);
  auto global_capture = observer.ImportChromeTraceJson(selection, &error);
  Require(global_capture && global_capture->samples.front().duration_milliseconds == 1.25 &&
              read() == global_bytes,
          "Read-only cross-process global import changed source or duration");
  const auto legacy_bytes = "[" + Global("S", 100, 42, true) + ',' + Global("F", 1350, 43) + "]";
  write(legacy_bytes);
  auto legacy_capture = observer.ImportChromeTraceJson(selection, &error);
  Require(legacy_capture && legacy_capture->samples.front().duration_milliseconds == 1.25 &&
              read() == legacy_bytes,
          "Read-only legacy async import changed source or duration");
  write("[" + Global("S", 100) + "]");
  Require(!observer.ImportChromeTraceJson(selection, &error) && !error.empty() &&
              legacy_capture->samples.front().duration_milliseconds == 1.25,
          "Unclosed legacy project import erased prior owning capture");
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
    AsyncPairs();
    GlobalPairs();
    LegacyAsyncPairs();
    SelfElapsed();
    ProjectIo();
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
