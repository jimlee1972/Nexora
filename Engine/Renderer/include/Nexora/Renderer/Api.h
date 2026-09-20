#pragma once
#if defined(NEXORA_RENDERER_STATIC)
#define NEXORA_RENDERER_API
#elif defined(_WIN32)
#if defined(NEXORA_RENDERER_EXPORTS)
#define NEXORA_RENDERER_API __declspec(dllexport)
#else
#define NEXORA_RENDERER_API __declspec(dllimport)
#endif
#else
#define NEXORA_RENDERER_API __attribute__((visibility("default")))
#endif
