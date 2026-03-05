#pragma once

// inherits includes from main
// probably not best design, but we can fix later

static const char* tess_factor_shader = R"WGSL(
    const TESS_LEVEL: f32 = 4.0;

    @group(0) @binding(1) var<storage, read_write> tessFactors : array<f32>;

    @compute @workgroup_size(256)
    fn tess_factor(@builtin(global_invocation_id) gid : vec3u) {
        let i = gid.x;
        if (i >= arrayLength(&tessFactors)) {
            return;
        }
        tessFactors[i] = TESS_LEVEL;
        //TODO: iPASS :)
    }
)WGSL";

static const char* calc_counts_shader = R"WGSL(
    @group(0) @binding(0) var<storage, read> cc_tessFactors : array<f32>;
    @group(0) @binding(1) var<storage, read_write> cc_triCount : array<u32>;
    @group(0) @binding(2) var<storage, read> cc_connectivity : array<vec4<i32>>;

    @compute @workgroup_size(256)
    fn calc_counts(@builtin(global_invocation_id) gid : vec3u) {
        let i = gid.x;
        if (i >= arrayLength(&cc_triCount)) {
            return;
        }

        let this_level = cc_tessFactors[i];
        let nb = cc_connectivity[i * 2u + 0u];

        var OL0 = u32(ceil(this_level));
        if (nb.x >= 0) {
            OL0 = u32(ceil(max(this_level, cc_tessFactors[u32(nb.x)])));
        }
        var OL1 = u32(ceil(this_level));
        if (nb.y >= 0) {
            OL1 = u32(ceil(max(this_level, cc_tessFactors[u32(nb.y)])));
        }
        var OL2 = u32(ceil(this_level));
        if (nb.z >= 0) {
            OL2 = u32(ceil(max(this_level, cc_tessFactors[u32(nb.z)])));
        }
        var OL3 = u32(ceil(this_level));
        if (nb.w >= 0) {
            OL3 = u32(ceil(max(this_level, cc_tessFactors[u32(nb.w)])));
        }

        var IL0 = max(u32(ceil(this_level)), 2u);
        var IL1 = max(u32(ceil(this_level)), 2u);

        cc_triCount[i] = 2u * ((IL0 - 2u)*(IL1 - 2u)) + OL0 + OL1 + OL2 + OL3 + 2u*IL0 + 2u*IL1 - 8u;
    }
)WGSL";
// TODO: prevent u32 underflow on degenerate tesselation cases

class TessCalcPass {
    wgpu::Device device;

    wgpu::ComputePipeline pipeline_tess_factor;
    wgpu::BindGroup bg_tess_factor;

    wgpu::ComputePipeline pipeline_calc_counts;
    wgpu::BindGroup bg_calc_counts;

public:
    bool init(wgpu::Device &device);
    wgpu::BindGroupLayout get_tess_factor_bgl();
    wgpu::BindGroupLayout get_calc_counts_bgl();
    bool set_bindgroups(wgpu::BindGroup tess_factor_bg, wgpu::BindGroup calc_counts_bg);
    bool exec(wgpu::CommandEncoder encoder, uint32_t num_quads);
};

bool TessCalcPass::init(wgpu::Device &dev) {
    this->device = dev;

    wgpu::ShaderModuleDescriptor shaderDesc;
    wgpu::ShaderSourceWGSL wgsl;
    wgsl.chain.sType = wgpu::SType::ShaderSourceWGSL;
    shaderDesc.nextInChain = &wgsl.chain;

    wgsl.code.data = tess_factor_shader;
    wgsl.code.length = std::strlen(tess_factor_shader);
    wgpu::ShaderModule module_tf = this->device.createShaderModule(shaderDesc);
    wgpu::ComputePipelineDescriptor pd;
    pd.compute.module = module_tf;
    pd.compute.entryPoint = {"tess_factor", 11};
    pipeline_tess_factor = this->device.createComputePipeline(pd);
    module_tf.release();

    wgsl.code.data = calc_counts_shader;
    wgsl.code.length = std::strlen(calc_counts_shader);
    wgpu::ShaderModule module_cc = this->device.createShaderModule(shaderDesc);
    pd.compute.module = module_cc;
    pd.compute.entryPoint = {"calc_counts", 11};
    pipeline_calc_counts = this->device.createComputePipeline(pd);
    module_cc.release();

    return true;
}

wgpu::BindGroupLayout TessCalcPass::get_tess_factor_bgl() {
    return pipeline_tess_factor.getBindGroupLayout(0);
}

wgpu::BindGroupLayout TessCalcPass::get_calc_counts_bgl() {
    return pipeline_calc_counts.getBindGroupLayout(0);
}

bool TessCalcPass::set_bindgroups(wgpu::BindGroup tess_factor_bg, wgpu::BindGroup calc_counts_bg) {
    bg_tess_factor = tess_factor_bg;
    bg_tess_factor.addRef();
    bg_calc_counts = calc_counts_bg;
    bg_calc_counts.addRef();
    return true;
}

bool TessCalcPass::exec(wgpu::CommandEncoder encoder, uint32_t num_quads) {
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
