include(NexoraTarget)
add_executable(NexoraHost main.cpp)
target_link_libraries(NexoraHost PRIVATE Nexora::Renderer)
nexora_apply_defaults(NexoraHost)
