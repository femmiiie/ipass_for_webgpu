#include "TessScanPass.h"
#include "Shader.h"
#include <cstring>

bool TessScanPass::Init(wgpu::Device dev) {
    this->device = dev;

    wgpu::ShaderModule module = utils::LoadShader(this->device, "Tessellator/tess-scan.wgsl");
    if (!module) return false;

    wgpu::ComputePipelineDescriptor pd;
    pd.compute.module = module;

    pd.compute.entryPoint = {"lvl1_scan", 9};
    pipeline_lvl1 = this->device.createComputePipeline(pd);
    pd.compute.entryPoint = {"lvl2_scan", 9};
    pipeline_lvl2 = this->device.createComputePipeline(pd);
    pd.compute.entryPoint = {"comb", 4};
    pipeline_comb = this->device.createComputePipeline(pd);

    module.release();
    return true;
}

wgpu::BindGroupLayout TessScanPass::GetLevel1BGL() {
    return pipeline_lvl1.getBindGroupLayout(0);
}

wgpu::BindGroupLayout TessScanPass::GetLevel2BGL() {
    return pipeline_lvl2.getBindGroupLayout(0);
}

wgpu::BindGroupLayout TessScanPass::GetCombineBGL() {
    return pipeline_comb.getBindGroupLayout(0);
}

bool TessScanPass::SetBindGroups(wgpu::BindGroup lvl1, wgpu::BindGroup lvl2, wgpu::BindGroup comb) {
    bg_lvl1 = lvl1;
    bg_lvl1.addRef();
    bg_lvl2 = lvl2;
    bg_lvl2.addRef();
    bg_comb = comb;
    bg_comb.addRef();
    return true;
}

bool TessScanPass::Execute(wgpu::CommandEncoder encoder, uint32_t max_tris) {
    uint32_t num_blocks = (max_tris + 255u) / 256u;

    wgpu::ComputePassDescriptor pd;

    // level 1 scan
    wgpu::ComputePassEncoder pass = encoder.beginComputePass(pd);
    pass.setPipeline(pipeline_lvl1);
    pass.setBindGroup(0, bg_lvl1, 0, nullptr);
    pass.dispatchWorkgroups(num_blocks, 1, 1);
    pass.end();
    pass.release();

    // level 2 scan
    pass = encoder.beginComputePass(pd);
    pass.setPipeline(pipeline_lvl2);
    pass.setBindGroup(0, bg_lvl2, 0, nullptr);
    pass.dispatchWorkgroups(1, 1, 1);
    pass.end();
    pass.release();

    // combine pass
    pass = encoder.beginComputePass(pd);
    pass.setPipeline(pipeline_comb);
    pass.setBindGroup(0, bg_comb, 0, nullptr);
    pass.dispatchWorkgroups(num_blocks, 1, 1);
    pass.end();
    pass.release();

    return true;
}
