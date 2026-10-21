    @group(0) @binding(0) var<storage, read>       ipassLevels : array<f32>;
    @group(0) @binding(1) var<storage, read_write> tessFactors : array<f32>;

    @compute @workgroup_size(256)
    fn tess_factor(@builtin(global_invocation_id) gid : vec3u) {
        let i = gid.x;
        if (i >= arrayLength(&tessFactors)) {
            return;
        }
        let level = ipassLevels[i];
        if (level <= 0.0) { tessFactors[i] = 0.0; return; }
        tessFactors[i] = clamp(level, 1.0, 64.0);
    }
