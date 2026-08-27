cmake_minimum_required(VERSION 3.16)

get_filename_component(IPASS_SOURCE_DIR
  "${CMAKE_CURRENT_SOURCE_DIR}/../source"
  ABSOLUTE
)

if(NOT COMMAND ipass_embed_shaders)
  list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake")
  include(EmbedShaders)
  include(CopyRuntime)
endif()

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
    source/Pass/ComputePass/ComputePass.cpp
    source/Pass/ComputePass/IPass.cpp

    # not yet moved into the library
    ${IPASS_SOURCE_DIR}/Tessellator/Tessellator.cpp
    ${IPASS_SOURCE_DIR}/Tessellator/TessCalcPass.cpp
    ${IPASS_SOURCE_DIR}/Tessellator/TessScanPass.cpp
    ${IPASS_SOURCE_DIR}/Tessellator/TessGenPass.cpp
    ${IPASS_SOURCE_DIR}/Tessellator/Connectivity.cpp
    ${IPASS_SOURCE_DIR}/Utils/Elevation.cpp
)

set(IPASS_PRIVATE_INCLUDES
    ${IPASS_SOURCE_DIR}/Tessellator
    ${CMAKE_CURRENT_SOURCE_DIR}/source/Pass/ComputePass
    ${CMAKE_CURRENT_SOURCE_DIR}/source/Utils
)

if(EMSCRIPTEN)
    set(_backend_define WEBGPU_BACKEND_EMDAWNWEBGPU)

    #generate embedded_shaders.h from .wgsl files
    ipass_embed_shaders(
        OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/embedded_shaders.h"
        SOURCES
            "${CMAKE_CURRENT_SOURCE_DIR}/source/Pass/ComputePass/ipass.wgsl"
            "${IPASS_SOURCE_DIR}/Tessellator/tess-calc.wgsl"
            "${IPASS_SOURCE_DIR}/Tessellator/tess-scan.wgsl"
            "${IPASS_SOURCE_DIR}/Tessellator/tess-gen.wgsl"
        NAMES ipass tess_calc tess_scan tess_gen
    )
else()
    set(_backend_define WEBGPU_BACKEND_${BACKEND})
endif()

#static library target, always built and reused by both the demo app and
#(on Emscripten) the JS bindings module below
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

if(EMSCRIPTEN)
    target_include_directories(ipass_lib PRIVATE
        ${CMAKE_CURRENT_BINARY_DIR} # for embedded_shaders.h
    )
endif()

target_link_libraries(ipass_lib PUBLIC webgpu glm::glm)

target_compile_definitions(ipass_lib PRIVATE ${_backend_define})

if(EMSCRIPTEN)
    set(IPASS_POST_JS "${CMAKE_CURRENT_SOURCE_DIR}/js/ipass.js")

    #WASM JS bindings module, links against ipass_lib instead of recompiling its sources
    add_executable(ipass_wasm
        source/ipass_bindings.cpp
    )

    set_target_properties(ipass_wasm PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
        OUTPUT_NAME "ipass"
        SUFFIX ".js"
    )

    target_link_libraries(ipass_wasm PUBLIC ipass_lib)

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
endif()
