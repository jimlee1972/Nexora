#include "Nexora/Editor/TwoDAssetImport.h"

namespace nexora::editor {
TwoDAssetImportResult ImportTwoDAsset(std::string_view type, std::span<const std::byte> source,
                                      runtime::AssetUuid expected,
                                      const std::function<bool()> &cancelled) {
  if (cancelled && cancelled())
    return {{}, {}, true};
  if (source.size() > kMaximumTwoDSourceBytes)
    return {{}, "2D source exceeds the 64 MiB limit.", false};
  auto payload = std::shared_ptr<TwoDAssetPayload>(new TwoDAssetPayload);
  payload->retained_bytes_ = sizeof(TwoDAssetPayload);
  if (type == ".nx2atlas") {
    auto atlas = twod::DecodeAtlas(source);
    if (!atlas)
      return {{}, "Invalid 2D Atlas source.", false};
    payload->retained_bytes_ += sizeof(twod::Atlas) + atlas->rgba.capacity() +
                                atlas->regions.capacity() * sizeof(twod::Region);
    payload->atlas_ = std::make_shared<const twod::Atlas>(std::move(*atlas));
  } else if (type == ".nx2clip") {
    auto clip = twod::DecodeClip(source);
    if (!clip)
      return {{}, "Invalid 2D Clip source.", false};
    payload->retained_bytes_ +=
        sizeof(twod::Clip) + clip->frames.capacity() * sizeof(twod::AnimationFrame);
    for (const auto &frame : clip->frames)
      payload->retained_bytes_ += frame.event.capacity() + 1;
    payload->clip_ = std::make_shared<const twod::Clip>(std::move(*clip));
  } else {
    return {{}, "Unsupported 2D source importer type.", false};
  }
  if (cancelled && cancelled())
    return {{}, {}, true};
  if (payload->Id() == runtime::AssetUuid{} ||
      (expected != runtime::AssetUuid{} && payload->Id() != expected))
    return {{}, "2D source UUID does not match its asset identity sidecar.", false};
  if (payload->RetainedBytes() > kMaximumWorkspaceTwoDBytes)
    return {{}, "2D payload exceeds the 64 MiB workspace budget.", false};
  return {std::move(payload), {}, false};
}
} // namespace nexora::editor
