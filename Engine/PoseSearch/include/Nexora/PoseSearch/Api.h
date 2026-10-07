#pragma once

#if defined(NEXORA_POSE_SEARCH_STATIC)
#define NEXORA_POSE_SEARCH_API
#elif defined(_WIN32)
#if defined(NEXORA_POSE_SEARCH_EXPORTS)
#define NEXORA_POSE_SEARCH_API __declspec(dllexport)
#else
#define NEXORA_POSE_SEARCH_API __declspec(dllimport)
#endif
#else
#define NEXORA_POSE_SEARCH_API __attribute__((visibility("default")))
#endif
