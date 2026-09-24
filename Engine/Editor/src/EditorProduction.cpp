#include "Nexora/Editor/EditorProduction.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <unordered_set>

namespace nexora::editor {
namespace {
bool SafePath(std::string_view path) {
  if (path.empty() || path.starts_with('/') || path.starts_with('\\'))
    return false;
  std::filesystem::path parsed(path);
  return path.find('\\') == std::string_view::npos &&
         std::ranges::none_of(parsed, [](const auto &part) { return part == ".." || part == "."; });
}

std::string Escape(std::string_view value) {
  std::ostringstream result;
  for (const unsigned char c : value) {
    if (c == '"' || c == '\\') {
      result << '\\' << c;
    } else if (c == '\n') {
      result << "\\n";
    } else if (c == '\r') {
      result << "\\r";
    } else if (c == '\t') {
      result << "\\t";
    } else if (c < 0x20) {
      result << "\\u" << std::hex << std::setfill('0') << std::setw(4) << static_cast<unsigned>(c);
    } else {
      result << c;
    }
  }
  return result.str();
}
} // namespace

bool SpecializedToolRegistry::Register(ToolDescriptor descriptor) {
  if (descriptor.id.empty() || descriptor.title.empty() ||
      std::ranges::any_of(tools_, [&](const auto &tool) { return tool.id == descriptor.id; }))
    return false;
  if (descriptor.state != CapabilityState::Implemented && descriptor.reason.empty())
    return false;
  tools_.push_back(std::move(descriptor));
  std::ranges::sort(tools_, {}, &ToolDescriptor::id);
  return true;
}

const ToolDescriptor *SpecializedToolRegistry::Find(std::string_view id) const noexcept {
  const auto found = std::ranges::find(tools_, id, &ToolDescriptor::id);
  return found == tools_.end() ? nullptr : &*found;
}

bool BuildFrontend::Validate(const BuildManifest &manifest, std::string *error) {
  if (manifest.schema_version != 1 || manifest.profile.name.empty() ||
      manifest.profile.target.empty() || manifest.profile.configuration.empty() ||
      manifest.profile.command.empty()) {
    if (error)
      *error = "incomplete build profile";
    return false;
  }
  std::unordered_set<std::string> paths;
  for (const auto &artifact : manifest.artifacts) {
    if (!SafePath(artifact.path) || artifact.checksum.empty() ||
        !paths.insert(artifact.path).second) {
      if (error)
        *error = "invalid or duplicate artifact: " + artifact.path;
      return false;
    }
  }
  return true;
}

bool BuildFrontend::Write(const BuildManifest &manifest, const std::filesystem::path &path,
                          std::string *error) {
  if (!Validate(manifest, error))
    return false;
  std::error_code ec;
  if (!path.parent_path().empty())
    std::filesystem::create_directories(path.parent_path(), ec);
  if (ec) {
    if (error)
      *error = ec.message();
    return false;
  }
  const auto temporary = path.string() + ".tmp";
  std::ofstream output(temporary, std::ios::trunc);
  output << "{\n  \"schema_version\": 1,\n  \"profile\": {\"name\": \""
         << Escape(manifest.profile.name) << "\", \"target\": \"" << Escape(manifest.profile.target)
         << "\", \"configuration\": \"" << Escape(manifest.profile.configuration)
         << "\", \"command\": \"" << Escape(manifest.profile.command) << "\"},\n  \"artifacts\": [";
  for (std::size_t i = 0; i < manifest.artifacts.size(); ++i) {
    const auto &artifact = manifest.artifacts[i];
    output << (i ? "," : "") << "\n    {\"path\": \"" << Escape(artifact.path)
           << "\", \"checksum\": \"" << Escape(artifact.checksum)
           << "\", \"bytes\": " << artifact.bytes << "}";
  }
  output << "\n  ]\n}\n";
  output.close();
  if (!output) {
    if (error)
      *error = "could not write build manifest";
    return false;
  }
  std::filesystem::rename(temporary, path, ec);
  if (ec) {
    std::filesystem::remove(path, ec);
    ec.clear();
    std::filesystem::rename(temporary, path, ec);
  }
  if (ec && error)
    *error = ec.message();
  return !ec;
}

bool ProfileSession::Add(FrameSample sample) {
  if (!std::isfinite(sample.cpu_ms) || !std::isfinite(sample.gpu_ms) || sample.cpu_ms < 0 ||
      sample.gpu_ms < 0 || (!samples_.empty() && sample.frame <= samples_.back().frame))
    return false;
  samples_.push_back(sample);
  return true;
}

std::optional<FrameSample> ProfileSession::Peak() const noexcept {
  if (samples_.empty())
    return std::nullopt;
  return *std::ranges::max_element(samples_, [](const auto &left, const auto &right) {
    return left.cpu_ms + left.gpu_ms < right.cpu_ms + right.gpu_ms;
  });
}

bool ExtensionPolicy::Allows(std::string_view publisher, bool signature_valid) const noexcept {
  if (require_signature && !signature_valid)
    return false;
  return std::ranges::find(trusted_publishers, publisher) != trusted_publishers.end();
}

bool TelemetryConsent::Record(std::string event) {
  if (!enabled_ || event.empty())
    return false;
  events_.push_back(std::move(event));
  return true;
}

std::pair<std::size_t, std::size_t> VirtualHierarchy::Visible(std::size_t first,
                                                              std::size_t capacity) const noexcept {
  first = std::min(first, count_);
  return {first, std::min(capacity, count_ - first)};
}
} // namespace nexora::editor
