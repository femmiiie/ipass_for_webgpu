cmake_minimum_required(VERSION 3.16)
project(ipass_webgpu)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(BACKEND "WGPU" CACHE STRING "Backend implementation of WebGPU")
set_property(CACHE BACKEND PROPERTY STRINGS WGPU DAWN)

include(FetchContent)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(CopyRuntime)
include(EmbedShaders)

FetchContent_Declare(webgpu
  GIT_REPOSITORY https://github.com/eliemichel/WebGPU-distribution
  GIT_TAG 17dcd42a7683355e7a40ac4e97e77f36dff5b5ab
)

FetchContent_Declare(nuklear
  GIT_REPOSITORY https://github.com/Immediate-Mode-UI/Nuklear
  GIT_TAG 8c9b376
)

FetchContent_Declare(glfw
  GIT_REPOSITORY https://github.com/glfw/glfw.git
  GIT_TAG 3.4
)

FetchContent_Declare(glfw3webgpu
  GIT_REPOSITORY https://github.com/eliemichel/glfw3webgpu
  GIT_TAG 8f14534
)

FetchContent_Declare(glm
  GIT_REPOSITORY https://github.com/g-truc/glm.git
  GIT_TAG 0af55ccecd98d4e5a8d1fad7de25ba429d60e863
)

FetchContent_Declare(nfd
  GIT_REPOSITORY https://github.com/btzy/nativefiledialog-extended
  GIT_TAG v1.2.1
)

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/source/shaders/*.wgsl"
  "${CMAKE_CURRENT_SOURCE_DIR}/source/*.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/source/*.h"
)

# Select correct main file for target platform
if(EMSCRIPTEN)
  list(FILTER SOURCES EXCLUDE REGEX ".*/main_native\\.cpp$")
else()
  list(FILTER SOURCES EXCLUDE REGEX ".*/main_wasm\\.cpp$")
endif()

# Embed shaders to embedded_shaders.h (faster than virtual FS on Emscripten)
if(EMSCRIPTEN)
  ipass_embed_shaders(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/embedded_shaders.h"
    SOURCES
      "${CMAKE_CURRENT_SOURCE_DIR}/source/Pass/ComputePass/ipass.wgsl"
      "${CMAKE_CURRENT_SOURCE_DIR}/source/Pass/RenderPass/shaders/blinn_phong.wgsl"
      "${CMAKE_CURRENT_SOURCE_DIR}/source/Pass/RenderPass/shaders/flat.wgsl"
      "${CMAKE_CURRENT_SOURCE_DIR}/source/Pass/RenderPass/shaders/parametric_error.wgsl"
      "${CMAKE_CURRENT_SOURCE_DIR}/source/Pass/RenderPass/shaders/triangle_size.wgsl"
      "${CMAKE_CURRENT_SOURCE_DIR}/source/Pass/RenderPass/ui.wgsl"
      "${CMAKE_CURRENT_SOURCE_DIR}/source/Tessellator/tess-calc.wgsl"
      "${CMAKE_CURRENT_SOURCE_DIR}/source/Tessellator/tess-scan.wgsl"
      "${CMAKE_CURRENT_SOURCE_DIR}/source/Tessellator/tess-gen.wgsl"
    NAMES
      ipass blinn_phong flat parametric_error triangle_size
      ui tess_calc tess_scan tess_gen
  )
endif()

add_executable(ipass_webgpu ${SOURCES})

set_target_properties(ipass_webgpu PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}
  RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}
)

set_target_properties(ipass_webgpu PROPERTIES
  CXX_STANDARD 23
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
  COMPILE_WARNING_AS_ERROR OFF
)

if(MSVC)
  target_compile_options(ipass_webgpu PRIVATE /W4 /external:anglebrackets /external:W0)
else()
  target_compile_options(ipass_webgpu PRIVATE -Wall -Wextra -pedantic)
endif()

if(EMSCRIPTEN)
  target_compile_options(ipass_webgpu PRIVATE -Wno-dollar-in-identifier-extension)
endif()

set(_backend_define "")

# ------------------------------------------
# WebGPU Backend Config
# ------------------------------------------
if(EMSCRIPTEN)
  set(GLFW3WEBGPU_BACKEND "EMSCRIPTEN" CACHE STRING "" FORCE)

  if(NOT TARGET glfw)
    add_library(glfw INTERFACE)

    if(CMAKE_SYSROOT)
      target_include_directories(glfw INTERFACE "${CMAKE_SYSROOT}/include")
    endif()

    target_link_options(glfw INTERFACE "-sUSE_GLFW=3")
  endif()

  FetchContent_MakeAvailable(webgpu nuklear glfw3webgpu glm)
  set(_backend_define WEBGPU_BACKEND_EMDAWNWEBGPU)

