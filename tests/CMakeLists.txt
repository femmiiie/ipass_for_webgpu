cmake_minimum_required(VERSION 3.16)

get_filename_component(IPASS_SOURCE_DIR
  "${CMAKE_CURRENT_SOURCE_DIR}/../source"
  ABSOLUTE
)

if(NOT COMMAND ipass_copy_webgpu_runtime)
  list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake")
  include(CopyRuntime)
endif()

if(NOT COMMAND ipass_embed_shaders)
  list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake")
  include(EmbedShaders)
endif()

include(FetchContent)
FetchContent_Declare(Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG v3.7.1
)
FetchContent_MakeAvailable(Catch2)
list(APPEND CMAKE_MODULE_PATH "${catch2_SOURCE_DIR}/extras")

ipass_embed_shaders(
  OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/embedded_shaders.h"
  SOURCES
  "${IPASS_SOURCE_DIR}/Pass/ipass.wgsl"
  "${IPASS_SOURCE_DIR}/Tessellator/tess-calc.wgsl"
  "${IPASS_SOURCE_DIR}/Tessellator/tess-scan.wgsl"
  "${IPASS_SOURCE_DIR}/Tessellator/tess-gen.wgsl"
  NAMES ipass tess_calc tess_scan tess_gen
)

add_executable(ipass_tests
  tests.cpp
  ${IPASS_SOURCE_DIR}/Tessellator/TessCalcPass.cpp
  ${IPASS_SOURCE_DIR}/Tessellator/TessScanPass.cpp
  ${IPASS_SOURCE_DIR}/Tessellator/TessGenPass.cpp
  ${IPASS_SOURCE_DIR}/Tessellator/Tessellator.cpp
  ${IPASS_SOURCE_DIR}/Tessellator/Connectivity.cpp
)

set_target_properties(ipass_tests PROPERTIES
  CXX_STANDARD 23
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
)

if(MSVC)
  target_compile_options(ipass_tests PRIVATE /W4)
else()
  target_compile_options(ipass_tests PRIVATE -Wall -Wextra -pedantic)
endif()

target_link_libraries(ipass_tests PRIVATE webgpu glm::glm Catch2::Catch2WithMain)
target_include_directories(ipass_tests PRIVATE
  ${webgpu_SOURCE_DIR}/include
  ${IPASS_SOURCE_DIR}/Tessellator
  ${IPASS_SOURCE_DIR}/Utils
  ${CMAKE_CURRENT_BINARY_DIR}
)
target_compile_definitions(ipass_tests PRIVATE WEBGPU_BACKEND_${BACKEND})

ipass_copy_webgpu_runtime(ipass_tests)

include(Catch)
catch_discover_tests(ipass_tests)
