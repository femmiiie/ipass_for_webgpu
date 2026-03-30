#pragma once

#include <cstdint>
#include <vector>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

namespace ipass {

enum class Status {
    Success,
    EmptyInput,
    GPUInitFailed,
    NotInitialized,
    PatchesNotLoaded,
    LoadFailed,
};


struct PatchData {
    std::vector<glm::vec4> control_points;   // num_patches * 16
    std::vector<uint32_t>  corner_indices;   // num_patches * 4
    uint32_t               num_patches = 0;
};


//Utility struct for end users to get output information
struct OutputVertexLayout {
    static constexpr uint32_t FLOATS_PER_VERTEX = 16;
    static constexpr uint32_t BYTES_PER_VERTEX  = 64;
    static constexpr uint32_t POSITION_OFFSET   = 0;
    static constexpr uint32_t NORMAL_OFFSET     = 16;
    static constexpr uint32_t COLOR_OFFSET      = 32;
    static constexpr uint32_t UV_OFFSET         = 48;
};


//Configuration for library initialization.
struct Config {
    uint32_t    max_patches      = 128;
    const char* shader_base_path = ".";
};

}
