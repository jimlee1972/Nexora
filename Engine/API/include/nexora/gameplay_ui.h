#ifndef NEXORA_GAMEPLAY_UI_H
#define NEXORA_GAMEPLAY_UI_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Host-owned immediate UI. All coordinates are logical pixels; RGBA packs R in low byte. */
typedef struct NexoraUiRect {
  float x, y, width, height;
} NexoraUiRect;
typedef enum NexoraUiButtonState {
  NEXORA_UI_HOVERED = 1u,
  NEXORA_UI_FOCUSED = 2u,
  NEXORA_UI_ACTIVATED = 4u,
  NEXORA_UI_CANCELLED = 8u
} NexoraUiButtonState;
typedef struct NexoraUiGlyph {
  uint32_t codepoint;
  float advance, x, y, width, height, u0, v0, u1, v1;
} NexoraUiGlyph;
/* All buffers borrow only register_font; data is copied before success. */
typedef struct NexoraUiFontV1 {
  uint32_t struct_size, width, height, reserved;
  uint64_t texture_id;
  float pixel_size, line_height;
  const uint8_t *rgba;
  uint64_t rgba_length;
  const NexoraUiGlyph *glyphs;
  uint32_t glyph_count, reserved2;
} NexoraUiFontV1;
typedef struct NexoraGameplayUiApiV1 {
  uint32_t struct_size, abi_version;
  void *context;
  int32_t (*begin_frame)(void *, float width, float height);
  int32_t (*panel)(void *, NexoraUiRect, uint32_t rgba);
  int32_t (*text)(void *, uint64_t font, float size, NexoraUiRect, const char *, uint32_t length,
                  uint32_t rgba);
  /* IDs must be nonzero/unique per frame; enabled is 0 or 1. */
  int32_t (*button)(void *, uint64_t id, NexoraUiRect, const char *, uint32_t length, uint64_t font,
                    float size, uint32_t enabled, uint32_t *state);
  int32_t (*end_frame)(void *);
  /* index enumerates supplied host fonts; missing index returns UNSUPPORTED. */
  int32_t (*get_font)(void *, uint32_t index, uint64_t *font);
  int32_t (*has_glyph)(void *, uint64_t font, uint32_t codepoint, uint32_t *present);
  int32_t (*register_font)(void *, const NexoraUiFontV1 *, uint64_t *font);
} NexoraGameplayUiApiV1;
#ifdef __cplusplus
}
#endif
#endif
