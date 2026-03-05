#pragma once

static const char* tess_gen_shader = R"WGSL(
    @group(0) @binding(0) var<storage, read> tg_quadsIn : array<vec4f>;
    @group(0) @binding(1) var<storage, read> tg_tessFactors : array<f32>;
    @group(0) @binding(2) var<storage, read> tg_triOffsets : array<u32>;
    @group(0) @binding(3) var<storage, read> tg_connectivity : array<vec4<i32>>;
    @group(0) @binding(4) var<storage, read_write> tg_vertsOut : array<vec4f>;

    fn eval_bicubic(patch_offset: u32, uv: vec2f) -> vec4f {
        let u = uv.x;
        let v = uv.y;

        let u2 = u * u;
        let u3 = u2 * u;
        let v2 = v * v;
        let v3 = v2 * v;
        var bu = array<f32, 4>(1.0 - 3.0*u + 3.0*u2 - u3, 3.0*u - 6.0*u2 + 3.0*u3, 3.0*u2 - 3.0*u3, u3);
        var bv = array<f32, 4>(1.0 - 3.0*v + 3.0*v2 - v3, 3.0*v - 6.0*v2 + 3.0*v3, 3.0*v2 - 3.0*v3, v3);
        var result = vec4f(0.0);
        for (var row = 0u; row < 4u; row++) {
            for (var col = 0u; col < 4u; col++) {
                let bu_r = bu[row];
                let bv_c = bv[col];
                let cp = tg_quadsIn[patch_offset + row * 4u + col];
                result += bu_r * bv_c * cp;
            }
        }
        return result;
    }

    fn st_to_uv(st: vec2f, side: u32) -> vec2f {
        if (side == 0) {
            return vec2f(st.x, st.y);
        }
        if (side == 1) {
            return vec2f(1.0 - st.y, st.x);
        }
        if (side == 2) {
            return vec2f(st.x, 1.0 - st.y);
        }
        return vec2f(st.y, st.x);
    }

    @compute @workgroup_size(256)
    fn tess_gen(@builtin(global_invocation_id) gid : vec3u) {
        let i = gid.x;
        if (i >= arrayLength(&tg_tessFactors)) {
            return;
        }

        let this_level = u32(ceil(tg_tessFactors[i]));
        let tri_offset = tg_triOffsets[i];
        let nb = tg_connectivity[i * 2u + 0u];
        let nb_edges = tg_connectivity[i * 2u + 1u];

        var OL0 = this_level;
        if (nb.x >= 0) {
            OL0 = max(this_level, u32(ceil(tg_tessFactors[u32(nb.x)])));
        }
        var OL1 = this_level;
        if (nb.y >= 0) {
            OL1 = max(this_level, u32(ceil(tg_tessFactors[u32(nb.y)])));
        }
        var OL2 = this_level;
        if (nb.z >= 0) {
            OL2 = max(this_level, u32(ceil(tg_tessFactors[u32(nb.z)])));
        }
        var OL3 = this_level;
        if (nb.w >= 0) {
            OL3 = max(this_level, u32(ceil(tg_tessFactors[u32(nb.w)])));
        }

        var IL0 = this_level;
        var IL1 = this_level;

        var k = 0u;
        let patch_offset = i * 16u;

        var curr_outer = vec4f(0.0);
        var curr_inner = vec4f(0.0);
        var outer_i = 0u;
        var inner_i = 0u;

        // degenerate case
        if (IL0 == 1u && IL1 == 1u && OL0 == 1u && OL1 == 1u && OL2 == 1u && OL3 == 1u) {
            tg_vertsOut[(tri_offset + k) * 3u] = eval_bicubic(patch_offset, vec2f(0.0, 0.0));
            tg_vertsOut[(tri_offset + k) * 3u + 1u] = eval_bicubic(patch_offset, vec2f(0.0, 1.0));
            tg_vertsOut[(tri_offset + k) * 3u + 2u] = eval_bicubic(patch_offset, vec2f(1.0, 1.0));
            tg_vertsOut[(tri_offset + k + 1u) * 3u] = eval_bicubic(patch_offset, vec2f(1.0, 1.0));
            tg_vertsOut[(tri_offset + k + 1u) * 3u + 1u] = eval_bicubic(patch_offset, vec2f(1.0, 0.0));
            tg_vertsOut[(tri_offset + k + 1u) * 3u + 2u] = eval_bicubic(patch_offset, vec2f(0.0, 0.0));
            return;
        }

        let IL0e = select(IL0, 2u, IL0 == 1u);
        let IL1e = select(IL1, 2u, IL1 == 1u);

        // inner rect
        for (var ii = 1u; ii < IL0e - 1u; ii++) {
            let LEFT = f32(ii) / f32(IL0e);
            let RIGHT = f32(ii + 1u) / f32(IL0e);
            for (var jj = 1u; jj < IL1e - 1u; jj++) {
                let BOTTOM = f32(jj) / f32(IL1e);
                let TOP = f32(jj + 1u) / f32(IL1e);

                let BL = eval_bicubic(patch_offset, vec2f(LEFT, BOTTOM));
                let BR = eval_bicubic(patch_offset, vec2f(RIGHT, BOTTOM));
                let TL = eval_bicubic(patch_offset, vec2f(LEFT, TOP));
                let TR = eval_bicubic(patch_offset, vec2f(RIGHT, TOP));

                tg_vertsOut[(tri_offset + k) * 3u] = BL;
                tg_vertsOut[(tri_offset + k) * 3u + 1u] = BR;
                tg_vertsOut[(tri_offset + k) * 3u + 2u] = TR;
                k += 1u;
                tg_vertsOut[(tri_offset + k) * 3u] = TR;
                tg_vertsOut[(tri_offset + k) * 3u + 1u] = TL;
                tg_vertsOut[(tri_offset + k) * 3u + 2u] = BL;
                k += 1u;
            }
        }

        // outer trapezoids
        for (var side = 0u; side < 4u; side++) {
            let is_even = (side % 2u) == 0u;
            let inner_max = select(IL1e, IL0e, is_even);
            let inner_height = select(IL0e, IL1e, is_even);
            var outer_max = OL0;
            if (side == 1u) {
                outer_max = OL1;
            } else if (side == 2u) {
                outer_max = OL2;
            } else if (side == 3u) {
                outer_max = OL3;
            }

            if (inner_height <= 1u || inner_max <= 1u || outer_max == 0u) {
                continue;
            }

            let outer_t = 0.0;
            let inner_t = 1.0 / f32(inner_height);
            curr_outer = eval_bicubic(patch_offset, st_to_uv(vec2f(0.0, outer_t), side));
            curr_inner = eval_bicubic(patch_offset, st_to_uv(vec2f(1.0 / f32(inner_max), inner_t), side));
            outer_i = 0u;
            inner_i = 1u;

            while (inner_i < inner_max - 1u || outer_i < outer_max) {
                let next_outer_s = select(1.0, f32(outer_i + 1u) / f32(outer_max), outer_i < outer_max);
                let next_inner_s = select(1.0, f32(inner_i + 1u) / f32(inner_max), inner_i < inner_max - 1u);

                if (outer_i < outer_max && (inner_i == inner_max - 1u || next_outer_s <= next_inner_s)) {
                    let next_outer = eval_bicubic(patch_offset, st_to_uv(vec2f(f32(outer_i + 1u) / f32(outer_max), outer_t), side));
                    tg_vertsOut[(tri_offset + k) * 3u] = curr_outer;
                    tg_vertsOut[(tri_offset + k) * 3u + 1u] = curr_inner;
                    tg_vertsOut[(tri_offset + k) * 3u + 2u] = next_outer;
                    k += 1u;
                    curr_outer = next_outer;
                    outer_i += 1u;
                } else {
                    let next_inner = eval_bicubic(patch_offset, st_to_uv(vec2f(f32(inner_i + 1u) / f32(inner_max), inner_t), side));
                    tg_vertsOut[(tri_offset + k) * 3u] = curr_outer;
                    tg_vertsOut[(tri_offset + k) * 3u + 1u] = next_inner;
                    tg_vertsOut[(tri_offset + k) * 3u + 2u] = curr_inner;
                    k += 1u;
                    curr_inner = next_inner;
                    inner_i += 1u;
                }
            }
        }


    }
)WGSL";

