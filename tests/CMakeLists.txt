include(FetchContent)
FetchContent_Declare(Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG v3.7.1
)
FetchContent_MakeAvailable(Catch2)
list(APPEND CMAKE_MODULE_PATH "${catch2_SOURCE_DIR}/extras")

add_executable(ipass_tests tests.cpp)

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

target_link_libraries(ipass_tests PRIVATE ipass_lib Catch2::Catch2WithMain)
target_include_directories(ipass_tests PRIVATE $<TARGET_PROPERTY:ipass_lib,INCLUDE_DIRECTORIES>)
target_compile_definitions(ipass_tests PRIVATE ${_backend_define})

ipass_copy_webgpu_runtime(ipass_tests)

include(Catch)
catch_discover_tests(ipass_tests)
