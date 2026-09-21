#pragma once

// Every Nexora target builds with hidden symbol visibility by default
// (nexora_apply_defaults), including plugin modules such as
// Plugins/Example. A plugin's ABI entry point must still be resolvable by
// PluginHost's dlopen/dlsym (LoadLibrary/GetProcAddress on Windows), so it
// needs default visibility / dllexport even though nothing else in the
// plugin does. This macro is the one piece of engine-provided plumbing a
// third-party plugin needs; everything else about the ABI contract is just
// the exported function's own signature.

#if defined(_WIN32)
#define NEXORA_PLUGIN_ABI_EXPORT extern "C" __declspec(dllexport)
#else
#define NEXORA_PLUGIN_ABI_EXPORT extern "C" __attribute__((visibility("default")))
#endif
