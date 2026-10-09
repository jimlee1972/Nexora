#include "Nexora/Foundation/PluginAbi.h"
#include <stddef.h>

static int32_t shutdown(void *context) { return context ? -1 : 0; }
static int32_t quiescent(void *context) { return context ? -1 : 1; }
NEXORA_PLUGIN_ABI_EXPORT int32_t NexoraPluginGetLifecycleV1(uint32_t requested,
                                                            NexoraPluginLifecycleV1 *lifecycle) {
  if (!lifecycle || lifecycle->struct_size < sizeof(*lifecycle) ||
      requested != NEXORA_PLUGIN_LIFECYCLE_SCHEMA_V1)
    return -1;
  lifecycle->struct_size = sizeof(*lifecycle);
  lifecycle->schema_version = requested;
  lifecycle->context = NULL;
  lifecycle->request_shutdown = shutdown;
  lifecycle->poll_quiescence = quiescent;
  return 0;
}
int main(void) {
  NexoraPluginLifecycleV1 lifecycle = {0};
  lifecycle.struct_size = sizeof(lifecycle);
  if (NexoraPluginGetLifecycleV1(NEXORA_PLUGIN_LIFECYCLE_SCHEMA_V1, &lifecycle) != 0 ||
      lifecycle.request_shutdown(lifecycle.context) != 0 ||
      lifecycle.poll_quiescence(lifecycle.context) != 1)
    return 1;
  return 0;
}
