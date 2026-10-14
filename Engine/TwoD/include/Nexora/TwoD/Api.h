#pragma once
#if defined(NEXORA_2D_STATIC)
#define NEXORA_2D_API
#elif defined(_WIN32)
#if defined(NEXORA_2D_EXPORTS)
#define NEXORA_2D_API __declspec(dllexport)
#else
#define NEXORA_2D_API __declspec(dllimport)
#endif
#else
#define NEXORA_2D_API __attribute__((visibility("default")))
#endif
