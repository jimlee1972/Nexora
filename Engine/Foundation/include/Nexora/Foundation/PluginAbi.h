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

// A plugin that wants to expose something to the engine (not just prove ABI
// compatibility) exports a second, optional symbol, "NexoraPluginRegister",
// with this signature. PluginHost calls it once, after the ABI check
// passes, with a `register_service` callback the plugin invokes once per
// service it wants to publish. `context` is opaque to the plugin -- it only
// routes the callback back to the specific registry that loaded it -- and
// every type crossing this boundary is a plain C function pointer or
// pointer/string, the same discipline Nexora/Foundation/GameplayABI.h uses,
// so the plugin side of the contract never needs a Nexora::Runtime type
// (such as ServiceRegistry) even though a real one sits behind the callback
// on the host side.

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*NexoraServiceRegisterCallback)(void *context, const char *name, void *service);
typedef void (*NexoraPluginRegisterFn)(void *context,
                                       NexoraServiceRegisterCallback register_service);

#ifdef __cplusplus
}
#endif
