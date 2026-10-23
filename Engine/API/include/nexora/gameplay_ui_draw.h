#ifndef NEXORA_GAMEPLAY_UI_DRAW_H
#define NEXORA_GAMEPLAY_UI_DRAW_H
#include "gameplay_ui.h"
#ifdef __cplusplus
extern "C" {
#endif
/* RGBA8 in the existing UI color contract, copied on registration. Reserve a unique,
 * immutable surface texture_id outside Sprite/font IDs; never reuse it within this owner.
 * Zero selects a host ID from the reserved upper 65536 IDs (excluding Presentation/white). */
typedef struct NexoraUiImageV1 {
  uint32_t struct_size, width, height, reserved;
  uint64_t texture_id;
  const uint8_t *rgba;
  uint64_t rgba_length;
} NexoraUiImageV1;
/* Source in image pixels, destination/clip/borders in logical pixels. Borders shrink
 * proportionally if the destination is smaller than their combined extent. */
typedef struct NexoraUiImageDrawV1 {
  uint32_t struct_size, rgba;
  uint64_t image;
  NexoraUiRect source, destination, clip;
} NexoraUiImageDrawV1;
typedef struct NexoraUiInsets {
  float left, top, right, bottom;
} NexoraUiInsets;
typedef struct NexoraGameplayUiDrawApiV1 {
  uint32_t struct_size, abi_version;
  void *context;
  int32_t (*register_image)(void *, const NexoraUiImageV1 *, uint64_t *out_handle);
  int32_t (*remove_image)(void *, uint64_t handle);
  int32_t (*image)(void *, const NexoraUiImageDrawV1 *);
  int32_t (*nine_slice)(void *, const NexoraUiImageDrawV1 *, NexoraUiInsets source_pixels,
                        NexoraUiInsets destination_logical);
  /* Button capture/navigation without drawing; compose images/labels in Zig. */
  int32_t (*interact)(void *, uint64_t id, NexoraUiRect, uint32_t enabled, uint32_t *state);
  int32_t (*get_glyph)(void *, uint64_t font, uint32_t codepoint, NexoraUiGlyph *);
  /* Same copied NXFONT1 descriptor; texture_id=0 allocates an immutable host generation. */
  int32_t (*register_font)(void *, const NexoraUiFontV1 *, uint64_t *out_handle);
  int32_t (*remove_font)(void *, uint64_t handle);
} NexoraGameplayUiDrawApiV1;
#ifdef __cplusplus
}
#endif
#endif
