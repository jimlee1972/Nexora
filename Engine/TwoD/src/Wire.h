#pragma once
#include "Nexora/TwoD/Assets.h"
#include <bit>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <type_traits>
namespace nexora::twod::detail {
inline constexpr std::size_t kMaximumBytes = 64 * 1024 * 1024;
class Writer {
public:
  Bytes bytes;
  void U64(std::uint64_t value) {
    Ensure(8);
    for (unsigned i = 0; i < 8; ++i)
      bytes.push_back(std::byte((value >> (8 * i)) & 255));
  }
  void U32(std::uint32_t value) {
    Ensure(4);
    for (unsigned i = 0; i < 4; ++i)
      bytes.push_back(std::byte((value >> (8 * i)) & 255));
  }
  void Float(float value) { U32(std::bit_cast<std::uint32_t>(value)); }
  void Double(double value) { U64(std::bit_cast<std::uint64_t>(value)); }
  void Id(AssetId id) {
    U64(id.high);
    U64(id.low);
  }
  void Data(std::span<const std::byte> data) {
    if (data.size() > kMaximumBytes - 4)
      throw std::length_error("2D wire budget");
    Ensure(data.size() + 4);
    U32(static_cast<std::uint32_t>(data.size()));
    bytes.insert(bytes.end(), data.begin(), data.end());
  }
  void Text(std::string_view value) { Data(std::as_bytes(std::span(value.data(), value.size()))); }

private:
  void Ensure(std::size_t count) const {
    if (count > kMaximumBytes || bytes.size() > kMaximumBytes - count)
      throw std::length_error("2D wire budget");
  }
};
class Reader {
public:
  explicit Reader(std::span<const std::byte> data) : data_(data) {
    if (data.size() > kMaximumBytes)
      valid_ = false;
  }
  std::uint64_t U64() {
    std::uint64_t value = 0;
    for (unsigned i = 0; i < 8; ++i)
      value |= std::uint64_t(Byte()) << (8 * i);
    return value;
  }
  std::uint32_t U32() {
    std::uint32_t value = 0;
    for (unsigned i = 0; i < 4; ++i)
      value |= std::uint32_t(Byte()) << (8 * i);
    return value;
  }
  float Float() { return std::bit_cast<float>(U32()); }
  double Double() { return std::bit_cast<double>(U64()); }
  AssetId Id() {
    const auto h = U64(), l = U64();
    return {h, l};
  }
  std::span<const std::byte> Data(std::size_t maximum = kMaximumBytes) {
    const auto size = U32();
    if (!valid_ || size > maximum || size > data_.size() - at_) {
      valid_ = false;
      return {};
    }
    auto result = data_.subspan(at_, size);
    at_ += size;
    return result;
  }
  std::string Text(std::size_t maximum = 256) {
    auto value = Data(maximum);
    if (value.empty())
      return {};
    return {reinterpret_cast<const char *>(value.data()), value.size()};
  }
  bool Done() const { return valid_ && at_ == data_.size(); }
  bool Valid() const { return valid_; }

private:
  std::uint8_t Byte() {
    if (!valid_ || at_ == data_.size()) {
      valid_ = false;
      return 0;
    }
    return std::to_integer<std::uint8_t>(data_[at_++]);
  }
  std::span<const std::byte> data_;
  std::size_t at_{};
  bool valid_{true};
};
inline bool ValidId(AssetId id) { return id.high != 0 || id.low != 0; }
} // namespace nexora::twod::detail
