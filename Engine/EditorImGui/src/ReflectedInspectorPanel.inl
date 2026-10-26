// Included in the serialized graphical owner implementation; no filesystem or plugin calls.
std::string ReflectedText(const ReflectedValue &value, ReflectedKind kind) {
  std::ostringstream output;
  output.imbue(std::locale::classic());
  output << std::setprecision(17);
  std::visit(
      [&](const auto &item) {
        using T = std::decay_t<decltype(item)>;
        if constexpr (std::is_same_v<T, foundation::Uuid>)
          output << item.ToString();
        else if constexpr (std::is_same_v<T, std::array<double, 4>>) {
          const std::size_t lanes = kind == ReflectedKind::Vector2   ? 2
                                    : kind == ReflectedKind::Vector3 ? 3
                                                                     : 4;
          for (std::size_t i = 0; i < lanes; ++i) {
            if (i)
              output << ", ";
            output << item[i];
          }
        } else
          output << item;
      },
      value);
  return output.str();
}
std::optional<ReflectedValue> ParseReflectedText(std::string_view text, ReflectedKind kind) {
  if (kind == ReflectedKind::AssetReference) {
    const auto parsed = foundation::Uuid::Parse(text);
    return parsed ? std::optional<ReflectedValue>{parsed.Value()} : std::nullopt;
  }
  if (kind == ReflectedKind::Vector2 || kind == ReflectedKind::Vector3 ||
      kind == ReflectedKind::Vector4 || kind == ReflectedKind::Color) {
    std::string copy(text);
    std::ranges::replace(copy, ',', ' ');
    std::istringstream input(copy);
    input.imbue(std::locale::classic());
    std::array<double, 4> lanes{};
    const std::size_t count = kind == ReflectedKind::Vector2   ? 2
                              : kind == ReflectedKind::Vector3 ? 3
                                                               : 4;
    for (std::size_t i = 0; i < count; ++i)
      if (!(input >> lanes[i]) || !std::isfinite(lanes[i]))
        return {};
    std::string extra;
    if (input >> extra)
      return {};
    return ReflectedValue{lanes};
  }
  const auto parse = [&](auto &number) {
    const auto result = std::from_chars(text.data(), text.data() + text.size(), number);
    return result.ec == std::errc{} && result.ptr == text.data() + text.size();
  };
  if (kind == ReflectedKind::Number) {
    double number{};
    if (parse(number) && std::isfinite(number))
      return ReflectedValue{number};
  } else if (kind == ReflectedKind::Integer) {
    std::int64_t number{};
    if (parse(number))
      return ReflectedValue{number};
  } else {
    std::uint64_t number{};
    if (parse(number))
      return ReflectedValue{number};
  }
  return {};
}
ReflectedValue InitialReflectedArrayValue(const ReflectedDynamicArray &array) {
  switch (array.kind) {
  case ReflectedKind::Boolean:
    return false;
  case ReflectedKind::Integer:
    return std::int64_t{};
  case ReflectedKind::Number:
    return 0.;
  case ReflectedKind::Enum:
    return array.choices.front().value;
  case ReflectedKind::Color:
    return std::array<double, 4>{0, 0, 0, 1};
  case ReflectedKind::Vector2:
  case ReflectedKind::Vector3:
  case ReflectedKind::Vector4:
    return std::array<double, 4>{};
  case ReflectedKind::AssetReference:
    return foundation::Uuid{};
  default:
    return std::uint64_t{};
  }
}
template <typename StateT>
void DrawReflectedInspector(StateT &state, SceneDocument &scene,
                            std::span<const SceneDocument::NodeKey> keys, bool editable) {
  state.reflected_positions.clear();
  state.reflected_choice_positions.clear();
  state.reflected_mixed.clear();
  state.reflected_array_controls.clear();
  if (detail::SmallIconButton("Reload property metadata", detail::EditorIcon::Reload))
    state.reflected_metadata_reload = true;
  if (keys.size() > ReflectedInspector::kMaximumTargets) {
    ImGui::TextDisabled("Reflected properties require at most %zu selected entities.",
                        ReflectedInspector::kMaximumTargets);
    return;
  }
  std::uint64_t identity = state.reflected_inspector.Revision() ^ state.reflected_draft_generation;
  for (const auto key : keys)
    for (const auto part : {key.id, key.entity_generation, key.document_generation})
      identity = (identity ^ part) * 1099511628211ULL;
  if (state.reflected_pending &&
      (state.reflected_pending_identity != identity || !editable || !state.app_focused))
    state.reflected_pending.reset();
  ImGui::PushID(static_cast<int>(identity));
  ImGui::PushID(static_cast<int>(identity >> 32U));
  for (const auto &component : state.reflected_inspector.Components()) {
    const auto observation = state.reflected_inspector.Inspect(scene, keys, component.type);
    if (!observation)
      continue;
    state.reflected_rendered_types.push_back(component.type);
    ImGui::PushID(static_cast<int>(component.type));
    ImGui::PushID(static_cast<int>(component.type >> 32U));
    ImGui::TextUnformatted(component.name.c_str());
    bool published = false;
    const auto control_position = [&](std::size_t array, std::string action,
                                      std::optional<std::size_t> element = {}) {
      const auto low = ImGui::GetItemRectMin(), high = ImGui::GetItemRectMax();
      state.reflected_array_controls.push_back({component.type,
                                                component.arrays[array].path,
                                                std::move(action),
                                                element,
                                                {(low.x + high.x) * .5F, (low.y + high.y) * .5F}});
    };
    const auto publish_array = [&](const ReflectedArrayEdit &edit) {
      const bool accepted =
          state.reflected_inspector.ApplyArray(scene, *observation, edit, editable);
      if (accepted)
        state.inspector_error.clear();
      else
        state.inspector_error = "Array edit rejected; selection, capacity or source bytes changed.";
      state.reflected_pending.reset();
      ++state.reflected_draft_generation;
      return accepted;
    };
    for (std::size_t array_index = 0; array_index < component.arrays.size(); ++array_index) {
      const auto &array = component.arrays[array_index];
      const auto &view = observation->arrays[array_index];
      const auto [minimum, maximum] = std::minmax_element(view.counts.begin(), view.counts.end());
      ImGui::PushID("dynamic-array");
      ImGui::PushID(static_cast<int>(array_index));
      ImGui::TextUnformatted(array.path.c_str());
      ImGui::SameLine();
      if (view.mixed)
        ImGui::TextDisabled("Mixed lengths (%zu..%zu)", *minimum, *maximum);
      else
        ImGui::TextDisabled("%zu items", *minimum);
      ImGui::BeginDisabled(!editable || *maximum == array.maximum_elements);
      if (detail::IconButton("Add", detail::EditorIcon::Add))
        published = publish_array(
            {ReflectedArrayAction::Append, array_index, 0, 0, InitialReflectedArrayValue(array)});
      control_position(array_index, "Add");
      if (*maximum == array.maximum_elements &&
          ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetTooltip("At least one selected array has reached its %zu item limit.",
                          array.maximum_elements);
      ImGui::EndDisabled();
      ImGui::SameLine();
      ImGui::BeginDisabled(!editable || !*maximum || published);
      if (detail::IconButton("Clear", detail::EditorIcon::Clear))
        published = publish_array({ReflectedArrayAction::Clear, array_index});
      control_position(array_index, "Clear");
      ImGui::EndDisabled();
      ImGui::PopID();
      ImGui::PopID();
      if (published)
        break;
    }
    if (published) {
      ImGui::PopID();
      ImGui::PopID();
      continue;
    }
    ImGui::BeginChild("reflected-properties", {0, 180}, ImGuiChildFlags_Borders);
    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(observation->properties.size()),
                  ImGui::GetFrameHeightWithSpacing() * (component.arrays.empty() ? 1 : 2));
    while (clipper.Step() && !published)
      for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
        const auto index = static_cast<std::size_t>(row);
        const auto &property = observation->properties[index];
        ImGui::PushID(row);
        ImGui::TextUnformatted(property.path.c_str());
        ImGui::SameLine(160);
        if (property.mixed) {
          ImGui::TextDisabled("Mixed");
          ImGui::SameLine();
        }
        state.reflected_mixed.emplace_back(property.path, property.mixed);
        auto value = property.value;
        bool changed = false;
        std::optional<std::pair<std::uint64_t, bool>> flag_edit;
        ImGui::SetNextItemWidth(180);
        ImGui::BeginDisabled(!editable);
        constexpr auto commit = ImGuiInputTextFlags_EnterReturnsTrue;
        switch (property.kind) {
        case ReflectedKind::Boolean:
          changed = ImGui::Checkbox("##value", &std::get<bool>(value));
          break;
        case ReflectedKind::Enum: {
          auto &number = std::get<std::uint64_t>(value);
          const auto selected =
              std::ranges::find(property.choices, number, &ReflectedChoice::value);
          if (ImGui::BeginCombo("##value", selected->label.c_str())) {
            for (const auto &choice : property.choices) {
              if (ImGui::Selectable(choice.label.c_str(), choice.value == number)) {
                number = choice.value;
                changed = true;
              }
              const auto low = ImGui::GetItemRectMin(), high = ImGui::GetItemRectMax();
              state.reflected_choice_positions.emplace_back(
                  property.path + "/" + choice.label,
                  std::array{(low.x + high.x) * .5F, (low.y + high.y) * .5F});
            }
            ImGui::EndCombo();
          }
          break;
        }
        case ReflectedKind::Flags: {
          auto &number = std::get<std::uint64_t>(value);
          const auto label = std::to_string(number);
          if (ImGui::BeginCombo("##value", label.c_str())) {
            for (const auto &choice : property.choices) {
              bool enabled = (number & choice.value) != 0;
              if (ImGui::Checkbox(choice.label.c_str(), &enabled)) {
                flag_edit = std::pair{choice.value, enabled};
                changed = true;
              }
              const auto low = ImGui::GetItemRectMin(), high = ImGui::GetItemRectMax();
              state.reflected_choice_positions.emplace_back(
                  property.path + "/" + choice.label,
                  std::array{(low.x + high.x) * .5F, (low.y + high.y) * .5F});
            }
            ImGui::EndCombo();
          }
          break;
        }
        default: {
          std::array<char, 512> initial{};
          const auto canonical = ReflectedText(value, property.kind);
          std::copy(canonical.begin(), canonical.end(), initial.begin());
          const bool pending = state.reflected_pending &&
                               state.reflected_pending->component.type == component.type &&
                               state.reflected_pending_path == property.path;
          auto &text = pending ? state.reflected_pending_text : initial;
          const bool submit = ImGui::InputText("##value", text.data(), text.size(), commit);
          if (ImGui::IsItemActivated()) {
            state.reflected_pending = *observation;
            state.reflected_pending_path = property.path;
            state.reflected_pending_identity = identity;
            state.reflected_pending_text = text;
          }
          if (submit) {
            const auto parsed = ParseReflectedText(text.data(), property.kind);
            if (parsed && state.reflected_pending) {
              value = *parsed;
              changed = true;
            } else
              state.inspector_error = "Invalid reflected value; component bytes preserved.";
          } else if (ImGui::IsItemDeactivated() && pending)
            state.reflected_pending.reset();
          break;
        }
        }
        const auto begin = ImGui::GetItemRectMin(), end = ImGui::GetItemRectMax();
        state.reflected_positions.emplace_back(
            property.path, std::array{(begin.x + end.x) * .5F, (begin.y + end.y) * .5F});
        ImGui::EndDisabled();
        std::optional<ReflectedArrayEdit> array_edit;
        if (!component.arrays.empty()) {
          if (property.array) {
            const auto array = *property.array;
            const auto element = property.element;
            const auto &schema = component.arrays[array];
            const auto &view = observation->arrays[array];
            const auto maximum = *std::max_element(view.counts.begin(), view.counts.end());
            ImGui::BeginDisabled(!editable || changed);
            ImGui::BeginDisabled(maximum == schema.maximum_elements);
            if (detail::IconButton("Insert", detail::EditorIcon::Add))
              array_edit = ReflectedArrayEdit{ReflectedArrayAction::Insert, array, element, 0,
                                              InitialReflectedArrayValue(schema)};
            control_position(array, "Insert", element);
            ImGui::EndDisabled();
            ImGui::SameLine();
            if (detail::IconButton("Remove", detail::EditorIcon::Delete))
              array_edit = ReflectedArrayEdit{ReflectedArrayAction::Remove, array, element};
            control_position(array, "Remove", element);
            ImGui::SameLine();
            ImGui::BeginDisabled(element == 0);
            if (detail::IconButton("Up", detail::EditorIcon::Up))
              array_edit =
                  ReflectedArrayEdit{ReflectedArrayAction::Move, array, element, element - 1};
            control_position(array, "Up", element);
            ImGui::EndDisabled();
            ImGui::SameLine();
            ImGui::BeginDisabled(element + 1 >= view.common_elements);
            if (detail::IconButton("Down", detail::EditorIcon::Down))
              array_edit =
                  ReflectedArrayEdit{ReflectedArrayAction::Move, array, element, element + 1};
            control_position(array, "Down", element);
            ImGui::EndDisabled();
            ImGui::EndDisabled();
          } else
            ImGui::Dummy({0, ImGui::GetFrameHeight()});
        }
        if (array_edit)
          published = publish_array(*array_edit);
        if (changed) {
          const auto &source = state.reflected_pending &&
                                       state.reflected_pending_path == property.path &&
                                       state.reflected_pending->component.type == component.type
                                   ? *state.reflected_pending
                                   : *observation;
          const bool accepted =
              flag_edit ? state.reflected_inspector.ApplyFlag(
                              scene, source, index, flag_edit->first, flag_edit->second, editable)
                        : state.reflected_inspector.Apply(scene, source, index, value, editable);
          if (accepted) {
            state.inspector_error.clear();
            published = true;
          } else
            state.inspector_error = "Reflected edit rejected; selection or source bytes changed.";
        }
        if (changed) {
          state.reflected_pending.reset();
          ++state.reflected_draft_generation;
        }
        ImGui::PopID();
        if (published)
          break;
      }
    ImGui::EndChild();
    ImGui::PopID();
    ImGui::PopID();
  }
  if (!ImGui::IsAnyItemActive())
    state.reflected_pending.reset();
  ImGui::PopID();
  ImGui::PopID();
}