class TessGenPass {
    wgpu::Device device;
    wgpu::ComputePipeline pipeline;
    wgpu::BindGroup bg;

public:
    bool init(wgpu::Device &device);
    wgpu::BindGroupLayout get_bgl();
    bool set_bindgroup(wgpu::BindGroup bind_group);
    bool exec(wgpu::CommandEncoder encoder, uint32_t num_quads);
};

bool TessGenPass::init(wgpu::Device &device) {
    this->device = device;

    wgpu::ShaderModuleDescriptor shaderDesc;
    wgpu::ShaderSourceWGSL wgsl;
    wgsl.chain.sType = wgpu::SType::ShaderSourceWGSL;
    wgsl.code.data = tess_gen_shader;
    wgsl.code.length = std::strlen(tess_gen_shader);
    shaderDesc.nextInChain = &wgsl.chain;
    wgpu::ShaderModule module = this->device.createShaderModule(shaderDesc);

    wgpu::ComputePipelineDescriptor pd;
    pd.compute.module = module;
    
    // printf("\n\n------------------------\n\n");

    pd.compute.entryPoint = {"tess_gen", 8};
    pipeline = this->device.createComputePipeline(pd);

    module.release();
    return true;
}

wgpu::BindGroupLayout TessGenPass::get_bgl() {
    return pipeline.getBindGroupLayout(0);
}

bool TessGenPass::set_bindgroup(wgpu::BindGroup bind_group) {
    bg = bind_group;
    bg.addRef();
    return true;
}

bool TessGenPass::exec(wgpu::CommandEncoder encoder, uint32_t num_quads) {
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
