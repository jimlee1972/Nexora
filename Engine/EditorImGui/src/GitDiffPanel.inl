// Frame-only provider references; persistent state owns captures and escaped display storage.
namespace {
template <class StateT> void ResetGitDiff(StateT &state, GitDiffProvider *provider) {
  if (provider)
    static_cast<void>(provider->Cancel());
  state.git_diff_selection.reset();
  state.git_diff_pending_operation = 0;
  state.git_diff_capture.reset();
  state.git_diff_text.clear();
  state.git_diff_lines.clear();
  state.git_diff_intake_error = GitDiffError::None;
  state.git_diff_fail_display = false;
}
const char *GitDiffErrorLabel(GitDiffError error) noexcept {
  switch (error) {
  case GitDiffError::None:
    return "";
  case GitDiffError::InvalidRequest:
    return "Select supported UTF-8 file paths and a valid Git installation.";
  case GitDiffError::Busy:
    return "A diff query is already active.";
  case GitDiffError::OperationExhausted:
  case GitDiffError::Stopped:
    return "Diff queries are unavailable for this session.";
  case GitDiffError::ToolUnavailable:
    return "The selected Git executable is unavailable.";
  case GitDiffError::RepositoryUnavailable:
    return "The project repository is unavailable.";
  case GitDiffError::ProcessRejected:
    return "The diff query could not be started.";
  case GitDiffError::ProcessFailed:
    return "Git could not complete the diff query.";
  case GitDiffError::OutputBudgetExceeded:
    return "Git output exceeds the supported diff budget.";
  case GitDiffError::AllocationFailed:
    return "The diff view could not allocate its required storage.";
  case GitDiffError::Cancelled:
    return "Diff query cancelled.";
  case GitDiffError::Stale:
    return "Previous project diff discarded.";
  }
  return "Diff query unavailable.";
}
template <class StateT>
void PollGitDiff(StateT &state, GitDiffProvider *provider,
                 const std::shared_ptr<const GitStatusCapture> &status, std::uint64_t scope,
                 const std::filesystem::path &root, bool allowed) {
  if (state.git_diff_selection && state.git_diff_selection != status)
    ResetGitDiff(state, provider);
  if (!provider)
    return;
  if (!allowed) {
    static_cast<void>(provider->Cancel());
    state.git_diff_pending_operation = 0;
  }
  if (!provider->Poll(scope) || !state.git_diff_pending_operation)
    return;
  const auto expected = std::exchange(state.git_diff_pending_operation, 0);
  const auto query = provider->Query();
  if (query.phase != GitDiffPhase::Ready || query.operation != expected || !allowed || !status ||
      status != state.git_diff_selection)
    return;
  const auto capture = provider->Capture(scope);
  if (!capture || capture->operation != expected || capture->repository_root != root ||
      capture->request.target != state.git_diff_pending_target ||
      state.git_diff_selected_row >= status->entries.size())
    return;
  const auto &entry = status->entries[state.git_diff_selected_row];
  if (capture->request.paths.size() != (entry.original_path.empty() ? 1U : 2U) ||
      capture->request.paths[0] != entry.path ||
      (!entry.original_path.empty() && capture->request.paths[1] != entry.original_path))
    return;
  try {
    // At most 4x256 KiB escaped text and (256 KiB+1) u32 line offsets. Build once per result.
    std::string text;
    std::vector<std::uint32_t> lines{0};
    const auto &raw = capture->output;
    for (std::size_t start = 0; start < raw.size();) {
      const auto end = raw.find('\n', start);
      const auto count = end == raw.npos ? raw.size() - start : end - start;
      text += GitDisplayPath(std::string_view(raw).substr(start, count));
      if (end == raw.npos)
        break;
      text.push_back('\n');
      lines.push_back(static_cast<std::uint32_t>(text.size()));
      start = end + 1;
    }
    if (std::exchange(state.git_diff_fail_display, false))
      throw std::bad_alloc{};
    state.git_diff_capture = capture;
    state.git_diff_text = std::move(text);
    state.git_diff_lines = std::move(lines);
    state.git_diff_intake_error = GitDiffError::None;
  } catch (const std::bad_alloc &) {
    state.git_diff_intake_error = GitDiffError::AllocationFailed;
  }
}
template <class StateT>
void DrawGitDiff(StateT &state, GitDiffProvider &provider, std::uint64_t scope,
                 const std::filesystem::path &root, bool allowed, bool status_busy) {
  allowed = allowed && provider.Query().phase != GitDiffPhase::Stopped;
  ImGui::Separator();
  const auto selection = state.git_diff_selection;
  const GitStatusEntry *entry = selection && state.git_diff_selected_row < selection->entries.size()
                                    ? &selection->entries[state.git_diff_selected_row]
                                    : nullptr;
  const bool supported =
      entry && entry->kind != GitStatusKind::Untracked && entry->kind != GitStatusKind::Ignored &&
      foundation::IsValidUtf8(entry->path) && foundation::IsValidUtf8(entry->original_path);
  ImGui::BeginDisabled(!allowed || status_busy || provider.Busy() || !supported);
  for (std::size_t index = 0; index < 2; ++index) {
    if (index)
      ImGui::SameLine();
    if (ImGui::Button(index ? "View staged diff" : "View working-tree diff")) {
      try {
        const std::string_view executable(state.git_executable.data());
        if (!foundation::IsValidUtf8(executable))
          state.git_diff_intake_error = GitDiffError::InvalidRequest;
        else {
          GitDiffRequest request{index ? GitDiffTarget::Index : GitDiffTarget::WorkingTree,
                                 {entry->path}};
          if (!entry->original_path.empty())
            request.paths.push_back(entry->original_path);
          const auto target = request.target;
          if (provider.Compare(
                  std::filesystem::path(std::u8string(executable.begin(), executable.end())), root,
                  scope, std::move(request), &state.git_diff_intake_error)) {
            state.git_diff_pending_operation = provider.Query().operation;
            state.git_diff_pending_target = target;
          }
        }
      } catch (const std::bad_alloc &) {
        state.git_diff_intake_error = GitDiffError::AllocationFailed;
      } catch (const std::filesystem::filesystem_error &) {
        state.git_diff_intake_error = GitDiffError::InvalidRequest;
      }
    }
    const auto low = ImGui::GetItemRectMin(), high = ImGui::GetItemRectMax();
    state.git_diff_control_positions[index] =
        std::array{(low.x + high.x) * .5F, (low.y + high.y) * .5F};
  }
  ImGui::EndDisabled();
  ImGui::SameLine();
  ImGui::BeginDisabled(!provider.Busy());
  if (ImGui::Button("Cancel diff") && provider.Cancel())
    state.git_diff_pending_operation = 0;
  const auto low = ImGui::GetItemRectMin(), high = ImGui::GetItemRectMax();
  state.git_diff_control_positions[2] = std::array{(low.x + high.x) * .5F, (low.y + high.y) * .5F};
  ImGui::EndDisabled();
  const auto query = provider.Query();
  const auto error = state.git_diff_intake_error != GitDiffError::None ? state.git_diff_intake_error
                     : query.phase == GitDiffPhase::Stopped            ? GitDiffError::Stopped
                     : query.scope == scope                            ? query.error
                                                                       : GitDiffError::None;
  if (error != GitDiffError::None)
    ImGui::TextWrapped("%s", GitDiffErrorLabel(error));
  if (!entry)
    ImGui::TextUnformatted("Select a tracked path to inspect its differences.");
  else if (!supported)
    ImGui::TextUnformatted("Diff queries require tracked UTF-8 file paths.");
  else if (provider.Busy())
    ImGui::TextUnformatted("Reading Git differences...");
  if (!state.git_diff_capture)
    return;
  const auto &capture = *state.git_diff_capture;
  ImGui::TextUnformatted(capture.request.target == GitDiffTarget::Index
                             ? "Staged Git output at last successful query."
                             : "Working-tree Git output at last successful query.");
  for (const auto &path : capture.request.paths) {
    try {
      const auto label = GitDisplayPath(path);
      ImGui::TextUnformatted(label.data(), label.data() + label.size());
    } catch (const std::bad_alloc &) {
      ImGui::TextUnformatted("Path label unavailable; captured output retained.");
    }
  }
  if (provider.Busy() || error != GitDiffError::None || capture.operation != query.operation)
    ImGui::TextUnformatted("Showing the previous complete query for the labeled paths and target.");
  if (capture.output.empty()) {
    ImGui::TextUnformatted("Git reported no differences for this request.");
    return;
  }
  ImGui::BeginChild("git-diff-output", {0, 0}, ImGuiChildFlags_Borders,
                    ImGuiWindowFlags_HorizontalScrollbar);
  ImGuiListClipper clipper;
  clipper.Begin(static_cast<int>(state.git_diff_lines.size()),
                ImGui::GetTextLineHeightWithSpacing());
  while (clipper.Step())
    for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
      ++state.git_diff_rendered_lines;
      const auto index = static_cast<std::size_t>(row);
      const auto start = state.git_diff_lines[index];
      const auto end = index + 1 < state.git_diff_lines.size() ? state.git_diff_lines[index + 1] - 1
                                                               : state.git_diff_text.size();
      if (end == start)
        ImGui::Dummy({0, ImGui::GetTextLineHeight()});
      else
        ImGui::TextUnformatted(state.git_diff_text.data() + start,
                               state.git_diff_text.data() + end);
    }
  ImGui::EndChild();
}
} // namespace
