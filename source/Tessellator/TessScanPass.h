#pragma once

#include <webgpu/webgpu.hpp>
#include <cstdint>

class TessScanPass {
    wgpu::Device& device;

    wgpu::ComputePipeline pipeline_lvl1;
    wgpu::BindGroup bg_lvl1;

    wgpu::ComputePipeline pipeline_lvl2;
    wgpu::BindGroup bg_lvl2;

    wgpu::ComputePipeline pipeline_comb;
    wgpu::BindGroup bg_comb;

public:
    bool Init(wgpu::Device &device);
    wgpu::BindGroupLayout GetLevel1BGL();
    wgpu::BindGroupLayout GetLevel2BGL();
    wgpu::BindGroupLayout GetCombineBGL();
    bool SetBindGroups(wgpu::BindGroup local, wgpu::BindGroup block, wgpu::BindGroup add);
    bool Execute(wgpu::CommandEncoder encoder, uint32_t max_tris);
};
