#ifndef NEXORA_GAMEPLAY_DISPLAY_H
#define NEXORA_GAMEPLAY_DISPLAY_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef enum NexoraDisplayMode {
  NEXORA_DISPLAY_WINDOWED = 0,
  NEXORA_DISPLAY_BORDERLESS = 1
} NexoraDisplayMode;
typedef struct NexoraDisplayRectV1 {
  float x, y, width, height;
} NexoraDisplayRectV1;
typedef struct NexoraDisplayStateV1 {
  uint32_t struct_size, abi_version;
  uint64_t revision;
  uint32_t pixel_width, pixel_height;
  float logical_width, logical_height, dpi_scale;
  uint32_t mode;
  NexoraDisplayRectV1 viewport_pixels, viewport_logical;
} NexoraDisplayStateV1;
typedef struct NexoraDisplayWindowV1 {
  uint32_t struct_size, abi_version;
  uint32_t width, height, mode, resizable;
} NexoraDisplayWindowV1;
/* Serialized owner-thread lifetime, including module Stop/Destroy. Input remains native pixels.
 * get_state copies a snapshot frozen before the frame's module callbacks. request_window returns
 * native request admission, not confirmation of the actual extent; observe the next snapshot.
 * set_aspect queues a policy for the next frame; (0,0) selects the whole surface. */
typedef struct NexoraGameplayDisplayApiV1 {
  uint32_t struct_size, abi_version;
  void *context;
  int32_t (*get_state)(void *, NexoraDisplayStateV1 *);
  int32_t (*request_window)(void *, const NexoraDisplayWindowV1 *);
  int32_t (*set_aspect)(void *, float minimum, float maximum);
} NexoraGameplayDisplayApiV1;
#ifdef __cplusplus
}
#endif
#endif
