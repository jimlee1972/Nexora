set(NEXORA_ZIG_EXECUTABLE "" CACHE FILEPATH "Path to the Zig compiler")
if(NOT NEXORA_ZIG_EXECUTABLE)
  # An empty cache entry prevents find_program from searching the environment.
  # Drop it first so CI-provisioned Zig installations can be discovered on PATH.
  unset(NEXORA_ZIG_EXECUTABLE CACHE)
  find_program(NEXORA_ZIG_EXECUTABLE NAMES zig)
endif()

if(NOT NEXORA_ZIG_EXECUTABLE)
  message(FATAL_ERROR "Zig compiler was not found; set NEXORA_ZIG_EXECUTABLE to the zig executable path")
endif()
if(NOT EXISTS "${NEXORA_ZIG_EXECUTABLE}")
  message(FATAL_ERROR "NEXORA_ZIG_EXECUTABLE does not exist: ${NEXORA_ZIG_EXECUTABLE}")
endif()

set(NEXORA_ZIG_SAMPLE_OBJECT "${CMAKE_CURRENT_BINARY_DIR}/zig_gameplay_sample${CMAKE_CXX_OUTPUT_EXTENSION}")
set(NEXORA_ZIG_SHARED_LIBRARY
    "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_SHARED_LIBRARY_PREFIX}NexoraZigGameplay${CMAKE_SHARED_LIBRARY_SUFFIX}")
if(NOT CMAKE_CXX_OUTPUT_EXTENSION)
  if(WIN32)
    set(NEXORA_ZIG_SAMPLE_OBJECT "${CMAKE_CURRENT_BINARY_DIR}/zig_gameplay_sample.obj")
  else()
    set(NEXORA_ZIG_SAMPLE_OBJECT "${CMAKE_CURRENT_BINARY_DIR}/zig_gameplay_sample.o")
  endif()
endif()

set(NEXORA_ZIG_OBJECT_OPTIMIZATION Debug)
if(CMAKE_BUILD_TYPE STREQUAL "Shipping")
  set(NEXORA_ZIG_OBJECT_OPTIMIZATION ReleaseSafe)
endif()

set(NEXORA_ZIG_BUILD_OBJ_ARGS
  build-obj
  --dep nexora
  -O ${NEXORA_ZIG_OBJECT_OPTIMIZATION}
  # A plain `zig build-obj` doesn't link in Zig's own compiler-rt, so the
  # stack-check safety instrumentation it emits by default is left calling
  # an undefined `__zig_probe_stack` on Linux/macOS.
  # See https://github.com/ziglang/zig/issues/6817.
  -fno-stack-check
  # Without an explicit CPU, Zig targets the *build machine's* native CPU, so a package built on a
  # CI runner can fault with an illegal instruction on older or different CPUs. Ship a portable
  # baseline instead.
  -mcpu=baseline)
if(WIN32)
  # -fno-stack-check above covers __zig_probe_stack, but large-stack-frame
  # functions still need a stack-probe thunk regardless of that safety
  # feature; the object references it as ___chkstk_ms (Zig's own name for
  # it), which only exists if Zig's compiler-rt is bundled into the object,
  # since link.exe doesn't provide it the way it would for MSVC-compiled
  # code. See https://github.com/ziglang/zig/issues/15107.
  list(APPEND NEXORA_ZIG_BUILD_OBJ_ARGS -fcompiler-rt)
else()
  # Without this, zig build-obj emits non-PIE code on Linux/macOS, which
  # cannot be linked into NexoraZigGameplayTests, a normal PIE executable:
  # on modern toolchains: "relocation R_X86_64_32S against `.rodata.cst16'
  # can not be used when making a PIE object; recompile with -fPIE".
  # Standalone objects also need compiler-rt because build-obj does not bundle
  # __zig_probe_stack with the object by default.
  list(APPEND NEXORA_ZIG_BUILD_OBJ_ARGS -fPIE -fcompiler-rt)
endif()
list(APPEND NEXORA_ZIG_BUILD_OBJ_ARGS
  -Mroot=${CMAKE_CURRENT_SOURCE_DIR}/src/game_module.zig
  -Mnexora=${PROJECT_SOURCE_DIR}/Bindings/Zig/nexora.zig
  -femit-bin=${NEXORA_ZIG_SAMPLE_OBJECT})

