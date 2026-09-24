#pragma once

#if defined(NEXORA_EDITOR_IMGUI_STATIC)
#define NEXORA_EDITOR_IMGUI_API
#elif defined(_WIN32)
#if defined(NEXORA_EDITOR_IMGUI_EXPORTS)
#define NEXORA_EDITOR_IMGUI_API __declspec(dllexport)
#else
#define NEXORA_EDITOR_IMGUI_API __declspec(dllimport)
#endif
#else
#define NEXORA_EDITOR_IMGUI_API __attribute__((visibility("default")))
#endif
