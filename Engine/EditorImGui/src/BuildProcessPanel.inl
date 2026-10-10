// Included inside the EditorImGui namespace after its private State definition.
namespace {
std::string BuildDisplayBytes(std::string_view bytes, bool lines = false) {
  constexpr char hex[] = "0123456789abcdef";
  std::string result;
  result.reserve(bytes.size());
  for (const unsigned char byte : bytes) {
    if ((byte >= 32 && byte < 127 && byte != '\\' && byte != '"') ||
        (lines && (byte == '\n' || byte == '\t'))) {
      result.push_back(static_cast<char>(byte));
    } else {
      result += "\\x";
      result.push_back(hex[byte >> 4]);
      result.push_back(hex[byte & 15]);
    }
  }
  return result;
}
const char *BuildPhaseLabel(BuildProcessPhase phase) noexcept {
  switch (phase) {
  case BuildProcessPhase::Idle:
    return "Ready to run";
  case BuildProcessPhase::Queued:
    return "Queued";
  case BuildProcessPhase::Running:
    return "Running";
  case BuildProcessPhase::AwaitingOwner:
    return "Finalizing process";
  case BuildProcessPhase::Exited:
    return "Process exited 0; artifacts remain unverified";
  case BuildProcessPhase::Failed:
    return "Process failed";
  case BuildProcessPhase::Cancelled:
    return "Cancelled";
  case BuildProcessPhase::Stale:
    return "Previous project operation discarded";
  case BuildProcessPhase::Unsupported:
    return "Process execution unavailable on this host";
  }
  return "Unavailable";
}
} // namespace

