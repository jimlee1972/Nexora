#pragma once

// API-M6 (Roadmap/*/Engine_API_*Roadmap.md section 3.6): wires the
// already-declared NexoraGameplayHostV2 C ABI (Nexora/Foundation/
// GameplayABI.h) to real nexora::game::GameWorld state, instead of that
// struct's read_component/write_component/log/subscribe_event/
// set_tick_enabled slots only ever being filled by test-scoped stand-ins
// (see Gameplay/Zig/ZigGameplayTests.cpp's HostState, which is exactly
// that: a fake host with its own private component_value, unrelated to any
// real World). A Zig or C game module built against this host can now
// actually read and write an entity's Transform through the supported
// public API rather than an engine-internal pointer.
//
// Scope of this pass: only the Transform component type is wired, and `log`
// is now wired to a real core::AsyncLogService (previously a pure no-op that
// silently dropped every module log call). subscribe_event and
// set_tick_enabled remain no-ops, for a reason beyond "not built yet": the
// ABI itself has no callback slot for the host to call back into the module
// (subscribe_event's signature is `(context, event_type) -> int32_t`, with
// no function pointer to invoke when that event later fires), and
// set_tick_enabled's effect (suppressing GameplayModuleHost::Update calls)
// would require either GameplayModuleHost to depend on this Game-namespace
// context or a new decoupled primitive threaded through both -- either is a
// real design decision, not a wiring gap, and belongs in its own pass, not
// this one. Camera/light/mesh-renderer component types are not exposed
// through this bridge yet. Extending read_component/write_component to
// another component type means adding another case alongside
// TransformComponentType()'s, not redesigning this file. The existing
// Gameplay/Zig/src/game_module.zig sample and its test were NOT changed to
// consume this bridge -- that would require rebuilding and re-verifying the
// Zig object, and no Zig toolchain is available to do that in this
// environment; it remains open follow-up work.
#include "Nexora/Core/Log.h"
#include "Nexora/Foundation/GameplayABI.h"
#include "Nexora/Game/GameWorld.h"
#include "Nexora/Runtime/Api.h"

#include <cstdint>

namespace nexora::game {

// Stable FNV-1a hash of "Nexora.Transform" (nexora::foundation::Name),
// shared by any C++ caller and by a game module across the ABI boundary, so
// neither side hardcodes a magic numeric component_type.
[[nodiscard]] NEXORA_RUNTIME_API std::uint64_t TransformComponentType() noexcept;

// The wire format read_component/write_component exchange for
// TransformComponentType(): three tightly packed doubles. This is
// deliberately not nexora::runtime::Transform's in-memory layout (which
// carries no ABI-stability guarantee of its own) -- the bridge copies field
// by field in both directions, so it keeps working even if Transform gains
// a member; only this struct's own shape is the actual wire contract.
struct GameplayTransformWire final {
  double x{}, y{}, z{};
};

// context for a NexoraGameplayHostV2 built by MakeHost(): `world` must
// outlive every NexoraGameModuleV2 built against that host, since the host's
// callbacks read through this pointer on every read_component/
// write_component call.
// A plain C++ type, not part of the stable, versioned C ABI surface
// (NexoraGameplayHostV2/NexoraGameModuleV2, which carry struct_size and an
// abi_version this repo's ABI gate checks) -- like every other Runtime C++
// facade type (GameWorld, EntitySpawnDescriptor, EntitySnapshot, ...), it
// has no struct_size of its own and callers must be rebuilt against the
// current header whenever it changes, exactly as they must for those
// types. This is a deliberate scope boundary, not an oversight: giving
// GameplayHostContext its own struct_size/versioning would only be
// consistent if every sibling C++ facade type got the same treatment,
// which is a real, larger design decision (a genuine ABI surface for the
// Game-namespace C++ facade, not just this one struct) that belongs in its
// own pass, not something to retrofit onto a single type in passing.
struct GameplayHostContext final {
  GameWorld *world{};
  // Optional: when null (the default), the host's `log` callback silently
  // drops every module log call, exactly as it did before this field
  // existed. When set, `log` forwards to this service instead.
  core::AsyncLogService *log{};
};

// Builds a NexoraGameplayHostV2 whose context is `&context`. Every callback
// pointer is a plain free function (no captures), matching the ABI's
// function-pointer-only shape; state lives in *context, not in a closure.
[[nodiscard]] NEXORA_RUNTIME_API NexoraGameplayHostV2
MakeHost(GameplayHostContext &context) noexcept;

} // namespace nexora::game
