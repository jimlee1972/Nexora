#pragma once

// Test-only optional bridge for proving host reentrancy rejection through actual native code.
struct NativeToolFixtureBridge final {
  void *context{};
  void (*call)(void *){};
};
