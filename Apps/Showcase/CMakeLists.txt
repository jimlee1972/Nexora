include(NexoraTarget)

if(NOT NEXORA_ZIG_SAMPLE_OBJECT)
  message(FATAL_ERROR "NEXORA_ZIG_SAMPLE_OBJECT must be provided by Gameplay/Zig")
endif()

add_executable(NexoraShowcase
  main.cpp
  "${NEXORA_ZIG_SAMPLE_OBJECT}")
add_dependencies(NexoraShowcase NexoraZigGameplayBuild)
add_dependencies(NexoraShowcase NexoraZigGameplaySharedBuild)
target_link_libraries(NexoraShowcase PRIVATE Nexora::Runtime Nexora::Presentation)
if(WIN32)
  # The Zig object may reference the NT syscall import directly. CMake's
  # link.exe invocation does not inherit the Zig driver's implicit ntdll link.
  target_link_libraries(NexoraShowcase PRIVATE ntdll)
endif()
nexora_apply_defaults(NexoraShowcase)
target_compile_definitions(NexoraShowcase PRIVATE
  NEXORA_ZIG_SHARED_LIBRARY_PATH="${NEXORA_ZIG_SHARED_LIBRARY}")
add_custom_command(TARGET NexoraShowcase POST_BUILD
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different
          "${NEXORA_ZIG_SHARED_LIBRARY}" "$<TARGET_FILE_DIR:NexoraShowcase>"
  VERBATIM)

if(BUILD_TESTING)
  add_test(
    NAME showcase.zig_headless
    COMMAND NexoraShowcase --headless --validate-v1 --frames=4)
  add_test(
    NAME showcase.feature_gallery
    COMMAND NexoraShowcase --headless --scene=tour --frames=1 --no-reload)
  add_test(
    NAME showcase.zig_dynamic
    COMMAND NexoraShowcase --headless --frames=4 --gameplay-module=dynamic)
  set_tests_properties(showcase.feature_gallery PROPERTIES
    PASS_REGULAR_EXPRESSION
      "\"id\": \"math\".*\"id\": \"scene\".*\"id\": \"gameplay\".*\"id\": \"presentation\".*\"id\": \"streaming\"")
endif()
