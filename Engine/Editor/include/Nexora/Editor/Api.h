#pragma once

#if defined(NEXORA_EDITOR_STATIC)
#define NEXORA_EDITOR_API
#elif defined(_WIN32)
#if defined(NEXORA_EDITOR_EXPORTS)
#define NEXORA_EDITOR_API __declspec(dllexport)
#else
#define NEXORA_EDITOR_API __declspec(dllimport)
#endif
#else
#define NEXORA_EDITOR_API __attribute__((visibility("default")))
#endif
