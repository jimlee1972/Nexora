#include "NativeToolFixtureBridge.h"
#include "Nexora/Foundation/BuildInfo.h"
#include "Nexora/Foundation/EditorToolAbi.h"
#include "Nexora/Foundation/PluginAbi.h"

#include <cstring>
#include <stdexcept>

namespace {
uint32_t calls{};
bool shutdown{};
NativeToolFixtureBridge bridge;
int32_t Invoke(void *, uint32_t operation, const uint8_t *input, uint32_t input_bytes,
               uint8_t *output, uint32_t capacity, uint32_t *written) {
  ++calls;
  static_cast<void>(operation);
  static_cast<void>(input);
  static_cast<void>(input_bytes);
  static_cast<void>(output);
  static_cast<void>(capacity);
  static_cast<void>(written);
#if NEXORA_NATIVE_TOOL_FIXTURE_MODE == 8
  return 42;
#elif NEXORA_NATIVE_TOOL_FIXTURE_MODE == 9
  *written = capacity + 1; // Report invalid size without overrunning the host-owned buffer.
  return NEXORA_EDITOR_TOOL_SUCCESS;
#elif NEXORA_NATIVE_TOOL_FIXTURE_MODE == 10
  if (capacity) {
    output[0] = 255;
    *written = 1;
  }
  return input_bytes ? input[0] : NEXORA_EDITOR_TOOL_REJECTED;
#elif NEXORA_NATIVE_TOOL_FIXTURE_MODE == 11
  throw std::runtime_error("deliberate native no-throw violation");
#elif NEXORA_NATIVE_TOOL_FIXTURE_MODE == 14
  return NEXORA_EDITOR_TOOL_SUCCESS; // Deliberately omit the required output size report.
#else
#if NEXORA_NATIVE_TOOL_FIXTURE_MODE == 13
  if (bridge.call)
    bridge.call(bridge.context);
#endif
  if (input_bytes > capacity)
    return NEXORA_EDITOR_TOOL_REJECTED;
  if (input_bytes)
    std::memcpy(output, input, input_bytes);
  *written = input_bytes;
  return NEXORA_EDITOR_TOOL_SUCCESS;
#endif
}
NexoraEditorToolServiceV1 table{sizeof(NexoraEditorToolServiceV1),
                                NEXORA_EDITOR_TOOL_SCHEMA_V1,
                                NEXORA_EDITOR_TOOL_INTERFACE_V1,
                                15,
                                65536,
                                65536,
                                nullptr,
                                Invoke};
int32_t Shutdown(void *) noexcept {
  shutdown = true;
  return 0;
}
int32_t Quiescent(void *) noexcept { return shutdown ? 1 : 0; }
} // namespace

NEXORA_PLUGIN_ABI_EXPORT uint32_t NexoraPluginAbiVersion() noexcept {
  return nexora::foundation::kEngineAbiVersion;
}
NEXORA_PLUGIN_ABI_EXPORT void NexoraPluginRegister(void *context,
                                                   NexoraServiceRegisterCallback register_service) {
  calls = 0;
  shutdown = false;
  bridge = {};
#if NEXORA_NATIVE_TOOL_FIXTURE_MODE == 2
  table.struct_size = 8;
#elif NEXORA_NATIVE_TOOL_FIXTURE_MODE == 3
  table.schema_version = 2;
#elif NEXORA_NATIVE_TOOL_FIXTURE_MODE == 4
  table.interface_version = 2;
#elif NEXORA_NATIVE_TOOL_FIXTURE_MODE == 5
  table.invoke = nullptr;
#elif NEXORA_NATIVE_TOOL_FIXTURE_MODE == 6
  table.maximum_output_bytes = 65537;
#elif NEXORA_NATIVE_TOOL_FIXTURE_MODE == 7
  table.operations = 32;
#elif NEXORA_NATIVE_TOOL_FIXTURE_MODE == 12
  table.operations = NEXORA_EDITOR_TOOL_INSPECT;
  table.maximum_input_bytes = 3;
  table.maximum_output_bytes = 2;
#endif
  register_service(context, "fixture.tool", &table);
  register_service(context, "fixture.calls", &calls);
  register_service(context, "fixture.bridge", &bridge);
}
NEXORA_PLUGIN_ABI_EXPORT int32_t
NexoraPluginGetLifecycleV1(uint32_t requested, NexoraPluginLifecycleV1 *lifecycle) noexcept {
  if (!lifecycle || lifecycle->struct_size < sizeof(*lifecycle) ||
      requested != NEXORA_PLUGIN_LIFECYCLE_SCHEMA_V1)
    return -1;
  *lifecycle = {sizeof(*lifecycle), NEXORA_PLUGIN_LIFECYCLE_SCHEMA_V1, nullptr, Shutdown,
                Quiescent};
  return 0;
}
