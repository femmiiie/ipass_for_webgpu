#include "TessGenPass.h"
#include "Utils.h"
#include <cstring>

bool TessGenPass::Init(wgpu::Device &dev) {
    this->device = dev;

    wgpu::ShaderModule module = utils::LoadShader(this->device, "Tessellator/tess-gen.wgsl");
    if (!module) return false;

    wgpu::ComputePipelineDescriptor pd;
    pd.compute.module = module;

    pd.compute.entryPoint = {"tess_gen", 8};
    pipeline = this->device.createComputePipeline(pd);

    module.release();
    return true;
}

wgpu::BindGroupLayout TessGenPass::GetBindGroupLayout() {
    return pipeline.getBindGroupLayout(0);
}

bool TessGenPass::SetBindGroup(wgpu::BindGroup bind_group) {
    bg = bind_group;
    bg.addRef();
    return true;
}

bool TessGenPass::Execute(wgpu::CommandEncoder encoder, uint32_t num_quads) {
    if (num_quads == 0)
        return true;

    const uint32_t workgroup_size = 256;
    uint32_t num_workgroups = (num_quads + workgroup_size - 1) / workgroup_size;

    wgpu::ComputePassDescriptor pd;

    wgpu::ComputePassEncoder pass = encoder.beginComputePass(pd);
    pass.setPipeline(pipeline);
    pass.setBindGroup(0, bg, 0, nullptr);
    pass.dispatchWorkgroups(num_workgroups, 1, 1);
    pass.end();
    pass.release();

    return true;
}
