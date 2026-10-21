#include "TessGenPass.h"
#include "Shader.h"
#include <cstring>

bool TessGenPass::Init(wgpu::Device dev) {
    this->device = dev;

    wgpu::ShaderModule module = utils::LoadShader(this->device, "Tessellator/tess-gen.wgsl");
    if (!module) return false;

    wgpu::ComputePipelineDescriptor pd;
    pd.compute.module = module;

    pd.compute.entryPoint = {"calc_counts", 11};
    pipeline_counts = this->device.createComputePipeline(pd);

    pd.compute.entryPoint = {"tess_gen", 8};
    pipeline_gen = this->device.createComputePipeline(pd);

    module.release();
    return true;
}

wgpu::BindGroupLayout TessGenPass::GetCountsBGL() {
    return pipeline_counts.getBindGroupLayout(0);
}

wgpu::BindGroupLayout TessGenPass::GetGenBGL() {
    return pipeline_gen.getBindGroupLayout(0);
}

bool TessGenPass::SetBindGroups(wgpu::BindGroup counts, wgpu::BindGroup gen) {
    bg_counts = counts;
    bg_counts.addRef();
    bg_gen = gen;
    bg_gen.addRef();
    return true;
}

bool TessGenPass::ExecuteCounts(wgpu::CommandEncoder encoder, uint32_t num_quads) {
    if (num_quads == 0)
        return true;
    utils::DispatchCompute(encoder, pipeline_counts, bg_counts, num_quads);
    return true;
}

bool TessGenPass::ExecuteGen(wgpu::CommandEncoder encoder, uint32_t num_quads) {
    if (num_quads == 0)
        return true;
    utils::DispatchCompute(encoder, pipeline_gen, bg_gen, num_quads);
    return true;
}
