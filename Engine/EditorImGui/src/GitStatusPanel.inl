// Included after the private State and common UI helpers. All provider borrows are frame-local.
namespace {
std::string GitDisplayPath(std::string_view bytes) {
  const bool utf8 = foundation::IsValidUtf8(bytes);
  constexpr char hex[] = "0123456789abcdef";
  std::string result;
  result.reserve(bytes.size());
  for (const unsigned char byte : bytes) {
    if (byte == '\\')
      result += "\\\\";
    else if (byte == '\n')
      result += "\\n";
    else if (byte == '\r')
      result += "\\r";
    else if (byte == '\t')
      result += "\\t";
    else if (byte < 32 || byte == 127 || (!utf8 && byte >= 128)) {
      result += "\\x";
      result.push_back(hex[byte >> 4]);
      result.push_back(hex[byte & 15]);
    } else
      result.push_back(static_cast<char>(byte));
  }
  return result;
}
const char *GitErrorLabel(GitStatusError error) noexcept {
  switch (error) {
  case GitStatusError::None:
    return "";
  case GitStatusError::ToolUnavailable:
    return "Git executable is unavailable. Enter its absolute path.";
  case GitStatusError::RepositoryUnavailable:
    return "The project root is not an available Git repository.";
  case GitStatusError::Busy:
    return "A status refresh is already active.";
  case GitStatusError::OutputBudgetExceeded:
    return "Status exceeds the supported view budget.";
  case GitStatusError::MalformedOutput:
    return "Git status output could not be validated.";
  case GitStatusError::AllocationFailed:
    return "Status refresh could not allocate its required storage.";
  case GitStatusError::Cancelled:
    return "Status refresh cancelled.";
  case GitStatusError::Stale:
    return "Previous project status discarded.";
  case GitStatusError::Stopped:
  case GitStatusError::OperationExhausted:
    return "Source control is unavailable for this session.";
  case GitStatusError::InvalidRequest:
    return "The Git path or project status context is invalid.";
  case GitStatusError::ProcessRejected:
    return "Git status could not be started.";
  case GitStatusError::ProcessFailed:
    return "Git status failed. The previous complete snapshot is retained.";
  }
  return "Git status is unavailable.";
}
} // namespace

