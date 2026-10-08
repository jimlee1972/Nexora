include(NexoraTarget)
add_test(NAME build.animation_profiles
  COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/Tools/Build/VerifyAnimationProfiles.py"
    --root "${PROJECT_SOURCE_DIR}" --cmake "${CMAKE_COMMAND}")
if(NEXORA_ENABLE_ANIMATION_V2)
  add_executable(NexoraPoseGraphTests PoseGraphTests.cpp)
  target_link_libraries(NexoraPoseGraphTests PRIVATE Nexora::Animation)
  nexora_apply_defaults(NexoraPoseGraphTests)
  add_test(NAME animation.v2_m8_synchronized_trs_graph COMMAND NexoraPoseGraphTests)

  add_executable(NexoraSyncGroupTests SyncGroupTests.cpp)
  target_link_libraries(NexoraSyncGroupTests PRIVATE Nexora::Animation)
  nexora_apply_defaults(NexoraSyncGroupTests)
  add_test(NAME animation.v2_m8_marker_sync COMMAND NexoraSyncGroupTests)

  add_executable(NexoraAnimationTests PoseTests.cpp)
  target_link_libraries(NexoraAnimationTests PRIVATE Nexora::Animation)
  nexora_apply_defaults(NexoraAnimationTests)
  add_test(NAME animation.v2_m8_pose_storage_retarget COMMAND NexoraAnimationTests)
endif()
