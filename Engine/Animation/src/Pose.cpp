#include "Nexora/Animation/Pose.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <limits>
#include <utility>

namespace nexora::animation {
namespace {
constexpr std::size_t HeaderSize = 16, BoundsSize = 48, SampleSize = 20;
using Channels = std::array<float, 6>;
using Bounds = std::array<Channels, 2>;
Channels Values(const JointPose &p) {
  return {p.translation.x, p.translation.y, p.translation.z, p.scale.x, p.scale.y, p.scale.z};
}
double Norm(math::Quaternion q) {
  return std::hypot(std::hypot(double(q.x), double(q.y)), std::hypot(double(q.z), double(q.w)));
}
math::Quaternion Canonical(math::Quaternion q) {
  const double n = Norm(q);
  q = {float(q.x / n), float(q.y / n), float(q.z / n), float(q.w / n)};
  // The first nonzero component in W,Z,Y,X order is positive: q and -q share bytes.
  const std::array<float, 4> sign{q.w, q.z, q.y, q.x};
  for (float v : sign) {
    if (v != 0) {
      if (v < 0)
        q = {-q.x, -q.y, -q.z, -q.w};
      break;
    }
  }
  return q;
}
bool Valid(const JointPose &p) {
  const auto v = Values(p);
  return std::ranges::all_of(v, [](float f) { return std::isfinite(f); }) && p.scale.x > 0 &&
         p.scale.y > 0 && p.scale.z > 0 && std::isfinite(p.rotation.x) &&
         std::isfinite(p.rotation.y) && std::isfinite(p.rotation.z) &&
         std::isfinite(p.rotation.w) && Norm(p.rotation) > 1e-12;
}
void Word(std::vector<std::uint8_t> &out, std::uint32_t value, unsigned count = 4) {
  for (unsigned i = 0; i < count; ++i)
    out.push_back(std::uint8_t(value >> (8U * i)));
}
std::uint32_t Read(std::span<const std::uint8_t> bytes, std::size_t at, unsigned count = 4) {
  std::uint32_t result = 0;
  for (unsigned i = 0; i < count; ++i)
    result |= std::uint32_t(bytes[at + i]) << (8U * i);
  return result;
}
Bounds ReadBounds(std::span<const std::uint8_t> bytes, std::uint32_t joint) {
  Bounds b{};
  for (unsigned i = 0; i < 12; ++i)
    b[i / 6][i % 6] = std::bit_cast<float>(Read(bytes, HeaderSize + joint * BoundsSize + i * 4));
  return b;
}
math::Quaternion ReadRotation(std::span<const std::uint8_t> bytes, std::size_t at) {
  std::array<float, 4> q{};
  for (unsigned i = 0; i < 4; ++i) {
    const auto word = Read(bytes, at + i * 2, 2);
    const auto signed_word = word < 32768 ? std::int32_t(word) : std::int32_t(word) - 65536;
    q[i] = float(signed_word) / 32767;
  }
  return {q[0], q[1], q[2], q[3]};
}
JointPose Decode(std::span<const std::uint8_t> bytes, std::uint32_t joints, std::uint32_t frame,
                 std::uint32_t joint) {
  const auto b = ReadBounds(bytes, joint);
  const auto at =
      HeaderSize + joints * BoundsSize + (std::size_t(frame) * joints + joint) * SampleSize;
  Channels v{};
  for (unsigned i = 0; i < 6; ++i) {
    const auto offset = i < 3 ? i * 2 : 14 + (i - 3) * 2;
    const double t = double(Read(bytes, at + offset, 2)) / 65535;
    v[i] = float(std::lerp(double(b[0][i]), double(b[1][i]), t));
  }
  return {{v[0], v[1], v[2]}, Canonical(ReadRotation(bytes, at + 6)), {v[3], v[4], v[5]}};
}
math::Quaternion Multiply(math::Quaternion a, math::Quaternion b) {
  return {
      a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y, a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
      a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w, a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z};
}
math::Quaternion Inverse(math::Quaternion q) { return {-q.x, -q.y, -q.z, q.w}; }
bool Skeleton(std::span<const JointPose> bind, std::span<const std::int32_t> parents) {
  if (bind.empty() || bind.size() > AnimationPoseStorage::MaximumJoints ||
      parents.size() != bind.size())
    return false;
  for (std::size_t i = 0; i < bind.size(); ++i)
    if (!Valid(bind[i]) || parents[i] < -1 || parents[i] >= std::int32_t(i))
      return false;
  return true;
}
} // namespace

AnimationPoseStorage::AnimationPoseStorage(AnimationPoseStorage &&other) noexcept
    : bytes_(std::move(other.bytes_)), joints_(std::exchange(other.joints_, 0)),
      frames_(std::exchange(other.frames_, 0)) {}

AnimationPoseStorage &AnimationPoseStorage::operator=(AnimationPoseStorage &&other) noexcept {
  if (this != &other) {
    bytes_ = std::move(other.bytes_);
    joints_ = std::exchange(other.joints_, 0);
    frames_ = std::exchange(other.frames_, 0);
  }
  return *this;
}

bool AnimationPoseStorage::Build(std::uint32_t joints, std::span<const JointPose> input) {
  if (joints == 0 || joints > MaximumJoints || input.empty() || input.size() % joints != 0 ||
      input.size() > MaximumSamples || input.size() / joints > MaximumFrames ||
      !std::ranges::all_of(input, Valid))
    return false;
  const auto frames = std::uint32_t(input.size() / joints);
  std::vector<Bounds> bounds(joints);
  for (std::uint32_t j = 0; j < joints; ++j) {
    bounds[j] = {Values(input[j]), Values(input[j])};
    for (std::uint32_t f = 1; f < frames; ++f) {
      const auto v = Values(input[std::size_t(f) * joints + j]);
      for (unsigned i = 0; i < 6; ++i) {
        bounds[j][0][i] = std::min(bounds[j][0][i], v[i]);
        bounds[j][1][i] = std::max(bounds[j][1][i], v[i]);
      }
    }
  }
  std::vector<std::uint8_t> bytes;
  bytes.reserve(HeaderSize + joints * BoundsSize + input.size() * SampleSize);
  Word(bytes, 0x3153504e); // NPS1, little endian
  Word(bytes, 1);
  Word(bytes, joints);
  Word(bytes, frames);
  for (const auto &b : bounds)
    for (const auto &v : b)
      for (float x : v)
        Word(bytes, std::bit_cast<std::uint32_t>(x == 0 ? 0.0F : x));
  for (std::size_t s = 0; s < input.size(); ++s) {
    const auto &b = bounds[s % joints];
    const auto v = Values(input[s]);
    std::array<std::uint32_t, 6> quantized{};
    for (unsigned i = 0; i < 6; ++i) {
      const double range = double(b[1][i]) - b[0][i];
      quantized[i] = range == 0
                         ? 0
                         : std::uint32_t(std::llround(
                               std::clamp((double(v[i]) - b[0][i]) / range, 0.0, 1.0) * 65535));
    }
    for (unsigned i = 0; i < 3; ++i)
      Word(bytes, quantized[i], 2);
    const auto q = Canonical(input[s].rotation);
    for (float x : {q.x, q.y, q.z, q.w})
      Word(bytes, std::uint32_t(std::int32_t(std::lround(double(x) * 32767))) & 0xffffU, 2);
    for (unsigned i = 3; i < 6; ++i)
      Word(bytes, quantized[i], 2);
  }
  bytes_ = std::move(bytes);
  joints_ = joints;
  frames_ = frames;
  return true;
}

bool AnimationPoseStorage::Load(std::span<const std::uint8_t> bytes) {
  if (bytes.size() < HeaderSize || Read(bytes, 0) != 0x3153504e || Read(bytes, 4) != 1)
    return false;
  const auto joints = Read(bytes, 8), frames = Read(bytes, 12);
  if (!joints || joints > MaximumJoints || !frames || frames > MaximumFrames ||
      std::uint64_t(joints) * frames > MaximumSamples ||
      bytes.size() != HeaderSize + joints * BoundsSize + std::size_t(joints) * frames * SampleSize)
    return false;
  for (std::uint32_t j = 0; j < joints; ++j) {
    const auto b = ReadBounds(bytes, j);
    for (unsigned i = 0; i < 6; ++i)
      if (!std::isfinite(b[0][i]) || !std::isfinite(b[1][i]) || b[0][i] > b[1][i] ||
          (i >= 3 && b[0][i] <= 0))
        return false;
  }
  for (std::size_t s = 0; s < std::size_t(joints) * frames; ++s) {
    const auto at = HeaderSize + joints * BoundsSize + s * SampleSize + 6;
    const double norm = Norm(ReadRotation(bytes, at));
    // Quantization of a normalized quaternion changes its norm by at most 1/32767.
    if (std::abs(norm - 1) > 0.0001)
      return false;
  }
  std::vector<std::uint8_t> owned(bytes.begin(), bytes.end());
  bytes_ = std::move(owned);
  joints_ = joints;
  frames_ = frames;
  return true;
}

std::optional<std::vector<JointPose>> AnimationPoseStorage::Sample(double frame) const {
  if (!frames_ || !std::isfinite(frame) || frame < 0 || frame > frames_ - 1)
    return std::nullopt;
  const auto a = std::uint32_t(std::floor(frame)), b = std::min(a + 1, frames_ - 1);
  const double t = frame - a;
  std::vector<JointPose> result(joints_);
  for (std::uint32_t j = 0; j < joints_; ++j) {
    const auto left = Decode(bytes_, joints_, a, j), right = Decode(bytes_, joints_, b, j);
    const auto x = Values(left), y = Values(right);
    Channels v{};
    for (unsigned i = 0; i < 6; ++i)
      v[i] = float(std::lerp(double(x[i]), double(y[i]), t));
    result[j] = {{v[0], v[1], v[2]},
                 math::Slerp(left.rotation, right.rotation, float(t)),
                 {v[3], v[4], v[5]}};
  }
  return result;
}

bool PoseRetargeter::Build(std::span<const JointPose> source, std::span<const std::int32_t> sp,
                           std::span<const JointPose> target, std::span<const std::int32_t> tp,
                           std::span<const RetargetJoint> mapping) {
  if (!Skeleton(source, sp) || !Skeleton(target, tp) || mapping.empty() ||
      mapping.size() > target.size())
    return false;
  std::vector<std::int32_t> mapped(source.size(), -1);
  std::vector<bool> used(target.size());
  for (const auto &m : mapping) {
    if (m.source >= source.size() || m.target >= target.size() || mapped[m.source] != -1 ||
        used[m.target] || !std::isfinite(m.translation_scale) || m.translation_scale <= 0)
      return false;
    mapped[m.source] = std::int32_t(m.target);
    used[m.target] = true;
  }
  for (const auto &m : mapping)
    if ((sp[m.source] == -1 && tp[m.target] != -1) ||
        (sp[m.source] != -1 && (mapped[std::size_t(sp[m.source])] == -1 ||
                                mapped[std::size_t(sp[m.source])] != tp[m.target])))
      return false;
  std::vector<JointPose> sb(source.begin(), source.end()), tb(target.begin(), target.end());
  std::vector<RetargetJoint> maps(mapping.begin(), mapping.end());
  std::ranges::sort(maps, {}, &RetargetJoint::target);
  for (auto &p : sb)
    p.rotation = Canonical(p.rotation);
  for (auto &p : tb)
    p.rotation = Canonical(p.rotation);
  source_bind_ = std::move(sb);
  target_bind_ = std::move(tb);
  mapping_ = std::move(maps);
  return true;
}

std::optional<std::vector<JointPose>>
PoseRetargeter::Apply(std::span<const JointPose> source) const {
  if (mapping_.empty() || source.size() != source_bind_.size() ||
      !std::ranges::all_of(source, Valid))
    return std::nullopt;
  auto result = target_bind_;
  for (const auto &m : mapping_) {
    const auto &s = source[m.source], &sb = source_bind_[m.source], &tb = target_bind_[m.target];
    auto &r = result[m.target];
    const auto align = Multiply(tb.rotation, Inverse(sb.rotation));
    // Double intermediates prevent finite extreme inputs overflowing before validation.
    const std::array<double, 3> delta{double(s.translation.x) - sb.translation.x,
                                      double(s.translation.y) - sb.translation.y,
                                      double(s.translation.z) - sb.translation.z};
    const auto matrix = math::Compose({{}, align, {1, 1, 1}});
    std::array<float, 3> translated{};
    const std::array<float, 3> bind{tb.translation.x, tb.translation.y, tb.translation.z};
    for (unsigned row = 0; row < 3; ++row) {
      double value = bind[row];
      for (unsigned col = 0; col < 3; ++col)
        value += double(matrix(row, col)) * delta[col] * m.translation_scale;
      if (!std::isfinite(value) || std::abs(value) > std::numeric_limits<float>::max())
        return std::nullopt;
      translated[row] = float(value);
    }
    r.translation = {translated[0], translated[1], translated[2]};
    r.rotation = Canonical(Multiply(align, Canonical(s.rotation)));
    const std::array<double, 3> scale{double(tb.scale.x) * s.scale.x / sb.scale.x,
                                      double(tb.scale.y) * s.scale.y / sb.scale.y,
                                      double(tb.scale.z) * s.scale.z / sb.scale.z};
    for (double value : scale)
      if (!std::isfinite(value) || value > std::numeric_limits<float>::max())
        return std::nullopt;
    r.scale = {float(scale[0]), float(scale[1]), float(scale[2])};
    if (!Valid(r))
      return std::nullopt;
  }
  return result;
}
} // namespace nexora::animation
