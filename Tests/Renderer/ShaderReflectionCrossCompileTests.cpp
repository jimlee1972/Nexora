#include "Nexora/RHI/ShaderReflection.h"

#include <charconv>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

namespace {
bool ReadLayoutHash(const std::string &json, std::uint64_t &value) {
  const auto key = json.find("\"layout_hash\"");
  if (key == std::string::npos)
    return false;
  const auto colon = json.find(':', key);
  if (colon == std::string::npos)
    return false;
  const auto first = json.find_first_not_of(" \t\r\n", colon + 1);
  if (first == std::string::npos)
    return false;
  const auto last = json.find_first_not_of("0123456789", first);
  const auto digits = json.substr(first, last == std::string::npos ? std::string::npos
                                                                    : last - first);
  if (digits.empty())
    return false;
  const auto result = std::from_chars(digits.data(), digits.data() + digits.size(), value);
  return result.ec == std::errc{} && result.ptr == digits.data() + digits.size();
}
} // namespace

int main(int argc, char **argv) {
  if (argc != 2) {
    std::cerr << "usage: NexoraShaderReflectionContractTests <canonical-reflection.json>\n";
    return 2;
  }

  std::ifstream input(argv[1], std::ios::binary);
  if (!input) {
    std::cerr << "unable to open canonical reflection: " << argv[1] << '\n';
    return 1;
  }
  const std::string json((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());

  std::uint64_t reflected_hash{};
  if (!ReadLayoutHash(json, reflected_hash)) {
    std::cerr << "canonical reflection does not contain a valid layout_hash\n";
    return 1;
  }

  const auto canonical = nexora::rhi::TrianglePipelineLayout();
  if (canonical.layout_hash != nexora::rhi::ComputeLayoutHash(canonical.bindings)) {
    std::cerr << "TrianglePipelineLayout has an inconsistent C++ layout hash\n";
    return 1;
  }
  if (reflected_hash != canonical.layout_hash) {
    std::cerr << "Slang reflection hash does not match TrianglePipelineLayout\n";
    return 1;
  }
  return 0;
}
