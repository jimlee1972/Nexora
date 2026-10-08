# Typed byte initializer: Linux verification

Windows desktop CI for PR #416, run 37714707509, job 113108727553 fails with MSVC C4244 / C2220 in Tests/Animation/PoseTests.cpp:86. The malformed quaternion fixture fills a uint8_t vector with an int zero. Use std::uint8_t{0} to preserve the exact zero bytes while avoiding the template conversion warning under warnings-as-errors. Test assertions and production source remain unchanged.

Full Linux Development configure/build and 106/106 CTest tests pass (133.09 seconds), including core/synchronization Vulkan validation. Commands: cmake --preset linux-development; cmake --build --preset linux-development --parallel 1; ctest --preset linux-development --output-on-failure.

The existing native Shipping evidence retains its runtime freeze 3dff93d9ba517e6d5e3787037c52f9227b8aaf81. This supplemental change only affects the test initializer. Windows CI must pass on the new PR head before merge.
