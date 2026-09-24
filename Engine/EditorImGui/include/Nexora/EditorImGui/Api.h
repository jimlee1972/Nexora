#pragma once

#if defined(_WIN32) && !defined(NEXORA_EDITOR_IMGUI_STATIC)
#if defined(NEXORA_EDITOR_IMGUI_EXPORTS)
#define NEXORA_EDITOR_IMGUI_API __declspec(dllexport)
#else
#define NEXORA_EDITOR_IMGUI_API __declspec(dllimport)
#endif
#else
#define NEXORA_EDITOR_IMGUI_API
#endif