add_custom_command(
  OUTPUT "${NEXORA_ZIG_SAMPLE_OBJECT}"
  COMMAND "${NEXORA_ZIG_EXECUTABLE}" ${NEXORA_ZIG_BUILD_OBJ_ARGS}
  DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/game_module.zig"
    "${PROJECT_SOURCE_DIR}/Bindings/Zig/nexora.zig"
    "${PROJECT_SOURCE_DIR}/Engine/API/include/nexora/nexora.h"
  COMMENT "Building the Zig gameplay ABI sample"
  VERBATIM)

set_source_files_properties("${NEXORA_ZIG_SAMPLE_OBJECT}" PROPERTIES
  EXTERNAL_OBJECT TRUE
  GENERATED TRUE)

add_custom_target(NexoraZigGameplayBuild DEPENDS "${NEXORA_ZIG_SAMPLE_OBJECT}")

if(APPLE)
  # Zig 0.14's Mach-O linker cannot resolve libSystem on the macOS CI SDK.
  # The PIC Zig object is already compiled; use the host linker for the dylib.
  add_custom_command(
    OUTPUT "${NEXORA_ZIG_SHARED_LIBRARY}"
    COMMAND "${CMAKE_CXX_COMPILER}" -dynamiclib "${NEXORA_ZIG_SAMPLE_OBJECT}"
            -o "${NEXORA_ZIG_SHARED_LIBRARY}"
    DEPENDS "${NEXORA_ZIG_SAMPLE_OBJECT}"
    COMMENT "Linking the Zig gameplay Development shared library with Apple Clang"
    VERBATIM)
else()
  set(NEXORA_ZIG_BUILD_LIB_ARGS
    build-lib
    --dep nexora
    -O Debug
    -mcpu=baseline
    -Mroot=${CMAKE_CURRENT_SOURCE_DIR}/src/game_module.zig
    -Mnexora=${PROJECT_SOURCE_DIR}/Bindings/Zig/nexora.zig
    -dynamic -femit-bin=${NEXORA_ZIG_SHARED_LIBRARY})
  if(WIN32)
    list(APPEND NEXORA_ZIG_BUILD_LIB_ARGS -fcompiler-rt)
  endif()
  add_custom_command(
    OUTPUT "${NEXORA_ZIG_SHARED_LIBRARY}"
    COMMAND "${NEXORA_ZIG_EXECUTABLE}" ${NEXORA_ZIG_BUILD_LIB_ARGS}
    DEPENDS
      "${CMAKE_CURRENT_SOURCE_DIR}/src/game_module.zig"
      "${PROJECT_SOURCE_DIR}/Bindings/Zig/nexora.zig"
      "${PROJECT_SOURCE_DIR}/Engine/API/include/nexora/nexora.h"
    COMMENT "Building the Zig gameplay Development shared library"
    VERBATIM)
endif()
add_custom_target(NexoraZigGameplaySharedBuild DEPENDS "${NEXORA_ZIG_SHARED_LIBRARY}")

set(NEXORA_ZIG_SAMPLE_OBJECT "${NEXORA_ZIG_SAMPLE_OBJECT}" PARENT_SCOPE)
set(NEXORA_ZIG_SHARED_LIBRARY "${NEXORA_ZIG_SHARED_LIBRARY}" PARENT_SCOPE)

