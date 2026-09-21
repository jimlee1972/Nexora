#include "Nexora/Runtime/Shipping.h"

#include <algorithm>
#include <array>
#include <limits>
#include <unordered_set>

namespace nexora::runtime::shipping {
namespace {
bool Contains(std::span<const std::string> values, std::string_view value) {
  return std::ranges::find(values, value) != values.end();
}

bool SafeRelativePath(std::string_view path) {
  if (path.empty() || path.front() == '/' || path.front() == '\\' || path.find('\\') != path.npos)
    return false;
  std::size_t begin = 0;
  while (begin <= path.size()) {
    const auto end = path.find('/', begin);
    const auto component = path.substr(begin, end == path.npos ? path.size() - begin : end - begin);
    if (component.empty() || component == "." || component == "..")
      return false;
    if (end == path.npos)
      break;
    begin = end + 1;
  }
  return true;
}

PackageResult Failure(Profile profile, bool presentation, PackageError error, std::string detail) {
  return {{profile, {}, 0, presentation}, error, std::move(detail)};
}
} // namespace

PackageResult Packager::Build(const PackageRequest &request) const {
  PackageResult result;
  result.manifest.profile = request.profile;
  result.manifest.presentation = request.profile != Profile::Dedicated;
  std::unordered_set<std::string> paths;

  for (const auto &artifact : request.artifacts) {
    if (artifact.name.empty() || artifact.digest.empty()) {
      return Failure(request.profile, result.manifest.presentation, PackageError::InvalidArtifact,
                     artifact.package_path);
    }
    if (!SafeRelativePath(artifact.package_path)) {
      return Failure(request.profile, result.manifest.presentation, PackageError::UnsafePath,
                     artifact.package_path);
    }
    if (!paths.insert(artifact.package_path).second) {
      return Failure(request.profile, result.manifest.presentation, PackageError::DuplicatePath,
                     artifact.package_path);
    }

    const bool stripped =
        (artifact.kind == ArtifactKind::Plugin &&
         !Contains(request.enabled_plugins, artifact.name)) ||
        (artifact.kind == ArtifactKind::Shader &&
         (!result.manifest.presentation ||
          !Contains(request.enabled_shader_families, artifact.name))) ||
        (artifact.kind == ArtifactKind::Presentation && !result.manifest.presentation) ||
        (artifact.kind == ArtifactKind::Sdk && !request.include_sdk) ||
        (request.profile == Profile::Minimal && artifact.optional);
    if (!stripped) {
      if (artifact.bytes >
          std::numeric_limits<std::uint64_t>::max() - result.manifest.total_bytes) {
        return Failure(request.profile, result.manifest.presentation, PackageError::InvalidArtifact,
                       artifact.package_path);
      }
      result.manifest.total_bytes += artifact.bytes;
      result.manifest.artifacts.push_back(artifact);
    }
  }
  std::ranges::sort(result.manifest.artifacts, {}, &Artifact::package_path);
  return result;
}

BundleUpdater::BundleUpdater(BundleGeneration installed) : current_(std::move(installed)) {}

bool BundleUpdater::Stage(BundleGeneration candidate) {
  if (staged_ || previous_ || candidate.generation <= current_.generation ||
      candidate.digest.empty())
    return false;
  staged_ = std::move(candidate);
  return true;
}

bool BundleUpdater::Activate() {
  if (!staged_)
    return false;
  previous_ = current_;
  current_ = std::move(*staged_);
  staged_.reset();
  return true;
}

bool BundleUpdater::Confirm() {
  if (!previous_)
    return false;
  previous_.reset();
  return true;
}

bool BundleUpdater::Rollback() {
  if (!previous_)
    return false;
  std::swap(current_, *previous_);
  previous_.reset();
  staged_.reset();
  return true;
}

std::optional<CrashReport> CrashReporter::Capture(const CrashContext &context) const {
  if (context.build_id.empty() || context.platform.empty() || context.reason.empty())
    return std::nullopt;
  CrashReport report{context.build_id, context.platform, context.reason, {}, false};
  const auto retained = std::min(breadcrumb_limit_, context.breadcrumbs.size());
  report.truncated = retained != context.breadcrumbs.size();
  report.breadcrumbs.insert(report.breadcrumbs.end(), context.breadcrumbs.end() - retained,
                            context.breadcrumbs.end());
  return report;
}

bool SoakMonitor::Sample(ResourceSample sample) {
  if (last_ && sample.frame <= last_->frame)
    return false;
  if (!first_)
    first_ = sample;
  last_ = sample;
  peak_bytes_ = std::max(peak_bytes_, sample.resident_bytes);
  peak_objects_ = std::max(peak_objects_, sample.live_objects);
  ++samples_;
  return true;
}

SoakResult SoakMonitor::Result() const noexcept {
  const bool byte_growth = first_ && last_ && last_->resident_bytes > first_->resident_bytes &&
                           last_->resident_bytes - first_->resident_bytes > allowed_byte_growth_;
  const bool object_growth = first_ && last_ && last_->live_objects > first_->live_objects &&
                             last_->live_objects - first_->live_objects > allowed_object_growth_;
  const bool leak = byte_growth || object_growth;
  return {samples_, peak_bytes_, peak_objects_, leak};
}

bool DeviceMatrix::RecordStartup(ShippingPlatform platform, Profile profile) {
  if (std::ranges::any_of(entries_, [=](const Entry &entry) {
        return entry.platform == platform && entry.profile == profile;
      }))
    return false;
  entries_.push_back({platform, profile});
  return true;
}

bool DeviceMatrix::Passed(Profile profile) const noexcept {
  constexpr std::array platforms{ShippingPlatform::Windows, ShippingPlatform::MacOS,
                                 ShippingPlatform::Android, ShippingPlatform::IOS};
  return std::ranges::all_of(platforms, [&](ShippingPlatform platform) {
    return std::ranges::any_of(entries_, [=](const Entry &entry) {
      return entry.platform == platform && entry.profile == profile;
    });
  });
}

bool ShippingEnabled() noexcept { return true; }

} // namespace nexora::runtime::shipping
