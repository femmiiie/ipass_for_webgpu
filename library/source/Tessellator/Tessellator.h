#pragma once

#include <cstdint>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#include "ComputePass.h"
#include "TessCalcPass.h"
#include "TessScanPass.h"
#include "TessGenPass.h"
#include "TessConstants.h"

class Tessellator : public ComputePass {
    TessCalcPass calc_pass;
    TessScanPass scan_pass;
    TessGenPass gen_pass;

    wgpu::Buffer buf_quads;
    wgpu::Buffer buf_tess_factors;
    wgpu::Buffer buf_tri_counts;
    wgpu::Buffer buf_tri_offsets;
    wgpu::Buffer buf_connectivity;
    wgpu::Buffer buf_block_sums;
    wgpu::Buffer buf_bs_total;
    wgpu::Buffer buf_verts_out;

    uint32_t max_quads = 0;
    uint32_t num_quads = 0;
    bool initialized = false;

public:
    Tessellator(wgpu::Device device, wgpu::Queue queue, uint32_t maxQuads)
        : ComputePass(device, queue), max_quads(maxQuads) {}

    bool Init(wgpu::Buffer ipass_levels);
    void Upload(const glm::vec4* control_points, const uint32_t* indices, uint32_t num_quads);
    bool Execute(wgpu::CommandEncoder& encoder) override;
    wgpu::Buffer GetVertexOutput() const { return buf_verts_out; }
    wgpu::Buffer GetControlPointBuffer() const { return buf_quads; }
    wgpu::Buffer GetTriCountBuffer() const { return buf_bs_total; }
    uint32_t GetNumQuads() const { return num_quads; }
    uint32_t GetMaxQuads() const { return max_quads; }
    bool IsInitialized() const { return initialized; }
    void Terminate();
    void ClearBuffers();
};
