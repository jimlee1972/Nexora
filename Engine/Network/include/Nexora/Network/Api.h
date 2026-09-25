#pragma once

#if defined(NEXORA_NETWORK_STATIC)
#define NEXORA_NETWORK_API
#elif defined(_WIN32)
#if defined(NEXORA_NETWORK_EXPORTS)
#define NEXORA_NETWORK_API __declspec(dllexport)
#else
#define NEXORA_NETWORK_API __declspec(dllimport)
#endif
#else
#define NEXORA_NETWORK_API __attribute__((visibility("default")))
#endif
