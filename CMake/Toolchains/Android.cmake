# Thin, checked-in entry point around the Android NDK's authoritative toolchain.
if(NOT DEFINED ENV{ANDROID_NDK_ROOT})
  message(FATAL_ERROR "ANDROID_NDK_ROOT must point to a supported Android NDK")
endif()
include("$ENV{ANDROID_NDK_ROOT}/build/cmake/android.toolchain.cmake")
