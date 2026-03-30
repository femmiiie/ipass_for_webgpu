cmake_minimum_required(VERSION 3.10)

add_library(ipass_lib STATIC

    # library wrapper sources
    src/LODPass.cpp
    src/TessellationPass.cpp
    src/Pipeline.cpp
    src/BVLoader.cpp

    # reused internal sources
    ${CMAKE_CURRENT_SOURCE_DIR}/../source/Pass/Pass.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../source/Pass/ComputePass/IPass.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../source/Tessellator/Tessellator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../source/Tessellator/TessCalcPass.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../source/Tessellator/TessScanPass.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../source/Tessellator/TessGenPass.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../source/Tessellator/Connectivity.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../source/Utils/BVParser.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../source/Utils/Elevation.cpp
)

set_target_properties(ipass_lib PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

target_include_directories(ipass_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# private so end user cannot directly access them
target_include_directories(ipass_lib PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../source/Renderer
    ${CMAKE_CURRENT_SOURCE_DIR}/../source/Pass
    ${CMAKE_CURRENT_SOURCE_DIR}/../source/Pass/ComputePass
    ${CMAKE_CURRENT_SOURCE_DIR}/../source/Tessellator
    ${CMAKE_CURRENT_SOURCE_DIR}/../source/Utils
)

target_link_libraries(ipass_lib PUBLIC webgpu glm::glm)

target_compile_definitions(ipass_lib PRIVATE ${_backend_define})
