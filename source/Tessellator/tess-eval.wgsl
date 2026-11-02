    // iPASS direct surface evaluation.
    // include this in a vertex shader and call ipass_vertex(vertex_index, instance_index) from an
    // entry point drawn with TessellationPass::Draw. IPASS_GROUP is replaced with the bind group index.

    // must match patch::CHUNK_TRIS in TessConstants.h and CHUNK_TRIS in tess-direct.wgsl
    const IPASS_CHUNK_TRIS : u32 = 64u;

    @group(IPASS_GROUP) @binding(0) var<storage, read> ipass_controlPoints : array<vec4f>;
    @group(IPASS_GROUP) @binding(1) var<storage, read> ipass_patchInfo     : array<vec2u>;
    @group(IPASS_GROUP) @binding(2) var<storage, read> ipass_chunks        : array<u32>;

    struct IPassVertex {
        position  : vec4f, // object space, w = 1
        normal    : vec4f, // w = 0
        uv        : vec2f,
        patch_idx : u32,
        bary_id   : u32,   // corner within the triangle, 0..2
        valid     : bool,  // false for padding in a patch's last chunk, all 3 corners collapse to one point
    }

    struct IPassSurfacePoint {
        pos  : vec4f,
        norm : vec4f,
    }

    // corner order matches tess_gen in tess-gen.wgsl
    const ipass_quad_corners = array<vec2u, 6>(
        vec2u(0u, 0u), vec2u(1u, 0u), vec2u(1u, 1u), // BL, BR, TR
        vec2u(1u, 1u), vec2u(0u, 1u), vec2u(0u, 0u), // TR, TL, BL
    );

    const ipass_single_quad = array<vec2f, 6>(
        vec2f(0.0, 0.0), vec2f(0.0, 1.0), vec2f(1.0, 1.0),
        vec2f(1.0, 1.0), vec2f(1.0, 0.0), vec2f(0.0, 0.0),
    );

    fn bernstein3(t: f32) -> vec4f {
        let s = 1.0 - t;
        return vec4f(s * s * s, 3.0 * t * s * s, 3.0 * t * t * s, t * t * t);
    }

    fn bernstein3_deriv(t: f32) -> vec4f {
        let s  = 1.0 - t;
        let b2 = vec3f(s * s, 2.0 * t * s, t * t);
        return 3.0 * vec4f(-b2.x, b2.x - b2.y, b2.y - b2.z, b2.z);
    }

    // evaluate position analytic surface normal via partial derivatives.
    fn ipass_eval_bicubic(patch_offset: u32, uv: vec2f) -> IPassSurfacePoint {
        let bu  = bernstein3(uv.x);
        let bv  = bernstein3(uv.y);
        let dbu = bernstein3_deriv(uv.x);
        let dbv = bernstein3_deriv(uv.y);

        var pos  = vec4f(0.0);  // (N_x, N_y, N_z, W)
        var du_h = vec4f(0.0);  // d/du of (N_x, N_y, N_z, W)
        var dv_h = vec4f(0.0);  // d/dv of (N_x, N_y, N_z, W)
        for (var row = 0u; row < 4u; row++) {
            var r  = vec4f(0.0);
            var rd = vec4f(0.0);
            for (var col = 0u; col < 4u; col++) {
                let cp = ipass_controlPoints[patch_offset + row * 4u + col];
                r  += bv[col]  * cp;
                rd += dbv[col] * cp;
            }
            pos  += bu[row]  * r;
            du_h += dbu[row] * r;
            dv_h += bu[row]  * rd;
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

        var out: IPassSurfacePoint;
        out.pos  = vec4f(N / W, 1.0);
        out.norm = vec4f(n, 0.0);
        return out;
    }

    fn ipass_st_to_uv(st: vec2f, side: u32) -> vec2f {
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

    fn ipass_ratio(a: u32, b: u32) -> f32 {
        return f32(a) / f32(b);
    }

    // triangle of the inner grid, IL >= 2
    fn ipass_inner_uv(IL: u32, tri: u32, corner: u32) -> vec2f {
        let n  = IL - 2u;
        let q  = tri / 2u;
        let ii = q / n + 1u;
        let jj = q % n + 1u;
        let c  = ipass_quad_corners[(tri & 1u) * 3u + corner];
        return vec2f(ipass_ratio(ii + c.x, IL), ipass_ratio(jj + c.y, IL));
    }

    // triangle k of the trapezoid between outer edge `side` (OL segments) and the inner grid (IL - 2 segments).
    // tess_gen builds it by merging the two edges, taking the outer step on ties. here the step
    // count is found directly with a merge path binary search over how many outer steps precede step k.
    fn ipass_side_uv(side: u32, OL: u32, IL: u32, k: u32, corner: u32) -> vec2f {
        let n_inner = IL - 2u;
        var lo = select(0u, k - n_inner, k > n_inner);
        var hi = min(k, OL);
        loop {
            if (lo >= hi) {
                break;
            }
            let m = (lo + hi) / 2u;
            // inner step k - m - 1 strictly before outer step m
            if ((k - m + 1u) * OL < (m + 1u) * IL) {
                hi = m;
            } else {
                lo = m + 1u;
            }
        }

        let outer_i = lo;
        let inner_i = 1u + k - outer_i;
        let take_outer = outer_i < OL && (inner_i >= IL - 1u || (outer_i + 1u) * IL <= (inner_i + 1u) * OL);

        let inner_t = 1.0 / f32(IL);
        var st = vec2f(ipass_ratio(outer_i, OL), 0.0); // current outer
        if (take_outer) {
            if (corner == 1u) {
                st = vec2f(ipass_ratio(inner_i, IL), inner_t);
            } else if (corner == 2u) {
                st = vec2f(ipass_ratio(outer_i + 1u, OL), 0.0);
            }
        } else {
            if (corner == 1u) {
                st = vec2f(ipass_ratio(inner_i + 1u, IL), inner_t);
            } else if (corner == 2u) {
                st = vec2f(ipass_ratio(inner_i, IL), inner_t);
            }
        }
        return ipass_st_to_uv(st, side);
    }

    // parametric coordinate of a corner of triangle `tri`, in the same order tess_gen emits triangles
    fn ipass_tri_uv(info: vec2u, tri: u32, corner: u32) -> vec2f {
        let IL = info.x & 0xFFu;
        let OL = vec4u((info.x >> 8u) & 0xFFu, (info.x >> 16u) & 0xFFu, info.x >> 24u, info.y & 0xFFu);

        if (IL == 1u && all(OL == vec4u(1u))) {
            return ipass_single_quad[tri * 3u + corner];
        }

        let ILe = max(IL, 2u);
        let inner_tris = 2u * (ILe - 2u) * (ILe - 2u);
        if (tri < inner_tris) {
            return ipass_inner_uv(ILe, tri, corner);
        }

        var k = tri - inner_tris;
        for (var side = 0u; side < 3u; side++) {
            let n = OL[side] + ILe - 2u;
            if (k < n) {
                return ipass_side_uv(side, OL[side], ILe, k, corner);
            }
            k -= n;
        }
        return ipass_side_uv(3u, OL[3], ILe, k, corner);
    }

    fn ipass_vertex(vertex_index: u32, instance_index: u32) -> IPassVertex {
        let chunk     = ipass_chunks[instance_index];
        let patch_idx = chunk >> 7u;
        let tri       = (chunk & 127u) * IPASS_CHUNK_TRIS + vertex_index / 3u;
        let corner    = vertex_index % 3u;
        let info      = ipass_patchInfo[patch_idx];

        var out: IPassVertex;
        out.patch_idx = patch_idx;
        out.bary_id   = corner;
        out.valid     = tri < (info.y >> 8u);
        if (!out.valid) {
            out.position = vec4f(0.0, 0.0, 0.0, 1.0);
            out.normal   = vec4f(0.0);
            out.uv       = vec2f(0.0);
            return out;
        }

        let uv = ipass_tri_uv(info, tri, corner);
        let p  = ipass_eval_bicubic(patch_idx * 16u, uv);
        out.position = p.pos;
        out.normal   = p.norm;
        out.uv       = uv;
        return out;
    }