void EditorImGuiHost::DrawGitStatus(GitStatusProvider &provider, std::uint64_t scope,
                                    const std::filesystem::path &root, bool allow_refresh) {
  ImGui::SetCurrentContext(state_->context);
  auto &state = *state_;
  state.git_control_positions = {};
  state.git_rendered_rows = 0;
  if (state.git_status_scope != scope || state.git_status_root != root) {
    auto &context = *ImGui::GetCurrentContext();
    const auto *window = ImGui::FindWindowByName("Source control###editor.git-status");
    if (window && context.ActiveIdWindow == window)
      ImGui::ClearActiveID();
    if (state.git_executable_id && context.InputTextState.ID == state.git_executable_id) {
      context.InputTextState.ClearFreeMemory();
      context.InputTextState.ID = 0;
    }
    if (state.git_executable_id && context.InputTextDeactivatedState.ID == state.git_executable_id)
      context.InputTextDeactivatedState.ClearFreeMemory();
    state.git_executable_id = 0;
    // Root changes revoke even incorrectly reused caller scopes; the provider will still reject
    // another root under a reused identity. A correct owner supplies a fresh nonzero scope.
    static_cast<void>(provider.Poll(0));
    state.git_executable = {};
    try {
      auto staged_root = root;
      state.git_status_root = std::move(staged_root);
      state.git_status_scope = scope;
      state.git_intake_error = GitStatusError::None;
    } catch (const std::bad_alloc &) {
      scope = state.git_status_scope = 0;
      state.git_status_root.clear();
      state.git_intake_error = GitStatusError::AllocationFailed;
    }
  }
  allow_refresh =
      allow_refresh && scope && !root.empty() && state.app_focused &&
      !state.build_console_interaction_blocked && state.play_command == PlayCommand::None &&
      !state.close_prompt_requested && state.close_choice == CloseChoice::None &&
      state.scene_file_dialog == State::FileDialog::None && !state.scene_file_output &&
      !state.scene_tab_dialog && !state.scene_tab_output && !state.hierarchy_rename_target &&
      !state.content_rename_target && !state.game_input_binding_open;
  if (!allow_refresh && provider.Busy())
    static_cast<void>(provider.Cancel());
  static_cast<void>(provider.Poll(scope));
  allow_refresh = allow_refresh && provider.Query().phase != GitStatusPhase::Stopped;
  if (state.app_focused && !state.game_input_focused && !ImGui::GetIO().WantTextInput &&
      ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiMod_Alt | ImGuiKey_G))
    state.git_status_open = !state.git_status_open;
  if (!state.git_status_open)
    return;
  ImGui::SetNextWindowSize({760, 480}, ImGuiCond_FirstUseEver);
  if (!ImGui::Begin("Source control###editor.git-status", &state.git_status_open,
                    ImGuiWindowFlags_NoSavedSettings)) {
    ImGui::End();
    return;
  }
  const auto position = [&](std::size_t index) {
    const auto low = ImGui::GetItemRectMin(), high = ImGui::GetItemRectMax();
    state.git_control_positions[index] = std::array{(low.x + high.x) * .5F, (low.y + high.y) * .5F};
  };
  ImGui::TextWrapped("Inspect the repository at the project root using your selected Git "
                     "installation and repository configuration.");
  ImGui::TextUnformatted("Git executable (absolute path)");
  const bool busy = provider.Busy();
  ImGui::BeginDisabled(!allow_refresh || busy);
  ImGui::SetNextItemWidth(-1);
  ImGui::InputText("##git-executable", state.git_executable.data(), state.git_executable.size());
  state.git_executable_id = ImGui::GetItemID();
  position(0);
  ImGui::SetItemDefaultFocus();
  if (ImGui::Button("Refresh local status")) {
    try {
      const std::string_view text(state.git_executable.data());
      if (!foundation::IsValidUtf8(text))
        state.git_intake_error = GitStatusError::InvalidRequest;
      else
        static_cast<void>(
            provider.Refresh(std::filesystem::path(std::u8string(text.begin(), text.end())), root,
                             scope, &state.git_intake_error));
    } catch (const std::bad_alloc &) {
      state.git_intake_error = GitStatusError::AllocationFailed;
    } catch (const std::filesystem::filesystem_error &) {
      state.git_intake_error = GitStatusError::InvalidRequest;
    }
  }
  position(1);
  ImGui::EndDisabled();
  ImGui::SameLine();
  ImGui::BeginDisabled(!provider.Busy());
  if (ImGui::Button("Cancel refresh"))
    static_cast<void>(provider.Cancel());
  position(2);
  ImGui::EndDisabled();
  const auto query = provider.Query();
  const auto capture = provider.Capture(scope);
  const bool captured = scope && capture && capture->repository_root == root;
  const auto error = state.git_intake_error != GitStatusError::None ? state.git_intake_error
                     : query.phase == GitStatusPhase::Stopped       ? GitStatusError::Stopped
                     : scope && query.scope == scope                ? query.error
                                                                    : GitStatusError::None;
  if (error != GitStatusError::None)
    ImGui::TextWrapped("%s", GitErrorLabel(error));
  else if (!scope || root.empty())
    ImGui::TextUnformatted(root.empty()
                               ? "Open a project to inspect its local repository."
                               : "Source control is unavailable for this project session.");
  else if (provider.Busy())
    ImGui::TextUnformatted("Refreshing local status...");
  else if (!captured)
    ImGui::TextUnformatted("Refresh to inspect the local repository.");
  if (captured) {
    if (provider.Busy() || error != GitStatusError::None || capture->operation != query.operation)
      ImGui::TextUnformatted("Showing the last successful complete snapshot.");
    if (capture->entries.empty())
      ImGui::TextUnformatted("Clean at last successful refresh.");
    else
      ImGui::Text("%zu changed paths at last successful refresh.", capture->entries.size());
    ImGui::TextDisabled("Index / working tree");
    ImGui::BeginChild("git-status-rows", {0, 0}, ImGuiChildFlags_Borders);
    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(capture->entries.size()), ImGui::GetTextLineHeightWithSpacing());
    while (clipper.Step())
      for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
        ++state.git_rendered_rows;
        const auto &entry = capture->entries[static_cast<std::size_t>(row)];
        const char status[]{entry.index, entry.working_tree, '\0'};
        ImGui::TextUnformatted(status);
        ImGui::SameLine(64);
        try {
          auto path = GitDisplayPath(entry.path);
          if (!entry.original_path.empty())
            path += " <- " + GitDisplayPath(entry.original_path);
          ImGui::TextUnformatted(path.data(), path.data() + path.size());
        } catch (const std::bad_alloc &) {
          ImGui::TextUnformatted("Path display unavailable; status snapshot retained.");
        }
      }
    ImGui::EndChild();
  }
  ImGui::End();
}

#if defined(NEXORA_EDITOR_IMGUI_TEST_ACCESS)
std::optional<std::array<float, 2>>
EditorImGuiTestAccess::GitControlPosition(const EditorImGuiHost &host,
                                          std::size_t control) noexcept {
  return control < host.state_->git_control_positions.size()
             ? host.state_->git_control_positions[control]
             : std::nullopt;
}
std::size_t EditorImGuiTestAccess::GitRenderedRows(const EditorImGuiHost &host) noexcept {
  return host.state_->git_rendered_rows;
}
#endif
