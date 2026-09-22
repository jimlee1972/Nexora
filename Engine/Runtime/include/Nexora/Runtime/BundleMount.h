#pragma once

// API-M3 (Roadmap/*/Engine_API_*Roadmap.md section 3.3) bundle-backend
// deliverable: connects the already-real V1-M5 bundle format
// (AssetPipeline.h's Bundle/BundleBuilder) to the already-real API-M3
// VirtualFileSystem memory backend, instead of leaving "bundle backend" as
// an unimplemented item in both READMEs. No changes to VirtualFileSystem
// itself are needed or made -- Core must not depend on Runtime, so this
// lives here, as a Runtime-side projection of a Bundle onto a VFS mount
// the caller already owns.
#include "Nexora/Core/Vfs.h"
#include "Nexora/Runtime/Api.h"
#include "Nexora/Runtime/AssetPipeline.h"

#include <string_view>

namespace nexora::runtime {

// Verifies `bundle` (BundleBuilder::Verify -- corrupt bytes, overlapping or
// duplicate entries, or an asset whose offset/size runs past the bundle's
// data never become browsable VFS content), then mounts a fresh in-memory
// VFS mount named `mount_name` (via VirtualFileSystem::MountMemory) with one
// file per asset at `<mount_name>://<uuid>.blob`, where `<uuid>` is the
// asset's canonical `AssetUuid::ToString()` and the file's bytes are exactly
// the asset's serialized RuntimeBlob range -- the same bytes
// AssetCooker::Serialize produced and AssetCooker::Deserialize consumes.
// This makes "read a cooked asset out of a shipped bundle" a real,
// exercisable path: vfs.Read(...) then AssetCooker::Deserialize(bytes),
// with no separate bundle-specific read API.
//
// Returns false and leaves no mount behind (unmounting first if any file
// write already happened) if verification fails, `mount_name` is already
// mounted, or any write fails.
[[nodiscard]] NEXORA_RUNTIME_API bool
MountBundle(core::VirtualFileSystem &vfs, std::string_view mount_name, const Bundle &bundle);

} // namespace nexora::runtime