if(BUILD_TESTING)
  add_executable(NexoraZigGameplayTests
    ZigGameplayTests.cpp
    "${NEXORA_ZIG_SAMPLE_OBJECT}")
  add_dependencies(NexoraZigGameplayTests NexoraZigGameplayBuild)
  target_link_libraries(NexoraZigGameplayTests PRIVATE Nexora::Runtime)
  target_include_directories(NexoraZigGameplayTests PRIVATE "${PROJECT_SOURCE_DIR}/Tests/Gameplay")
  if(WIN32)
    # Zig's std library calls raw NT syscalls directly (file I/O, debug-info
    # lookup for its panic handler, etc.); zig's own linker driver links
    # ntdll.lib for this automatically, but link.exe invoked from CMake does
    # not. See https://github.com/ziglang/zig/issues/18893.
    target_link_libraries(NexoraZigGameplayTests PRIVATE ntdll)
  endif()
  nexora_apply_defaults(NexoraZigGameplayTests)
  add_test(NAME gameplay.zig_abi_smoke COMMAND NexoraZigGameplayTests)
  get_filename_component(pointer_object_extension "${NEXORA_ZIG_SAMPLE_OBJECT}" LAST_EXT)
  set(pointer_object "${CMAKE_CURRENT_BINARY_DIR}/zig_pointer_layout${pointer_object_extension}")
  set(pointer_object_args ${NEXORA_ZIG_BUILD_OBJ_ARGS})
  list(TRANSFORM pointer_object_args REPLACE "src/game_module.zig" "src/pointer_layout.zig")
  list(TRANSFORM pointer_object_args REPLACE "-femit-bin=.*" "-femit-bin=${pointer_object}")
  add_custom_command(OUTPUT "${pointer_object}"
    COMMAND "${NEXORA_ZIG_EXECUTABLE}" ${pointer_object_args}
    DEPENDS src/pointer_layout.zig "${PROJECT_SOURCE_DIR}/Bindings/Zig/nexora.zig"
            "${PROJECT_SOURCE_DIR}/Engine/API/include/nexora/nexora.h" VERBATIM)
  set_source_files_properties("${pointer_object}" PROPERTIES EXTERNAL_OBJECT TRUE GENERATED TRUE)
  add_executable(NexoraZigPointerLayoutTests PointerLayoutTests.cpp "${pointer_object}")
  target_link_libraries(NexoraZigPointerLayoutTests PRIVATE Nexora::Foundation)
  if(WIN32)
    target_link_libraries(NexoraZigPointerLayoutTests PRIVATE ntdll)
  endif()
  nexora_apply_defaults(NexoraZigPointerLayoutTests)
  add_test(NAME gameplay.zig_pointer_wire_layout COMMAND NexoraZigPointerLayoutTests)
endif()

# A separate 2D module uses the same C++ process owner and V3 lifecycle as the 3D sample.
if(NEXORA_ENABLE_2D)
  get_filename_component(twod_object_extension "${NEXORA_ZIG_SAMPLE_OBJECT}" LAST_EXT)
  set(twod_object "${CMAKE_CURRENT_BINARY_DIR}/zig_twod${twod_object_extension}")
  set(twod_library "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_SHARED_LIBRARY_PREFIX}NexoraZigTwoD${CMAKE_SHARED_LIBRARY_SUFFIX}")
  set(twod_object_args ${NEXORA_ZIG_BUILD_OBJ_ARGS})
  list(TRANSFORM twod_object_args REPLACE "src/game_module.zig" "src/twod_module.zig")
  list(TRANSFORM twod_object_args REPLACE "-femit-bin=.*" "-femit-bin=${twod_object}")
  add_custom_command(OUTPUT "${twod_object}"
    COMMAND "${NEXORA_ZIG_EXECUTABLE}" ${twod_object_args}
    DEPENDS src/twod_module.zig "${PROJECT_SOURCE_DIR}/Bindings/Zig/nexora.zig"
            "${PROJECT_SOURCE_DIR}/Engine/API/include/nexora/nexora.h"
    COMMENT "Building Zig 2D gameplay module" VERBATIM)
  set_source_files_properties("${twod_object}" PROPERTIES EXTERNAL_OBJECT TRUE GENERATED TRUE)
  if(APPLE)
    add_custom_command(OUTPUT "${twod_library}"
      COMMAND "${CMAKE_CXX_COMPILER}" -dynamiclib "${twod_object}" -o "${twod_library}"
      DEPENDS "${twod_object}" VERBATIM)
  else()
    set(twod_library_args ${NEXORA_ZIG_BUILD_LIB_ARGS})
    list(TRANSFORM twod_library_args REPLACE "src/game_module.zig" "src/twod_module.zig")
    list(TRANSFORM twod_library_args REPLACE "-femit-bin=.*" "-femit-bin=${twod_library}")
    add_custom_command(OUTPUT "${twod_library}"
      COMMAND "${NEXORA_ZIG_EXECUTABLE}" ${twod_library_args}
      DEPENDS src/twod_module.zig "${PROJECT_SOURCE_DIR}/Bindings/Zig/nexora.zig"
              "${PROJECT_SOURCE_DIR}/Engine/API/include/nexora/nexora.h" VERBATIM)
  endif()
  add_custom_target(NexoraZigTwoDSharedBuild DEPENDS "${twod_library}")
  add_executable(NexoraZigTwoD TwoDHost.cpp "${twod_object}")
  if(NOT CMAKE_BUILD_TYPE STREQUAL "Shipping")
    add_dependencies(NexoraZigTwoD NexoraZigTwoDSharedBuild)
  endif()
  target_link_libraries(NexoraZigTwoD PRIVATE NexoraTwoDPlayerHost)
  if(MINGW)
    target_link_options(NexoraZigTwoD PRIVATE -municode)
  endif()
  if(WIN32)
    target_link_libraries(NexoraZigTwoD PRIVATE ntdll)
  endif()
  nexora_apply_defaults(NexoraZigTwoD)
  if(BUILD_TESTING)
    add_test(NAME gameplay.zig_twod_static COMMAND NexoraZigTwoD --frames 60)
    add_test(NAME gameplay.zig_twod_dynamic COMMAND NexoraZigTwoD --frames 60 --module "${twod_library}")
    add_executable(NexoraZigTwoDLayoutTests TwoDLayoutTests.cpp "${twod_object}")
    target_link_libraries(NexoraZigTwoDLayoutTests PRIVATE Nexora::TwoD)
    if(WIN32)
      target_link_libraries(NexoraZigTwoDLayoutTests PRIVATE ntdll)
    endif()
    nexora_apply_defaults(NexoraZigTwoDLayoutTests)
    add_test(NAME gameplay.zig_twod_wire_layout COMMAND NexoraZigTwoDLayoutTests)
  endif()
