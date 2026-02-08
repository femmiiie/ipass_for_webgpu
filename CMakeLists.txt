cmake_minimum_required(VERSION 4.0)
project(ipass_webgpu)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(BACKEND "WGPU" CACHE STRING "Backend implementation of WebGPU")
set_property(CACHE BACKEND PROPERTY STRINGS WGPU DAWN)

include(FetchContent)

FetchContent_Declare(webgpu
  GIT_REPOSITORY https://github.com/eliemichel/WebGPU-distribution
  GIT_TAG main
)

FetchContent_Declare(nuklear
  GIT_REPOSITORY https://github.com/Immediate-Mode-UI/Nuklear
  GIT_TAG
)

FetchContent_Declare(glfw3webgpu
  GIT_REPOSITORY https://github.com/eliemichel/glfw3webgpu
  GIT_TAG main
)
FetchContent_Declare(glm
  GIT_REPOSITORY https://github.com/g-truc/glm.git
  GIT_TAG 0af55ccecd98d4e5a8d1fad7de25ba429d60e863
)

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/source/shaders/*.wgsl"
  "${CMAKE_CURRENT_SOURCE_DIR}/source/*.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/source/*.h"
)

# Remove both main files from the sources list

# Add the appropriate main file based on target platform
if(EMSCRIPTEN)
  list(FILTER SOURCES EXCLUDE REGEX ".*/main_native\\.cpp$")
  list(FILTER SOURCES EXCLUDE REGEX ".*/RenderContext/GLFWRenderContext\\.cpp$")
  list(FILTER SOURCES EXCLUDE REGEX ".*/RenderContext/GLFWRenderContext\\.h$")
else()
  list(FILTER SOURCES EXCLUDE REGEX ".*/main_wasm\\.cpp$")
  list(FILTER SOURCES EXCLUDE REGEX ".*/RenderContext/WebRenderContext\\.cpp$")
  list(FILTER SOURCES EXCLUDE REGEX ".*/RenderContext/WebRenderContext\\.h$")
endif()

add_executable(ipass_webgpu ${SOURCES})

set_target_properties(ipass_webgpu PROPERTIES
  CXX_STANDARD 23
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
  COMPILE_WARNING_AS_ERROR ON
)

if(MSVC)
  target_compile_options(ipass_webgpu PRIVATE /W4 /external:anglebrackets /external:W0)
else()
  target_compile_options(ipass_webgpu PRIVATE -Wall -Wextra -pedantic)
endif()

set(_backend_define "")

# ------------------------------------------
# WebGPU Backend Config
# ------------------------------------------
if(EMSCRIPTEN) # Building for WASM is still kinda broken, need to fix
  set(WEBGPU_BACKEND "EMSCRIPTEN" CACHE STRING "" FORCE)
  set(GLFW3WEBGPU_BACKEND "EMSCRIPTEN" CACHE STRING "" FORCE)

  if(NOT TARGET glfw)
    add_library(glfw INTERFACE)

    if(CMAKE_SYSROOT)
      target_include_directories(glfw INTERFACE "${CMAKE_SYSROOT}/include")
    endif()

    target_link_options(glfw INTERFACE "-sUSE_GLFW=3")
  endif()

  FetchContent_MakeAvailable(webgpu nuklear)
  set(_backend_define WEBGPU_BACKEND_EMSCRIPTEN)

