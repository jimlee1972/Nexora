#pragma once

#include "Nexora/Runtime/Api.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace nexora::runtime::shipping {

enum class Profile { Minimal, Full, Dedicated };
enum class ArtifactKind { Asset, Plugin, Shader, Sdk, Presentation };

struct Artifact final {
  std::string name;
  std::string package_path;
  std::string digest;
  ArtifactKind kind{ArtifactKind::Asset};
  std::uint64_t bytes{};
  bool optional{};
};

struct PackageRequest final {
  Profile profile{Profile::Minimal};
  std::vector<Artifact> artifacts;
  std::vector<std::string> enabled_plugins;
  std::vector<std::string> enabled_shader_families;
  bool include_sdk{};
};

enum class PackageError { None, InvalidArtifact, UnsafePath, DuplicatePath };
struct PackageManifest final {
  Profile profile{Profile::Minimal};
  std::vector<Artifact> artifacts;
  std::uint64_t total_bytes{};
  bool presentation{};
};
struct PackageResult final {
  PackageManifest manifest;
  PackageError error{PackageError::None};
  std::string detail;
  [[nodiscard]] explicit operator bool() const noexcept { return error == PackageError::None; }
};

// Synchronous and allocation-owning. Returned manifests do not reference the request.
class NEXORA_RUNTIME_API Packager final {
public:
  [[nodiscard]] PackageResult Build(const PackageRequest &request) const;
};

struct BundleGeneration final {
  std::uint64_t generation{};
  std::string digest;
};

// The caller persists Current()/Previous()/Staged() around process restarts. Activation retains
// the last confirmed generation until Confirm() makes rollback intentionally unavailable.
class NEXORA_RUNTIME_API BundleUpdater final {
public:
  explicit BundleUpdater(BundleGeneration installed);
  bool Stage(BundleGeneration candidate);
  bool Activate();
  bool Confirm();
  bool Rollback();
  [[nodiscard]] BundleGeneration Restart() const noexcept { return current_; }
  [[nodiscard]] const BundleGeneration &Current() const noexcept { return current_; }
  [[nodiscard]] const std::optional<BundleGeneration> &Previous() const noexcept {
    return previous_;
  }
  [[nodiscard]] const std::optional<BundleGeneration> &Staged() const noexcept { return staged_; }

private:
  BundleGeneration current_;
  std::optional<BundleGeneration> previous_;
  std::optional<BundleGeneration> staged_;
};

struct CrashContext final {
  std::string build_id;
  std::string platform;
  std::string reason;
  std::vector<std::string> breadcrumbs;
};
struct CrashReport final {
  std::string build_id;
  std::string platform;
  std::string reason;
  std::vector<std::string> breadcrumbs;
  bool truncated{};
};

class NEXORA_RUNTIME_API CrashReporter final {
public:
  explicit CrashReporter(std::size_t breadcrumb_limit = 64) : breadcrumb_limit_(breadcrumb_limit) {}
  [[nodiscard]] std::optional<CrashReport> Capture(const CrashContext &context) const;

private:
  std::size_t breadcrumb_limit_;
};

struct ResourceSample final {
  std::uint64_t frame{};
  std::uint64_t resident_bytes{};
  std::uint64_t live_objects{};
};
struct SoakResult final {
  std::size_t samples{};
  std::uint64_t peak_bytes{};
  std::uint64_t peak_objects{};
  bool suspected_leak{};
};

class NEXORA_RUNTIME_API SoakMonitor final {
public:
  SoakMonitor(std::uint64_t allowed_byte_growth, std::uint64_t allowed_object_growth)
      : allowed_byte_growth_(allowed_byte_growth), allowed_object_growth_(allowed_object_growth) {}
  bool Sample(ResourceSample sample);
  [[nodiscard]] SoakResult Result() const noexcept;

private:
  std::uint64_t allowed_byte_growth_{};
  std::uint64_t allowed_object_growth_{};
  std::optional<ResourceSample> first_;
  std::optional<ResourceSample> last_;
  std::uint64_t peak_bytes_{};
  std::uint64_t peak_objects_{};
  std::size_t samples_{};
};

enum class ShippingPlatform { Windows, MacOS, Android, IOS };
class NEXORA_RUNTIME_API DeviceMatrix final {
public:
  bool RecordStartup(ShippingPlatform platform, Profile profile);
  [[nodiscard]] bool Passed(Profile profile) const noexcept;

private:
  struct Entry final {
    ShippingPlatform platform;
    Profile profile;
  };
  std::vector<Entry> entries_;
};

[[nodiscard]] NEXORA_RUNTIME_API bool ShippingEnabled() noexcept;

} // namespace nexora::runtime::shipping
