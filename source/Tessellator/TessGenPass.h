#pragma once

#include <webgpu/webgpu.hpp>
#include <cstdint>

class TessGenPass {
    wgpu::Device& device;
    wgpu::ComputePipeline pipeline;
    wgpu::BindGroup bg;

public:
    bool Init(wgpu::Device &device);
    wgpu::BindGroupLayout GetBindGroupLayout();
    bool SetBindGroup(wgpu::BindGroup bind_group);
    bool Execute(wgpu::CommandEncoder encoder, uint32_t num_quads);
};
