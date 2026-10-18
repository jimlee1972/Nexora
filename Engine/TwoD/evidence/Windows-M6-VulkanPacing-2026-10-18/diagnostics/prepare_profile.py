from pathlib import Path
root = Path(__file__).resolve().parents[2]
work = Path(__file__).resolve().parent
s = (root/'Tools/TwoD/Benchmark.cpp').read_text()
s = '#include "ProfileTrace.h"\n#include <cstdlib>\n#include <array>\n' + s
s = s.replace('  constexpr unsigned warmup = 4;', '''  constexpr unsigned warmup = 4;
  struct Sample { unsigned frame; std::array<double, 8> stages; ProfileTrace::Frame native; };
  std::vector<Sample> samples; samples.reserve(frames + warmup);
  const auto elapsed = [](auto a, auto b) { return std::chrono::duration<double, std::milli>(b-a).count(); };
''')
s = s.replace('    const auto begin = Clock::now();', '    ProfileTrace::Begin();\n    const auto begin = Clock::now();')
s = s.replace('    if (native)\n      Check(surface.surface->BeginFrame()', '    const auto beforeAcquire = Clock::now();\n    if (native)\n      Check(surface.surface->BeginFrame()')
s = s.replace('    Check(index.Build(shapes)', '    const auto populated = Clock::now();\n    Check(index.Build(shapes)')
s = s.replace('    twod::Shape query;', '    const auto indexed = Clock::now();\n    twod::Shape query;')
s = s.replace('    auto extracted =', '    const auto queried = Clock::now();\n    auto extracted =')
s = s.replace('    visible = extracted->statistics.visible;', '    const auto extractedAt = Clock::now();\n    visible = extracted->statistics.visible;')
s = s.replace('    if (native) {\n      Check(extracted->Draw', '    auto drawn = Clock::now();\n    if (native) {\n      Check(extracted->Draw')
s = s.replace('      Check(surface.surface->EndFrame()', '      drawn = Clock::now();\n      Check(surface.surface->EndFrame()')
s = s.replace('    peak = std::max(peak, ResidentBytes());', '''    const auto end = Clock::now();
    samples.push_back({frame,{elapsed(begin,beforeAcquire),elapsed(beforeAcquire,update),elapsed(update,populated),elapsed(populated,indexed),elapsed(indexed,queried),elapsed(queried,extractedAt),elapsed(extractedAt,drawn),elapsed(drawn,end)},ProfileTrace::Current()});
    peak = std::max(peak, ResidentBytes());''')
s = s.replace('  report << count', '''  for (const auto &s : samples) {
    std::cerr << "TRACE," << count << ',' << sparse << ',' << overlap << ',' << s.frame;
    for (auto t : s.stages) std::cerr << ',' << t;
    const auto &n = s.native;
    std::cerr << ',' << n.fence << ',' << n.acquire << ',' << n.submit << ',' << n.present << ',' << n.image << ',' << n.images << ',' << n.requestedImages << ',' << n.mode << '\\n';
  }
  report << count''',1)
(work/'BenchmarkProfile.cpp').write_text(s)
cmake = root/'Tools/TwoD/CMakeLists.txt'
original = cmake.read_bytes()
(work/'TwoDCMake.original').write_bytes(original)
cmake.write_bytes(original + b'''\n# Temporary ignored diagnostic target; removed before delivery.\nadd_executable(NexoraTwoDTailProfile "${CMAKE_SOURCE_DIR}/build/windows-vulkan-tail-20261018/BenchmarkProfile.cpp" "${CMAKE_SOURCE_DIR}/build/windows-vulkan-tail-20261018/VulkanProfile.cpp")\ntarget_include_directories(NexoraTwoDTailProfile PRIVATE "${NEXORA_VULKAN_INCLUDE_DIR}")\ntarget_link_libraries(NexoraTwoDTailProfile PRIVATE Nexora::TwoD "${NEXORA_VULKAN_LIBRARY}")\nnexora_apply_defaults(NexoraTwoDTailProfile)\n''')
