add_executable(NexoraEditor main.cpp)
target_link_libraries(NexoraEditor PRIVATE Nexora::Editor)
nexora_apply_defaults(NexoraEditor)
