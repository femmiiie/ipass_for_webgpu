    @group(0) @binding(0) var<storage, read>       ipassLevels : array<f32>;
    @group(0) @binding(1) var<storage, read_write> tessFactors : array<f32>;

    @compute @workgroup_size(256)
    fn tess_factor(@builtin(global_invocation_id) gid : vec3u) {
        let i = gid.x;
        if (i >= arrayLength(&tessFactors)) {
            return;
        }
        // ipass should guarantee this bound, but good to guard regardless
        tessFactors[i] = clamp(ipassLevels[i], 1.0, 64.0);
    }

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
