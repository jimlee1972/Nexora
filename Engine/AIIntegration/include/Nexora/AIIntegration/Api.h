#pragma once

#if defined(_WIN32)
#if defined(NEXORA_AI_INTEGRATION_STATIC)
#define NEXORA_AI_INTEGRATION_API
#elif defined(NEXORA_AI_INTEGRATION_EXPORTS)
#define NEXORA_AI_INTEGRATION_API __declspec(dllexport)
#else
#define NEXORA_AI_INTEGRATION_API __declspec(dllimport)
#endif
#else
#define NEXORA_AI_INTEGRATION_API __attribute__((visibility("default")))
#endif
