#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "GPU.h"
#include "Tessellator.h"
#include "TessScanPass.h"
#include "TessDirectPass.h"
#include "TessConstants.h"

class VertexTessellator : public Tessellator {
    TessDirectPass direct_pass;
    TessScanPass chunk_scan_pass;

    wgpu::Buffer buf_direct_params;
    wgpu::Buffer buf_patch_info;
    wgpu::Buffer buf_chunk_counts;
    wgpu::Buffer buf_chunk_offsets;
    wgpu::Buffer buf_chunk_block_sums;
    wgpu::Buffer buf_chunk_total;
    wgpu::Buffer buf_chunks;
    wgpu::Buffer buf_draw_args;
    wgpu::Buffer buf_tri_total;
    wgpu::BindGroupLayout surface_bgl;
    wgpu::BindGroup surface_bg;

    std::vector<wgpu::Buffer*> Buffers();

public:
    VertexTessellator(wgpu::Device device, wgpu::Queue queue, uint32_t max_quads);

    bool Init(wgpu::Buffer ipass_levels);
    void Upload(const glm::vec4* control_points, const uint32_t* indices, uint32_t num_quads);
    bool Execute(wgpu::CommandEncoder& encoder);
    bool Draw(wgpu::RenderPassEncoder& pass, uint32_t group);
    void Terminate();

    // vertex stage WGSL defining ipass_vertex(), with its bindings in bind group `group`
    static std::string SurfaceShaderSource(uint32_t group);

    wgpu::Buffer GetTriCountBuffer() const { return buf_tri_total; }
    wgpu::Buffer GetIndirectBuffer() const { return buf_draw_args; }
    wgpu::BindGroupLayout GetSurfaceBindGroupLayout() const { return surface_bgl; }
    wgpu::BindGroup GetSurfaceBindGroup() const { return surface_bg; }
};