endif()

# Track every included wire module when compiling consumers of the canonical Zig module.
file(GLOB gameplay_wire_dependencies CONFIGURE_DEPENDS
  "${PROJECT_SOURCE_DIR}/Bindings/Zig/*.zig"
  "${PROJECT_SOURCE_DIR}/Engine/API/include/nexora/gameplay_*.h")
add_custom_command(OUTPUT "${NEXORA_ZIG_SAMPLE_OBJECT}" APPEND DEPENDS ${gameplay_wire_dependencies})
add_custom_command(OUTPUT "${NEXORA_ZIG_SHARED_LIBRARY}" APPEND DEPENDS ${gameplay_wire_dependencies})
if(BUILD_TESTING)
  add_custom_command(OUTPUT "${pointer_object}" APPEND DEPENDS ${gameplay_wire_dependencies})
endif()
if(NEXORA_ENABLE_2D)
  add_custom_command(OUTPUT "${twod_object}" APPEND DEPENDS ${gameplay_wire_dependencies})
  add_custom_command(OUTPUT "${twod_library}" APPEND DEPENDS ${gameplay_wire_dependencies})
endif()
if(BUILD_TESTING AND NEXORA_ENABLE_2D)
  set(services_object "${CMAKE_CURRENT_BINARY_DIR}/zig_services${twod_object_extension}")
  set(services_args ${NEXORA_ZIG_BUILD_OBJ_ARGS})
  list(TRANSFORM services_args REPLACE "src/game_module.zig" "src/services_consumer.zig")
  list(TRANSFORM services_args REPLACE "-femit-bin=.*" "-femit-bin=${services_object}")
  add_custom_command(OUTPUT "${services_object}"
    COMMAND "${NEXORA_ZIG_EXECUTABLE}" ${services_args}
    DEPENDS src/services_consumer.zig ${gameplay_wire_dependencies}
            "${PROJECT_SOURCE_DIR}/Bindings/Zig/nexora.zig"
            "${PROJECT_SOURCE_DIR}/Engine/API/include/nexora/nexora.h" VERBATIM)
  set_source_files_properties("${services_object}" PROPERTIES EXTERNAL_OBJECT TRUE GENERATED TRUE)
  add_executable(NexoraZigServicesTests ServicesConsumerTests.cpp "${services_object}")
  target_link_libraries(NexoraZigServicesTests PRIVATE Nexora::TwoD)
  if(WIN32)
    target_link_libraries(NexoraZigServicesTests PRIVATE ntdll)
  endif()
  nexora_apply_defaults(NexoraZigServicesTests)
  add_test(NAME gameplay.zig_services_consumer COMMAND NexoraZigServicesTests)
  foreach(wire input ui audio storage content display ui_draw assets color)
    nexora_add_zig_test(gameplay.zig_${wire}_wrapper
        "${PROJECT_SOURCE_DIR}/Bindings/Zig/${wire}.zig"
        --cache-dir "${CMAKE_CURRENT_BINARY_DIR}/zig-test-${wire}"
        --global-cache-dir "${CMAKE_CURRENT_BINARY_DIR}/zig-global-${wire}")
  endforeach()
