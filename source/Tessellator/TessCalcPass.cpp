#include "TessCalcPass.h"
#include "Shader.h"
#include <cstring>

bool TessCalcPass::Init(wgpu::Device dev) {
    this->device = dev;

    wgpu::ShaderModule module = utils::LoadShader(this->device, "Tessellator/tess-calc.wgsl");
    if (!module) return false;

    wgpu::ComputePipelineDescriptor pd;
    pd.compute.module = module;

    pd.compute.entryPoint = {"tess_factor", 11};
    pipeline = this->device.createComputePipeline(pd);

    module.release();
    return true;
}

wgpu::BindGroupLayout TessCalcPass::GetBindGroupLayout() {
    return pipeline.getBindGroupLayout(0);
}

bool TessCalcPass::SetBindGroup(wgpu::BindGroup bind_group) {
    bg = bind_group;
    bg.addRef();
    return true;
}

bool TessCalcPass::Execute(wgpu::CommandEncoder encoder, uint32_t num_quads) {
    if (num_quads == 0)
        return true;

    utils::DispatchCompute(encoder, pipeline, bg, num_quads);

    return true;
}
