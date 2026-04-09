cmake_minimum_required(VERSION 3.16)

get_filename_component(IPASS_SOURCE_DIR
  "${CMAKE_CURRENT_SOURCE_DIR}/../source"
  ABSOLUTE
)

if(NOT COMMAND ipass_copy_webgpu_runtime)
  list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake")
  include(CopyRuntime)
endif()

set(TESSELLATOR_SHADER_DIR "${IPASS_SOURCE_DIR}/Tessellator")

# copy tessellator shaders to target output directory
function(copy_tess_shaders TARGET_NAME)
  foreach(SHADER tess-calc.wgsl tess-scan.wgsl tess-gen.wgsl)
    add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${TARGET_NAME}>/Tessellator"
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${TESSELLATOR_SHADER_DIR}/${SHADER}"
        "$<TARGET_FILE_DIR:${TARGET_NAME}>/Tessellator/${SHADER}"
      COMMENT "Copying ${SHADER} for ${TARGET_NAME}"
    )
  endforeach()
endfunction()

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
  ${webgpu_hpp_SOURCE_DIR}
  ${IPASS_SOURCE_DIR}
  ${IPASS_SOURCE_DIR}/Tessellator
  ${IPASS_SOURCE_DIR}/Utils
  ${nfd_SOURCE_DIR}/src/include
)
target_compile_definitions(test_scan PRIVATE WEBGPU_BACKEND_${BACKEND})

copy_tess_shaders(test_scan)
ipass_copy_webgpu_runtime(test_scan)

add_test(NAME scan_pass COMMAND test_scan)

# test_tess_quad

add_executable(test_tess_quad
  test_tess_quad.cpp
  ${IPASS_SOURCE_DIR}/Pass/Pass.cpp
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
  ${webgpu_hpp_SOURCE_DIR}
  ${IPASS_SOURCE_DIR}
  ${IPASS_SOURCE_DIR}/Pass
  ${IPASS_SOURCE_DIR}/Renderer
  ${IPASS_SOURCE_DIR}/Tessellator
  ${IPASS_SOURCE_DIR}/Utils
  ${nfd_SOURCE_DIR}/src/include
)
target_compile_definitions(test_tess_quad PRIVATE WEBGPU_BACKEND_${BACKEND})

copy_tess_shaders(test_tess_quad)
ipass_copy_webgpu_runtime(test_tess_quad)
