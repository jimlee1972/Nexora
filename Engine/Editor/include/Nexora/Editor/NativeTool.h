#pragma once

#include "Nexora/Editor/Api.h"
#include "Nexora/Foundation/EditorToolAbi.h"
#include "Nexora/Runtime/EditorSdk.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace nexora::editor {
enum class NativeToolOperation : std::uint32_t {
  Inspect = NEXORA_EDITOR_TOOL_INSPECT,
  Edit = NEXORA_EDITOR_TOOL_EDIT,
  Preview = NEXORA_EDITOR_TOOL_PREVIEW,
  Serialize = NEXORA_EDITOR_TOOL_SERIALIZE
};
enum class NativeToolState {
  Success,
  Rejected,
  Unavailable,
  Failed,
  Invalid,
  WrongThread,
  Reentrant
};
struct NativeToolOutcome final {
  NativeToolState state{NativeToolState::Invalid};
  std::optional<std::int32_t> callback_result;
  std::vector<std::byte> bytes;
  std::string message;
};

// Fresh qualified service lookup per invocation; no retained native table/context/function.
// Owner calls serialize with PluginHost/registry mutation and unload. The host authorizes actual
// document operations independently and parses/revalidates copied bytes before authoring/IO.
// This wrapper does not expose engine objects, perform publication or acquire lifetime leases.
class NEXORA_EDITOR_API NativeToolInvoker final {
public:
  static constexpr std::size_t kMaximumInputBytes = 64 * 1024;
  static constexpr std::size_t kMaximumOutputBytes = 64 * 1024;
  NativeToolInvoker() : owner_(std::this_thread::get_id()) {}
  NativeToolInvoker(const NativeToolInvoker &) = delete;
  NativeToolInvoker &operator=(const NativeToolInvoker &) = delete;
  [[nodiscard]] NativeToolOutcome Invoke(const runtime::PluginHost &,
                                         const runtime::ServiceRegistry &, std::uint64_t admission,
                                         std::string_view service, NativeToolOperation,
                                         std::span<const std::byte> input);

private:
  friend class SignedExtensionHost;
  friend class PluginManager;
  [[nodiscard]] NativeToolState ContextState() const noexcept;
  const std::thread::id owner_;
};
} // namespace nexora::editor
