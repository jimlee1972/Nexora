// Test-only independent command oracle; never derives expected values from the authored World.
constexpr std::size_t kReplayTransactions = 1000;
struct ReplayExpected final {
  std::array<runtime::Transform, 2> poses{Pose(false, false), Pose(true, false)};
  std::optional<editor::OpaqueComponent> metadata;
};
std::vector<ReplayExpected> ReplayOracle() {
  std::vector<ReplayExpected> result;
  result.reserve(kReplayTransactions + 1);
  ReplayExpected expected;
  result.push_back(expected);
  expected.poses[0].x = expected.poses[1].x = -12.5;
  result.push_back(expected); // The first transaction is actual Inspector typing/Enter.
  for (std::size_t step = 1; step < kReplayTransactions; ++step) {
    if (step % 3 == 0)
      expected.metadata = editor::OpaqueComponent{
          0x5555666677778888ULL,
          "Replay.受控 %s",
          {1, static_cast<std::uint8_t>(step & 255), static_cast<std::uint8_t>(step >> 8), 255, 0}};
    else {
      expected.poses[0].x = static_cast<double>(step) * .125;
      expected.poses[0].y = 2 + static_cast<double>(step % 11) * .5;
      expected.poses[1].x = -static_cast<double>(step) * .25;
      expected.poses[1].z = 6 + static_cast<double>(step % 7) * .25;
    }
    result.push_back(expected);
  }
  return result;
}
void VerifyReplay(const Fixture &fixture, const ReplayExpected &expected) {
  const auto unknown = Opaque();
  for (std::size_t index = 0; index < expected.poses.size(); ++index) {
    const auto entity = fixture.Find(index ? "Second" : "First");
    Require(fixture.scene.Transform(entity) == expected.poses[index],
            "Mixed replay diverged from independent poses/negative scale oracle");
    const auto components = fixture.scene.OpaqueComponents(*fixture.scene.Key(entity));
    Require(components && components->size() == (expected.metadata ? 2U : 1U),
            "Mixed replay lost/duplicated opaque component ownership");
    const auto original =
        std::ranges::find(*components, unknown.type, &editor::OpaqueComponent::type);
    Require(original != components->end() && *original == unknown,
            "Mixed replay changed unknown component identity/raw bytes");
    if (expected.metadata) {
      const auto payload =
          std::ranges::find(*components, expected.metadata->type, &editor::OpaqueComponent::type);
      Require(payload != components->end() && *payload == *expected.metadata,
              "Mixed replay diverged from independent opaque payload oracle");
    }
  }
}
void WriteReplayPhase(const std::filesystem::path &root, float scale) {
  Fixture fixture(root, scale, true);
  // Production reopen clears fixture-seeding history before the counted command sequence.
  Require(fixture.files->Open(fixture.files->Token(), Fixture::kScene).Applied(),
          "Replay baseline could not reset seed history through production Open");
  fixture.Draw();
  const auto baseline = Read(root / Fixture::kScene);
  const auto oracle = ReplayOracle();
  fixture.MultiSelect();
  fixture.Draft("-12.5");
  fixture.Tap(Key::Enter);
  VerifyReplay(fixture, oracle[1]);
  const std::array keys{*fixture.scene.Key(fixture.Find("First")),
                        *fixture.scene.Key(fixture.Find("Second"))};
  for (std::size_t step = 1; step < kReplayTransactions; ++step) {
    const auto &expected = oracle[step + 1];
    const bool applied = step % 3 == 0 ? fixture.scene.SetOpaqueComponents(keys, *expected.metadata)
                                       : fixture.scene.SetTransforms(keys, expected.poses);
    Require(applied, "Counted mixed authoring transaction was rejected/no-op");
    VerifyReplay(fixture, expected);
    if (step % 31 == 0)
      fixture.Draw();
  }
  Require(fixture.scene.Dirty() && Read(root / Fixture::kScene) == baseline,
          "Command recording wrote source or lost dirty state");
  fixture.HierarchyFocus();
  for (std::size_t step = kReplayTransactions; step > 0; --step) {
    fixture.Tap(Key::Z, Mods::Control);
    VerifyReplay(fixture, oracle[step - 1]);
    Require(fixture.scene.Selection().size() == 2, "History replay lost the multi-selection");
  }
  Require(!fixture.scene.Dirty() && Read(root / Fixture::kScene) == baseline,
          "1000 actual UI Undo operations did not restore the saved baseline");
  fixture.Tap(Key::Z, Mods::Control);
  VerifyReplay(fixture, oracle.front());
  Require(!fixture.scene.Dirty(), "No-op UI Undo changed the baseline/history");
  for (std::size_t step = 1; step <= kReplayTransactions; ++step) {
    fixture.Tap(Key::Y, Mods::Control);
    VerifyReplay(fixture, oracle[step]);
    Require(fixture.scene.Selection().size() == 2, "UI Redo lost the multi-selection");
  }
  fixture.Tap(Key::Y, Mods::Control);
  VerifyReplay(fixture, oracle.back());
  Require(fixture.scene.Dirty() && Read(root / Fixture::kScene) == baseline,
          "1000 UI Redo operations wrote source or lost dirty state");
  const auto token = fixture.files->Token();
  fixture.Tap(Key::S, Mods::Control);
  Require(fixture.ui.TakeSceneSaveRequest() && fixture.files->Save(token).Applied() &&
              !fixture.scene.Dirty(),
          "Replay Save shortcut did not publish through production file authority");
  const auto saved = Read(root / Fixture::kScene);
  Require(saved != baseline, "Replay Save failed to publish the final authored state");
  fixture.Tap(Key::Z, Mods::Control);
  VerifyReplay(fixture, oracle[kReplayTransactions - 1]);
  Require(fixture.scene.Dirty() && Read(root / Fixture::kScene) == saved,
          "Undo after Save changed source or lost the saved-baseline distinction");
  fixture.Tap(Key::Y, Mods::Control);
  VerifyReplay(fixture, oracle.back());
  Require(!fixture.scene.Dirty() && Read(root / Fixture::kScene) == saved,
          "Redo after Save failed to restore exact saved state without another write");
  std::cout << "1000 mixed transactions / 1000 UI Undo / 1000 UI Redo and Save passed\n";
}
void ReadReplayPhase(const std::filesystem::path &root, float scale) {
  Fixture fixture(root, scale, false);
  const auto oracle = ReplayOracle();
  VerifyReplay(fixture, oracle.back());
  Require(!fixture.scene.Dirty(), "Fresh replay process did not reopen a clean saved state");
  const auto saved = Read(root / Fixture::kScene);
  fixture.MultiSelect();
  Require(Access::InspectorTransformMixed(fixture.ui)[0],
          "Fresh-process actual Inspector lost the final mixed X values");
  fixture.HierarchyFocus();
  fixture.Tap(Key::Z, Mods::Control);
  fixture.Tap(Key::Y, Mods::Control);
  VerifyReplay(fixture, oracle.back());
  Require(!fixture.scene.Dirty() && Read(root / Fixture::kScene) == saved,
          "Fresh-process empty history controls changed saved state/source");
}
