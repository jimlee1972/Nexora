#pragma once

#if defined(NEXORA_ANIMATION_STATIC)
#define NEXORA_ANIMATION_API
#elif defined(_WIN32)
#if defined(NEXORA_ANIMATION_EXPORTS)
#define NEXORA_ANIMATION_API __declspec(dllexport)
#else
#define NEXORA_ANIMATION_API __declspec(dllimport)
#endif
#else
#define NEXORA_ANIMATION_API __attribute__((visibility("default")))
#endif
