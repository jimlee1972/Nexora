#pragma once

#if defined(NEXORA_FOUNDATION_STATIC)
#define NEXORA_FOUNDATION_API
#elif defined(_WIN32)
#if defined(NEXORA_FOUNDATION_EXPORTS)
#define NEXORA_FOUNDATION_API __declspec(dllexport)
#else
#define NEXORA_FOUNDATION_API __declspec(dllimport)
#endif
#else
#define NEXORA_FOUNDATION_API __attribute__((visibility("default")))
#endif
