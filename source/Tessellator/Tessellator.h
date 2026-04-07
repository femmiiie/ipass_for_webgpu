#pragma once

#include <cstdint>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#include "Pass.h"
#include "TessCalcPass.h"
#include "TessScanPass.h"
#include "TessGenPass.h"
#include "TessConstants.h"

class Tessellator : public Pass {
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

public:
    Tessellator(GPUContext& ctx) : Pass(ctx) {}

    bool Init(uint32_t max_quads, wgpu::Buffer ipass_levels);
    void Upload(const glm::vec4* control_points, const uint32_t* indices, uint32_t num_quads);
    bool Execute(wgpu::CommandEncoder encoder, uint32_t num_quads);
    wgpu::Buffer GetVertexOutput() const { return buf_verts_out; }
    wgpu::Buffer GetControlPointBuffer() const { return buf_quads; }
    void Terminate();
    void ClearBuffers();
};