void EditorImGuiHost::DrawBuildProcess(BuildProcess &process, std::uint64_t scope,
                                       const std::filesystem::path &root, bool allow_start) {
  ImGui::SetCurrentContext(state_->context);
  auto &state = *state_;
  allow_start = allow_start && state.app_focused && !state.build_console_interaction_blocked &&
                state.play_command == PlayCommand::None && !state.close_prompt_requested &&
                state.close_choice == CloseChoice::None &&
                state.scene_file_dialog == State::FileDialog::None && !state.scene_file_output &&
                !state.scene_tab_dialog && !state.scene_tab_output &&
                !state.hierarchy_rename_target && !state.content_rename_target &&
                !state.game_input_binding_open;
  state.build_console_positions = {};
  if (state.build_console_scope != scope) {
    static_cast<void>(process.Cancel());
    state.build_console_scope = scope;
    state.build_executable = {};
    state.build_cwd = {};
    state.build_arguments.clear();
    state.build_console_error.clear();
    state.build_console_status = {};
    state.build_console_output.clear();
    try {
      const auto cwd = root.u8string();
      if (cwd.size() < state.build_cwd.size() &&
          foundation::IsValidUtf8(
              std::string_view{reinterpret_cast<const char *>(cwd.data()), cwd.size()}))
        std::memcpy(state.build_cwd.data(), cwd.data(), cwd.size());
    } catch (const std::exception &) {
      state.build_console_error = "Project path cannot be represented.";
    }
  }
  if (!allow_start && process.Busy())
    static_cast<void>(process.Cancel());
  static_cast<void>(process.Poll(scope));
  auto observation = process.Snapshot();
  if (scope && observation.scope == scope) {
    if (observation.output != state.build_console_status.output)
      state.build_console_output = BuildDisplayBytes(observation.output, true);
    state.build_console_status = std::move(observation);
  }
  const auto &io = ImGui::GetIO();
  if (state.app_focused && io.KeyCtrl && io.KeyAlt && !io.WantTextInput &&
      ImGui::IsKeyPressed(ImGuiKey_B, false))
    state.build_console_open = !state.build_console_open;
  if (!state.build_console_open)
    return;
  ImGui::SetNextWindowSize({720, 580}, ImGuiCond_FirstUseEver);
  if (!ImGui::Begin("Build process console", &state.build_console_open,
                    ImGuiWindowFlags_NoSavedSettings)) {
    ImGui::End();
    return;
  }
  const auto position = [&](std::size_t index) {
    const auto low = ImGui::GetItemRectMin(), high = ImGui::GetItemRectMax();
    state.build_console_positions[index] =
        std::array{(low.x + high.x) * .5F, (low.y + high.y) * .5F};
  };
  ImGui::TextWrapped("Run a local program with separate arguments. Configuration and output stay "
                     "in this session.");
  ImGui::TextDisabled("The program uses your current environment and can modify files.");
  const bool busy = process.Busy();
  ImGui::BeginDisabled(busy || !allow_start || !scope);
  ImGui::InputText("Executable", state.build_executable.data(), state.build_executable.size());
  position(0);
  ImGui::SetItemDefaultFocus();
  ImGui::InputText("Working directory", state.build_cwd.data(), state.build_cwd.size());
  position(1);
  if (ImGui::Button("Add argument") &&
      state.build_arguments.size() < BuildProcess::kMaximumArguments)
    state.build_arguments.emplace_back();
  position(2);
  ImGui::SameLine();
  if (ImGui::Button("Remove last argument") && !state.build_arguments.empty())
    state.build_arguments.pop_back();
  position(3);
  ImGui::BeginChild("arguments", {0, 100}, ImGuiChildFlags_Borders);
  ImGuiListClipper argument_rows;
  argument_rows.Begin(static_cast<int>(state.build_arguments.size()),
                      ImGui::GetFrameHeightWithSpacing());
  while (argument_rows.Step()) {
    for (int row = argument_rows.DisplayStart; row < argument_rows.DisplayEnd; ++row) {
      const auto index = static_cast<std::size_t>(row);
      ImGui::PushID(row);
      ImGui::InputText("Argument", state.build_arguments[index].data(),
                       state.build_arguments[index].size());
      if (index == 0)
        position(6);
      ImGui::PopID();
    }
  }
  ImGui::EndChild();
  const bool run_clicked = ImGui::Button("Run process");
  const bool command_focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
  const bool run_key = command_focused && io.KeyCtrl && !io.KeyShift && !io.KeyAlt &&
                       ImGui::IsKeyPressed(ImGuiKey_Enter, false);
  if ((run_clicked || run_key) && !busy && allow_start && scope) {
    BuildProcessRequest request;
    request.scope = scope;
    request.output_capacity = 16 * 1024;
    state.build_console_error.clear();
    try {
      const std::string_view executable{state.build_executable.data()}, cwd{state.build_cwd.data()};
      if (!foundation::IsValidUtf8(executable) || !foundation::IsValidUtf8(cwd)) {
        state.build_console_error = "Command paths must be valid UTF-8.";
      } else {
        request.executable =
            std::filesystem::path{std::u8string(executable.begin(), executable.end())};
        request.working_directory = std::filesystem::path{std::u8string(cwd.begin(), cwd.end())};
        for (const auto &argument : state.build_arguments)
          request.arguments.emplace_back(argument.data());
        if (!process.Start(std::move(request), &state.build_console_error) &&
            state.build_console_error.empty())
          state.build_console_error = "Process request was rejected.";
      }
    } catch (const std::exception &) {
      state.build_console_error = "Command paths cannot be represented.";
    }
  }
  position(4);
  ImGui::EndDisabled();
  ImGui::SameLine();
  ImGui::BeginDisabled(!process.Busy());
  const bool cancel_clicked = ImGui::Button("Cancel process");
  const bool cancel_key = command_focused && io.KeyCtrl && io.KeyShift && !io.KeyAlt &&
                          ImGui::IsKeyPressed(ImGuiKey_Enter, false);
  if ((cancel_clicked || cancel_key) && process.Busy())
    static_cast<void>(process.Cancel());
  position(5);
  ImGui::EndDisabled();
  ImGui::TextDisabled("Ctrl+Enter runs; Ctrl+Shift+Enter cancels this process.");
  if (!allow_start || !scope)
    ImGui::TextWrapped("Run is unavailable while this project is read-only, blocked, or playing.");
  if (!state.build_console_error.empty())
    ImGui::TextWrapped("%s", BuildDisplayBytes(state.build_console_error).c_str());
  ImGui::SeparatorText("Exact command preview (escaped bytes; no shell)");
  ImGui::BeginChild("exact command", {0, 80}, ImGuiChildFlags_Borders,
                    ImGuiWindowFlags_HorizontalScrollbar);
  ImGuiListClipper preview_rows;
  preview_rows.Begin(static_cast<int>(state.build_arguments.size()) + 2);
  while (preview_rows.Step()) {
    for (int row = preview_rows.DisplayStart; row < preview_rows.DisplayEnd; ++row) {
      const auto label = row == 0   ? std::string{"Executable"}
                         : row == 1 ? std::string{"Working directory"}
                                    : "argv[" + std::to_string(row - 1) + "]";
      const std::string_view text =
          row == 0   ? state.build_executable.data()
          : row == 1 ? state.build_cwd.data()
                     : state.build_arguments[static_cast<std::size_t>(row - 2)].data();
      const auto escaped = BuildDisplayBytes(text);
      ImGui::Text("%s: \"%s\"", label.c_str(), escaped.c_str());
    }
  }
  ImGui::EndChild();
  ImGui::SeparatorText("Process result");
  ImGui::TextUnformatted(BuildPhaseLabel(state.build_console_status.phase));
  if (!state.build_console_status.message.empty())
    ImGui::TextWrapped("%s", BuildDisplayBytes(state.build_console_status.message).c_str());
  if (state.build_console_status.exit_code)
    ImGui::Text("Exit code: %u", *state.build_console_status.exit_code);
  ImGui::Text("Retained tail: %zu bytes / older bytes dropped: %llu",
              state.build_console_status.output.size(),
              static_cast<unsigned long long>(state.build_console_status.dropped_output_bytes));
  ImGui::BeginChild("process output", {0, 160}, ImGuiChildFlags_Borders);
  ImGui::TextUnformatted(state.build_console_output.data(),
                         state.build_console_output.data() + state.build_console_output.size());
  ImGui::EndChild();
  ImGui::End();
}

