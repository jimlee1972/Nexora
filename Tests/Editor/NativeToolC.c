#include "Nexora/Foundation/EditorToolAbi.h"

static int32_t invoke(void *context, uint32_t operation, const uint8_t *input, uint32_t bytes,
                      uint8_t *output, uint32_t capacity, uint32_t *written) {
  (void)context;
  if (operation != NEXORA_EDITOR_TOOL_INSPECT || bytes != 1 || capacity != 1)
    return NEXORA_EDITOR_TOOL_REJECTED;
  output[0] = input[0];
  *written = 1;
  return NEXORA_EDITOR_TOOL_SUCCESS;
}
int main(void) {
  NexoraEditorToolServiceV1 table = {sizeof(NexoraEditorToolServiceV1),
                                     NEXORA_EDITOR_TOOL_SCHEMA_V1,
                                     NEXORA_EDITOR_TOOL_INTERFACE_V1,
                                     NEXORA_EDITOR_TOOL_INSPECT,
                                     1,
                                     1,
                                     0,
                                     invoke};
  uint8_t input = 17, output = 0;
  uint32_t written = 0;
  return table.invoke(table.context, table.operations, &input, 1, &output, 1, &written) ||
         written != 1 || output != input;
}
