include(NexoraTarget)

if(NOT NEXORA_ZIG_SAMPLE_OBJECT)
  message(FATAL_ERROR "NEXORA_ZIG_SAMPLE_OBJECT must be provided by Gameplay/Zig")
endif()

add_executable(NexoraShowcase
  main.cpp
  "${NEXORA_ZIG_SAMPLE_OBJECT}")
add_dependencies(NexoraShowcase NexoraZigGameplayBuild)
target_link_libraries(NexoraShowcase PRIVATE Nexora::Runtime Nexora::Presentation)
if(WIN32)
  # The Zig object may reference the NT syscall import directly. CMake's
  # link.exe invocation does not inherit the Zig driver's implicit ntdll link.
  target_link_libraries(NexoraShowcase PRIVATE ntdll)
endif()
nexora_apply_defaults(NexoraShowcase)

if(BUILD_TESTING)
  add_test(
    NAME showcase.zig_headless
    COMMAND NexoraShowcase --headless --validate-v1 --frames=4)
endif()
