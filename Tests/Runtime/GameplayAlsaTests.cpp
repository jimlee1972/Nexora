#include "Nexora/Runtime/GameplayAudio.h"
#include <cstdlib>
#include <dlfcn.h>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
void Check(bool condition, const char *message) {
  if (!condition)
    throw std::runtime_error(message);
}
template <typename T> T Symbol(void *library, const char *name) {
  auto result = reinterpret_cast<T>(dlsym(library, name));
  Check(result != nullptr, name);
  return result;
}
// This fixture owns the external client's global cache. Production code must
// neither replace nor free it. Only this fixture releases it, after its PCM/ref.
struct ExternalClient {
  void *library{}, *root{}, *device{};
  void **global{};
  int (*close)(void *){};
  void (*unref)(void *){};
  int (*free_global)(){};
  ~ExternalClient() {
    if (device)
      close(device);
    if (root)
      unref(root);
    if (free_global)
      free_global();
    if (library)
      dlclose(library);
  }
  void Open(const char *config) {
    Check(setenv("ALSA_CONFIG_PATH", config, 1) == 0, "set peer config");
    library = dlopen("libasound.so.2", RTLD_NOW | RTLD_LOCAL);
    Check(library, "external ALSA library");
    close = Symbol<int (*)(void *)>(library, "snd_pcm_close");
    unref = Symbol<void (*)(void *)>(library, "snd_config_unref");
    free_global = Symbol<int (*)()>(library, "snd_config_update_free_global");
    global = Symbol<void **>(library, "snd_config");
    const auto ref = Symbol<int (*)(void **)>(library, "snd_config_update_ref");
    const auto open = Symbol<int (*)(void **, const char *, int, int)>(library, "snd_pcm_open");
    Check(ref(&root) >= 0 && root && *global == root, "external global config reference");
    Check(open(&device, "default", 0, 0) >= 0, "external null PCM open");
    const auto params = Symbol<int (*)(void *, int, int, unsigned, unsigned, int, unsigned)>(
        library, "snd_pcm_set_params");
    Check(params(device, 14, 3, 2, 48000, 1, 50000) >= 0, "external null PCM params");
  }
  void Verify() {
    if (!library)
      return;
    Check(*global == root, "Nexora replaced the external global config");
    const auto search =
        Symbol<int (*)(void *, const char *, void **)>(library, "snd_config_search");
    const auto get = Symbol<int (*)(void *, const char **)>(library, "snd_config_get_string");
    void *node{};
    const char *type{};
    Check(search(root, "pcm.default.type", &node) >= 0 && get(node, &type) >= 0 &&
              std::string(type) == "null",
          "external borrowed config changed");
    const auto write =
        Symbol<long (*)(void *, const void *, unsigned long)>(library, "snd_pcm_writei");
    const float silence[2]{};
    Check(write(device, silence, 1) == 1, "external PCM stopped working");
  }
};
bool Open(nexora::runtime::GameplayAudio &audio, int expected) {
  std::string error;
  const bool opened = audio.OpenDefaultOutput(error);
  Check(expected < 0 || opened == (expected != 0), "unexpected output availability");
  Check(opened == error.empty(), "error disagrees with availability");
  const auto api = audio.Api();
  std::uint32_t available{};
  Check(api.output_available(api.context, &available) == 0 && available == unsigned(opened),
        "availability query disagrees with open");
  if (opened)
    Check(audio.Pump(1), "silent PCM write");
  else {
    const NexoraAudioPlayDesc desc{1, NEXORA_AUDIO_BUS_SFX, 0, 1};
    std::uint64_t voice{};
    Check(api.play(api.context, &desc, &voice) == -2 && !audio.Pump(1),
          "failed output accepted playback");
  }
  return opened;
}
} // namespace
int main(int argc, char **argv) {
  try {
    Check(argc == 2 || argc == 3, "usage: tests available|unavailable|system [peer-null-config]");
    const std::string mode = argv[1];
    Check(mode == "available" || mode == "unavailable" || mode == "system", "invalid mode");
    const int expected = mode == "system" ? -1 : mode == "available" ? 1 : 0;
    const char *path = std::getenv("ALSA_CONFIG_PATH");
    const bool configured = path != nullptr;
    const std::string config = path ? path : "";
    ExternalClient external;
    if (argc == 3) {
      external.Open(argv[2]);
      Check((configured ? setenv("ALSA_CONFIG_PATH", config.c_str(), 1)
                        : unsetenv("ALSA_CONFIG_PATH")) == 0,
            "restore tested config");
    }
    unsigned opened_count{};
    {
      nexora::runtime::GameplayAudio persistent;
      const bool persistent_opened = Open(persistent, expected);
      opened_count += persistent_opened;
      for (unsigned i = 0; i < 32; ++i) {
        {
          nexora::runtime::GameplayAudio transient;
          opened_count += Open(transient, expected);
          transient.CloseOutput();
          transient.CloseOutput();
          opened_count += Open(transient, expected);
          // Release a live generation through the destructor on each iteration.
        }
        if (persistent_opened)
          Check(persistent.Pump(1), "persistent owner invalidated by another close");
        external.Verify();
      }
    }
    external.Verify();
    std::cout << "{\"opens\":65,\"opened\":" << opened_count
              << ",\"peer\":" << (argc == 3 ? "true" : "false")
              << ",\"audible_acceptance\":\"NOT VERIFIED\"}\n";
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