#if defined(NEXORA_EDITOR_IMGUI_TEST_ACCESS)
std::optional<std::array<float, 2>>
EditorImGuiTestAccess::BuildControlPosition(const EditorImGuiHost &host,
                                            std::size_t control) noexcept {
  return control < host.state_->build_console_positions.size()
             ? host.state_->build_console_positions[control]
             : std::nullopt;
}
void EditorImGuiTestAccess::SetBuildCommand(EditorImGuiHost &host, std::string_view executable,
                                            std::string_view cwd,
                                            std::span<const std::string> arguments) {
  const auto assign = [](auto &buffer, std::string_view text) {
    if (text.size() >= buffer.size() || text.find('\0') != std::string_view::npos)
      throw std::invalid_argument("Test command exceeds input budget");
    buffer = {};
    std::memcpy(buffer.data(), text.data(), text.size());
  };
  assign(host.state_->build_executable, executable);
  assign(host.state_->build_cwd, cwd);
  host.state_->build_arguments.clear();
  if (arguments.size() > BuildProcess::kMaximumArguments)
    throw std::invalid_argument("Test argv exceeds input budget");
  for (const auto &text : arguments) {
    auto &argument = host.state_->build_arguments.emplace_back();
    assign(argument, text);
  }
}
BuildProcessSnapshot EditorImGuiTestAccess::BuildStatus(const EditorImGuiHost &host) {
  return host.state_->build_console_status;
}
std::string EditorImGuiTestAccess::BuildOutput(const EditorImGuiHost &host) {
  return host.state_->build_console_output;
}
#endif
