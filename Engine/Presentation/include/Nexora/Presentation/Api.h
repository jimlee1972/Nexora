#pragma once

#if defined(NEXORA_PRESENTATION_STATIC)
#define NEXORA_PRESENTATION_API
#elif defined(_WIN32)
#if defined(NEXORA_PRESENTATION_EXPORTS)
#define NEXORA_PRESENTATION_API __declspec(dllexport)
#else
#define NEXORA_PRESENTATION_API __declspec(dllimport)
#endif
#else
#define NEXORA_PRESENTATION_API __attribute__((visibility("default")))
#endif
