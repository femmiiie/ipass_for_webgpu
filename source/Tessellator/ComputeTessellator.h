#pragma once

#include <cstdint>
#include <vector>

#include "GPU.h"
#include "Tessellator.h"
#include "TessScanPass.h"
#include "TessGenPass.h"
#include "TessConstants.h"

// tessellates every patch into a vertex buffer; triangle counts, scanned into offsets, then tess_gen
class ComputeTessellator : public Tessellator {
    TessGenPass gen_pass;
    TessScanPass scan_pass;

    wgpu::Buffer buf_tri_counts;
    wgpu::Buffer buf_tri_offsets;
    wgpu::Buffer buf_block_sums;
    wgpu::Buffer buf_bs_total;
    wgpu::Buffer buf_verts_out;

    std::vector<wgpu::Buffer*> Buffers();

public:
    ComputeTessellator(wgpu::Device device, wgpu::Queue queue, uint32_t max_quads)
        : Tessellator(device, queue, max_quads) {}

    bool Init(wgpu::Buffer ipass_levels);
    void Upload(const glm::vec4* control_points, const uint32_t* indices, uint32_t num_quads);
    bool Execute(wgpu::CommandEncoder& encoder);
    void Terminate();

    wgpu::Buffer GetVertexOutput() const { return buf_verts_out; }
    wgpu::Buffer GetTriCountBuffer() const { return buf_bs_total; }
};
