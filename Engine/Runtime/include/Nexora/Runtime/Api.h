#pragma once

#if defined(NEXORA_RUNTIME_STATIC)
#define NEXORA_RUNTIME_API
#elif defined(_WIN32)
#if defined(NEXORA_RUNTIME_EXPORTS)
#define NEXORA_RUNTIME_API __declspec(dllexport)
#else
#define NEXORA_RUNTIME_API __declspec(dllimport)
#endif
#else
#define NEXORA_RUNTIME_API __attribute__((visibility("default")))
#endif
