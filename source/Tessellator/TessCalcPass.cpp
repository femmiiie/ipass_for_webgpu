#include "TessCalcPass.h"
#include "Utils.h"
#include <cstring>

bool TessCalcPass::Init(wgpu::Device dev) {
    this->device = dev;

    wgpu::ShaderModule module = utils::LoadShader(this->device, "Tessellator/tess-calc.wgsl");
    if (!module) return false;

    wgpu::ComputePipelineDescriptor pd;
    pd.compute.module = module;

    pd.compute.entryPoint = {"tess_factor", 11};
    pipeline_tess_factor = this->device.createComputePipeline(pd);

    pd.compute.entryPoint = {"calc_counts", 11};
    pipeline_calc_counts = this->device.createComputePipeline(pd);

    module.release();
    return true;
}

wgpu::BindGroupLayout TessCalcPass::GetTessFactorBGL() {
    return pipeline_tess_factor.getBindGroupLayout(0);
}

wgpu::BindGroupLayout TessCalcPass::GetCalcCountsBGL() {
    return pipeline_calc_counts.getBindGroupLayout(0);
}

bool TessCalcPass::SetBindGroups(wgpu::BindGroup tess_factor_bg, wgpu::BindGroup calc_counts_bg) {
    bg_tess_factor = tess_factor_bg;
    bg_tess_factor.addRef();
    bg_calc_counts = calc_counts_bg;
    bg_calc_counts.addRef();
    return true;
}

bool TessCalcPass::Execute(wgpu::CommandEncoder encoder, uint32_t num_quads) {
    if (num_quads == 0)
        return true;

    const uint32_t workgroup_size = 256;
    uint32_t num_workgroups = (num_quads + workgroup_size - 1) / workgroup_size;

    wgpu::ComputePassDescriptor pd;

    wgpu::ComputePassEncoder pass = encoder.beginComputePass(pd);
    pass.setPipeline(pipeline_tess_factor);
    pass.setBindGroup(0, bg_tess_factor, 0, nullptr);
    pass.dispatchWorkgroups(num_workgroups, 1, 1);
    pass.end();
    pass.release();

    pass = encoder.beginComputePass(pd);
    pass.setPipeline(pipeline_calc_counts);
    pass.setBindGroup(0, bg_calc_counts, 0, nullptr);
    pass.dispatchWorkgroups(num_workgroups, 1, 1);
    pass.end();
    pass.release();

    return true;
}
