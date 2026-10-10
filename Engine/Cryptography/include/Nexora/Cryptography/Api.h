#pragma once
#if defined(NEXORA_CRYPTOGRAPHY_STATIC)
#define NEXORA_CRYPTOGRAPHY_API
#elif defined(_WIN32)
#if defined(NEXORA_CRYPTOGRAPHY_EXPORTS)
#define NEXORA_CRYPTOGRAPHY_API __declspec(dllexport)
#else
#define NEXORA_CRYPTOGRAPHY_API __declspec(dllimport)
#endif
#else
#define NEXORA_CRYPTOGRAPHY_API __attribute__((visibility("default")))
#endif
