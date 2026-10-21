#include "TessDirectPass.h"
#include "Shader.h"

bool TessDirectPass::Init(wgpu::Device dev) {
    this->device = dev;

    wgpu::ShaderModule module = utils::LoadShader(this->device, "Tessellator/tess-direct.wgsl");
    if (!module) return false;

    wgpu::ComputePipelineDescriptor pd;
    pd.compute.module = module;

    pd.compute.entryPoint = {"calc_direct", 11};
    pipeline_calc = this->device.createComputePipeline(pd);

    pd.compute.entryPoint = {"emit_chunks", 11};
    pipeline_emit = this->device.createComputePipeline(pd);

    module.release();
    return true;
}

wgpu::BindGroupLayout TessDirectPass::GetCalcBGL() {
    return pipeline_calc.getBindGroupLayout(0);
}

wgpu::BindGroupLayout TessDirectPass::GetEmitBGL() {
    return pipeline_emit.getBindGroupLayout(0);
}

bool TessDirectPass::SetBindGroups(wgpu::BindGroup calc, wgpu::BindGroup emit) {
    bg_calc = calc;
    bg_calc.addRef();
    bg_emit = emit;
    bg_emit.addRef();
    return true;
}

bool TessDirectPass::ExecuteCalc(wgpu::CommandEncoder encoder, uint32_t num_quads) {
    if (num_quads == 0)
        return true;
    utils::DispatchCompute(encoder, pipeline_calc, bg_calc, num_quads);
    return true;
}

bool TessDirectPass::ExecuteEmit(wgpu::CommandEncoder encoder, uint32_t num_quads) {
    if (num_quads == 0)
        return true;
    utils::DispatchCompute(encoder, pipeline_emit, bg_emit, num_quads);
    return true;
}
