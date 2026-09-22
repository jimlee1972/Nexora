#include "Nexora/Runtime/BundleMount.h"

#include <span>
#include <string>

namespace nexora::runtime {

bool MountBundle(core::VirtualFileSystem &vfs, std::string_view mount_name, const Bundle &bundle) {
  if (!BundleBuilder::Verify(bundle))
    return false;
  if (!vfs.MountMemory(mount_name))
    return false;
  for (const auto &asset : bundle.manifest.assets) {
    if (asset.offset > bundle.data.size() || asset.size > bundle.data.size() - asset.offset) {
      vfs.Unmount(mount_name);
      return false;
    }
    const std::span<const std::byte> bytes{bundle.data.data() + asset.offset, asset.size};
    const std::string path = std::string(mount_name) + "://" + asset.id.ToString() + ".blob";
    if (vfs.WriteAtomic(path, bytes) != core::ReadResult::Status::Completed) {
      vfs.Unmount(mount_name);
      return false;
    }
  }
  return true;
}

} // namespace nexora::runtime
