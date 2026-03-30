set(TESSELLATOR_SHADER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../source/Tessellator")

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
  ${CMAKE_CURRENT_SOURCE_DIR}/../source/Tessellator/TessScanPass.cpp
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
  ${CMAKE_CURRENT_SOURCE_DIR}/../source
  ${CMAKE_CURRENT_SOURCE_DIR}/../source/Tessellator
  ${CMAKE_CURRENT_SOURCE_DIR}/../source/Utils
  ${nfd_SOURCE_DIR}/src/include
)
target_compile_definitions(test_scan PRIVATE WEBGPU_BACKEND_${BACKEND})

copy_tess_shaders(test_scan)

if(BACKEND STREQUAL "WGPU")
  add_custom_command(TARGET test_scan POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_BINARY_DIR}/_deps/wgpu-windows-x86_64-${WGPU_NATIVE_RUNTIME}-release-src/lib/wgpu_native.dll"
    "$<TARGET_FILE_DIR:test_scan>"
    COMMENT "Copying wgpu_native.dll for test_scan"
  )
endif()

add_test(NAME scan_pass COMMAND test_scan)

# test_tess_quad

add_executable(test_tess_quad
  test_tess_quad.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../source/Tessellator/TessCalcPass.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../source/Tessellator/TessScanPass.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../source/Tessellator/TessGenPass.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../source/Tessellator/Tessellator.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../source/Tessellator/Connectivity.cpp
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
  ${CMAKE_CURRENT_SOURCE_DIR}/../source
  ${CMAKE_CURRENT_SOURCE_DIR}/../source/Tessellator
  ${CMAKE_CURRENT_SOURCE_DIR}/../source/Utils
  ${nfd_SOURCE_DIR}/src/include
)
target_compile_definitions(test_tess_quad PRIVATE WEBGPU_BACKEND_${BACKEND})

copy_tess_shaders(test_tess_quad)

if(BACKEND STREQUAL "WGPU")
  add_custom_command(TARGET test_tess_quad POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_BINARY_DIR}/_deps/wgpu-windows-x86_64-${WGPU_NATIVE_RUNTIME}-release-src/lib/wgpu_native.dll"
    "$<TARGET_FILE_DIR:test_tess_quad>"
    COMMENT "Copying wgpu_native.dll for test_tess_quad"
  )
endif()
