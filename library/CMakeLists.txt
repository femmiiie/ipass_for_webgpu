cmake_minimum_required(VERSION 3.5)


if(EMSCRIPTEN)
  include(FetchContent)
  FetchContent_Declare(webgpu
    GIT_REPOSITORY https://github.com/eliemichel/WebGPU-distribution
    GIT_TAG 17dcd42a7683355e7a40ac4e97e77f36dff5b5ab
  )
  FetchContent_Declare(glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 0af55ccecd98d4e5a8d1fad7de25ba429d60e863
  )
  FetchContent_MakeAvailable(webgpu glm)
endif()

set(IPASS_SOURCES
    source/LODPass.cpp
    source/TessellationPass.cpp
    source/Pipeline.cpp
    source/BVLoader.cpp

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

set(IPASS_PRIVATE_INCLUDES
    ${CMAKE_CURRENT_SOURCE_DIR}/../source/Renderer
    ${CMAKE_CURRENT_SOURCE_DIR}/../source/Pass
    ${CMAKE_CURRENT_SOURCE_DIR}/../source/Pass/ComputePass
    ${CMAKE_CURRENT_SOURCE_DIR}/../source/Tessellator
    ${CMAKE_CURRENT_SOURCE_DIR}/../source/Utils
)

if(EMSCRIPTEN)
    set(_backend_define WEBGPU_BACKEND_EMDAWNWEBGPU)
    set(IPASS_POST_JS "${CMAKE_CURRENT_SOURCE_DIR}/js/ipass.js")

    #generate embedded_shaders.h from .wgsl files
    set(SHADER_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/../source/Pass/ComputePass/ipass.wgsl"
        "${CMAKE_CURRENT_SOURCE_DIR}/../source/Tessellator/tess-calc.wgsl"
        "${CMAKE_CURRENT_SOURCE_DIR}/../source/Tessellator/tess-scan.wgsl"
        "${CMAKE_CURRENT_SOURCE_DIR}/../source/Tessellator/tess-gen.wgsl"
    )
    set(SHADER_VAR_NAMES "ipass" "tess_calc" "tess_scan" "tess_gen")

    set(EMBEDDED_SHADER_HEADER "${CMAKE_CURRENT_BINARY_DIR}/embedded_shaders.h")
    file(WRITE "${EMBEDDED_SHADER_HEADER}"
        "#pragma once\n#include <string>\nnamespace embedded_shaders {\n"
    )

    list(LENGTH SHADER_SOURCES SHADER_COUNT)
    math(EXPR SHADER_LAST "${SHADER_COUNT} - 1")
    foreach(IDX RANGE ${SHADER_LAST})
        list(GET SHADER_SOURCES ${IDX} SHADER_FILE)
        list(GET SHADER_VAR_NAMES ${IDX} VAR_NAME)
        file(READ "${SHADER_FILE}" SHADER_CONTENT)
        file(APPEND "${EMBEDDED_SHADER_HEADER}"
            "inline const std::string ${VAR_NAME} = R\"WGSL(\n${SHADER_CONTENT})WGSL\";\n"
        )
    endforeach()

    file(APPEND "${EMBEDDED_SHADER_HEADER}" "} // embedded_shaders\n")

    #WASM library target
    add_executable(ipass_wasm
        ${IPASS_SOURCES}
        source/ipass_bindings.cpp
    )

    set_target_properties(ipass_wasm PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
        SUFFIX ".js"
    )

    target_include_directories(ipass_wasm PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

    target_include_directories(ipass_wasm PRIVATE
        ${IPASS_PRIVATE_INCLUDES}
        ${CMAKE_CURRENT_BINARY_DIR}  # for embedded_shaders.h
        ${webgpu_SOURCE_DIR}/include
        ${webgpu_headers_SOURCE_DIR}
    )

    target_link_libraries(ipass_wasm PUBLIC webgpu glm::glm)
    target_compile_definitions(ipass_wasm PRIVATE ${_backend_define} IPASS_LIBRARY_BUILD)

    target_link_options(ipass_wasm PRIVATE
        "--bind"
        "-sMODULARIZE=1"
        "-sEXPORT_NAME=createIPASS"
        "-sALLOW_MEMORY_GROWTH=1"
        "-sFORCE_FILESYSTEM=1"
        "-sEXPORTED_RUNTIME_METHODS=['FS','WebGPU']"
        "--post-js=${IPASS_POST_JS}"
    )

    set_property(TARGET ipass_wasm APPEND PROPERTY LINK_DEPENDS "${IPASS_POST_JS}")

else()
    #native static library target
    add_library(ipass_lib STATIC ${IPASS_SOURCES})

    set_target_properties(ipass_lib PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
    )

    target_include_directories(ipass_lib PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

    #private so end user cannot directly access them
    target_include_directories(ipass_lib PRIVATE
        ${IPASS_PRIVATE_INCLUDES}
        ${webgpu_SOURCE_DIR}/include
        ${webgpu_headers_SOURCE_DIR}
    )

    target_link_libraries(ipass_lib PUBLIC webgpu glm::glm)

    target_compile_definitions(ipass_lib PRIVATE ${_backend_define} IPASS_LIBRARY_BUILD)
endif()
