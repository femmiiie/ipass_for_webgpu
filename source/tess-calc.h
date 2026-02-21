#pragma once

// inherits includes from main
// probably not best design, but we can fix later


static const char* tess_calc_shader = R"WGSL(
    @group(0) @binding(0) var<storage, read> triIn : array<vec4f>;
    @group(0) @binding(1) var<storage, read_write> triCount : array<u32>;

    // Uniform tessellation level. Replace with per-patch logic later
    const TESS_LEVEL: u32 = 4u;  // each triangle has TESS_LEVEL**2 sub-triangles

    @compute @workgroup_size(256)
    fn main(@builtin(global_invocation_id) gid : vec3u) {
        let i = gid.x;
        if (i >= arrayLength(&triCount)) { return; }
        triCount[i] = TESS_LEVEL * TESS_LEVEL;
    }
)WGSL";

class TessCalcPass {
 private:
    wgpu::Device device;
    wgpu::ComputePipeline pipeline;
    wgpu::BindGroup bindgroup;
 public:
    bool init(wgpu::Device &device);
    wgpu::BindGroupLayout get_bind_group_layout();
    bool set_bindgroup(wgpu::BindGroup bindGroup);
    bool exec(wgpu::CommandEncoder encoder, uint32_t num_tris);
};

bool TessCalcPass::init(wgpu::Device &device) {
    this->device = device;

    // make shader
    wgpu::ShaderModuleDescriptor shaderDesc;
    wgpu::ShaderSourceWGSL wgsl;
    wgsl.chain.sType = wgpu::SType::ShaderSourceWGSL;
    wgsl.code.data = tess_calc_shader;
    wgsl.code.length = std::strlen(tess_calc_shader);
    shaderDesc.nextInChain = &wgsl.chain;
    wgpu::ShaderModule module = this->device.createShaderModule(shaderDesc);

    // make pipeline
    wgpu::ComputePipelineDescriptor pipelineDesc;
    pipelineDesc.compute.module = module;
    pipelineDesc.compute.entryPoint = {"main", 4};
    this->pipeline = this->device.createComputePipeline(pipelineDesc);

    module.release();
    return true;
}

wgpu::BindGroupLayout TessCalcPass::get_bind_group_layout() {
    return pipeline.getBindGroupLayout(0);
}

bool TessCalcPass::set_bindgroup(wgpu::BindGroup bindgroup) {
    this->bindgroup = bindgroup;
    return true;
}

bool TessCalcPass::exec(wgpu::CommandEncoder encoder, uint32_t num_tris) {
    if (num_tris == 0) return true;

    wgpu::ComputePassDescriptor pass_desc;
    wgpu::ComputePassEncoder pass = encoder.beginComputePass(pass_desc);

    pass.setPipeline(pipeline);
    pass.setBindGroup(0, bindgroup, 0, nullptr);

    const uint32_t workgroup_size = 256;
    uint32_t num_workgroups = (num_tris + workgroup_size - 1) / workgroup_size;
    pass.dispatchWorkgroups(num_workgroups, 1, 1);

    pass.end();
    pass.release();
    return true;
}