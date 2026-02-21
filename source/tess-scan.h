
// prefix

#include <cstring>

static const char* tess_scan_shader = R"WGSL(
    @group(0) @binding(0) var<storage, read> triCount : array<u32>;
    @group(0) @binding(1) var<storage, read_write> triOffset : array<u32>;
    @group(0) @binding(2) var<storage, read_write> blockSums : array<u32>;

    var<workgroup> shared_ls : array<u32, 256>;

    // level 1 scan
    // lid.x = thread id within workgroup
    // wid.x = workgroup id
    // gid.x = wid.x*256 + lid.x
    @compute @workgroup_size(256)
    fn lvl1_scan (@builtin(global_invocation_id) gid : vec3u, @builtin(local_invocation_id) lid : vec3u, @builtin(workgroup_id) wid : vec3u) {
        let i = gid.x;
        let n = arrayLength(&triCount);

        shared_ls[lid.x] = select(0u, triCount[i], i < n);

        workgroupBarrier();

        // up sweep
        var stride = 1u;
        // it looks like while(stride < 256u) would be unsafe for internal sync
        loop {
            if (stride >= 256u) { break; }
            if ((lid.x + 1u) % (stride * 2u) == 0u) {
                shared_ls[lid.x] += shared_ls[lid.x - stride];
            }
            stride *= 2u;
            workgroupBarrier();
        }

        if (lid.x == 255u) {
            blockSums[wid.x] = shared_ls[255u];
            shared_ls[255u] = 0u;
        }
        workgroupBarrier();

        // down sweep
        stride = 128u;
        loop {
            if (stride == 0u) { break; }
            if ((lid.x + 1u) % (stride * 2u) == 0u) {
                // lid.x = right
                // lid.x - stride = left
                // left -> right
                // right -> left + right

                // tmp = right
                // right = left + right
                // left = tmp
                let tmp = shared_ls[lid.x];
                shared_ls[lid.x] += shared_ls[lid.x - stride];
                shared_ls[lid.x - stride] = tmp;
            }
            stride /= 2u;
            workgroupBarrier();
        }

        if (i < n) {
            triOffset[i] = shared_ls[lid.x];
        }
    }

    // level 2 scan

    @group(0) @binding(0) var<storage, read_write> bs_blockSums : array<u32>;
    @group(0) @binding(1) var<storage, read_write> bs_total : array<u32>;

    var<workgroup> shared_bs : array<u32, 256>;

    @compute @workgroup_size(256)
    fn lvl2_scan(@builtin(local_invocation_id) lid : vec3u) {
        shared_bs[lid.x] = bs_blockSums[lid.x];
        workgroupBarrier();

        // upsweep
        var stride = 1u;
        loop {
            if (stride >= 256u) { break; }
            if ((lid.x + 1u) % (stride * 2u) == 0u) {
                shared_bs[lid.x] += shared_bs[lid.x - stride];
            }
            stride *= 2u;
            workgroupBarrier();
        }

        if (lid.x == 255u) {
            bs_total[0] = shared_bs[255u]; // need sum since this is level 2
            shared_bs[255u] = 0u;
        }

        workgroupBarrier();

        // downsweep
        stride = 128u;
        loop {
            if (stride == 0u) { break; }
            if ((lid.x + 1u) % (stride * 2u) == 0u) {
                let tmp = shared_bs[lid.x];
                shared_bs[lid.x] += shared_bs[lid.x - stride];
                shared_bs[lid.x - stride] = tmp;
            }
            stride /= 2u;
            workgroupBarrier();
        }

        bs_blockSums[lid.x] = shared_bs[lid.x];
    }

    // combine

    @compute @workgroup_size(256)
    fn comb(@builtin(global_invocation_id) gid : vec3u, @builtin(workgroup_id) wid : vec3u) {
        let i = gid.x;
        if (i >= arrayLength(&triOffset)) { return; }
        triOffset[i] += blockSums[wid.x];
    }
)WGSL";

class TessScanPass {
    wgpu::Device device;

    wgpu::ComputePipeline pipeline_lvl1;
    wgpu::BindGroup bg_lvl1;

    wgpu::ComputePipeline pipeline_lvl2;
    wgpu::BindGroup bg_lvl2;

    wgpu::ComputePipeline pipeline_comb;
    wgpu::BindGroup bg_comb;

 public:
    bool init(wgpu::Device &device);
    wgpu::BindGroupLayout get_lvl1_bgl();
    wgpu::BindGroupLayout get_lvl2_bgl();
    wgpu::BindGroupLayout get_comb_bgl();
    bool set_bindgroups(wgpu::BindGroup local, wgpu::BindGroup block, wgpu::BindGroup add);
    bool exec(wgpu::CommandEncoder encoder, uint32_t max_tris);
};

bool TessScanPass::init(wgpu::Device &device) {
    this->device = device;

    wgpu::ShaderModuleDescriptor shaderDesc;
    wgpu::ShaderSourceWGSL wgsl;
    wgsl.chain.sType = wgpu::SType::ShaderSourceWGSL;
    wgsl.code.data = tess_scan_shader;
    wgsl.code.length = std::strlen(tess_scan_shader);
    shaderDesc.nextInChain = &wgsl.chain;
    wgpu::ShaderModule module = this->device.createShaderModule(shaderDesc);

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

wgpu::BindGroupLayout TessScanPass::get_lvl1_bgl() {
    return pipeline_lvl1.getBindGroupLayout(0);
}

wgpu::BindGroupLayout TessScanPass::get_lvl2_bgl() {
    return pipeline_lvl2.getBindGroupLayout(0);
}

wgpu::BindGroupLayout TessScanPass::get_comb_bgl() {
    return pipeline_comb.getBindGroupLayout(0);
}

bool TessScanPass::set_bindgroups(wgpu::BindGroup lvl1, wgpu::BindGroup lvl2, wgpu::BindGroup comb) {
    bg_lvl1 = lvl1;
    bg_lvl2 = lvl2;
    bg_comb = comb;
    return true;
}

bool TessScanPass::exec(wgpu::CommandEncoder encoder, uint32_t max_tris) {
    uint32_t num_blocks = (max_tris + 255u) / 256u;

    wgpu::ComputePassDescriptor pd;

    // level 1 scam
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