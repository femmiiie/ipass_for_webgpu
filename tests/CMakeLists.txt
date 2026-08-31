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

ipass_embed_shaders(
  OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/embedded_shaders.h"
  SOURCES
  "${IPASS_SOURCE_DIR}/Pass/ipass.wgsl"
  "${IPASS_SOURCE_DIR}/Tessellator/tess-calc.wgsl"
  "${IPASS_SOURCE_DIR}/Tessellator/tess-scan.wgsl"
  "${IPASS_SOURCE_DIR}/Tessellator/tess-gen.wgsl"
  NAMES ipass tess_calc tess_scan tess_gen
)

# test_scan
add_executable(test_scan
  test_scan.cpp
  ${IPASS_SOURCE_DIR}/Tessellator/TessScanPass.cpp
)

set_target_properties(test_scan PROPERTIES
  CXX_STANDARD 23
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
)

if(MSVC)
  target_compile_options(test_scan PRIVATE /W4)
else()
  target_compile_options(test_scan PRIVATE -Wall -Wextra -pedantic)
endif()

target_link_libraries(test_scan PRIVATE webgpu glm::glm)
target_include_directories(test_scan PRIVATE
  ${webgpu_SOURCE_DIR}/include
  ${IPASS_SOURCE_DIR}/Tessellator
  ${IPASS_SOURCE_DIR}/Utils
  ${CMAKE_CURRENT_BINARY_DIR}
)
target_compile_definitions(test_scan PRIVATE WEBGPU_BACKEND_${BACKEND})

ipass_copy_webgpu_runtime(test_scan)

add_test(NAME scan_pass COMMAND test_scan)

# test_tess_quad
add_executable(test_tess_quad
  test_tess_quad.cpp
  ${IPASS_SOURCE_DIR}/Tessellator/TessCalcPass.cpp
  ${IPASS_SOURCE_DIR}/Tessellator/TessScanPass.cpp
  ${IPASS_SOURCE_DIR}/Tessellator/TessGenPass.cpp
  ${IPASS_SOURCE_DIR}/Tessellator/Tessellator.cpp
  ${IPASS_SOURCE_DIR}/Tessellator/Connectivity.cpp
)

set_target_properties(test_tess_quad PROPERTIES
  CXX_STANDARD 23
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
)

if(MSVC)
  target_compile_options(test_tess_quad PRIVATE /W4 /Zi /Od)
  target_link_options(test_tess_quad PRIVATE /DEBUG)
else()
  target_compile_options(test_tess_quad PRIVATE -Wall -Wextra -pedantic -g -O0)
endif()

target_link_libraries(test_tess_quad PRIVATE webgpu glm::glm)
target_include_directories(test_tess_quad PRIVATE
  ${webgpu_SOURCE_DIR}/include
  ${IPASS_SOURCE_DIR}/Tessellator
  ${IPASS_SOURCE_DIR}/Utils
  ${CMAKE_CURRENT_BINARY_DIR}
)
target_compile_definitions(test_tess_quad PRIVATE WEBGPU_BACKEND_${BACKEND})

ipass_copy_webgpu_runtime(test_tess_quad)