else() # WGPU or DAWN backend
  if(MINGW)
    set(WEBGPU_BACKEND "WGPU" CACHE STRING "" FORCE)
    set(WGPU_NATIVE_RUNTIME "gnu" CACHE STRING "" FORCE)
  elseif(WIN32)
    set(WGPU_NATIVE_RUNTIME "msvc" CACHE STRING "" FORCE)
  endif()

  set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
  set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
  set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(GLFW3WEBGPU_BACKEND ${BACKEND} CACHE STRING "" FORCE)

  FetchContent_MakeAvailable(webgpu nuklear glfw glfw3webgpu glm nfd)

  if(TARGET glfw3webgpu)
    if(GLFW_BUILD_X11)
      target_compile_definitions(glfw3webgpu PRIVATE _GLFW_X11)
    endif()
    if(GLFW_BUILD_WAYLAND)
      target_compile_definitions(glfw3webgpu PRIVATE _GLFW_WAYLAND)
    endif()
    if(WIN32)
      target_compile_definitions(glfw3webgpu PRIVATE _GLFW_WIN32 GLFW_EXPOSE_NATIVE_WIN32)
    endif()
    if(APPLE)
      target_compile_definitions(glfw3webgpu PRIVATE _GLFW_COCOA)
    endif()
  endif()

  set(_backend_define WEBGPU_BACKEND_${BACKEND})
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

set_source_files_properties(
  source/nuklear_impl.cpp
  PROPERTIES COMPILE_OPTIONS "$<IF:$<CXX_COMPILER_ID:MSVC>,/w,-w>"
)

if(EMSCRIPTEN)
  target_compile_definitions(ipass_webgpu PRIVATE
    NK_NO_STANDARD_FILE
    NK_NO_POSIX
  )
endif()

# Copy shader files to output directory
file(GLOB_RECURSE SHADER_FILES "${CMAKE_CURRENT_SOURCE_DIR}/source/*.wgsl")

set(SHADER_STAMP_DIR "${CMAKE_BINARY_DIR}/shader_stamps")
set(SHADER_STAMP_FILES "")

foreach(SHADER_FILE ${SHADER_FILES})
  file(RELATIVE_PATH SHADER_RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}/source" "${SHADER_FILE}")
  string(REPLACE "/" "_" SHADER_STAMP_NAME "${SHADER_RELATIVE}")
  set(STAMP_FILE "${SHADER_STAMP_DIR}/${SHADER_STAMP_NAME}.stamp")

  get_filename_component(SHADER_OUT_DIR "${CMAKE_BINARY_DIR}/${SHADER_RELATIVE}" DIRECTORY)

  add_custom_command(
    OUTPUT "${STAMP_FILE}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${SHADER_OUT_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy "${SHADER_FILE}" "${CMAKE_BINARY_DIR}/${SHADER_RELATIVE}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${SHADER_STAMP_DIR}"
    COMMAND ${CMAKE_COMMAND} -E touch "${STAMP_FILE}"
    DEPENDS "${SHADER_FILE}"
    COMMENT "Copying shader: ${SHADER_RELATIVE}"
  )
  list(APPEND SHADER_STAMP_FILES "${STAMP_FILE}")
endforeach()

add_custom_target(copy_shaders ALL DEPENDS ${SHADER_STAMP_FILES})
add_dependencies(ipass_webgpu copy_shaders)

# Copy fonts directory to build directory
add_custom_target(copy_fonts ALL
  COMMAND ${CMAKE_COMMAND} -E copy_directory
  "${CMAKE_CURRENT_SOURCE_DIR}/fonts"
  "${CMAKE_BINARY_DIR}/fonts"
  COMMENT "Copying fonts directory"
)
add_dependencies(ipass_webgpu copy_fonts)

target_link_libraries(ipass_webgpu PRIVATE webgpu glfw glfw3webgpu nuklear glm::glm)

if(NOT EMSCRIPTEN)
  target_link_libraries(ipass_webgpu PRIVATE nfd)
endif()

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
  ${CMAKE_BINARY_DIR}
)
target_compile_definitions(ipass_webgpu PRIVATE ${_backend_define})

ipass_copy_webgpu_runtime(ipass_webgpu)

set_target_properties(ipass_webgpu PROPERTIES
  VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:ipass_webgpu>"
)

if(EMSCRIPTEN)
  set(EMSCRIPTEN_SHELL_FILE "${CMAKE_CURRENT_SOURCE_DIR}/wasm/shell.html")
  set(PRELOAD_OPTIONS "")

  foreach(SHADER_FILE ${SHADER_FILES})
    get_filename_component(SHADER_NAME ${SHADER_FILE} NAME)
    list(APPEND PRELOAD_OPTIONS "--preload-file=${SHADER_FILE}@${SHADER_NAME}")
  endforeach()

  list(APPEND PRELOAD_OPTIONS "--preload-file=${CMAKE_CURRENT_SOURCE_DIR}/fonts@/fonts")

  target_link_options(ipass_webgpu PRIVATE
    "-sUSE_GLFW=3"
    "-sASYNCIFY"
    "-sFORCE_FILESYSTEM=1"
    "-sEXPORTED_FUNCTIONS=['_main','_malloc','_free']"
    "-sEXPORTED_RUNTIME_METHODS=dynCall"
    "--shell-file=${EMSCRIPTEN_SHELL_FILE}"
    ${PRELOAD_OPTIONS}
  )
  set_target_properties(ipass_webgpu PROPERTIES SUFFIX ".html")
endif()

option(BUILD_LIBRARY "Build the ipass library" ON)

if(BUILD_LIBRARY)
  add_subdirectory(library)
endif()

if(NOT EMSCRIPTEN)
  enable_testing()
  add_subdirectory(tests)
endif()