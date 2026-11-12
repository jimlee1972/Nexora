#include "Nexora/Core/Engine.h"
#include "Nexora/EditorImGui/EditorImGui.h"
#include "Nexora/EditorImGui/TwoDPanel.h"
#include <algorithm>
#include <charconv>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace nexora;
namespace {
void Check(bool value, const char *reason) {
  if (!value)
    throw std::runtime_error(reason);
}
std::filesystem::path Safe(const std::filesystem::path &root,
                           const std::filesystem::path &relative) {
  Check(!relative.empty() && !relative.is_absolute(), "workspace-relative file required");
  auto path = std::filesystem::weakly_canonical(root / relative);
  auto rel = path.lexically_relative(root);
  Check(!rel.empty() && *rel.begin() != "..", "path leaves workspace");
  return path;
}
twod::Bytes Read(const std::filesystem::path &p) {
  const auto n = std::filesystem::file_size(p);
  Check(n <= 64 * 1024 * 1024, "file budget");
  twod::Bytes out(static_cast<std::size_t>(n));
  std::ifstream f(p, std::ios::binary);
  Check(bool(f.read(reinterpret_cast<char *>(out.data()), std::streamsize(out.size()))),
        "file read");
  return out;
}
void Write(const std::filesystem::path &root, const std::string &file,
           std::span<const std::byte> bytes) {
  const auto path = Safe(root, file);
  core::Engine engine;
  core::EngineConfiguration config;
  config.worker_count = 1;
  config.content_root = root;
  engine.Initialize(config);
  Check(engine.Services().vfs->WriteAtomic(
            "content://" + path.lexically_relative(root).generic_string(), bytes) ==
            core::ReadResult::Status::Completed,
        "atomic document publication");
  engine.Shutdown();
}
} // namespace
int main(int argc, char **argv) {
  try {
    std::filesystem::path workspace;
    std::string atlasFile, clipFile, spriteFile, documentFile = "scene.nx2doc";
    std::vector<std::string> sceneFiles;
    bool explicitDocument = false;
    std::uint32_t frames = 0;
    bool smoke = false, readOnly = false;
    for (int i = 1; i < argc; ++i) {
      std::string_view a = argv[i];
      if (a == "--workspace" && i + 1 < argc)
        workspace = argv[++i];
      else if (a == "--atlas" && i + 1 < argc)
        atlasFile = argv[++i];
      else if (a == "--clip" && i + 1 < argc)
        clipFile = argv[++i];
      else if (a == "--sprite" && i + 1 < argc)
        spriteFile = argv[++i];
      else if (a == "--document" && i + 1 < argc) {
        documentFile = argv[++i];
        explicitDocument = true;
      } else if (a == "--scene" && i + 1 < argc) {
        Check(sceneFiles.size() < 64, "scene file count exceeds 64");
        sceneFiles.emplace_back(argv[++i]);
      } else if (a == "--frames" && i + 1 < argc) {
        std::string_view n = argv[++i];
        auto result = std::from_chars(n.data(), n.data() + n.size(), frames);
        Check(result.ec == std::errc{} && result.ptr == n.data() + n.size(), "frame count");
      } else if (a == "--smoke")
        smoke = true;
      else if (a == "--read-only")
        readOnly = true;
      else
        throw std::runtime_error(
            "usage: NexoraTwoDStudio --workspace DIR [--atlas FILE --clip "
            "FILE --sprite FILE --document FILE --frames N --smoke --read-only]");
    }
    Check(!(smoke && readOnly), "smoke requires a writable isolated workspace");
    Check(!workspace.empty() && std::filesystem::is_directory(workspace),
          "existing workspace required");
    workspace = std::filesystem::canonical(workspace);
    if (!explicitDocument && !sceneFiles.empty())
      documentFile = sceneFiles.front();
    if (std::find(sceneFiles.begin(), sceneFiles.end(), documentFile) == sceneFiles.end())
      sceneFiles.insert(sceneFiles.begin(), documentFile);
    Check(sceneFiles.size() <= 64, "scene file count exceeds 64");
    for (const auto &file : sceneFiles)
      static_cast<void>(Safe(workspace, file));
    static_cast<void>(Safe(workspace, documentFile));
    editor::TwoDDocument document;
    const auto importAtlas = [&] {
      Check(!atlasFile.empty(), "provide --atlas FILE");
      auto a = twod::DecodeAtlas(Read(Safe(workspace, atlasFile)));
      Check(a && document.ReplaceAtlas(std::move(*a)), "atlas import");
    };
    const auto importClip = [&] {
      Check(!clipFile.empty(), "provide --clip FILE");
      auto c = twod::DecodeClip(Read(Safe(workspace, clipFile)));
      Check(c && document.ReplaceClip(std::move(*c)), "clip import");
    };
    const auto importSprite = [&] {
      Check(!spriteFile.empty(), "provide --sprite FILE");
      auto source = twod::DecodeSpriteAsset(Read(Safe(workspace, spriteFile)));
      Check(source && document.ReplaceSpriteSource(std::move(*source)), "Sprite source import");
    };
    if (std::filesystem::exists(Safe(workspace, documentFile)))
      Check(document.Reopen(Read(Safe(workspace, documentFile))), "document open");
    else {
      if (!atlasFile.empty())
        importAtlas();
      if (!clipFile.empty())
        importClip();
      if (!spriteFile.empty())
        importSprite();
    }
    if (smoke) {
      if (atlasFile.empty()) {
        twod::Image image{1, 2, 2, {}};
        image.rgba.resize(16);
        for (std::size_t i = 0; i < 16; i += 4) {
          image.rgba[i] = std::byte{240};
          image.rgba[i + 1] = std::byte{90};
          image.rgba[i + 2] = std::byte{40};
          image.rgba[i + 3] = std::byte{255};
        }
        auto a = twod::PackAtlas({71, 72}, std::span(&image, 1), 16, 16, true);
        Check(a && document.ReplaceAtlas(*a), "smoke atlas");
      }
      twod::Sprite s;
      s.atlas = document.Atlas().id;
      s.region = 1;
      if (!document.SpriteSources().empty())
        s.source = document.SpriteSources().front().id;
      auto id = document.AddSprite(s);
      Check(id && document.SetCollider(*id, {}), "smoke scene");
      twod::Tile tile{-2, -2, 0, 1, true};
      Check(document.Paint(std::span(&tile, 1)), "smoke paint");
      if (clipFile.empty())
        Check(document.ReplaceClip({{73, 74}, document.Atlas().id, true, {{1, 0.1, "preview"}}}),
              "smoke animation");
      const auto saved = document.Save();
      Write(workspace, documentFile, saved);
      Check(document.Reopen(Read(Safe(workspace, documentFile))), "smoke save reopen");
      if (!frames)
        frames = 12;
    }
    document.SetReadOnly(readOnly);
    using namespace Nexora::Presentation;
    auto surface = CreateRenderSurface({"Nexora 2D Studio", 1100, 800});
    Check(bool(surface), "native surface");
    editor::imgui::EditorImGuiHost host;
    editor::imgui::TwoDPanelState panel;
    panel.sceneFiles = sceneFiles;
    panel.activeScene = std::size_t(std::find(sceneFiles.begin(), sceneFiles.end(), documentFile) -
                                    sceneFiles.begin());
    std::uint64_t texture{}, revision = UINT64_MAX;
    for (std::uint32_t frame = 0; !surface.surface->CloseRequested() && (!frames || frame < frames);
         ++frame) {
      auto status = surface.surface->BeginFrame();
      if (surface.surface->CloseRequested())
        break;
      if (status == SurfaceStatus::ZeroExtent || status == SurfaceStatus::OutOfDate) {
        --frame;
        continue;
      }
      Check(status == SurfaceStatus::Ready, "acquire");
      if (revision != document.Revision()) {
        revision = document.Revision();
        if (texture)
          Check(host.UnregisterTexture(texture), "old preview texture");
        texture = 0;
        if (twod::ValidateAtlas(document.Atlas())) {
          const auto &a = document.Atlas();
          const auto factor = std::max(1U, (std::max(a.width, a.height) + 1023) / 1024);
          const auto width = (a.width + factor - 1) / factor,
                     height = (a.height + factor - 1) / factor;
          twod::Bytes pixels(std::size_t(width) * height * 4);
          for (std::uint32_t y = 0; y < height; ++y)
            for (std::uint32_t x = 0; x < width; ++x)
              for (std::uint32_t c = 0; c < 4; ++c)
                pixels[(std::size_t(y) * width + x) * 4 + c] =
                    a.rgba[(std::size_t(std::min(y * factor, a.height - 1)) * a.width +
                            std::min(x * factor, a.width - 1)) *
                               4 +
                           c];
          texture = host.RegisterNativeTexture(width, height, pixels, a.nearest);
          Check(texture != 0, "atlas preview registration");
        }
      }
      const auto &info = surface.surface->FrameInfo();
      host.SetDisplay(float(info.width) / info.dpiScale, float(info.height) / info.dpiScale,
                      info.dpiScale);
      host.ProcessEvents(surface.surface->Events());
      host.BeginFrame();
      if (smoke)
        panel.requestedTab = editor::imgui::TwoDPanelTab((frame / 3) % 4);
      editor::imgui::DrawTwoDPanel(document, panel, !readOnly, texture, smoke ? 0.2F : 1.0F / 60);
      static_cast<void>(host.EndFrame());
      Check(host.Render(*surface.surface, info.width, info.height) == SurfaceStatus::Ready,
            "graphical preview render");
      Check(surface.surface->EndFrame() == SurfaceStatus::Ready, "present");
      if (panel.requestedScene) {
        try {
          const auto selected = *panel.requestedScene;
          Check(selected < sceneFiles.size() && !document.Dirty(), "save before scene switch");
          const auto bytes = Read(Safe(workspace, sceneFiles[selected]));
          document.SetReadOnly(false);
          const bool reopened = document.Reopen(bytes);
          document.SetReadOnly(readOnly);
          Check(reopened, "scene open failed; current document retained");
          documentFile = sceneFiles[selected];
          panel.activeScene = selected;
          panel.selected = 0;
          panel.region = 0;
          panel.newSpriteSource.reset();
          panel.preview = {};
          panel.previewRevision = UINT64_MAX;
          panel.status = "Scene opened.";
        } catch (const std::exception &e) {
          document.SetReadOnly(readOnly);
          panel.status = e.what();
        }
        panel.requestedScene.reset();
      }
      if (panel.saveRequested || panel.openRequested || panel.importAtlasRequested ||
          panel.importClipRequested || panel.importSpriteRequested) {
        try {
          Check(!readOnly, "read-only publication");
          if (panel.saveRequested) {
            auto bytes = document.Save();
            Check(!bytes.empty(), "snapshot");
            Write(workspace, documentFile, bytes);
            document.MarkSaved();
          }
          if (panel.openRequested)
            Check(document.Reopen(Read(Safe(workspace, documentFile))), "reopen");
          if (panel.importAtlasRequested)
            importAtlas();
          if (panel.importClipRequested)
            importClip();
          if (panel.importSpriteRequested)
            importSprite();
          panel.status = "File operation completed.";
        } catch (const std::exception &e) {
          panel.status = e.what();
        }
        panel.saveRequested = panel.openRequested = panel.importAtlasRequested =
            panel.importClipRequested = panel.importSpriteRequested = false;
      }
    }
    Check(surface.surface->Diagnostics().nativeUiDrawCalls > 0, "native draws");
    Check(surface.surface->Diagnostics().nativeUiRejectedTextures == 0,
          "native texture resolution");
    Check(surface.surface->DrainAndDestroy() == SurfaceStatus::Ready, "native shutdown");
    std::cout << "2D Studio native preview and document lifecycle passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
