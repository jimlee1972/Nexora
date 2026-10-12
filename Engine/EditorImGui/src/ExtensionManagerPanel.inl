// Included in the graphical owner; widgets only transfer owning observations and requests.
bool EditorImGuiHost::SetExtensionManagerObservation(ExtensionManagerObservation observation) {
  Activate(state_->context);
  if (!observation.scope || (observation.permissions & ~SignedExtensionHost::kKnownPermissions) ||
      observation.publishers.size() > ExtensionTrust::kMaximumPublishers ||
      observation.packages.size() > PluginManager::kMaximumPackages ||
      observation.rejected_files > 64)
    return false;
  const auto path = observation.root.generic_u8string();
  if (path.size() > runtime::PluginHost::kMaximumPathBytes ||
      std::ranges::find(path, char8_t{}) != path.end() ||
      !foundation::IsValidUtf8({reinterpret_cast<const char *>(path.data()), path.size()}) ||
      (!observation.root.empty() && !observation.root.is_absolute()) ||
      (observation.root.empty() != (observation.project == foundation::Uuid{})))
    return false;
  ExtensionTrust keys;
  std::unordered_set<std::string> publishers, packages;
  for (const auto &publisher : observation.publishers)
    if (!publishers.insert(publisher.publisher).second ||
        !keys.SetPublisher(publisher.publisher, publisher.public_key))
      return false;
  for (const auto &package : observation.packages) {
    const auto relative = package.relative_path.generic_u8string();
    if (relative.size() > ProjectWorkspace::kMaximumDocumentPathBytes ||
        std::ranges::find(relative, char8_t{}) != relative.end() ||
        !foundation::IsValidUtf8(
            {reinterpret_cast<const char *>(relative.data()), relative.size()}))
      return false;
    if (!SignedExtensionHost::EncodeManifest(package.manifest) ||
        !packages.insert(package.manifest.id + "/" + package.manifest.version).second ||
        package.package_bytes < 84 || package.package_bytes > PluginManager::kMaximumPackageBytes ||
        static_cast<unsigned>(package.state) >
            static_cast<unsigned>(ManagedExtensionState::Rejected) ||
        static_cast<unsigned>(package.error) >
            static_cast<unsigned>(ExtensionAdmissionError::NativeLoadFailed) ||
        static_cast<unsigned>(package.native_error) >
            static_cast<unsigned>(runtime::PluginLoadError::InspectionFailed) ||
        static_cast<unsigned>(package.lifecycle_error) >
            static_cast<unsigned>(runtime::PluginLifecycleError::QuiescenceFailed))
      return false;
  }
  if (observation.review &&
      (!SignedExtensionHost::EncodeManifest(observation.review->manifest) ||
       observation.review->scope != observation.scope || !observation.review->configuration ||
       observation.review->package_bytes < 84 ||
       observation.review->package_bytes > PluginManager::kMaximumPackageBytes))
    return false;
  const bool replaced = !state_->extension_manager ||
                        state_->extension_manager->project != observation.project ||
                        state_->extension_manager->root != observation.root ||
                        state_->extension_manager->scope != observation.scope;
  if (replaced) {
    state_->extension_manager_request.reset();
    state_->extension_package_path = {};
    state_->extension_publisher = {};
    state_->extension_public_key = {};
    state_->extension_manager_status.clear();
  }
  if (replaced || state_->extension_manager->permissions != observation.permissions)
    state_->extension_permission_draft = observation.permissions;
  state_->extension_manager = std::move(observation);
  return true;
}
void EditorImGuiHost::SetExtensionManagerStatus(std::string status) {
  if (status.size() > 1024 || !foundation::IsValidUtf8(status) ||
      std::ranges::any_of(status, [](unsigned char c) { return c < 32 || c == 127; }))
    status = "Extension operation failed; status was unavailable.";
  state_->extension_manager_status = std::move(status);
}
std::optional<ExtensionManagerRequest> EditorImGuiHost::TakeExtensionManagerRequest() {
  return std::exchange(state_->extension_manager_request, {});
}
void EditorImGuiHost::DrawExtensionManager(bool settings_allowed, bool authoring_allowed) {
  Activate(state_->context);
  settings_allowed = settings_allowed && ExtensionManagerInteractionAllowed();
  authoring_allowed = authoring_allowed && settings_allowed;
  state_->extension_positions.clear();
  const auto cancel_closed_review = [&] {
    if (!state_->extension_manager_open && state_->extension_manager &&
        state_->extension_manager->review) {
      ExtensionManagerRequest output;
      output.action = ExtensionManagerAction::CancelReview;
      output.project = state_->extension_manager->project;
      output.root = state_->extension_manager->root;
      output.scope = state_->extension_manager->scope;
      state_->extension_manager_request = std::move(output);
    }
  };
  if (!state_->extension_manager_open) {
    cancel_closed_review();
    return;
  }
  ImGui::SetNextWindowPos({30, 50}, ImGuiCond_FirstUseEver);
  const auto display = ImGui::GetIO().DisplaySize;
  ImGui::SetNextWindowSize({std::min(700.0F, display.x - 60), std::min(700.0F, display.y - 80)},
                           ImGuiCond_FirstUseEver);
  if (!ImGui::Begin("Extensions", &state_->extension_manager_open)) {
    ImGui::End();
    cancel_closed_review();
    return;
  }
  const auto position = [&](std::string key) {
    const auto low = ImGui::GetItemRectMin(), high = ImGui::GetItemRectMax();
    state_->extension_positions.emplace_back(
        std::move(key), std::array{(low.x + high.x) * .5F, (low.y + high.y) * .5F});
  };
  if (!state_->extension_manager || state_->extension_manager->root.empty()) {
    ImGui::TextUnformatted("Open a project to inspect and manage extensions.");
    ImGui::End();
    return;
  }
  const auto &view = *state_->extension_manager;
  const auto request = [&](ExtensionManagerAction action, std::string id = {},
                           std::string version = {}) {
    ExtensionManagerRequest output;
    output.action = action;
    output.project = view.project;
    output.root = view.root;
    output.scope = view.scope;
    output.id = std::move(id);
    output.version = std::move(version);
    return output;
  };
  const bool pending = state_->extension_manager_request.has_value();
  ImGui::TextWrapped("Native extensions run in this process. Enable only publishers you trust.");
  if (view.restart_required)
    ImGui::TextWrapped("Restart required: previous native code cannot safely unload.");
  if (!state_->extension_manager_status.empty())
    ImGui::TextWrapped("%s", state_->extension_manager_status.c_str());
  if (ImGui::BeginTabBar("extension-pages")) {
    const bool trust_tab = ImGui::BeginTabItem("Publisher trust");
    position("trust-tab");
    if (trust_tab) {
      ImGui::BeginDisabled(!settings_allowed || pending);
      ImGui::InputText("Publisher", state_->extension_publisher.data(),
                       state_->extension_publisher.size());
      position("publisher");
      ImGui::InputText("Public key (64 hex digits)", state_->extension_public_key.data(),
                       state_->extension_public_key.size());
      position("public-key");
      if (ImGui::Button("Trust public key")) {
        auto output =
            request(ExtensionManagerAction::SetPublisher, state_->extension_publisher.data());
        const std::string_view text = state_->extension_public_key.data();
        const auto digit = [](char c) {
          return c >= '0' && c <= '9'   ? c - '0'
                 : c >= 'a' && c <= 'f' ? c - 'a' + 10
                 : c >= 'A' && c <= 'F' ? c - 'A' + 10
                                        : -1;
        };
        bool valid = text.size() == 64;
        if (valid)
          for (std::size_t i = 0; i < 32; ++i) {
            const auto a = digit(text[2 * i]), b = digit(text[2 * i + 1]);
            if (a < 0 || b < 0) {
              valid = false;
              break;
            }
            output.public_key[i] = static_cast<std::byte>((a << 4) | b);
          }
        if (valid)
          state_->extension_manager_request = std::move(output);
        else
          state_->extension_manager_status = "Enter exactly 64 hexadecimal public-key digits.";
      }
      position("trust-key");
      ImGui::SeparatorText("Allowed capabilities");
      constexpr std::array labels{"Console",         "Reflection",    "Document read",
                                  "Scene authoring", "Project files", "Network"};
      for (std::size_t i = 0; i < labels.size(); ++i) {
        const auto bit = std::uint32_t{1} << i;
        bool enabled = (state_->extension_permission_draft & bit) != 0;
        if (ImGui::Checkbox(labels[i], &enabled))
          state_->extension_permission_draft = enabled ? state_->extension_permission_draft | bit
                                                       : state_->extension_permission_draft & ~bit;
        position("permission-" + std::to_string(i));
        if (i % 3 != 2)
          ImGui::SameLine();
      }
      if (ImGui::Button("Apply capabilities")) {
        auto output = request(ExtensionManagerAction::SetPermissions);
        output.permissions = state_->extension_permission_draft;
        state_->extension_manager_request = std::move(output);
      }
      position("apply-permissions");
      ImGui::EndDisabled();
      ImGui::TextWrapped("Capabilities govern admission. Trusted native code is not sandboxed.");
      ImGui::BeginChild("trusted-publishers", {0, 120}, ImGuiChildFlags_Borders);
      ImGuiListClipper clipper;
      clipper.Begin(static_cast<int>(view.publishers.size()));
      while (clipper.Step())
        for (int index = clipper.DisplayStart; index < clipper.DisplayEnd; ++index) {
          const auto &publisher = view.publishers[static_cast<std::size_t>(index)];
          ImGui::PushID(index);
          ImGui::BeginDisabled(!settings_allowed || pending);
          if (ImGui::SmallButton("Revoke"))
            state_->extension_manager_request =
                request(ExtensionManagerAction::RevokePublisher, publisher.publisher);
          position(publisher.publisher + "/revoke");
          ImGui::EndDisabled();
          ImGui::SameLine();
          ImGui::TextUnformatted(publisher.publisher.c_str());
          ImGui::PopID();
        }
      ImGui::EndChild();
      ImGui::EndTabItem();
    }
    const bool packages_tab = ImGui::BeginTabItem("Packages");
    position("packages-tab");
    if (packages_tab) {
      ImGui::BeginDisabled(!settings_allowed || pending);
      if (ImGui::InputText("Package path", state_->extension_package_path.data(),
                           state_->extension_package_path.size()) &&
          view.review)
        state_->extension_manager_request = request(ExtensionManagerAction::CancelReview);
      position("package-path");
      if (ImGui::Button("Review package")) {
        auto output = request(ExtensionManagerAction::Review);
        const std::string_view path = state_->extension_package_path.data();
        output.package_path = std::filesystem::path{std::u8string(path.begin(), path.end())};
        state_->extension_manager_request = std::move(output);
      }
      position("review");
      ImGui::SameLine();
      if (ImGui::Button("Refresh installed"))
        state_->extension_manager_request = request(ExtensionManagerAction::Refresh);
      position("refresh");
      ImGui::EndDisabled();
      if (view.review) {
        const auto &review = *view.review;
        ImGui::Text("Verified: %s / %s", review.manifest.id.c_str(),
                    review.manifest.version.c_str());
        ImGui::Text("Publisher: %s  Target: %s  ABI: %u", review.manifest.publisher.c_str(),
                    review.manifest.target.c_str(), review.manifest.engine_abi);
        ImGui::Text("Package: %zu bytes  Dependencies: %zu", review.package_bytes,
                    review.manifest.dependencies.size());
        if (ImGui::TreeNode("Dependency identities")) {
          for (const auto &dependency : review.manifest.dependencies)
            ImGui::TextUnformatted(dependency.c_str());
          ImGui::TreePop();
        }
        ImGui::BeginDisabled(!authoring_allowed || state_->extension_manager_request.has_value());
        if (ImGui::Button("Install verified copy")) {
          auto output = request(ExtensionManagerAction::Install);
          output.configuration = review.configuration;
          state_->extension_manager_request = std::move(output);
        }
        position("install");
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button("Cancel review"))
          state_->extension_manager_request = request(ExtensionManagerAction::CancelReview);
        position("cancel-review");
      }
      ImGui::SeparatorText("Installed packages");
      if (view.rejected_files)
        ImGui::Text("Malformed or foreign packages excluded: %zu", view.rejected_files);
      if (view.packages.empty())
        ImGui::TextUnformatted("No installed packages. Packages never enable automatically.");
      ImGui::BeginChild("installed-extensions", {0, 180}, ImGuiChildFlags_Borders);
      ImGuiListClipper clipper;
      clipper.Begin(static_cast<int>(view.packages.size()), ImGui::GetFrameHeightWithSpacing() * 2);
      while (clipper.Step())
        for (int index = clipper.DisplayStart; index < clipper.DisplayEnd; ++index) {
          const auto &entry = view.packages[static_cast<std::size_t>(index)];
          constexpr std::array states{"Disabled", "Enabled", "Draining", "Restart required",
                                      "Rejected"};
          const auto identity = entry.manifest.id + "/" + entry.manifest.version;
          ImGui::PushID(index);
          ImGui::Text("%s / %s — %s", entry.manifest.id.c_str(), entry.manifest.version.c_str(),
                      states[static_cast<std::size_t>(entry.state)]);
          constexpr std::array admission_errors{"None",
                                                "Invalid manifest",
                                                "Policy denied",
                                                "Untrusted publisher or invalid signature",
                                                "Artifact mismatch",
                                                "Backend unavailable",
                                                "Stale review",
                                                "Budget exhausted",
                                                "Duplicate identity",
                                                "Native load failed"};
          constexpr std::array load_errors{"None",
                                           "Open failed",
                                           "Missing ABI symbol",
                                           "ABI mismatch",
                                           "Invalid lifecycle",
                                           "Registration rejected",
                                           "Budget exhausted",
                                           "Invalid path",
                                           "Inspection failed"};
          constexpr std::array lifecycle_errors{"None", "Legacy module needs restart",
                                                "Stop request failed", "Quiescence failed"};
          ImGui::SetItemTooltip(
              "Admission: %s\nNative loader: %s\nLifecycle: %s (result %d)\nReported ABI: "
              "%u\nRegistered services: %zu\nCooperative unload: %s",
              admission_errors[static_cast<std::size_t>(entry.error)],
              load_errors[static_cast<std::size_t>(entry.native_error)],
              lifecycle_errors[static_cast<std::size_t>(entry.lifecycle_error)],
              entry.lifecycle_result, entry.reported_abi, entry.registered_services,
              entry.cooperative ? "available" : "unavailable");
          ImGui::BeginDisabled(!authoring_allowed || pending || view.restart_required ||
                               (entry.state != ManagedExtensionState::Disabled &&
                                entry.state != ManagedExtensionState::Rejected));
          if (ImGui::SmallButton("Enable"))
            state_->extension_manager_request =
                request(ExtensionManagerAction::Enable, entry.manifest.id, entry.manifest.version);
          position(identity + "/enable");
          ImGui::EndDisabled();
          ImGui::SameLine();
          ImGui::BeginDisabled(!settings_allowed || pending ||
                               (entry.state != ManagedExtensionState::Loaded &&
                                entry.state != ManagedExtensionState::ShutdownPending));
          if (ImGui::SmallButton("Disable"))
            state_->extension_manager_request =
                request(ExtensionManagerAction::Disable, entry.manifest.id, entry.manifest.version);
          position(identity + "/disable");
          ImGui::EndDisabled();
          ImGui::SameLine();
          ImGui::BeginDisabled(!authoring_allowed || pending ||
                               entry.state == ManagedExtensionState::Loaded ||
                               entry.state == ManagedExtensionState::ShutdownPending ||
                               entry.state == ManagedExtensionState::RestartRequired);
          if (ImGui::SmallButton("Remove copy"))
            state_->extension_manager_request =
                request(ExtensionManagerAction::Remove, entry.manifest.id, entry.manifest.version);
          position(identity + "/remove");
          ImGui::EndDisabled();
          ImGui::PopID();
        }
      ImGui::EndChild();
      ImGui::EndTabItem();
    }
    ImGui::EndTabBar();
  }
  ImGui::End();
  cancel_closed_review();
}
bool EditorImGuiHost::ExtensionManagerInteractionAllowed() const noexcept {
  return state_->app_focused && !state_->extension_input_blocked &&
         state_->play_command == PlayCommand::None && !state_->play_apply_open &&
         !state_->close_prompt_requested && state_->close_choice == CloseChoice::None &&
         state_->scene_file_dialog == State::FileDialog::None && !state_->scene_file_output &&
         !state_->scene_tab_dialog && !state_->scene_tab_output &&
         !state_->hierarchy_rename_target && !state_->content_rename_target &&
         !state_->game_input_binding_open;
}
