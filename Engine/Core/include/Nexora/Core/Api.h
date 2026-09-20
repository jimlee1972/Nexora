#pragma once

#if defined(NEXORA_CORE_STATIC)
#define NEXORA_CORE_API
#elif defined(_WIN32)
#if defined(NEXORA_CORE_EXPORTS)
#define NEXORA_CORE_API __declspec(dllexport)
#else
#define NEXORA_CORE_API __declspec(dllimport)
#endif
#else
#define NEXORA_CORE_API __attribute__((visibility("default")))
#endif
