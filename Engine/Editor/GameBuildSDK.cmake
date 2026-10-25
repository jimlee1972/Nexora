# Embed the existing SDK adapter as data, so an installed Editor does not need an engine checkout.
# These remain the canonical sources; changes trigger normal CMake regeneration.
file(GLOB nexora_game_sdk_bindings CONFIGURE_DEPENDS RELATIVE "${PROJECT_SOURCE_DIR}"
  "${PROJECT_SOURCE_DIR}/Bindings/Zig/*.zig")
set(nexora_game_sdk_files ${nexora_game_sdk_bindings}
  CMake/Modules/NexoraGameProject.cmake Tools/Zig/build.zig Tools/Zig/game_entry.zig
  Tools/Build/RunZigTestWithHostLinker.py LICENSE)
set(nexora_game_sdk_header "// Generated from canonical engine SDK sources.\n#pragma once\n#include <string_view>\nnamespace nexora::editor::detail {\nstruct GameSDKFile {std::string_view path, bytes;};\ninline constexpr GameSDKFile kGameSDKFiles[] = {\n")
foreach(relative IN LISTS nexora_game_sdk_files)
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/${relative}")
  file(READ "${PROJECT_SOURCE_DIR}/${relative}" contents)
  string(SHA256 digest "${contents}")
  string(SUBSTRING "${digest}" 0 12 delimiter)
  string(APPEND nexora_game_sdk_header "{\"${relative}\", R\"${delimiter}(${contents})${delimiter}\"},\n")
endforeach()
string(APPEND nexora_game_sdk_header "};\n}\n")
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/generated/GameBuildSDKFiles.h"
  CONTENT "${nexora_game_sdk_header}")
target_include_directories(NexoraEditorCore PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
