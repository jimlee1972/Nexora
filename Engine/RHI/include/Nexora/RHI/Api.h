#pragma once
#if defined(NEXORA_RHI_STATIC)
#define NEXORA_RHI_API
#elif defined(_WIN32)
#if defined(NEXORA_RHI_EXPORTS)
#define NEXORA_RHI_API __declspec(dllexport)
#else
#define NEXORA_RHI_API __declspec(dllimport)
#endif
#else
#define NEXORA_RHI_API __attribute__((visibility("default")))
#endif
