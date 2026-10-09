#include "Nexora/Editor/EditorProduction.h"
#include "AtomicFile.h"
#include "Nexora/Core/ProcessMemory.h"
#include "Nexora/Foundation/Types.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>
#include <unordered_set>

namespace nexora::editor {
namespace {
bool SafePath(std::string_view path) {
  if (path.empty() || path.starts_with('/') || path.ends_with('/') ||
      path.find_first_of("\\:") != std::string_view::npos ||
      path.find("//") != std::string_view::npos ||
      std::ranges::any_of(path, [](unsigned char value) { return value < 0x20 || value == 0x7F; }))
    return false;
  // Text is validated below before reaching this parser. Interpret UTF-8 consistently on
  // every host; drive/stream syntax and separator aliases cannot depend on the writer's OS.
  const std::filesystem::path parsed(std::u8string(path.begin(), path.end()));
  return !parsed.is_absolute() && !parsed.has_root_name() &&
         std::ranges::none_of(parsed, [](const auto &part) {
           const auto text = part.u8string();
           return text.ends_with(u8'.') || text.ends_with(u8' ');
         });
}

std::string Escape(std::string_view value) {
  std::ostringstream result;
  result.imbue(std::locale::classic());
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
  if (error)
    error->clear();
  if (manifest.schema_version != 1 || manifest.profile.name.empty() ||
      manifest.profile.target.empty() || manifest.profile.configuration.empty() ||
      manifest.profile.command.empty()) {
    if (error)
      *error = "incomplete build profile";
    return false;
  }
  if (!foundation::IsValidUtf8(manifest.profile.name) ||
      !foundation::IsValidUtf8(manifest.profile.target) ||
      !foundation::IsValidUtf8(manifest.profile.configuration) ||
      !foundation::IsValidUtf8(manifest.profile.command)) {
    if (error)
      *error = "invalid UTF-8 build profile text";
    return false;
  }
  std::unordered_set<std::string> paths;
  for (const auto &artifact : manifest.artifacts) {
    if (!foundation::IsValidUtf8(artifact.path) || !foundation::IsValidUtf8(artifact.checksum)) {
      if (error)
        *error = "invalid UTF-8 build artifact text";
      return false;
    }
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
  return detail::AtomicWriteWith(
      path,
      [&](std::ostream &output) {
        output.imbue(std::locale::classic());
        output << "{\n  \"schema_version\": 1,\n  \"profile\": {\"name\": \""
               << Escape(manifest.profile.name) << "\", \"target\": \""
               << Escape(manifest.profile.target) << "\", \"configuration\": \""
               << Escape(manifest.profile.configuration) << "\", \"command\": \""
               << Escape(manifest.profile.command) << "\"},\n  \"artifacts\": [";
        for (std::size_t i = 0; i < manifest.artifacts.size(); ++i) {
          const auto &artifact = manifest.artifacts[i];
          output << (i ? "," : "") << "\n    {\"path\": \"" << Escape(artifact.path)
                 << "\", \"checksum\": \"" << Escape(artifact.checksum)
                 << "\", \"bytes\": " << artifact.bytes << "}";
        }
        output << "\n  ]\n}\n";
      },
      error);
}

bool ProfileSession::Add(FrameSample sample) {
  if (!capturing_ || capacity_ == 0 || !std::isfinite(sample.cpu_ms) ||
      !std::isfinite(sample.gpu_ms) || sample.cpu_ms < 0 || sample.gpu_ms < 0 ||
      (!samples_.empty() && sample.frame <= samples_.back().frame))
    return false;
  if (samples_.size() == capacity_) {
    samples_.erase(samples_.begin());
    ++dropped_;
  }
  samples_.push_back(sample);
  return true;
}

bool ValidateProcessMemorySamples(std::span<const ProcessMemorySample> samples) noexcept {
  if (samples.empty() || samples.size() > ProfileSession::kMaximumMemorySamples)
    return false;
  std::uint64_t previous_sequence = 0;
  double previous_elapsed = -1;
  for (const auto &sample : samples) {
    if (sample.sequence == 0 || sample.sequence <= previous_sequence ||
        !std::isfinite(sample.elapsed_ms) || sample.elapsed_ms < 0 ||
        sample.elapsed_ms <= previous_elapsed)
      return false;
    previous_sequence = sample.sequence;
    previous_elapsed = sample.elapsed_ms;
  }
  return true;
}

void ProfileSession::Clear() noexcept {
  samples_.clear();
  dropped_ = 0;
  memory_ = {};
  memory_sample_count_ = 0;
  memory_dropped_ = 0;
  memory_origin_.reset();

  gpu_.milliseconds.reset();
  gpu_.observed_peak_ms.reset();
  gpu_.completed_submission = 0;
  gpu_sample_count_ = 0;
  gpu_dropped_ = 0;
  // Keep the native completion watermark so Clear cannot re-admit an old completed result.
  next_memory_sample_.reset();
  last_memory_sample_.reset();
}

bool ProfileSession::SampleProcessMemory(std::chrono::steady_clock::time_point now,
                                         ProcessMemoryReader reader) noexcept {
  if (!capturing_ || (last_memory_sample_ && now <= *last_memory_sample_) ||
      (next_memory_sample_ && now < *next_memory_sample_))
    return false;
  last_memory_sample_ = now;
  next_memory_sample_ =
      now > std::chrono::steady_clock::time_point::max() - kProcessMemorySampleInterval
          ? std::chrono::steady_clock::time_point::max()
          : now + kProcessMemorySampleInterval;
  memory_.resident_bytes = (reader ? reader : core::CurrentProcessResidentBytes)();
  if (memory_.attempts != std::numeric_limits<std::uint64_t>::max())
    ++memory_.attempts;
  if (memory_.resident_bytes) {
    if (memory_.successful_samples != std::numeric_limits<std::uint64_t>::max())
      ++memory_.successful_samples;
    memory_.observed_peak_bytes =
        std::max(memory_.observed_peak_bytes.value_or(0), *memory_.resident_bytes);
  }
  if (!memory_origin_)
    memory_origin_ = now;
  const auto capacity = std::min(capacity_, kMaximumMemorySamples);
  // Convert before subtraction so even clock endpoints cannot overflow signed durations.
  const double elapsed =
      std::chrono::duration<double, std::milli>(now.time_since_epoch()).count() -
      std::chrono::duration<double, std::milli>(memory_origin_->time_since_epoch()).count();
  if (capacity && (memory_sample_count_ == 0 ||
                   (memory_.attempts > memory_samples_[memory_sample_count_ - 1].sequence &&
                    elapsed > memory_samples_[memory_sample_count_ - 1].elapsed_ms))) {
    if (memory_sample_count_ == capacity) {
      std::move(memory_samples_.begin() + 1, memory_samples_.begin() + memory_sample_count_,
                memory_samples_.begin());
      --memory_sample_count_;
      if (memory_dropped_ != std::numeric_limits<std::uint64_t>::max())
        ++memory_dropped_;
    }
    memory_samples_[memory_sample_count_++] = {memory_.attempts, elapsed, memory_.resident_bytes};
  }
  return true;
}

bool ProfileSession::ObserveGpuFrame(std::uint64_t domain, GpuProfileSource source,
                                     bool software_rasterizer, GpuProfileSample sample) noexcept {
  if (!domain || source > GpuProfileSource::MetalCommandBuffer ||
      (sample.milliseconds && (!std::isfinite(*sample.milliseconds) || *sample.milliseconds < 0)))
    return false;
  if (gpu_domain_ != domain || gpu_.source != source ||
      gpu_.software_rasterizer != software_rasterizer) {
    gpu_domain_ = domain;
    gpu_ = {source, software_rasterizer, 0, std::nullopt, std::nullopt};
    gpu_watermark_ = 0;
    gpu_sample_count_ = 0;
    gpu_dropped_ = 0;
  }
  if (source == GpuProfileSource::Unavailable || !sample.submission ||
      sample.submission <= gpu_watermark_)
    return false;
  gpu_watermark_ = sample.submission;
  if (!capturing_)
    return false;
  gpu_.completed_submission = sample.submission;
  gpu_.milliseconds = sample.milliseconds;
  if (sample.milliseconds)
    gpu_.observed_peak_ms = std::max(gpu_.observed_peak_ms.value_or(0), *sample.milliseconds);
  const auto capacity = std::min(capacity_, kMaximumGpuSamples);
  if (capacity) {
    if (gpu_sample_count_ == capacity) {
      std::move(gpu_samples_.begin() + 1, gpu_samples_.begin() + gpu_sample_count_,
                gpu_samples_.begin());
      --gpu_sample_count_;
      if (gpu_dropped_ != UINT64_MAX)
        ++gpu_dropped_;
    }
    gpu_samples_[gpu_sample_count_++] = sample;
  }
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
  if (!enabled_ || event.empty() || event.size() > kMaximumEventBytes ||
      event.find('\0') != std::string::npos || !foundation::IsValidUtf8(event) ||
      events_.size() >= kMaximumEvents)
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
