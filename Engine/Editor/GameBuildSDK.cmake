# Embed the existing SDK adapter as data, so an installed Editor does not need an engine checkout.
# These remain the canonical sources; changes trigger normal CMake regeneration.
file(GLOB nexora_game_sdk_bindings CONFIGURE_DEPENDS RELATIVE "${PROJECT_SOURCE_DIR}"
  "${PROJECT_SOURCE_DIR}/Bindings/Zig/*.zig")
set(nexora_game_sdk_files ${nexora_game_sdk_bindings}
  CMake/Modules/NexoraGameProject.cmake Tools/Zig/build.zig Tools/Zig/game_entry.zig
  Tools/Build/RunZigTestWithHostLinker.py LICENSE)
# Keep each C++ literal below MSVC's single-literal limit. Store byte chunks without
# startup allocations; the worker assembles an owning string only when consuming a file.
set(nexora_game_sdk_header "// Generated from canonical engine SDK sources.\n#pragma once\n#include <span>\n#include <string>\n#include <string_view>\nnamespace nexora::editor::detail {\nstruct GameSDKFile {\n  std::string_view path;\n  std::span<const std::string_view> chunks;\n  std::string Bytes() const {\n    std::string result;\n    for (auto chunk : chunks) result.append(chunk);\n    return result;\n  }\n};\n")
set(nexora_game_sdk_entries "")
set(nexora_game_sdk_index 0)
foreach(relative IN LISTS nexora_game_sdk_files)
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/${relative}")
  file(READ "${PROJECT_SOURCE_DIR}/${relative}" contents HEX)
  string(LENGTH "${contents}" hex_size)
  string(APPEND nexora_game_sdk_header "inline constexpr std::string_view kGameSDKChunks${nexora_game_sdk_index}[] = {\n")
  set(offset 0)
  while(offset LESS hex_size)
    math(EXPR remaining "${hex_size} - ${offset}")
    if(remaining GREATER 4096)
      set(remaining 4096)
    endif()
    string(SUBSTRING "${contents}" ${offset} ${remaining} chunk)
    string(REGEX REPLACE "(..)" "\\\\x\\1" encoded "${chunk}")
    math(EXPR byte_size "${remaining} / 2")
    string(APPEND nexora_game_sdk_header "{\"${encoded}\", ${byte_size}},\n")
    math(EXPR offset "${offset} + ${remaining}")
  endwhile()
  if(hex_size EQUAL 0)
    string(APPEND nexora_game_sdk_header "{\"\", 0},\n")
  endif()
  string(APPEND nexora_game_sdk_header "};\n")
  string(APPEND nexora_game_sdk_entries "{\"${relative}\", kGameSDKChunks${nexora_game_sdk_index}},\n")
  math(EXPR nexora_game_sdk_index "${nexora_game_sdk_index} + 1")
endforeach()
string(APPEND nexora_game_sdk_header "inline constexpr GameSDKFile kGameSDKFiles[] = {\n${nexora_game_sdk_entries}};\n}\n")
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/generated/GameBuildSDKFiles.h"
  CONTENT "${nexora_game_sdk_header}")
target_include_directories(NexoraEditorCore PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
