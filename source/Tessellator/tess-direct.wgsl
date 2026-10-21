    // must match patch::CHUNK_TRIS in TessConstants.h and IPASS_CHUNK_TRIS in tess-eval.wgsl
    const CHUNK_TRIS : u32 = 64u;

    @group(0) @binding(0) var<storage, read>       cd_tessFactors  : array<f32>;
    @group(0) @binding(1) var<storage, read>       cd_connectivity : array<vec4<i32>>;
    @group(0) @binding(2) var<storage, read_write> cd_patchInfo    : array<vec2u>;
    @group(0) @binding(3) var<storage, read_write> cd_chunkCount   : array<u32>;
    @group(0) @binding(4) var<storage, read_write> cd_triTotal     : array<atomic<u32>>;
    @group(0) @binding(5) var<uniform>             cd_numQuads     : u32;

    fn outer_level(this_level: f32, nb: i32) -> u32 {
        if (nb < 0) {
            return u32(ceil(this_level));
        }
        return u32(ceil(max(this_level, cd_tessFactors[u32(nb)])));
    }

    // per patch levels and triangle count, packed for the vertex stage:
    //   x = IL | OL0 << 8 | OL1 << 16 | OL2 << 24
    //   y = OL3 | tri_count << 8
    @compute @workgroup_size(256)
    fn calc_direct(@builtin(global_invocation_id) gid : vec3u) {
        let i = gid.x;
        if (i >= arrayLength(&cd_chunkCount)) {
            return;
        }

        let this_level = cd_tessFactors[i];
        if (i >= cd_numQuads || this_level <= 0.0) {
            cd_patchInfo[i] = vec2u(0u);
            cd_chunkCount[i] = 0u;
            return;
        }

        let nb = cd_connectivity[i * 2u + 0u];
        let IL  = u32(ceil(this_level));
        let OL0 = outer_level(this_level, nb.x);
        let OL1 = outer_level(this_level, nb.y);
        let OL2 = outer_level(this_level, nb.z);
        let OL3 = outer_level(this_level, nb.w);

        // a fully level 1 patch is a single quad
        var tris = 2u;
        if (IL != 1u || OL0 != 1u || OL1 != 1u || OL2 != 1u || OL3 != 1u) {
            let ILe = max(IL, 2u);
            tris = 2u * (ILe - 2u) * (ILe - 2u) + OL0 + OL1 + OL2 + OL3 + 4u * (ILe - 2u);
        }

        cd_patchInfo[i] = vec2u(IL | (OL0 << 8u) | (OL1 << 16u) | (OL2 << 24u), OL3 | (tris << 8u));
        cd_chunkCount[i] = (tris + CHUNK_TRIS - 1u) / CHUNK_TRIS;
        atomicAdd(&cd_triTotal[0], tris);
    }

    @group(0) @binding(0) var<storage, read>       ec_chunkCount  : array<u32>;
    @group(0) @binding(1) var<storage, read>       ec_chunkOffset : array<u32>;
    @group(0) @binding(2) var<storage, read>       ec_chunkTotal  : array<u32>;
    @group(0) @binding(3) var<storage, read_write> ec_chunks      : array<u32>;
    @group(0) @binding(4) var<storage, read_write> ec_drawArgs    : array<u32>;

    // one draw instance per chunk of CHUNK_TRIS triangles, record = patch << 7 | chunk
    @compute @workgroup_size(256)
    fn emit_chunks(@builtin(global_invocation_id) gid : vec3u) {
        let i = gid.x;
        if (i == 0u) { // drawIndirect args: vertexCount, instanceCount, firstVertex, firstInstance
            ec_drawArgs[0] = CHUNK_TRIS * 3u;
            ec_drawArgs[1] = ec_chunkTotal[0];
            ec_drawArgs[2] = 0u;
            ec_drawArgs[3] = 0u;
        }
        if (i >= arrayLength(&ec_chunkCount)) {
            return;
        }

        let count = ec_chunkCount[i];
        let offset = ec_chunkOffset[i];
        for (var c = 0u; c < count; c++) {
            ec_chunks[offset + c] = (i << 7u) | c;
        }
    }
