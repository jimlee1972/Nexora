include(NexoraTarget)
add_test(NAME build.pose_search_profiles
  COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/Tools/Build/VerifyPoseSearchProfiles.py"
    --root "${PROJECT_SOURCE_DIR}" --cmake "${CMAKE_COMMAND}")
if(NEXORA_ENABLE_POSE_SEARCH)
  add_executable(NexoraPoseSearchTests PoseSearchTests.cpp)
  target_link_libraries(NexoraPoseSearchTests PRIVATE Nexora::PoseSearch)
  nexora_apply_defaults(NexoraPoseSearchTests)
  add_test(NAME animation.v2_m8_pose_search COMMAND NexoraPoseSearchTests)
else()
  if(TARGET NexoraPoseSearch)
    message(FATAL_ERROR "Disabled PoseSearch still has a target")
  endif()
  add_test(NAME animation.v2_m8_pose_search_feature_strip
    COMMAND "${CMAKE_COMMAND}" -E echo "V2-M8 PoseSearch implementation stripped")
endif()
