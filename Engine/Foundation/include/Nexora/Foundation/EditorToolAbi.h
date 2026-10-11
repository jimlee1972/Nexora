#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Optional typed PluginHost service. Required engine/plugin ABI is unchanged.
// The registered pointer names a readable immutable table for its loaded admission.
// All calls are synchronous, serialized and nonreentrant. No input/output/context borrow
// may escape a call; consumers drain calls before requesting native unload. Native code
// remains trusted and must obey buffer capacity and no-throw contracts; this is no sandbox.
#define NEXORA_EDITOR_TOOL_SCHEMA_V1 1U
#define NEXORA_EDITOR_TOOL_INTERFACE_V1 1U
#define NEXORA_EDITOR_TOOL_INSPECT 1U
#define NEXORA_EDITOR_TOOL_EDIT 2U
#define NEXORA_EDITOR_TOOL_PREVIEW 4U
#define NEXORA_EDITOR_TOOL_SERIALIZE 8U
#define NEXORA_EDITOR_TOOL_SUCCESS 0
#define NEXORA_EDITOR_TOOL_REJECTED 1
#define NEXORA_EDITOR_TOOL_UNAVAILABLE 2
#define NEXORA_EDITOR_TOOL_FAILED 3

typedef struct NexoraEditorToolServiceV1 {
  uint32_t struct_size;
  uint32_t schema_version;
  uint32_t interface_version;
  uint32_t operations;
  uint32_t maximum_input_bytes;
  uint32_t maximum_output_bytes;
  void *context;
  // Operation is one declared bit above. Input is read-only; output is caller-owned.
  // On success, set output_bytes within output_capacity; failure bytes are discarded by the host.
  // These byte operations grant no document, filesystem, GPU or publication authority.
  int32_t (*invoke)(void *context, uint32_t operation, const uint8_t *input, uint32_t input_bytes,
                    uint8_t *output, uint32_t output_capacity, uint32_t *output_bytes);
} NexoraEditorToolServiceV1;

#ifdef __cplusplus
}
#endif