endif()

if(BUILD_TESTING AND NEXORA_ENABLE_2D AND NOT APPLE)
  find_package(Python3 REQUIRED COMPONENTS Interpreter)
  set(display_library "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_SHARED_LIBRARY_PREFIX}NexoraZigDisplay${CMAKE_SHARED_LIBRARY_SUFFIX}")
  set(display_args ${NEXORA_ZIG_BUILD_LIB_ARGS})
  list(TRANSFORM display_args REPLACE "src/game_module.zig" "src/display_module.zig")
  list(TRANSFORM display_args REPLACE "-femit-bin=.*" "-femit-bin=${display_library}")
  add_custom_command(OUTPUT "${display_library}"
    COMMAND "${NEXORA_ZIG_EXECUTABLE}" ${display_args}
    DEPENDS src/display_module.zig ${gameplay_wire_dependencies} VERBATIM)
  add_custom_target(NexoraZigDisplayBuild ALL DEPENDS "${display_library}")
  add_test(NAME gameplay.zig_display_player COMMAND "${Python3_EXECUTABLE}"
    "${PROJECT_SOURCE_DIR}/Tests/TwoD/DisplayAcceptance.py"
    --app "$<TARGET_FILE:NexoraTwoDPlayer>" --module "${display_library}")
  set_tests_properties(gameplay.zig_display_player PROPERTIES TIMEOUT 90 LABELS "twod;zig;display;project")
endif()

if(BUILD_TESTING AND NEXORA_ENABLE_2D)
  set(ui_showcase_library "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_SHARED_LIBRARY_PREFIX}NexoraZigUiShowcase${CMAKE_SHARED_LIBRARY_SUFFIX}")
  set(ui_showcase_object "${CMAKE_CURRENT_BINARY_DIR}/zig_ui_showcase${twod_object_extension}")
  foreach(kind IN ITEMS library object)
    if(APPLE AND kind STREQUAL "library")
      # Match the existing Zig dylib path: Apple Clang resolves the platform SDK/libSystem.
      add_custom_command(OUTPUT "${ui_showcase_library}"
        COMMAND "${CMAKE_CXX_COMPILER}" -dynamiclib "${ui_showcase_object}" -o "${ui_showcase_library}"
        DEPENDS "${ui_showcase_object}" VERBATIM)
      continue()
    endif()
    if(kind STREQUAL "library")
      set(ui_showcase_args ${NEXORA_ZIG_BUILD_LIB_ARGS})
    else()
      set(ui_showcase_args ${NEXORA_ZIG_BUILD_OBJ_ARGS})
    endif()
    list(TRANSFORM ui_showcase_args REPLACE "src/game_module.zig" "src/ui_showcase.zig")
    list(TRANSFORM ui_showcase_args REPLACE "-femit-bin=.*" "-femit-bin=${ui_showcase_${kind}}")
    add_custom_command(OUTPUT "${ui_showcase_${kind}}"
      COMMAND "${NEXORA_ZIG_EXECUTABLE}" ${ui_showcase_args}
      DEPENDS src/ui_showcase.zig src/fixtures/ui_fixture.nxfont ${gameplay_wire_dependencies} VERBATIM)
  endforeach()
  add_custom_target(NexoraZigUiShowcaseBuild ALL DEPENDS "${ui_showcase_library}")
  set_source_files_properties("${ui_showcase_object}" PROPERTIES EXTERNAL_OBJECT TRUE GENERATED TRUE)
  add_executable(NexoraZigUiShowcaseTests UiShowcaseTests.cpp "${ui_showcase_object}")
  target_link_libraries(NexoraZigUiShowcaseTests PRIVATE Nexora::TwoD)
  if(WIN32)
    target_link_libraries(NexoraZigUiShowcaseTests PRIVATE ntdll)
  endif()
  nexora_apply_defaults(NexoraZigUiShowcaseTests)
  add_test(NAME gameplay.zig_ui_showcase COMMAND NexoraZigUiShowcaseTests)
  if(NOT APPLE)
    add_test(NAME gameplay.zig_image_ui_player COMMAND "${Python3_EXECUTABLE}"
      "${PROJECT_SOURCE_DIR}/Tests/TwoD/UiShowcaseAcceptance.py"
      --app "$<TARGET_FILE:NexoraTwoDPlayer>" --module "${ui_showcase_library}")
    set_tests_properties(gameplay.zig_image_ui_player PROPERTIES TIMEOUT 90 LABELS "twod;zig;ui;project")
  endif()
