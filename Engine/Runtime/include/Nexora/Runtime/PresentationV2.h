#pragma once

#include "Nexora/Runtime/InputUi.h"
#include "Nexora/Runtime/Presentation.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace nexora::runtime::presentation_v2 {

struct TimelineKey final {
  double time{};
  float value{};
};

struct TimelineTrack final {
  std::string binding;
  std::vector<TimelineKey> keys;
};

class NEXORA_RUNTIME_API Timeline final {
public:
  explicit Timeline(double duration = 0.0) noexcept;
  bool SetDuration(double duration) noexcept;
  bool AddTrack(TimelineTrack track);
  bool Seek(double time) noexcept;
  [[nodiscard]] double Time() const noexcept { return time_; }
  [[nodiscard]] double Duration() const noexcept { return duration_; }
  [[nodiscard]] std::optional<float> Evaluate(std::string_view binding) const noexcept;

private:
  double duration_{};
  double time_{};
  std::vector<TimelineTrack> tracks_;
};

struct CameraState final {
  float yaw{};
  float pitch{};
  float distance{1.0F};
  float field_of_view{60.0F};
};
struct CameraRigLayer final {
  std::int32_t priority{};
  float weight{1.0F};
  CameraState state{};
};
class NEXORA_RUNTIME_API CameraRig final {
public:
  bool SetLayers(std::vector<CameraRigLayer> layers);
  [[nodiscard]] std::optional<CameraState> Resolve() const noexcept;

private:
  std::vector<CameraRigLayer> layers_;
};

enum class FlexDirection { Row, Column };
struct FlexItem final {
  UIElementId id{};
  float basis{};
  float grow{};
};
struct GridItem final {
  UIElementId id{};
  std::size_t column{};
  std::size_t row{};
  std::size_t column_span{1};
  std::size_t row_span{1};
};
struct LayoutBox final {
  UIElementId id{};
  Rect rect{};
};
[[nodiscard]] NEXORA_RUNTIME_API std::vector<LayoutBox>
ResolveFlex(Rect container, std::span<const FlexItem> items, FlexDirection direction,
            float gap = 0.0F);
[[nodiscard]] NEXORA_RUNTIME_API std::vector<LayoutBox>
ResolveGrid(Rect container, std::size_t columns, std::size_t rows,
            std::span<const GridItem> items, float column_gap = 0.0F, float row_gap = 0.0F);

struct RichTextSpan final {
  std::string value;
  bool localization_key{};
  std::string style_class;
};
class NEXORA_RUNTIME_API RichText final {
public:
  void Add(RichTextSpan span);
  [[nodiscard]] std::string Resolve(const LocalizationTable &localization) const;
  [[nodiscard]] std::span<const RichTextSpan> Spans() const noexcept { return spans_; }

private:
  std::vector<RichTextSpan> spans_;
};

struct Style final {
  std::optional<float> opacity;
  std::optional<float> font_scale;
  std::optional<std::string> color_token;
};
struct ResolvedStyle final {
  float opacity{1.0F};
  float font_scale{1.0F};
  std::string color;
};
class NEXORA_RUNTIME_API Theme final {
public:
  bool SetColor(std::string token, std::string value);
  [[nodiscard]] std::string ResolveColor(std::string_view token) const;

private:
  std::unordered_map<std::string, std::string> colors_;
};
class NEXORA_RUNTIME_API StyleSheet final {
public:
  bool SetRule(std::string style_class, Style style);
  [[nodiscard]] ResolvedStyle Resolve(std::span<const std::string> classes,
                                      const Style &inline_style,
                                      const Theme &theme) const;

private:
  std::unordered_map<std::string, Style> rules_;
};

enum class AccessibilityRole { Generic, Button, Label, Image, TextField, List, ListItem };
struct AccessibilityNode final {
  UIElementId id{};
  UIElementId parent{};
  AccessibilityRole role{AccessibilityRole::Generic};
  std::string name;
  bool enabled{true};
  bool focusable{};
};
class NEXORA_RUNTIME_API AccessibilityTree final {
public:
  bool SetNodes(std::vector<AccessibilityNode> nodes);
  [[nodiscard]] const AccessibilityNode *Find(UIElementId id) const noexcept;
  [[nodiscard]] std::vector<UIElementId> ReadingOrder() const;

private:
  std::vector<AccessibilityNode> nodes_;
};

struct SurfaceRay final {
  presentation::Vec3 origin{};
  presentation::Vec3 direction{};
};
struct SurfaceVertex final {
  presentation::Vec3 position{};
  float u{};
  float v{};
};
struct SurfacePointerHit final {
  float distance{};
  float u{};
  float v{};
  UIPointerEvent event{};
};
[[nodiscard]] NEXORA_RUNTIME_API std::optional<SurfacePointerHit>
ProjectSurfacePointer(const SurfaceRay &ray, const SurfaceVertex &a, const SurfaceVertex &b,
                      const SurfaceVertex &c, Rect ui_rect, PointerId pointer, bool pressed);

class NEXORA_RUNTIME_API RoomAudioGraph final {
public:
  bool AddPortal(std::uint64_t room_a, std::uint64_t room_b, float transmission);
  [[nodiscard]] float Transmission(std::uint64_t source_room,
                                   std::uint64_t listener_room) const;

private:
  struct Portal final {
    std::uint64_t a{}, b{};
    float transmission{};
  };
  std::vector<Portal> portals_;
};

enum class MediaProtocol { LocalFile, Hls, Dash };
struct MediaSegment final {
  std::uint64_t sequence{};
  double start{};
  double duration{};
  std::string uri;
};
class NEXORA_RUNTIME_API AdaptiveMediaStream final {
public:
  bool Configure(MediaProtocol protocol, std::vector<MediaSegment> segments);
  bool Seek(double time, presentation::VideoPlayer &player) noexcept;
  [[nodiscard]] std::optional<MediaSegment> SegmentFor(double time) const;
  bool SubmitDecoded(presentation::VideoPlayer &player,
                     presentation::DecodedVideoFrame frame) const;
  [[nodiscard]] MediaProtocol Protocol() const noexcept { return protocol_; }

private:
  MediaProtocol protocol_{MediaProtocol::LocalFile};
  std::vector<MediaSegment> segments_;
};

#if NEXORA_MEDIA_DRM_ENABLED
class NEXORA_RUNTIME_API IDrmProvider {
public:
  virtual ~IDrmProvider() = default;
  [[nodiscard]] virtual bool Authorize(std::string_view key_id) = 0;
};
#endif

#if NEXORA_CAPTURE_ENCODER_ENABLED
class NEXORA_RUNTIME_API ICaptureEncoder {
public:
  virtual ~ICaptureEncoder() = default;
  virtual bool SubmitVideoFrame(presentation::ResourceId texture, double presentation_time) = 0;
};
#endif

} // namespace nexora::runtime::presentation_v2
