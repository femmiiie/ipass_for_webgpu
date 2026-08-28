#pragma once

#include <webgpu/webgpu.hpp>
#include <cstdint>

class TessCalcPass {
    wgpu::Device device;

    wgpu::ComputePipeline pipeline_tess_factor;
    wgpu::BindGroup bg_tess_factor;

    wgpu::ComputePipeline pipeline_calc_counts;
    wgpu::BindGroup bg_calc_counts;

public:
    bool Init(wgpu::Device device);
    wgpu::BindGroupLayout GetTessFactorBGL();
    wgpu::BindGroupLayout GetCalcCountsBGL();
    bool SetBindGroups(wgpu::BindGroup tess_factor_bg, wgpu::BindGroup calc_counts_bg);
    bool Execute(wgpu::CommandEncoder encoder, uint32_t num_quads);
};