endif()

if(BUILD_TESTING AND NEXORA_ENABLE_2D)
  set(asset_transitions_library "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_SHARED_LIBRARY_PREFIX}NexoraZigAssetTransitions${CMAKE_SHARED_LIBRARY_SUFFIX}")
  if(APPLE)
    set(asset_transitions_object "${CMAKE_CURRENT_BINARY_DIR}/zig_asset_transitions${twod_object_extension}")
    set(asset_transitions_args ${NEXORA_ZIG_BUILD_OBJ_ARGS})
    list(TRANSFORM asset_transitions_args REPLACE "src/game_module.zig" "src/asset_transitions.zig")
    list(TRANSFORM asset_transitions_args REPLACE "-femit-bin=.*" "-femit-bin=${asset_transitions_object}")
    add_custom_command(OUTPUT "${asset_transitions_object}"
      COMMAND "${NEXORA_ZIG_EXECUTABLE}" ${asset_transitions_args}
      DEPENDS src/asset_transitions.zig ${gameplay_wire_dependencies} VERBATIM)
    add_custom_command(OUTPUT "${asset_transitions_library}"
      COMMAND "${CMAKE_CXX_COMPILER}" -dynamiclib "${asset_transitions_object}" -o "${asset_transitions_library}"
      DEPENDS "${asset_transitions_object}" VERBATIM)
  else()
    set(asset_transitions_args ${NEXORA_ZIG_BUILD_LIB_ARGS})
    list(TRANSFORM asset_transitions_args REPLACE "src/game_module.zig" "src/asset_transitions.zig")
    list(TRANSFORM asset_transitions_args REPLACE "-femit-bin=.*" "-femit-bin=${asset_transitions_library}")
    add_custom_command(OUTPUT "${asset_transitions_library}"
      COMMAND "${NEXORA_ZIG_EXECUTABLE}" ${asset_transitions_args}
      DEPENDS src/asset_transitions.zig ${gameplay_wire_dependencies} VERBATIM)
  endif()
  add_custom_target(NexoraZigAssetTransitionsBuild ALL DEPENDS "${asset_transitions_library}")
  if(NOT APPLE)
    add_test(NAME gameplay.zig_asset_transitions COMMAND "${Python3_EXECUTABLE}"
      "${PROJECT_SOURCE_DIR}/Tests/TwoD/AssetTransitionsAcceptance.py"
      --app "$<TARGET_FILE:NexoraTwoDPlayer>" --module "${asset_transitions_library}"
      --fixture "$<TARGET_FILE:NexoraGameplayAssetsTests>")
    set_tests_properties(gameplay.zig_asset_transitions PROPERTIES TIMEOUT 180 LABELS "twod;zig;assets;project")
  endif()
endif()

