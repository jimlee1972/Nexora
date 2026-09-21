#include "Nexora/Runtime/Shipping.h"

#include <iostream>
#include <stdexcept>
#include <string>

namespace {
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}

bool Has(const nexora::runtime::shipping::PackageManifest &manifest, std::string_view path) {
  for (const auto &artifact : manifest.artifacts)
    if (artifact.package_path == path)
      return true;
  return false;
}

int RunTests() {
  using namespace nexora::runtime::shipping;
  const std::vector artifacts{
      Artifact{"base", "assets/base.bundle", "a1", ArtifactKind::Asset, 100, false},
      Artifact{"demo", "assets/demo.bundle", "a2", ArtifactKind::Asset, 20, true},
      Artifact{"game", "plugins/game.plugin", "p1", ArtifactKind::Plugin, 30, false},
      Artifact{"editor", "plugins/editor.plugin", "p2", ArtifactKind::Plugin, 40, false},
      Artifact{"lit", "shaders/lit.shader", "s1", ArtifactKind::Shader, 50, false},
      Artifact{"debug", "shaders/debug.shader", "s2", ArtifactKind::Shader, 60, false},
      Artifact{"headers", "sdk/runtime.h", "d1", ArtifactKind::Sdk, 10, false},
      Artifact{"ui", "presentation/ui.bundle", "u1", ArtifactKind::Presentation, 70, false},
  };

  Packager packager;
  PackageRequest minimal{Profile::Minimal, artifacts, {"game"}, {"lit"}, false};
  const auto minimal_result = packager.Build(minimal);
  Require(minimal_result && minimal_result.manifest.presentation &&
              minimal_result.manifest.artifacts.size() == 4 &&
              Has(minimal_result.manifest, "plugins/game.plugin") &&
              !Has(minimal_result.manifest, "plugins/editor.plugin") &&
              !Has(minimal_result.manifest, "assets/demo.bundle") &&
              !Has(minimal_result.manifest, "sdk/runtime.h"),
          "minimal profile did not strip optional, plugin, shader, and SDK artifacts");

  minimal.profile = Profile::Dedicated;
  const auto dedicated = packager.Build(minimal);
  Require(dedicated && !dedicated.manifest.presentation &&
              !Has(dedicated.manifest, "presentation/ui.bundle") &&
              !Has(dedicated.manifest, "shaders/lit.shader"),
          "dedicated profile retained presentation baggage");

  PackageRequest full{Profile::Full, artifacts, {"game", "editor"}, {"lit", "debug"}, true};
  const auto complete = packager.Build(full);
  Require(complete && complete.manifest.artifacts.size() == artifacts.size(),
          "full profile did not retain requested artifacts");

  auto invalid = full;
  invalid.artifacts.front().package_path = "../escape.bundle";
  Require(packager.Build(invalid).error == PackageError::UnsafePath,
          "packager accepted an escaping output path");
  invalid = full;
  invalid.artifacts.back().package_path = invalid.artifacts.front().package_path;
  Require(packager.Build(invalid).error == PackageError::DuplicatePath,
          "packager accepted colliding output paths");

  BundleUpdater updates({1, "generation-one"});
  Require(!updates.Stage({1, "stale"}) && !updates.Stage({2, ""}) &&
              updates.Stage({2, "generation-two"}) && updates.Activate() &&
              updates.Restart().generation == 2 && updates.Rollback() &&
              updates.Restart().generation == 1 && updates.Stage({3, "generation-three"}) &&
              updates.Activate() && updates.Confirm() && !updates.Rollback(),
          "verified update, restart, rollback, or confirmation contract failed");

  CrashReporter reporter{2};
  const auto report = reporter.Capture({"build-1", "linux", "fatal", {"a", "b", "c"}});
  Require(report && report->truncated && report->breadcrumbs.size() == 2 &&
              report->breadcrumbs.front() == "b" && !reporter.Capture({"", "linux", "fatal", {}}),
          "bounded crash report contract failed");

  SoakMonitor stable{16, 2};
  for (std::uint64_t frame = 0; frame < 10000; ++frame)
    Require(stable.Sample({frame, 1024 + frame % 8, 100 + frame % 2}),
            "soak monitor rejected an ordered sample");
  Require(!stable.Result().suspected_leak && stable.Result().samples == 10000,
          "stable soak run reported a leak");
  SoakMonitor leaking{16, 2};
  Require(leaking.Sample({1, 100, 10}) && leaking.Sample({2, 117, 10}) &&
              leaking.Result().suspected_leak && !leaking.Sample({2, 100, 10}),
          "soak growth or ordering failure was not detected");

  DeviceMatrix matrix;
  Require(matrix.RecordStartup(ShippingPlatform::Windows, Profile::Minimal) &&
              matrix.RecordStartup(ShippingPlatform::MacOS, Profile::Minimal) &&
              matrix.RecordStartup(ShippingPlatform::Android, Profile::Minimal) &&
              !matrix.Passed(Profile::Minimal) &&
              matrix.RecordStartup(ShippingPlatform::IOS, Profile::Minimal) &&
              matrix.Passed(Profile::Minimal) &&
              !matrix.RecordStartup(ShippingPlatform::IOS, Profile::Minimal),
          "four-platform shipping startup matrix contract failed");

  PackageRequest large{Profile::Minimal, {}, {}, {}, false};
  large.artifacts.reserve(10000);
  for (std::size_t index = 0; index < 10000; ++index)
    large.artifacts.push_back({"asset-" + std::to_string(index),
                               "assets/" + std::to_string(index) + ".bundle", "digest",
                               ArtifactKind::Asset, 1, false});
  const auto baseline = packager.Build(large);
  Require(baseline && baseline.manifest.artifacts.size() == 10000 &&
              baseline.manifest.total_bytes == 10000,
          "10,000-artifact packaging performance baseline failed");
  return 0;
}
} // namespace

int main() {
  try {
    return RunTests();
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
