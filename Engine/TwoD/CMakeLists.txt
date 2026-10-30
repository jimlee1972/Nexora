include(NexoraTarget)
add_library(NexoraTwoD ${NEXORA_MODULE_LIBRARY_TYPE}
  src/Assets.cpp src/SpriteAsset.cpp src/Scene.cpp src/Queries.cpp src/Lifecycle.cpp src/Tilemap.cpp src/GameplayBridge.cpp)
add_library(Nexora::TwoD ALIAS NexoraTwoD)
target_include_directories(NexoraTwoD PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>")
target_link_libraries(NexoraTwoD PUBLIC Nexora::Runtime Nexora::Presentation)
target_compile_definitions(NexoraTwoD PRIVATE NEXORA_2D_EXPORTS)
if(NEXORA_LINK_MODE STREQUAL "Monolithic")
  target_compile_definitions(NexoraTwoD PUBLIC NEXORA_2D_STATIC=1)
endif()
nexora_apply_defaults(NexoraTwoD)
target_sources(NexoraTwoD PRIVATE src/GameplayUi.cpp)
target_sources(NexoraTwoD PRIVATE src/GameplayContent.cpp)

target_sources(NexoraTwoD PRIVATE src/GameplayAssets.cpp)

target_sources(NexoraTwoD PRIVATE src/GameplayEffects.cpp)

target_sources(NexoraTwoD PRIVATE src/EffectsRendering.cpp)

target_sources(NexoraTwoD PRIVATE src/EmissionAssets.cpp)