if(BUILD_TESTING AND NEXORA_ENABLE_2D AND NOT APPLE)
  set(color_slice_library "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_SHARED_LIBRARY_PREFIX}NexoraZigColorSlice${CMAKE_SHARED_LIBRARY_SUFFIX}")
  set(color_slice_args ${NEXORA_ZIG_BUILD_LIB_ARGS})
  list(TRANSFORM color_slice_args REPLACE "src/game_module.zig" "src/color_slice.zig")
  list(TRANSFORM color_slice_args REPLACE "-femit-bin=.*" "-femit-bin=${color_slice_library}")
  add_custom_command(OUTPUT "${color_slice_library}"
    COMMAND "${NEXORA_ZIG_EXECUTABLE}" ${color_slice_args}
    DEPENDS src/color_slice.zig ${gameplay_wire_dependencies} VERBATIM)
  add_custom_target(NexoraZigColorSliceBuild ALL DEPENDS "${color_slice_library}")
  add_test(NAME gameplay.zig_color_slice COMMAND "${Python3_EXECUTABLE}"
    "${PROJECT_SOURCE_DIR}/Tests/TwoD/ColorSliceAcceptance.py"
    --app "$<TARGET_FILE:NexoraTwoDPlayer>" --module "${color_slice_library}"
    --compiler "$<TARGET_FILE:NexoraTwoDAssetCompiler>")
  set_tests_properties(gameplay.zig_color_slice PROPERTIES TIMEOUT 90 LABELS "twod;zig;color;project")
endif()

if(BUILD_TESTING AND NEXORA_ENABLE_2D)
  set(voice_library "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_SHARED_LIBRARY_PREFIX}NexoraZigVoiceDialogue${CMAKE_SHARED_LIBRARY_SUFFIX}")
  set(voice_object "${CMAKE_CURRENT_BINARY_DIR}/zig_voice_dialogue${twod_object_extension}")
  foreach(kind IN ITEMS library object)
    if(APPLE AND kind STREQUAL "library")
      add_custom_command(OUTPUT "${voice_library}"
        COMMAND "${CMAKE_CXX_COMPILER}" -dynamiclib "${voice_object}" -o "${voice_library}"
        DEPENDS "${voice_object}" VERBATIM)
      continue()
    endif()
    if(kind STREQUAL "library")
      set(voice_args ${NEXORA_ZIG_BUILD_LIB_ARGS})
    else()
      set(voice_args ${NEXORA_ZIG_BUILD_OBJ_ARGS})
    endif()
    list(TRANSFORM voice_args REPLACE "src/game_module.zig" "src/voice_dialogue.zig")
    list(TRANSFORM voice_args REPLACE "-femit-bin=.*" "-femit-bin=${voice_${kind}}")
    add_custom_command(OUTPUT "${voice_${kind}}"
      COMMAND "${NEXORA_ZIG_EXECUTABLE}" ${voice_args}
      DEPENDS src/voice_dialogue.zig ${gameplay_wire_dependencies} VERBATIM)
  endforeach()
  add_custom_target(NexoraZigVoiceDialogueBuild ALL DEPENDS "${voice_library}")
  set_source_files_properties("${voice_object}" PROPERTIES EXTERNAL_OBJECT TRUE GENERATED TRUE)
  add_executable(NexoraZigVoiceDialogueTests VoiceDialogueTests.cpp "${voice_object}")
  target_link_libraries(NexoraZigVoiceDialogueTests PRIVATE Nexora::TwoD)
  if(WIN32)
    target_link_libraries(NexoraZigVoiceDialogueTests PRIVATE ntdll)
  endif()
  nexora_apply_defaults(NexoraZigVoiceDialogueTests)
  add_test(NAME gameplay.zig_voice_dialogue COMMAND NexoraZigVoiceDialogueTests)
  set_tests_properties(gameplay.zig_voice_dialogue PROPERTIES TIMEOUT 90 LABELS "zig;audio")
  if(NOT APPLE)
    add_test(NAME gameplay.zig_voice_player COMMAND "${Python3_EXECUTABLE}"
      "${PROJECT_SOURCE_DIR}/Tests/TwoD/VoiceDialogueAcceptance.py"
      --app "$<TARGET_FILE:NexoraTwoDPlayer>" --module "${voice_library}")
    set_tests_properties(gameplay.zig_voice_player PROPERTIES TIMEOUT 240 LABELS "twod;zig;audio;project")
  endif()
endif()
