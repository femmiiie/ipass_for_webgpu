    @group(0) @binding(0) var<storage, read> tg_quadsIn : array<vec4f>;
    @group(0) @binding(1) var<storage, read> tg_tessFactors : array<f32>;
    @group(0) @binding(2) var<storage, read> tg_triOffsets : array<u32>;
    @group(0) @binding(3) var<storage, read> tg_connectivity : array<vec4<i32>>;
    @group(0) @binding(4) var<storage, read_write> tg_vertsOut : array<vec4f>;

    struct Vertex { // vert with position and norm only
        pos:  vec4f,
        norm: vec4f,
    }

    // evaluate position analytic surface normal via partial derivatives.
    fn eval_bicubic_vert(patch_offset: u32, uv: vec2f) -> Vertex {
        let u = uv.x;
        let v = uv.y;

        let u2 = u * u;
        let u3 = u2 * u;
        let v2 = v * v;
        let v3 = v2 * v;

        var bu  = array<f32, 4>(1.0 - 3.0*u + 3.0*u2 -     u3,
                                      3.0*u - 6.0*u2 + 3.0*u3,
                                      3.0*u2 - 3.0*u3,
                                      u3);
        var bv  = array<f32, 4>(1.0 - 3.0*v + 3.0*v2 -     v3,
                                      3.0*v - 6.0*v2 + 3.0*v3,
                                      3.0*v2 - 3.0*v3,
                                      v3);
        var dbu = array<f32, 4>(-3.0 + 6.0*u - 3.0*u2,
                                 3.0 - 12.0*u + 9.0*u2,
                                 6.0*u - 9.0*u2,
                                 3.0*u2);
        var dbv = array<f32, 4>(-3.0 + 6.0*v - 3.0*v2,
                                 3.0 - 12.0*v + 9.0*v2,
                                 6.0*v - 9.0*v2,
                                 3.0*v2);

        var pos  = vec4f(0.0);  // (N_x, N_y, N_z, W)
        var du_h = vec4f(0.0);  // d/du of (N_x, N_y, N_z, W)
        var dv_h = vec4f(0.0);  // d/dv of (N_x, N_y, N_z, W)
        for (var row = 0u; row < 4u; row++) {
            for (var col = 0u; col < 4u; col++) {
                let cp = tg_quadsIn[patch_offset + row * 4u + col];
                pos  += bu[row]  * bv[col]  * cp;
                du_h += dbu[row] * bv[col]  * cp;
                dv_h += bu[row]  * dbv[col] * cp;
            }
        }

        let W    = pos.w;
        let N    = pos.xyz;
        let dNdu = du_h.xyz;
        let dWdu = du_h.w;
        let dNdv = dv_h.xyz;
        let dWdv = dv_h.w;

        let tan_u = W * dNdu - N * dWdu;
        let tan_v = W * dNdv - N * dWdv;

        var n = cross(tan_u, tan_v);
        let len = length(n);
        if (len > 1e-7) {
            n = n / len;
        }

        var out: Vertex; // w is not guaranteed to be 1, normalize here
        out.pos  = vec4f(N / W, 1.0);
        out.norm = vec4f(n, 0.0);
        return out;
    }

    // Write a vertex into the output buffer
    fn write_vert(vi: u32, vert: Vertex, uv: vec2f, patch_idx: u32) {
        let base = vi * 4u;
        tg_vertsOut[base]      = vert.pos;
        tg_vertsOut[base + 1u] = vert.norm;
        tg_vertsOut[base + 2u] = vec4f(0.8, 0.85, 0.9, 1.0);  // default color
        tg_vertsOut[base + 3u] = vec4f(uv.x, uv.y, f32(patch_idx), f32(vi % 3u));
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

        if (tg_tessFactors[i] <= 0.0) { return; }
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

        var outer_i = 0u;
        var inner_i = 0u;

        // degenerate case
        if (IL0 == 1u && IL1 == 1u && OL0 == 1u && OL1 == 1u && OL2 == 1u && OL3 == 1u) {
            write_vert((tri_offset + k) * 3u,      eval_bicubic_vert(patch_offset, vec2f(0.0, 0.0)), vec2f(0.0, 0.0), i);
            write_vert((tri_offset + k) * 3u + 1u, eval_bicubic_vert(patch_offset, vec2f(0.0, 1.0)), vec2f(0.0, 1.0), i);
            write_vert((tri_offset + k) * 3u + 2u, eval_bicubic_vert(patch_offset, vec2f(1.0, 1.0)), vec2f(1.0, 1.0), i);
            write_vert((tri_offset + k + 1u) * 3u,      eval_bicubic_vert(patch_offset, vec2f(1.0, 1.0)), vec2f(1.0, 1.0), i);
            write_vert((tri_offset + k + 1u) * 3u + 1u, eval_bicubic_vert(patch_offset, vec2f(1.0, 0.0)), vec2f(1.0, 0.0), i);
            write_vert((tri_offset + k + 1u) * 3u + 2u, eval_bicubic_vert(patch_offset, vec2f(0.0, 0.0)), vec2f(0.0, 0.0), i);
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

                let uv_bl = vec2f(LEFT, BOTTOM);
                let uv_br = vec2f(RIGHT, BOTTOM);
                let uv_tl = vec2f(LEFT, TOP);
                let uv_tr = vec2f(RIGHT, TOP);
                let BL = eval_bicubic_vert(patch_offset, uv_bl);
                let BR = eval_bicubic_vert(patch_offset, uv_br);
                let TL = eval_bicubic_vert(patch_offset, uv_tl);
                let TR = eval_bicubic_vert(patch_offset, uv_tr);

                write_vert((tri_offset + k) * 3u,      BL, uv_bl, i);
                write_vert((tri_offset + k) * 3u + 1u, BR, uv_br, i);
                write_vert((tri_offset + k) * 3u + 2u, TR, uv_tr, i);
                k += 1u;
                write_vert((tri_offset + k) * 3u,      TR, uv_tr, i);
                write_vert((tri_offset + k) * 3u + 1u, TL, uv_tl, i);
                write_vert((tri_offset + k) * 3u + 2u, BL, uv_bl, i);
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
            var curr_outer_uv = st_to_uv(vec2f(0.0, outer_t), side);
            var curr_inner_uv = st_to_uv(vec2f(1.0 / f32(inner_max), inner_t), side);
            var curr_outer = eval_bicubic_vert(patch_offset, curr_outer_uv);
            var curr_inner = eval_bicubic_vert(patch_offset, curr_inner_uv);
            outer_i = 0u;
            inner_i = 1u;

            while (inner_i < inner_max - 1u || outer_i < outer_max) {
                let next_outer_s = select(1.0, f32(outer_i + 1u) / f32(outer_max), outer_i < outer_max);
                let next_inner_s = select(1.0, f32(inner_i + 1u) / f32(inner_max), inner_i < inner_max - 1u);

                if (outer_i < outer_max && (inner_i == inner_max - 1u || next_outer_s <= next_inner_s)) {
                    let next_outer_uv = st_to_uv(vec2f(f32(outer_i + 1u) / f32(outer_max), outer_t), side);
                    let next_outer = eval_bicubic_vert(patch_offset, next_outer_uv);
                    write_vert((tri_offset + k) * 3u,      curr_outer, curr_outer_uv, i);
                    write_vert((tri_offset + k) * 3u + 1u, curr_inner, curr_inner_uv, i);
                    write_vert((tri_offset + k) * 3u + 2u, next_outer, next_outer_uv, i);
                    k += 1u;
                    curr_outer = next_outer;
                    curr_outer_uv = next_outer_uv;
                    outer_i += 1u;
                } else {
                    let next_inner_uv = st_to_uv(vec2f(f32(inner_i + 1u) / f32(inner_max), inner_t), side);
                    let next_inner = eval_bicubic_vert(patch_offset, next_inner_uv);
                    write_vert((tri_offset + k) * 3u,      curr_outer, curr_outer_uv, i);
                    write_vert((tri_offset + k) * 3u + 1u, next_inner, next_inner_uv, i);
                    write_vert((tri_offset + k) * 3u + 2u, curr_inner, curr_inner_uv, i);
                    k += 1u;
                    curr_inner = next_inner;
                    curr_inner_uv = next_inner_uv;
                    inner_i += 1u;
                }
            }
        }
    }
