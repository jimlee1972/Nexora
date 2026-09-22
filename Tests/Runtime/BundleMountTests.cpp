// API-M3 conformance: MountBundle actually projects a real V1-M5 Bundle onto
// a real VirtualFileSystem memory mount, and a caller can read an asset back
// out through the VFS and deserialize it -- not two isolated contracts that
// happen to compile against each other.
#include "Nexora/Core/JobSystem.h"
#include "Nexora/Core/Vfs.h"
#include "Nexora/Runtime/AssetPipeline.h"
#include "Nexora/Runtime/BundleMount.h"

#include <iostream>
#include <stdexcept>

namespace {
using namespace nexora::runtime;
using namespace nexora::core;

void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}

ByteBuffer Bytes(std::string_view text) {
  return {reinterpret_cast<const std::byte *>(text.data()),
          reinterpret_cast<const std::byte *>(text.data() + text.size())};
}

int Run() {
  const auto hero = AssetUuid::Parse("01234567-89ab-cdef-0123-456789abcdef");
  const auto sidekick = AssetUuid::Parse("11223344-5566-7788-99aa-bbccddeeff00");
  Require(hero.has_value() && sidekick.has_value(), "test UUIDs invalid");

  DerivedDataCache cache;
  AssetCooker cooker;
  const CanonicalAsset hero_canonical{*hero, "mesh", {}, Bytes("hero payload")};
  const CanonicalAsset sidekick_canonical{*sidekick, "mesh", {}, Bytes("sidekick payload")};
  const auto hero_blob = cooker.Cook(hero_canonical, "test", "quality=high", cache);
  const auto sidekick_blob = cooker.Cook(sidekick_canonical, "test", "quality=high", cache);
  Require(hero_blob.has_value() && sidekick_blob.has_value(), "cook failed");

  const auto bundle =
      BundleBuilder::Build("mount-test.bundle", 1, {}, {*hero_blob, *sidekick_blob});
  Require(bundle.has_value() && BundleBuilder::Verify(*bundle), "bundle build/verify failed");

  JobSystem jobs{1};
  VirtualFileSystem vfs{jobs};

  // ---- Real round trip: mount, read back through the VFS, deserialize ----
  Require(MountBundle(vfs, "bundle", *bundle), "MountBundle must succeed for a verified bundle");
  const auto hero_path = std::string("bundle://") + hero->ToString() + ".blob";
  const auto hero_read = vfs.Read(hero_path);
  Require(hero_read.status == ReadResult::Status::Completed, "mounted asset must be readable");
  const auto hero_decoded = AssetCooker::Deserialize(hero_read.bytes);
  Require(hero_decoded.has_value() && hero_decoded->payload == hero_blob->payload &&
              hero_decoded->id == *hero,
          "the bytes read back through the VFS must deserialize to the original blob");
  const auto sidekick_path = std::string("bundle://") + sidekick->ToString() + ".blob";
  Require(vfs.Read(sidekick_path).status == ReadResult::Status::Completed,
          "every asset in the bundle must be mounted, not just the first");

  const auto [enumerate_status, entries] = vfs.Enumerate("bundle://");
  Require(enumerate_status == ReadResult::Status::Completed && entries.size() == 2,
          "enumerating the bundle mount must list exactly the assets it contains");

  // ---- A corrupt bundle must not become browsable VFS content ----
  auto damaged = *bundle;
  damaged.data.back() ^= std::byte{1};
  Require(!MountBundle(vfs, "damaged", damaged),
          "MountBundle must refuse a bundle that fails BundleBuilder::Verify");
  Require(vfs.MountMemory("damaged"),
          "a rejected MountBundle must not have left a mount behind under that name");

  // ---- An already-mounted name is rejected without disturbing it ----
  Require(!MountBundle(vfs, "bundle", *bundle),
          "MountBundle must fail, not silently overwrite, an already-mounted name");
  Require(vfs.Read(hero_path).status == ReadResult::Status::Completed,
          "a rejected re-mount must leave the existing mount's content intact");

  return 0;
}
} // namespace

int main() {
  try {
    return Run();
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
