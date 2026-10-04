#pragma once

#include "Nexora/Game/GameplayHostBridge.h"
#include "Nexora/Runtime/GameplayModuleHost.h"
#include "PlayInputState.h"
#include <algorithm>
#include <bit>
#include <filesystem>
#include <functional>
#include <memory>
#include <new>
#include <string>
#include <unordered_map>

namespace nexora::editor::preview {

// Owner-thread embedding of a component-oriented V3 gameplay library in the actual Play clone.
// Unload must precede PlaySession::Stop: destroy callbacks may still read/write that World.
class PlayGameplayModule final {
public:
  using LogSink = std::function<void(std::uint32_t, std::string)>;
  explicit PlayGameplayModule(LogSink log = {}) : log_(std::move(log)), module_(Host()) {}
  ~PlayGameplayModule() { Unload(); }
  PlayGameplayModule(const PlayGameplayModule &) = delete;
  PlayGameplayModule &operator=(const PlayGameplayModule &) = delete;

  bool LoadRelative(runtime::World &world, const std::filesystem::path &root,
                    std::string_view relative, std::string &error) try {
    Unload();
    const std::filesystem::path name{std::u8string(relative.begin(), relative.end())};
    if (name.empty() || name.is_absolute() || name.has_root_name()) {
      error = "Choose a gameplay library relative to the project root.";
      return false;
    }
    std::error_code ec;
    const auto directory = std::filesystem::canonical(root, ec);
    if (ec) {
      error = "Project root is unavailable.";
      return false;
    }
    const auto library = std::filesystem::canonical(directory / name, ec);
    const auto local = library.lexically_relative(directory);
    if (ec || local.empty() || local.is_absolute() || *local.begin() == ".." ||
        !std::filesystem::is_regular_file(library, ec) || ec) {
      error = "Gameplay library must be a file inside this project.";
      return false;
    }
    world_ = &world;
    if (!module_.Load(library)) {
      Unload();
      error = "Gameplay library could not start (path, ABI, or lifecycle failure).";
      return false;
    }
    return true;
  } catch (const std::exception &) {
    Unload();
    error = "Gameplay library path or load failed.";
    return false;
  }
  // Static loader overload is useful for embedding tests; it has the same owning lifetime.
  bool Load(runtime::World &world, NexoraGameModuleLoadV3Fn loader) {
    Unload();
    world_ = &world;
    if (module_.Load(loader))
      return true;
    Unload();
    return false;
  }
  void SetInputFocus(bool focused) noexcept { input_.SetFocused(focused); }
  void ProcessInput(std::span<const Nexora::Window::WindowEvent> events) noexcept {
    input_.Process(events);
  }
  [[nodiscard]] bool IsLoaded() const noexcept { return module_.IsLoaded(); }
  bool FixedUpdate(double seconds) {
    return !IsLoaded() || !module_.SupportsFixedUpdate() || module_.FixedUpdate(seconds);
  }
  bool Update(double seconds) { return !IsLoaded() || module_.Update(seconds); }
  void Unload() noexcept {
    input_.SetFocused(false);
    module_.Unload();
    world_ = nullptr;
    for (const auto &[pointer, allocation] : allocations_)
      ::operator delete(pointer, std::align_val_t(allocation.alignment));
    allocations_.clear();
    bytes_ = 0;
  }

private:
  struct Allocation final {
    std::uint64_t owner;
    std::size_t size, alignment;
  };
  static PlayGameplayModule &Self(void *context) {
    return *static_cast<PlayGameplayModule *>(context);
  }
  NexoraGameplayHostV3 Host() {
    NexoraGameplayHostV3 host{};
    host.struct_size = sizeof(host);
    host.abi_version = NEXORA_GAMEPLAY_ABI_VERSION;
    host.context = this;
    host.log = [](void *context, std::uint32_t level, const char *message, std::uint32_t length) {
      try {
        auto &self = Self(context);
        if (self.log_ && message)
          self.log_(level, std::string(message, std::min(length, 4096U)));
      } catch (...) {
      }
    };
    host.read_component = [](void *context, std::uint64_t entity, std::uint64_t type, void *data,
                             std::uint32_t size) -> int32_t {
      try {
        const auto *world = Self(context).world_;
        return world ? game::ReadGameplayComponent(*world, entity, type, data, size)
                     : NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT;
      } catch (...) {
        return NEXORA_GAMEPLAY_ERROR_LIFECYCLE;
      }
    };
    host.write_component = [](void *context, std::uint64_t entity, std::uint64_t type,
                              const void *data, std::uint32_t size) -> int32_t {
      try {
        auto *world = Self(context).world_;
        return world ? game::WriteGameplayComponent(*world, entity, type, data, size)
                     : NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT;
      } catch (...) {
        return NEXORA_GAMEPLAY_ERROR_LIFECYCLE;
      }
    };
    host.allocate = [](void *context, std::uint64_t owner, std::uint64_t size,
                       std::uint64_t alignment) -> void * {
      auto &self = Self(context);
      if (!size || size > 16 * 1024 * 1024 || !std::has_single_bit(alignment) ||
          alignment < alignof(void *) || alignment > 4096 || size > 64 * 1024 * 1024 - self.bytes_)
        return nullptr;
      auto *pointer =
          ::operator new(static_cast<std::size_t>(size), std::align_val_t(alignment), std::nothrow);
      if (!pointer)
        return nullptr;
      try {
        self.allocations_.emplace(pointer, Allocation{owner, static_cast<std::size_t>(size),
                                                      static_cast<std::size_t>(alignment)});
        self.bytes_ += size;
        return pointer;
      } catch (...) {
        ::operator delete(pointer, std::align_val_t(alignment));
        return nullptr;
      }
    };
    host.deallocate = [](void *context, std::uint64_t owner, void *pointer, std::uint64_t size,
                         std::uint64_t alignment) {
      auto &self = Self(context);
      const auto found = self.allocations_.find(pointer);
      if (found == self.allocations_.end() || found->second.owner != owner ||
          found->second.size != size || found->second.alignment != alignment)
        return;
      self.bytes_ -= found->second.size;
      ::operator delete(pointer, std::align_val_t(found->second.alignment));
      self.allocations_.erase(found);
    };
    host.capture_input = [](void *context, std::uint32_t user,
                            NexoraInputSnapshot *snapshot) -> int32_t {
      if (!snapshot || user != 0)
        return NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT;
      *snapshot = Self(context).input_.Snapshot();
      return NEXORA_GAMEPLAY_OK;
    };
    // No scene/physics capability is advertised: modules operate on cloned authored entities.
    return host;
  }
  runtime::World *world_{};
  LogSink log_;
  PlayInputState input_;
  std::unordered_map<void *, Allocation> allocations_;
  std::uint64_t bytes_{};
  runtime::GameplayModuleHost module_;
};
} // namespace nexora::editor::preview
