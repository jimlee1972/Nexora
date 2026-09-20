include_guard(GLOBAL)

function(nexora_configure_mimalloc)
  if(NOT NEXORA_ENABLE_MIMALLOC)
    return()
  endif()

  include(FetchContent)

  # Build only the static allocator library; we call mi_malloc_aligned/mi_free
  # directly from TrackingAllocator rather than overriding global new/delete.
  set(MI_BUILD_SHARED OFF CACHE BOOL "" FORCE)
  set(MI_BUILD_OBJECT OFF CACHE BOOL "" FORCE)
  set(MI_BUILD_TESTS OFF CACHE BOOL "" FORCE)
  set(MI_OVERRIDE OFF CACHE BOOL "" FORCE)

  FetchContent_Declare(
    nexora_mimalloc
    GIT_REPOSITORY https://github.com/microsoft/mimalloc.git
    GIT_TAG v2.1.9
    GIT_SHALLOW TRUE)
  FetchContent_MakeAvailable(nexora_mimalloc)

  set(NEXORA_MIMALLOC_TARGET mimalloc-static PARENT_SCOPE)
endfunction()
