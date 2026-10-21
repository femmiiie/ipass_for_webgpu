#pragma once

#include <webgpu/webgpu.hpp>
#include <cstdint>

class TessDirectPass {
    wgpu::Device device;

    wgpu::ComputePipeline pipeline_calc;
    wgpu::BindGroup bg_calc;

    wgpu::ComputePipeline pipeline_emit;
    wgpu::BindGroup bg_emit;

public:
    bool Init(wgpu::Device device);
    wgpu::BindGroupLayout GetCalcBGL();
    wgpu::BindGroupLayout GetEmitBGL();
    bool SetBindGroups(wgpu::BindGroup calc, wgpu::BindGroup emit);
    bool ExecuteCalc(wgpu::CommandEncoder encoder, uint32_t num_quads);
    bool ExecuteEmit(wgpu::CommandEncoder encoder, uint32_t num_quads);
};