else() # WGPU or DAWN backend
  if(MINGW)
    set(WEBGPU_BACKEND "WGPU" CACHE STRING "" FORCE)
    set(WGPU_NATIVE_RUNTIME "gnu" CACHE STRING "" FORCE)
  elseif(WIN32)
    set(WGPU_NATIVE_RUNTIME "msvc" CACHE STRING "" FORCE)
  endif()

  FetchContent_Declare(glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.4
  )
  set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
  set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
  set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(GLFW3WEBGPU_BACKEND ${BACKEND} CACHE STRING "" FORCE)

  FetchContent_MakeAvailable(webgpu nuklear glfw glfw3webgpu glm)

  if(WIN32)
    target_compile_definitions(glfw3webgpu PRIVATE GLFW_EXPOSE_NATIVE_WIN32)
  endif()

  set(_backend_define WEBGPU_BACKEND_${BACKEND})

  if(BACKEND STREQUAL "WGPU")
    set(_wgpu_runtime_dir "${CMAKE_BINARY_DIR}/_deps/wgpu-windows-x86_64-${WGPU_NATIVE_RUNTIME}-release-src/lib")
    add_custom_command(TARGET ipass_webgpu POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${_wgpu_runtime_dir}/wgpu_native.dll"
      "$<TARGET_FILE_DIR:ipass_webgpu>"
      COMMENT "Copying wgpu_native.dll to output directory"
    )
  endif()
endif()

# ------------------------------------------
# Nuklear Config
# ------------------------------------------
add_library(nuklear INTERFACE)
target_include_directories(nuklear SYSTEM INTERFACE
  ${nuklear_SOURCE_DIR}
)
target_compile_definitions(nuklear INTERFACE
  NK_INCLUDE_FIXED_TYPES
  NK_INCLUDE_STANDARD_IO
  NK_INCLUDE_DEFAULT_ALLOCATOR
  NK_INCLUDE_VERTEX_BUFFER_OUTPUT
  NK_INCLUDE_FONT_BAKING
  NK_INCLUDE_DEFAULT_FONT
)

if(MSVC)
  set_source_files_properties(
    source/nuklear_impl.cpp
    PROPERTIES
    COMPILE_OPTIONS
    "$<$<CXX_COMPILER_ID:MSVC>:/w>$<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-w>"
  )
else()
  set_source_files_properties(
    source/nuklear_impl.cpp
    PROPERTIES
    COMPILE_OPTIONS "-w"
  )
endif()

if(EMSCRIPTEN)
  target_compile_definitions(ipass_webgpu PRIVATE
    NK_NO_STANDARD_FILE
    NK_NO_POSIX
  )
endif()

# Copy shader files to output directory
file(GLOB SHADER_FILES "${CMAKE_CURRENT_SOURCE_DIR}/source/*.wgsl")

foreach(SHADER_FILE ${SHADER_FILES})
  add_custom_command(TARGET ipass_webgpu POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${SHADER_FILE}"
    "$<TARGET_FILE_DIR:ipass_webgpu>"
    COMMENT "Copying shader file: ${SHADER_FILE}"
  )
endforeach()

target_link_libraries(ipass_webgpu PRIVATE webgpu glfw glfw3webgpu nuklear glm::glm)

file(GLOB_RECURSE _all_source_children LIST_DIRECTORIES true "${CMAKE_CURRENT_SOURCE_DIR}/source/*")
set(_source_include_dirs ${CMAKE_CURRENT_SOURCE_DIR}/source)
foreach(_child ${_all_source_children})
  if(IS_DIRECTORY ${_child})
    list(APPEND _source_include_dirs ${_child})
  endif()
endforeach()

target_include_directories(ipass_webgpu PRIVATE
  ${_source_include_dirs}
  ${webgpu_SOURCE_DIR}/include
  ${webgpu_headers_SOURCE_DIR}
)
target_compile_definitions(ipass_webgpu PRIVATE ${_backend_define})

set_target_properties(ipass_webgpu PROPERTIES
  VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:ipass_webgpu>"
)

if(EMSCRIPTEN)
  # Build preload options for all shader files
  set(PRELOAD_OPTIONS "")

  foreach(SHADER_FILE ${SHADER_FILES})
    get_filename_component(SHADER_NAME ${SHADER_FILE} NAME)
    list(APPEND PRELOAD_OPTIONS "--preload-file=${SHADER_FILE}@${SHADER_NAME}")
  endforeach()

  target_link_options(ipass_webgpu PRIVATE
    "-sUSE_WEBGPU=1"
    "-sUSE_GLFW=3"
    "-sASYNCIFY"
    ${PRELOAD_OPTIONS}
  )
  set_target_properties(ipass_webgpu PROPERTIES SUFFIX ".html")
endif()