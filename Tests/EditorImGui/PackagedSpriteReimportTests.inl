namespace source_reimport {
void Write(const std::filesystem::path &path, std::span<const std::byte> bytes) {
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  out.write(reinterpret_cast<const char *>(bytes.data()),
            static_cast<std::streamsize>(bytes.size()));
  out.close();
  Check(bool(out), "source reimport fixture write");
}
void Run() {
  const auto root = std::filesystem::canonical(std::filesystem::temp_directory_path()) /
                    ("nexora-packaged-reimport-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  struct Cleanup final {
    std::filesystem::path root;
    ~Cleanup() {
      std::error_code ignored;
      std::filesystem::remove_all(root, ignored);
    }
  } cleanup{root};
  editor::ProjectWorkspace workspace;
  Check(workspace.Create(root, "Source reimport"), "reimport workspace");
  std::filesystem::create_directories(root / "Content");
  auto package = Package();
  package.project = {workspace.Project().id.high, workspace.Project().id.low};
  const auto project = Load(package);
  const auto source_path = root / "Content/Source.nx2sprite";
  const auto atlas_path = root / "Content/Atlas.nx2atlas";
  for (const auto &blob : project->SpriteAssets())
    Write(blob.type == "sprite_atlas" ? atlas_path : source_path, blob.payload);
  editor::AssetWorkspace imported;
  Check(
      imported.ImportTree(root / "Content", {}, {}, editor::AssetIdentityMode::PersistentReadWrite),
      "reimport initial import");
  editor::ProjectContentSession content;
  Check(content.Open(workspace, imported, 17), "reimport Content");
  auto descriptor = editor::ReadGameBuildSource(root, "project.nexora", 256 * 1024, false);
  Check(bool(descriptor), "reimport descriptor baseline");
  editor::preview::PackagedSpriteReimport reimport(*project, content, {*descriptor});
  editor::imgui::EditorImGuiHost ui;
  editor::preview::PackagedPlayContext play(project);
  auto textures = std::make_unique<editor::preview::PackagedSpriteTextures>(ui);
  Check(textures->Prepare(*project, play.SpriteAssets()), "reimport initial textures");
  const auto draw = [&](const auto &, const auto &frame, const auto &owner) {
    Check(bool(owner.Remap(frame)), "source reimport candidate/native owner mismatch");
    return SurfaceStatus::Ready;
  };
  Check(play.RenderComposed(
                {0, 0, 640, 480}, false,
                [&](const auto &scene, const auto &frame) { return draw(scene, frame, *textures); })
            .committed,
        "reimport initial scene publication");
  NexoraTwoDApiV1 api{};
  api.struct_size = sizeof(api);
  NexoraTwoDSourceApiV1 sources{};
  sources.struct_size = sizeof(sources);
  Check(play.Sprites()->Bridge().GetApi(&api) == 0 &&
            api.get_source_api(api.context, &sources) == 0,
        "reimport main services");
  NexoraTwoDSourceInstanceV1 pooled{};
  pooled.transform.rotation.w = 1;
  pooled.transform.scale = {-1, 1, 1};
  pooled.sprite.sprite.flags = 8 | 1;
  pooled.sprite.sprite.color[0] = pooled.sprite.sprite.color[3] = 1;
  pooled.sprite.sprite.size = {1, 1};
  pooled.sprite.source_high = sourceId.high;
  pooled.sprite.source_low = sourceId.low;
  NexoraTwoDInstanceHandle handle{};
  Check(sources.create_instances(sources.context, &pooled, 1, &handle) == 0,
        "reimport stable pool handle");
  const auto world = play.World().InternalWorld().SaveScene(play.Scenes().View().primary);
  const auto original = play.SpriteAssets().Source(sourceId);
  const auto change_source = [&](float pivot) {
    auto source = *original;
    source.pivotX = pivot;
    Write(source_path, twod::EncodeSpriteAsset(source));
    Check(content.Reimport(sourceId), "source reimport publication");
  };
  std::string error;
  const auto stage = [&] { return reimport.Stage(workspace, content, play, *textures, ui, error); };
  // All64 registrations are occupied. A metadata-only update must share its unchanged Atlas.
  const std::array pixel{std::byte{255}, std::byte{0}, std::byte{0}, std::byte{255}};
  std::vector<std::uint64_t> held;
  for (unsigned i = 0; i < 63; ++i) {
    held.push_back(ui.RegisterNativeTexture(1, 1, pixel));
    Check(held.back() != 0, "reimport texture reservation");
  }
  change_source(.25F);
  auto candidate = stage();
  Check(bool(candidate), error.c_str());
  auto stale = *original;
  stale.pivotX = .75F;
  Write(source_path, twod::EncodeSpriteAsset(stale));
  unsigned calls{};
  const auto rejected =
      reimport.Present(*candidate, workspace, content, play, textures, {0, 0, 640, 480}, false,
                       [&](const auto &, const auto &, const auto &) {
                         ++calls;
                         return SurfaceStatus::Ready;
                       });
  candidate.reset();
  Check(rejected.rejected && !calls && reimport.Accepted() == 0 && play.Sprites()->Available() &&
            play.SpriteAssets().Source(sourceId) == original,
        "stale source reached native draw/publication");
  for (const auto failure : {SurfaceStatus::InvalidDescriptor, SurfaceStatus::DeviceLost}) {
    Check(content.Reimport(sourceId), "stale source retry");
    candidate = stage();
    Check(bool(candidate), error.c_str());
    unsigned attempts{};
    const auto failed =
        reimport.Present(*candidate, workspace, content, play, textures, {0, 0, 640, 480}, false,
                         [&](const auto &, const auto &, const auto &) {
                           ++attempts;
                           return failure;
                         });
    candidate.reset();
    Check(failed.rejected && !failed.drawn && failed.status == failure && attempts == 1 &&
              play.SpriteAssets().Source(sourceId) == original && reimport.Accepted() == 0 &&
              nexora::sceneview::CanRedrawRetainedScene(failed.status) ==
                  (failure == SurfaceStatus::InvalidDescriptor),
          "failed native admission published metadata or lost surface recovery status");
  }
  Check(content.Reimport(sourceId), "native admission retry");
  candidate = stage();
  Check(bool(candidate), error.c_str());
  Check(reimport
            .Present(*candidate, workspace, content, play, textures, {0, 0, 640, 480}, false, draw)
            .drawn,
        "same-Atlas update failed at full registration capacity");
  candidate.reset();
  Check(reimport.Accepted() == 1 && play.SpriteAssets().Source(sourceId)->pivotX == .75F,
        "source metadata was not published");
  auto atlas = *play.SpriteAssets().ResolveAtlas(atlasId);
  atlas.rgba[0] = std::byte{0};
  Write(atlas_path, twod::EncodeAtlas(atlas));
  Check(content.Reimport(atlasId), "Atlas reimport");
  const auto before_atlas = play.SpriteAssets().ResolveAtlas(atlasId);
  Check(!stage() && error.find("capacity") != std::string::npos && play.Sprites()->Available() &&
            play.SpriteAssets().ResolveAtlas(atlasId) == before_atlas,
        "capacity failure revoked/published the current texture owner");
  Check(ui.UnregisterTexture(held.back()), "release one candidate texture slot");
  held.pop_back();
  std::uint64_t previous_texture{};
  for (unsigned cycle = 0; cycle < 32; ++cycle) {
    atlas.rgba[0] = static_cast<std::byte>(cycle);
    Write(atlas_path, twod::EncodeAtlas(atlas));
    Check(content.Reimport(atlasId), "repeated Atlas source import");
    candidate = stage();
    Check(bool(candidate), error.c_str());
    Check(
        reimport
            .Present(*candidate, workspace, content, play, textures, {0, 0, 640, 480}, false, draw)
            .drawn,
        "repeated native source publication");
    candidate.reset();
    const auto frame = play.Sprites()->Extract(640, 480);
    const auto mapped = frame ? textures->Remap(*frame) : std::nullopt;
    NexoraTwoDSourceInstanceV1 read{};
    Check(mapped && mapped->statistics.visible == 2 &&
              mapped->batches.front().textureId != previous_texture &&
              reimport.Accepted() == cycle + 2 &&
              play.World().InternalWorld().SaveScene(play.Scenes().View().primary) == world &&
              sources.read_instances(sources.context, &handle, 1, &read) == 0 &&
              read.sprite.source_high == sourceId.high && read.transform.scale.x == -1 &&
              read.sprite.sprite.flags == pooled.sprite.sprite.flags,
          "reimport leaked registrations or changed World/pool identities/values");
    previous_texture = mapped->batches.front().textureId;
  }
  change_source(.25F);
  const auto source_bytes = twod::EncodeSpriteAsset(stale);
  std::filesystem::remove(source_path);
  Check(!stage() && !error.empty() && reimport.Accepted() == 33 && play.Sprites()->Available(),
        "missing source admitted a new catalog");
  Write(source_path, source_bytes);
  Check(content.Reimport(sourceId), "restore missing source");
  auto changed_descriptor = *descriptor->bytes;
  changed_descriptor.push_back(std::byte{' '});
  Write(root / "project.nexora", changed_descriptor);
  Check(!stage() && !error.empty() && reimport.Accepted() == 33 && play.Sprites()->Available(),
        "obsolete project descriptor admitted source publication");
  Write(root / "project.nexora", *descriptor->bytes);
  for (const auto id : held)
    Check(ui.UnregisterTexture(id), "reimport leaked another view's registrations");
  textures.reset();
  std::vector<std::uint64_t> final;
  for (unsigned i = 0; i < 64; ++i) {
    final.push_back(ui.RegisterNativeTexture(1, 1, pixel));
    Check(final.back() != 0, "source retirement leaked a native registration");
  }
  for (const auto id : final)
    Check(ui.UnregisterTexture(id), "final source retirement");
}
} // namespace source_reimport
