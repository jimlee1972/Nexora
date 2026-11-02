template <typename StateT> void ApplySceneToolKeys(StateT &state) {
  if (ImGui::IsKeyPressed(ImGuiKey_Q, false))
    state.native_scene_tool = NativeSceneTool::Hand;
  else if (ImGui::IsKeyPressed(ImGuiKey_W, false))
    state.native_scene_tool = NativeSceneTool::Move;
  else if (ImGui::IsKeyPressed(ImGuiKey_E, false))
    state.native_scene_tool = NativeSceneTool::Rotate;
  else if (ImGui::IsKeyPressed(ImGuiKey_R, false)) {
    state.native_scene_tool = NativeSceneTool::Scale;
    state.native_scene_local_axes = true;
  }
  if (state.native_scene_tool != NativeSceneTool::Scale && ImGui::IsKeyPressed(ImGuiKey_X, false))
    state.native_scene_local_axes = !state.native_scene_local_axes;
  if (ImGui::IsKeyPressed(ImGuiKey_Z, false) || ImGui::IsKeyPressed(ImGuiKey_P, false))
    state.native_scene_center_pivot = !state.native_scene_center_pivot;
}

// View navigation changes only the Editor camera, never the authoring World or history.
template <typename StateT> void NavigateNativeScene(StateT &state) {
  const auto &io = ImGui::GetIO();
  const bool active = ImGui::IsItemActive();
  const bool left = active && ImGui::IsMouseDown(ImGuiMouseButton_Left);
  const bool right = active && ImGui::IsMouseDown(ImGuiMouseButton_Right);
  const bool middle = active && ImGui::IsMouseDown(ImGuiMouseButton_Middle);
  const bool fly = right && !io.KeyAlt;
  const bool pan =
      middle || (left && ((io.KeyAlt && io.KeyCtrl) ||
                          (!io.KeyAlt && (state.native_scene_tool == NativeSceneTool::Hand ||
                                          ImGui::IsKeyDown(ImGuiKey_Space)))));
  const bool orbit = left && io.KeyAlt && !io.KeyCtrl;
  const bool zoom = right && io.KeyAlt;
  auto &camera = state.native_scene_orbit;
  if (io.MouseWheel != 0.0F) {
    if (fly)
      state.native_scene_fly_speed =
          std::clamp(state.native_scene_fly_speed * std::pow(1.2, io.MouseWheel), 0.1, 1000.0);
    else
      camera.distance = std::clamp(camera.distance * std::pow(0.85, io.MouseWheel), 2.0, 100.0);
  }
  const auto translate = [&](double x, double y, double z) {
    state.scene_center_world.x = static_cast<float>(
        std::clamp(static_cast<double>(state.scene_center_world.x) + x, -100000.0, 100000.0));
    camera.target_y = std::clamp(camera.target_y + y, -100000.0, 100000.0);
    state.scene_center_world.y = static_cast<float>(
        std::clamp(static_cast<double>(state.scene_center_world.y) + z, -100000.0, 100000.0));
  };
  if (orbit || fly) {
    const auto old_yaw = camera.yaw, old_pitch = camera.pitch;
    camera.yaw = std::remainder(camera.yaw - io.MouseDelta.x * 0.005, 6.283185307179586);
    camera.pitch = std::clamp(camera.pitch + io.MouseDelta.y * 0.005, -1.55, 1.55);
    // Orbit keeps the target fixed; free look keeps the eye fixed.
    if (fly)
      translate(camera.distance * (std::sin(old_yaw) * std::cos(old_pitch) -
                                   std::sin(camera.yaw) * std::cos(camera.pitch)),
                camera.distance * (std::sin(old_pitch) - std::sin(camera.pitch)),
                camera.distance * (std::cos(old_yaw) * std::cos(old_pitch) -
                                   std::cos(camera.yaw) * std::cos(camera.pitch)));
  }
  if (pan) {
    const auto speed = camera.distance * 0.003;
    const double dx = -io.MouseDelta.x * speed, dy = io.MouseDelta.y * speed;
    translate(std::cos(camera.yaw) * dx - std::sin(camera.yaw) * std::sin(camera.pitch) * dy,
              std::cos(camera.pitch) * dy,
              -std::sin(camera.yaw) * dx - std::cos(camera.yaw) * std::sin(camera.pitch) * dy);
  }
  if (zoom)
    camera.distance = std::clamp(
        camera.distance * std::exp((io.MouseDelta.x + io.MouseDelta.y) * 0.01), 2.0, 100.0);
  // Keyboard navigation belongs to the focused Scene window; RMB captures fly input.
  if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && !io.KeyCtrl && !io.KeyAlt &&
      !io.KeySuper) {
    const auto held = [](ImGuiKey key) { return ImGui::IsKeyDown(key) ? 1.0 : 0.0; };
    const auto forward = held(ImGuiKey_UpArrow) - held(ImGuiKey_DownArrow) +
                         (fly ? held(ImGuiKey_W) - held(ImGuiKey_S) : 0.0);
    const auto sideways = held(ImGuiKey_RightArrow) - held(ImGuiKey_LeftArrow) +
                          (fly ? held(ImGuiKey_D) - held(ImGuiKey_A) : 0.0);
    const auto vertical = fly ? held(ImGuiKey_E) - held(ImGuiKey_Q) : 0.0;
    const auto length = std::sqrt(forward * forward + sideways * sideways + vertical * vertical);
    if (length > 0.0) {
      const auto step = state.native_scene_fly_speed * (io.KeyShift ? 4.0 : 1.0) *
                        std::clamp(static_cast<double>(io.DeltaTime), 0.0, 0.1) / length;
      translate((-std::sin(camera.yaw) * std::cos(camera.pitch) * forward +
                 std::cos(camera.yaw) * sideways) *
                    step,
                (-std::sin(camera.pitch) * forward + vertical) * step,
                (-std::cos(camera.yaw) * std::cos(camera.pitch) * forward -
                 std::sin(camera.yaw) * sideways) *
                    step);
    }
  }
}
