include(NexoraTarget)
add_test(NAME build.animation_profiles
  COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/Tools/Build/VerifyAnimationProfiles.py"
    --root "${PROJECT_SOURCE_DIR}" --cmake "${CMAKE_COMMAND}")
if(NEXORA_ENABLE_ANIMATION_V2)
  add_executable(NexoraAnimationTests PoseTests.cpp)
  target_link_libraries(NexoraAnimationTests PRIVATE Nexora::Animation)
  nexora_apply_defaults(NexoraAnimationTests)
  add_test(NAME animation.v2_m8_pose_storage_retarget COMMAND NexoraAnimationTests)
endif()
