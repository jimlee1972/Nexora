#pragma once

#include <stdint.h>

// Every Nexora target builds with hidden symbol visibility by default
// (nexora_apply_defaults), including plugin modules such as
// Plugins/Example. A plugin's ABI entry point must still be resolvable by
// PluginHost's dlopen/dlsym (LoadLibrary/GetProcAddress on Windows), so it
// needs default visibility / dllexport even though nothing else in the
// plugin does. This macro is the one piece of engine-provided plumbing a
// third-party plugin needs; everything else about the ABI contract is just
// the exported function's own signature.

#ifdef __cplusplus
#define NEXORA_PLUGIN_C_LINKAGE extern "C"
#else
#define NEXORA_PLUGIN_C_LINKAGE extern
#endif
#if defined(_WIN32)
#define NEXORA_PLUGIN_ABI_EXPORT NEXORA_PLUGIN_C_LINKAGE __declspec(dllexport)
#else
#define NEXORA_PLUGIN_ABI_EXPORT NEXORA_PLUGIN_C_LINKAGE __attribute__((visibility("default")))
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

// Optional export NexoraPluginGetLifecycleV1(requested_schema, lifecycle). Host initializes
// struct_size to its writable capacity; plugins must never write beyond that capacity. Getter
// returns zero on success and must not activate work. Schema one requires the complete prefix
// below; unknown future suffixes are ignored. Callbacks/context are borrowed until native unload.
// Registration callback/context are valid only during synchronous NexoraPluginRegister.
#define NEXORA_PLUGIN_LIFECYCLE_SCHEMA_V1 1U
typedef struct NexoraPluginLifecycleV1 {
  uint32_t struct_size;
  uint32_t schema_version;
  void *context;
  // Zero accepts shutdown; any other result requires restart. Must not throw across this ABI.
  int32_t (*request_shutdown)(void *context);
  // One proves all plugin work/calls quiescent, zero is pending, any other result requires restart.
  // Host consumers release borrowed service pointers before requesting shutdown.
  int32_t (*poll_quiescence)(void *context);
} NexoraPluginLifecycleV1;
typedef int32_t (*NexoraPluginGetLifecycleV1Fn)(uint32_t requested_schema,
                                                NexoraPluginLifecycleV1 *lifecycle);

#ifdef __cplusplus
}
#endif
