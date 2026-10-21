#pragma once

#include <cstdint>
#include <vector>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#include "GPU.h"
#include "TessCalcPass.h"
#include "TessConstants.h"

class Tessellator {
    TessCalcPass calc_pass;

    wgpu::Buffer buf_quads;
    wgpu::Buffer buf_tess_factors;
    wgpu::Buffer buf_connectivity;

    uint32_t max_quads = 0;
    uint32_t num_quads = 0;

    std::vector<wgpu::Buffer*> InputBuffers();

protected:
    wgpu::Device device;
    wgpu::Queue queue;

    bool initialized = false;

    Tessellator(wgpu::Device device, wgpu::Queue queue, uint32_t max_quads)
        : max_quads(max_quads), device(device), queue(queue) {}
    ~Tessellator() = default;

    bool InitInput(wgpu::Buffer ipass_levels);
    void UploadInput(const glm::vec4* control_points, const uint32_t* indices, uint32_t count);
    bool ExecuteInput(wgpu::CommandEncoder& encoder);
    void TerminateInput();

    wgpu::Buffer GetTessFactors() const { return buf_tess_factors; }
    wgpu::Buffer GetConnectivity() const { return buf_connectivity; }

public:
    Tessellator(const Tessellator&) = delete;
    Tessellator& operator=(const Tessellator&) = delete;

    wgpu::Buffer GetControlPointBuffer() const { return buf_quads; }
    uint32_t GetNumQuads() const { return num_quads; }
    uint32_t GetMaxQuads() const { return max_quads; }
    bool IsInitialized() const { return initialized; }
};
