#pragma once

#include <webgpu/webgpu.hpp>
#include <cstdint>

class TessGenPass {
    wgpu::Device device;

    wgpu::ComputePipeline pipeline_counts;
    wgpu::BindGroup bg_counts;

    wgpu::ComputePipeline pipeline_gen;
    wgpu::BindGroup bg_gen;

public:
    bool Init(wgpu::Device device);
    wgpu::BindGroupLayout GetCountsBGL();
    wgpu::BindGroupLayout GetGenBGL();
    bool SetBindGroups(wgpu::BindGroup counts, wgpu::BindGroup gen);
    bool ExecuteCounts(wgpu::CommandEncoder encoder, uint32_t num_quads);
    bool ExecuteGen(wgpu::CommandEncoder encoder, uint32_t num_quads);
};
