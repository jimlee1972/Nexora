#ifndef NEXORA_GAMEPLAY_INPUT_H
#define NEXORA_GAMEPLAY_INPUT_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* All callbacks run synchronously on the owning host thread. Results use
 * NexoraGameplayResult numeric values; errors preserve caller output bytes.
 * User IDs are 0..7, action IDs 1..256. Keys use Window::Key physical codes;
 * mouse buttons 0..4; gamepad buttons 0..31 and signed axes 32..47. */
typedef enum NexoraGameplayInputDevice {
  NEXORA_INPUT_DEVICE_NONE = 0,
  NEXORA_INPUT_DEVICE_KEYBOARD = 1,
  NEXORA_INPUT_DEVICE_MOUSE = 2,
  NEXORA_INPUT_DEVICE_GAMEPAD = 3
} NexoraGameplayInputDevice;
/* Stable physical keys; independent of native OS key codes. */
typedef enum NexoraGameplayInputKey {
  NEXORA_INPUT_KEY_UNKNOWN = 0,
  NEXORA_INPUT_KEY_TAB = 1,
  NEXORA_INPUT_KEY_LEFT_ARROW = 2,
  NEXORA_INPUT_KEY_RIGHT_ARROW = 3,
  NEXORA_INPUT_KEY_UP_ARROW = 4,
  NEXORA_INPUT_KEY_DOWN_ARROW = 5,
  NEXORA_INPUT_KEY_PAGE_UP = 6,
  NEXORA_INPUT_KEY_PAGE_DOWN = 7,
  NEXORA_INPUT_KEY_HOME = 8,
  NEXORA_INPUT_KEY_END = 9,
  NEXORA_INPUT_KEY_INSERT = 10,
  NEXORA_INPUT_KEY_DELETE = 11,
  NEXORA_INPUT_KEY_BACKSPACE = 12,
  NEXORA_INPUT_KEY_SPACE = 13,
  NEXORA_INPUT_KEY_ENTER = 14,
  NEXORA_INPUT_KEY_ESCAPE = 15,
  NEXORA_INPUT_KEY_APOSTROPHE = 16,
  NEXORA_INPUT_KEY_COMMA = 17,
  NEXORA_INPUT_KEY_MINUS = 18,
  NEXORA_INPUT_KEY_PERIOD = 19,
  NEXORA_INPUT_KEY_SLASH = 20,
  NEXORA_INPUT_KEY_SEMICOLON = 21,
  NEXORA_INPUT_KEY_EQUAL = 22,
  NEXORA_INPUT_KEY_LEFT_BRACKET = 23,
  NEXORA_INPUT_KEY_BACKSLASH = 24,
  NEXORA_INPUT_KEY_RIGHT_BRACKET = 25,
  NEXORA_INPUT_KEY_GRAVE_ACCENT = 26,
  NEXORA_INPUT_KEY_CAPS_LOCK = 27,
  NEXORA_INPUT_KEY_SCROLL_LOCK = 28,
  NEXORA_INPUT_KEY_NUM_LOCK = 29,
  NEXORA_INPUT_KEY_PRINT_SCREEN = 30,
  NEXORA_INPUT_KEY_PAUSE = 31,
  NEXORA_INPUT_KEY_KEYPAD0 = 32,
  NEXORA_INPUT_KEY_KEYPAD1 = 33,
  NEXORA_INPUT_KEY_KEYPAD2 = 34,
  NEXORA_INPUT_KEY_KEYPAD3 = 35,
  NEXORA_INPUT_KEY_KEYPAD4 = 36,
  NEXORA_INPUT_KEY_KEYPAD5 = 37,
  NEXORA_INPUT_KEY_KEYPAD6 = 38,
  NEXORA_INPUT_KEY_KEYPAD7 = 39,
  NEXORA_INPUT_KEY_KEYPAD8 = 40,
  NEXORA_INPUT_KEY_KEYPAD9 = 41,
  NEXORA_INPUT_KEY_KEYPAD_DECIMAL = 42,
  NEXORA_INPUT_KEY_KEYPAD_DIVIDE = 43,
  NEXORA_INPUT_KEY_KEYPAD_MULTIPLY = 44,
  NEXORA_INPUT_KEY_KEYPAD_SUBTRACT = 45,
  NEXORA_INPUT_KEY_KEYPAD_ADD = 46,
  NEXORA_INPUT_KEY_KEYPAD_ENTER = 47,
  NEXORA_INPUT_KEY_KEYPAD_EQUAL = 48,
  NEXORA_INPUT_KEY_LEFT_SHIFT = 49,
  NEXORA_INPUT_KEY_LEFT_CONTROL = 50,
  NEXORA_INPUT_KEY_LEFT_ALT = 51,
  NEXORA_INPUT_KEY_LEFT_SUPER = 52,
  NEXORA_INPUT_KEY_RIGHT_SHIFT = 53,
  NEXORA_INPUT_KEY_RIGHT_CONTROL = 54,
  NEXORA_INPUT_KEY_RIGHT_ALT = 55,
  NEXORA_INPUT_KEY_RIGHT_SUPER = 56,
  NEXORA_INPUT_KEY_MENU = 57,
  NEXORA_INPUT_KEY_DIGIT0 = 58,
  NEXORA_INPUT_KEY_DIGIT1 = 59,
  NEXORA_INPUT_KEY_DIGIT2 = 60,
  NEXORA_INPUT_KEY_DIGIT3 = 61,
  NEXORA_INPUT_KEY_DIGIT4 = 62,
  NEXORA_INPUT_KEY_DIGIT5 = 63,
  NEXORA_INPUT_KEY_DIGIT6 = 64,
  NEXORA_INPUT_KEY_DIGIT7 = 65,
  NEXORA_INPUT_KEY_DIGIT8 = 66,
  NEXORA_INPUT_KEY_DIGIT9 = 67,
  NEXORA_INPUT_KEY_A = 68,
  NEXORA_INPUT_KEY_B = 69,
  NEXORA_INPUT_KEY_C = 70,
  NEXORA_INPUT_KEY_D = 71,
  NEXORA_INPUT_KEY_E = 72,
  NEXORA_INPUT_KEY_F = 73,
  NEXORA_INPUT_KEY_G = 74,
  NEXORA_INPUT_KEY_H = 75,
  NEXORA_INPUT_KEY_I = 76,
  NEXORA_INPUT_KEY_J = 77,
  NEXORA_INPUT_KEY_K = 78,
  NEXORA_INPUT_KEY_L = 79,
  NEXORA_INPUT_KEY_M = 80,
  NEXORA_INPUT_KEY_N = 81,
  NEXORA_INPUT_KEY_O = 82,
  NEXORA_INPUT_KEY_P = 83,
  NEXORA_INPUT_KEY_Q = 84,
  NEXORA_INPUT_KEY_R = 85,
  NEXORA_INPUT_KEY_S = 86,
  NEXORA_INPUT_KEY_T = 87,
  NEXORA_INPUT_KEY_U = 88,
  NEXORA_INPUT_KEY_V = 89,
  NEXORA_INPUT_KEY_W = 90,
  NEXORA_INPUT_KEY_X = 91,
  NEXORA_INPUT_KEY_Y = 92,
  NEXORA_INPUT_KEY_Z = 93,
  NEXORA_INPUT_KEY_F1 = 94,
  NEXORA_INPUT_KEY_F2 = 95,
  NEXORA_INPUT_KEY_F3 = 96,
  NEXORA_INPUT_KEY_F4 = 97,
  NEXORA_INPUT_KEY_F5 = 98,
  NEXORA_INPUT_KEY_F6 = 99,
  NEXORA_INPUT_KEY_F7 = 100,
  NEXORA_INPUT_KEY_F8 = 101,
  NEXORA_INPUT_KEY_F9 = 102,
  NEXORA_INPUT_KEY_F10 = 103,
  NEXORA_INPUT_KEY_F11 = 104,
  NEXORA_INPUT_KEY_F12 = 105,
} NexoraGameplayInputKey;
typedef struct NexoraGameplayInputBindingV1 {
  uint32_t struct_size;
  uint32_t abi_version;
  uint32_t device;
  uint32_t device_id;
  uint32_t control;
  float scale;
  float deadzone;
  uint32_t reserved;
} NexoraGameplayInputBindingV1;
typedef struct NexoraGameplayActionStateV1 {
  uint32_t struct_size;
  uint32_t abi_version;
  uint64_t sequence;
  float value;
  uint32_t held;
  uint32_t pressed;
  uint32_t released;
} NexoraGameplayActionStateV1;
typedef struct NexoraGameplayDeviceStateV1 {
  uint32_t struct_size;
  uint32_t abi_version;
  uint64_t sequence;
  uint32_t active_device;
  uint32_t device_id;
  uint32_t focused;
  uint32_t reserved;
} NexoraGameplayDeviceStateV1;
typedef struct NexoraGameplayPointerStateV1 {
  uint32_t struct_size;
  uint32_t abi_version;
  uint64_t sequence;
  double x;
  double y;
  double delta_x;
  double delta_y;
  double wheel_x;
  double wheel_y;
  uint32_t buttons;
  uint32_t focused;
} NexoraGameplayPointerStateV1;
typedef struct NexoraGameplayInputApiV1 {
  uint32_t struct_size;
  uint32_t abi_version;
  void *context;
  int32_t (*read_action)(void *context, uint32_t user, uint32_t action,
                         NexoraGameplayActionStateV1 *output);
  int32_t (*bind)(void *context, uint32_t user, uint32_t action,
                  const NexoraGameplayInputBindingV1 *binding);
  int32_t (*clear_bindings)(void *context, uint32_t user, uint32_t action);
  int32_t (*read_device)(void *context, uint32_t user, NexoraGameplayDeviceStateV1 *output);
  int32_t (*read_pointer)(void *context, uint32_t user, NexoraGameplayPointerStateV1 *output);
} NexoraGameplayInputApiV1;
#ifdef __cplusplus
}
#endif
#endif
