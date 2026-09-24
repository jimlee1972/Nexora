#pragma once

#if defined(NEXORA_WINDOW_STATIC)
#define NEXORA_WINDOW_API
#elif defined(_WIN32)
#if defined(NEXORA_WINDOW_EXPORTS)
#define NEXORA_WINDOW_API __declspec(dllexport)
#else
#define NEXORA_WINDOW_API __declspec(dllimport)
#endif
#else
#define NEXORA_WINDOW_API __attribute__((visibility("default")))
#endif
