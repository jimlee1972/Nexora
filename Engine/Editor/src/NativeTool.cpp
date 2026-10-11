#include "Nexora/Editor/NativeTool.h"

#include <limits>

namespace nexora::editor {
namespace {
thread_local bool invoking;
}
NativeToolState NativeToolInvoker::ContextState() const noexcept {
  if (std::this_thread::get_id() != owner_)
    return NativeToolState::WrongThread;
  return invoking ? NativeToolState::Reentrant : NativeToolState::Success;
}
NativeToolOutcome NativeToolInvoker::Invoke(const runtime::PluginHost &host,
                                            const runtime::ServiceRegistry &registry,
                                            std::uint64_t admission, std::string_view name,
                                            NativeToolOperation operation,
                                            std::span<const std::byte> input) {
  const auto fail = [](NativeToolState state, const char *message) {
    return NativeToolOutcome{state, {}, {}, message};
  };
  const auto context = ContextState();
  if (context == NativeToolState::WrongThread)
    return fail(NativeToolState::WrongThread, "Native tool invocation requires its owner thread");
  if (context == NativeToolState::Reentrant)
    return fail(NativeToolState::Reentrant, "Native tool invocation cannot reenter");
  const auto requested = static_cast<std::uint32_t>(operation);
  constexpr auto known = NEXORA_EDITOR_TOOL_INSPECT | NEXORA_EDITOR_TOOL_EDIT |
                         NEXORA_EDITOR_TOOL_PREVIEW | NEXORA_EDITOR_TOOL_SERIALIZE;
  if (input.size() > kMaximumInputBytes || !requested || (requested & ~known) ||
      (requested & (requested - 1)))
    return fail(NativeToolState::Invalid, "Invalid native tool operation or input budget");
  const auto *table =
      static_cast<const NexoraEditorToolServiceV1 *>(host.FindService(admission, registry, name));
  if (!table)
    return fail(NativeToolState::Unavailable, "Qualified native tool provider is unavailable");
  if (table->struct_size < sizeof(*table) || table->struct_size > 4096 ||
      table->schema_version != NEXORA_EDITOR_TOOL_SCHEMA_V1 ||
      table->interface_version != NEXORA_EDITOR_TOOL_INTERFACE_V1 || !table->invoke ||
      !table->operations || (table->operations & ~known) ||
      table->maximum_input_bytes > kMaximumInputBytes || !table->maximum_output_bytes ||
      table->maximum_output_bytes > kMaximumOutputBytes)
    return fail(NativeToolState::Invalid,
                "Invalid native tool service prefix, interface or budgets");
  if (!(table->operations & requested))
    return fail(NativeToolState::Unavailable, "Native tool operation is unavailable");
  if (input.size() > table->maximum_input_bytes)
    return fail(NativeToolState::Rejected, "Native tool input exceeds the provider budget");
  // Allocate before entering native code; no borrowed table fields survive this synchronous call.
  NativeToolOutcome outcome;
  outcome.bytes.resize(table->maximum_output_bytes);
  struct Guard final {
    bool &busy;
    explicit Guard(bool &value) : busy(value) { busy = true; }
    ~Guard() { busy = false; }
  } guard(invoking);
  std::uint32_t written = std::numeric_limits<std::uint32_t>::max();
  try {
    outcome.callback_result = table->invoke(
        table->context, requested, reinterpret_cast<const std::uint8_t *>(input.data()),
        static_cast<std::uint32_t>(input.size()),
        reinterpret_cast<std::uint8_t *>(outcome.bytes.data()),
        static_cast<std::uint32_t>(outcome.bytes.size()), &written);
  } catch (...) {
    outcome.bytes.clear();
    outcome.state = NativeToolState::Failed;
    outcome.message = "Native tool callback violated its no-throw contract";
    return outcome;
  }
  if (*outcome.callback_result == NEXORA_EDITOR_TOOL_SUCCESS && written <= outcome.bytes.size()) {
    outcome.bytes.resize(written);
    outcome.state = NativeToolState::Success;
    return outcome;
  }
  outcome.bytes.clear();
  switch (*outcome.callback_result) {
  case NEXORA_EDITOR_TOOL_REJECTED:
    outcome.state = NativeToolState::Rejected;
    break;
  case NEXORA_EDITOR_TOOL_UNAVAILABLE:
    outcome.state = NativeToolState::Unavailable;
    break;
  case NEXORA_EDITOR_TOOL_FAILED:
    outcome.state = NativeToolState::Failed;
    break;
  default:
    outcome.state = NativeToolState::Invalid;
    break;
  }
  outcome.message = "Native tool callback rejected, failed or returned invalid output";
  return outcome;
}
} // namespace nexora::editor
