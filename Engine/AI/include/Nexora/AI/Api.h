#pragma once

#if defined(NEXORA_AI_STATIC)
#define NEXORA_AI_API
#elif defined(_WIN32)
#if defined(NEXORA_AI_EXPORTS)
#define NEXORA_AI_API __declspec(dllexport)
#else
#define NEXORA_AI_API __declspec(dllimport)
#endif
#else
#define NEXORA_AI_API __attribute__((visibility("default")))
#endif
