#include "Project.h"
#include <chrono>
#include <iostream>

namespace {
using namespace nexora;
void Check(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
void U32(twod::Bytes &bytes, std::uint32_t value) {
  for (unsigned n = 0; n < 4; ++n)
    bytes.push_back(static_cast<std::byte>(value >> (n * 8)));
}
void Float(twod::Bytes &bytes, float value) { U32(bytes, std::bit_cast<std::uint32_t>(value)); }
void Run(const std::filesystem::path &root) {
  std::filesystem::create_directories(root);
  const auto module = root / "gameplay.dll";
  {
    std::ofstream output(module);
    output << "path admission only";
  }
  const auto manifest = root / "game.nx2game";
  const std::string base = "NEXORA_2D_GAME 1\nid \"com.example\"\ntitle \"A game\"\nscene "
                           "\"Main\"\nmodule \"gameplay.dll\"\naudio \"off\"\n";
  const auto load = [&](const std::string &text) {
    {
      std::ofstream output(manifest);
      output << text;
    }
    return player::GameProject::Load(manifest);
  };
  Check(load(base).module == std::filesystem::canonical(module) && load(base).audio == "off",
        "valid descriptor");
  Check(load(base).width == 640 && load(base).height == 480, "legacy display default");
  const auto display = load(base + "display 1920 1080 windowed 1\n");
  Check(display.width == 1920 && display.height == 1080 && !display.borderless && display.resizable,
        "configured display");
  Check(load(base + "display 1920 1080 borderless 0\n").borderless, "borderless display");
#if !defined(_WIN32)
  (void)load(base);
  std::filesystem::create_directory_symlink(root, root / "alias");
  const auto aliased = player::GameProject::Load(root / "alias/game.nx2game");
  Check(aliased.module == std::filesystem::canonical(module) &&
            aliased.root == std::filesystem::canonical(root),
        "project directory alias normalization");
#endif
  for (const auto &bad :
       {base + "unknown \"value\"\n", base + "id \"duplicate\"\n", base + "tile_atlas 0 0\n",
        base + "display 0 1080 windowed 1\n", base + "display 16385 1080 windowed 1\n",
        base + "display -1 1080 windowed 1\n", base + "display 1920 1080 exclusive 1\n",
        base + "display 1920.5 1080 windowed 1\n", base + "display 1920 1080 windowed 2\n",
        base + "display 1920 1080 windowed 1\ndisplay 640 480 windowed 1\n",
        base + "font 33 \"gameplay.dll\"\n", base + "wave 1 \"../gameplay.dll\"\n",
        base + "wave 1 \"gameplay.dll:stream\"\n",
        base + "wave 1 \"gameplay.dll\"\nwave 1 \"gameplay.dll\"\n"}) {
    bool rejected{};
    try {
      (void)load(bad);
    } catch (const std::exception &) {
      rejected = true;
    }
    Check(rejected, "malformed descriptor admitted");
  }
  twod::Bytes font;
  for (char c : std::string_view("NXFONT1\0", 8))
    font.push_back(static_cast<std::byte>(c));
  U32(font, 1);
  U32(font, 1);
  Float(font, 1);
  Float(font, 1);
  U32(font, 1);
  U32(font,
      0x4e2d); // Real binary parser coverage for a CJK codepoint, not a font appearance claim.
  for (float v : {1.F, 0.F, 0.F, 1.F, 1.F, 0.F, 0.F, 1.F, 1.F})
    Float(font, v);
  font.insert(font.end(), 4, std::byte{255});
  const auto decoded = player::ReadGameFont(font, 1);
  Check(decoded.glyphs.size() == 1 && decoded.glyphs[0].codepoint == 0x4e2d &&
            decoded.rgba.size() == 4,
        "font binary layout");
  twod::GameplayUi ui;
  Check(ui.RegisterFont(decoded), "decoded font admission");
  for (unsigned mode = 0; mode < 3; ++mode) {
    auto bad = font;
    if (mode == 0)
      bad.pop_back();
    if (mode == 1)
      bad[0] = std::byte{};
    if (mode == 2)
      bad[8] = std::byte{};
    bool rejected{};
    try {
      (void)player::ReadGameFont(bad, 1);
    } catch (const std::exception &) {
      rejected = true;
    }
    Check(rejected, "malformed font admitted");
  }
  for (const auto scope : {"../escape", "CON", "a/b", "a:", ".hidden", "tail."}) {
    bool rejected{};
    try {
      (void)runtime::UserGameplaySaveDirectory(scope);
    } catch (const std::invalid_argument &) {
      rejected = true;
    }
    Check(rejected, "invalid save scope admitted");
  }
  const auto saves = runtime::UserGameplaySaveDirectory("com.example");
  Check(saves.is_absolute() && saves.filename() == "saves" &&
            saves.parent_path().filename() == "game-com.example",
        "per-user save scope");
}
} // namespace
int main() {
  const auto root = std::filesystem::temp_directory_path() /
                    ("nexora-project-manifest-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  try {
    Run(root);
    std::filesystem::remove_all(root);
    return 0;
  } catch (const std::exception &error) {
    std::filesystem::remove_all(root);
    std::cerr << error.what() << '\n';
    return 1;
  }
}
