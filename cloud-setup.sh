#!/usr/bin/env bash
# Nexora — Claude Code cloud environment setup script
#
# 貼進 claude.ai/code → Environments → 你的環境 → Setup script。
# 這支 script 在 VM 建立時跑一次，結果會被快取，之後每個 session 直接用。
# 目標：讓 `cmake --preset linux-development` 到 `ctest` 全程可跑。

set -euo pipefail

echo "=== Nexora cloud environment setup ==="

SUDO=""
if [ "$(id -u)" -ne 0 ]; then
  SUDO="sudo"
fi

# ---------------------------------------------------------------------------
# 1. 工具鏈
#    CMakeLists.txt 要求 cmake >= 3.25，preset generator 是 Ninja。
#    Apt 版 cmake 在舊 image 上可能 < 3.25，所以檢查後從 Kitware 官方 tarball 補。
# ---------------------------------------------------------------------------
export DEBIAN_FRONTEND=noninteractive

$SUDO apt-get update -qq
$SUDO apt-get install -y -qq --no-install-recommends \
  build-essential \
  ninja-build \
  clang \
  clang-format \
  clang-tidy \
  git \
  curl \
  ca-certificates \
  python3 \
  pkg-config

need_cmake=1
if command -v cmake >/dev/null 2>&1; then
  current="$(cmake --version | head -n1 | awk '{print $3}')"
  if [ "$(printf '%s\n3.25.0\n' "$current" | sort -V | head -n1)" = "3.25.0" ]; then
    echo "cmake ${current} 已足夠 (>= 3.25)"
    need_cmake=0
  else
    echo "cmake ${current} 太舊，改裝 Kitware 版本"
  fi
fi

if [ "$need_cmake" -eq 1 ]; then
  CMAKE_VERSION="3.31.6"
  ARCH="$(uname -m)"
  curl -fsSL -o /tmp/cmake.tar.gz \
    "https://github.com/Kitware/CMake/releases/download/v${CMAKE_VERSION}/cmake-${CMAKE_VERSION}-linux-${ARCH}.tar.gz"
  $SUDO mkdir -p /opt/cmake
  $SUDO tar -xzf /tmp/cmake.tar.gz -C /opt/cmake --strip-components=1
  $SUDO ln -sf /opt/cmake/bin/cmake /usr/local/bin/cmake
  $SUDO ln -sf /opt/cmake/bin/ctest /usr/local/bin/ctest
  $SUDO ln -sf /opt/cmake/bin/cpack /usr/local/bin/cpack
  rm -f /tmp/cmake.tar.gz
fi

# ---------------------------------------------------------------------------
# 2. Zig（gameplay ABI smoke build 與 CI 使用的版本）
# ---------------------------------------------------------------------------
ZIG_VERSION="0.14.0"
if ! command -v zig >/dev/null 2>&1 || [ "$(zig version)" != "$ZIG_VERSION" ]; then
  curl -fsSL -o /tmp/zig.tar.xz \
    "https://ziglang.org/download/${ZIG_VERSION}/zig-linux-$(uname -m)-${ZIG_VERSION}.tar.xz"
  $SUDO mkdir -p /opt/zig
  $SUDO tar -xJf /tmp/zig.tar.xz -C /opt/zig --strip-components=1
  $SUDO ln -sf /opt/zig/zig /usr/local/bin/zig
  rm -f /tmp/zig.tar.xz
fi

# ---------------------------------------------------------------------------
# 3. 預熱一次 configure，讓第一個 session 不用從零開始
#    build/ 已在 .gitignore 內，不會污染 diff。
# ---------------------------------------------------------------------------
if [ -f CMakePresets.json ]; then
  cmake --preset linux-development -DNEXORA_ENABLE_ZIG_GAMEPLAY=ON || {
    echo "預熱 configure 失敗 — 不擋 setup，session 內再處理"
  }
fi

# ---------------------------------------------------------------------------
# 4. 版本回報，方便之後看 setup log 排查
# ---------------------------------------------------------------------------
echo "=== toolchain ==="
cmake --version | head -n1
ninja --version
clang --version | head -n1
zig version
echo "=== setup complete ==="
