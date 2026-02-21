add_executable(test_scan test_scan.cpp)

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

target_link_libraries(test_scan PRIVATE webgpu)
target_include_directories(test_scan PRIVATE
  ${webgpu_SOURCE_DIR}/include
  ${webgpu_hpp_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/../source
)
target_compile_definitions(test_scan PRIVATE WEBGPU_BACKEND_${BACKEND})

if(BACKEND STREQUAL "WGPU")
  add_custom_command(TARGET test_scan POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_BINARY_DIR}/_deps/wgpu-windows-x86_64-gnu-release-src/lib/wgpu_native.dll"
    "$<TARGET_FILE_DIR:test_scan>"
    COMMENT "Copying wgpu_native.dll for test_scan"
  )
endif()

add_test(NAME scan_pass COMMAND test_scan)
