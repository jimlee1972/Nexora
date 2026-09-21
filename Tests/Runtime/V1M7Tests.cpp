#include "Nexora/Runtime/InputUi.h"

#include <chrono>
#include <stdexcept>

using namespace nexora::runtime;
static void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}

int main() {
  InputSystem input;
  Require(input.Assign(0, InputDeviceKind::Keyboard, 0) &&
              input.Assign(0, InputDeviceKind::Mouse, 0) &&
              input.Assign(0, InputDeviceKind::Gamepad, 1) &&
              input.Assign(0, InputDeviceKind::Touch, 2),
          "multi-device assignment failed");
  Require(
      input.Push(
          {1, InputDeviceKind::Keyboard, 0, InputEventKind::Button, "jump", 1, 0, 0, 0, {}}) &&
          input.Push(
              {2, InputDeviceKind::Gamepad, 1, InputEventKind::Axis, "move_x", .5F, 0, 0, 0, {}}) &&
          input.Push({3,
                      InputDeviceKind::Touch,
                      2,
                      InputEventKind::PointerDown,
                      "touch",
                      1,
                      42,
                      0,
                      0,
                      {}}),
      "simultaneous device input failed");
  Require(
      !input.Push(
          {3, InputDeviceKind::Touch, 2, InputEventKind::PointerDown, "touch", 1, 42, 0, 0, {}}),
      "duplicate touch delivered");
  auto events = input.Consume(0);
  Require(events.size() == 3, "input events lost");
  ActionMap actions;
  Require(actions.Bind("user-defined-action", {"jump", 1}), "action bind failed");
  Require(actions.Evaluate(events).at("user-defined-action") == 1, "action evaluation failed");

  UIDocument document(100, 100);
  const auto panel = document.Create(UIElementKind::Panel);
  const auto button = document.Create(UIElementKind::Button, panel);
  document.Find(panel)->transform = {0, 0, 1, 1, 0, 0, 0, 0};
  document.Find(button)->transform = {0, 0, 0, 0, 10, 10, 50, 50};
  int capture = 0, target = 0, bubble = 0;
  document.Find(panel)->on_pointer = [&](const auto &, PointerPhase phase) {
    phase == PointerPhase::Capture ? ++capture : ++bubble;
    return false;
  };
  document.Find(button)->on_pointer = [&](const auto &, PointerPhase phase) {
    if (phase == PointerPhase::Target)
      ++target;
    return false;
  };
  document.Layout();
  Require(document.Dispatch({7, 20, 20, true}), "UI did not consume hit");
  Require(capture == 1 && target == 1 && bubble == 1, "capture-target-bubble order incomplete");
  Require(document.CapturePointer(7, button) && !document.CapturePointer(7, panel),
          "pointer had multiple owners");
  Require(document.Dispatch({7, 99, 99, false}), "captured event was not consumed");
  Require(target == 2, "captured pointer did not reach owner");

  VirtualizedListModel list;
  list.Configure(1'000'000, 20, 100, 2);
  list.ScrollTo(400);
  Require(list.FirstVisible() == 20 && list.RealizedCount() == 9,
          "list realization is not virtualized");
  Require(BuildNineSlice({0, 0, 100, 50}, {10, 10, 10, 10}).size() == 9,
          "nine-slice generation failed");

  LocalizationTable localization;
  localization.Set("en", "play", "Play");
  localization.Set("zh", "play", "遊玩");
  document.Find(button)->localization_key = "play";
  document.RefreshLocalization(localization);
  document.SetEnabled(false);
  Require(localization.SetLocale("zh"), "locale change failed");
  const auto before_content_update = localization.Generation();
  localization.Set("zh", "play", "遊玩+");
  Require(localization.Generation() > before_content_update,
          "localization content update did not advance generation");
  document.SetEnabled(true, &localization);
  Require(document.Find(button)->text == "遊玩+" &&
              document.AppliedLocalizationGeneration() == localization.Generation(),
          "disabled UI did not refresh localization");

  Require(localization.SetLocale("fr"), "locale change to fr failed");
  Require(localization.Resolve("play") == "Play",
          "resolve did not fall back to the default locale for an untranslated key");
  Require(!localization.SetFallbackLocale("en"), "no-op fallback locale change reported success");
  Require(localization.SetFallbackLocale("zh"), "fallback locale change failed");
  Require(localization.Resolve("play") == "遊玩+",
          "resolve did not use the updated fallback locale");
  Require(localization.Resolve("unknown-key") == "unknown-key",
          "resolve did not fall back to the raw key when no locale has a translation");

  const auto logical = document.LogicalViewport();
  document.SetLogicalResolution(1280, 720);
  Require(logical.width == 100 && document.LogicalViewport().width == 1280,
          "logical resolution was not independently controlled");

  TextEditBuffer edit;
  edit.Set("A界");
  Require(edit.BeginComposition(1) && edit.UpdateComposition("好") && edit.CommitComposition(),
          "IME composition failed");
  Require(edit.Text() == "A好界" && edit.Undo() && edit.Text() == "A界", "UTF-8 edit undo failed");

  TextEditBuffer invalid_utf8;
  invalid_utf8.Set(std::string{static_cast<char>(0xC0), static_cast<char>(0xAF)});
  Require(invalid_utf8.Text().empty(), "invalid UTF-8 was accepted");
  Require(!invalid_utf8.BeginComposition(1), "invalid UTF-8 exposed a code-point boundary");

  InputSystem baseline;
  Require(baseline.Assign(0, InputDeviceKind::Keyboard, 0), "baseline input assignment failed");
  const auto begin = std::chrono::steady_clock::now();
  for (std::uint64_t sequence = 1; sequence <= 10000; ++sequence)
    Require(baseline.Push({sequence, InputDeviceKind::Keyboard, 0, InputEventKind::Button,
                           "baseline", 1, 0, 0, 0, {}}),
            "baseline input push failed");
  Require(baseline.Consume(0).size() == 10000 &&
              std::chrono::steady_clock::now() - begin < std::chrono::seconds(2),
          "input routing performance baseline failed");
  return 0;
}
