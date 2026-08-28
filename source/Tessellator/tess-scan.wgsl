    @group(0) @binding(0) var<storage, read> triCount : array<u32>;
    @group(0) @binding(1) var<storage, read_write> triOffset : array<u32>;
    @group(0) @binding(2) var<storage, read_write> blockSums : array<u32>;

    struct WG256 { data: array<u32, 256> }
    var<workgroup> shared_ls : WG256;

    // level 1 scan
    // lid.x = thread id within workgroup
    // wid.x = workgroup id
    // gid.x = wid.x*256 + lid.x
    @compute @workgroup_size(256)
    fn lvl1_scan (@builtin(global_invocation_id) gid : vec3u, @builtin(local_invocation_id) lid : vec3u, @builtin(workgroup_id) wid : vec3u) {
        let i = gid.x;
        let n = arrayLength(&triCount);

        shared_ls.data[lid.x] = select(0u, triCount[i], i < n);

        workgroupBarrier();

        // up sweep
        var stride = 1u;
        // it looks like while(stride < 256u) would be unsafe for internal sync
        loop {
            if (stride >= 256u) {
                break;
            }
            if ((lid.x + 1u) % (stride * 2u) == 0u) {
                let rhs_ls = shared_ls.data[lid.x - stride];
                shared_ls.data[lid.x] += rhs_ls;
            }
            stride *= 2u;
            workgroupBarrier();
        }

        if (lid.x == 255u) {
            blockSums[wid.x] = shared_ls.data[255u];
            shared_ls.data[255u] = 0u;
        }
        workgroupBarrier();

        // down sweep
        stride = 128u;
        loop {
            if (stride == 0u) {
                break;
            }
            if ((lid.x + 1u) % (stride * 2u) == 0u) {
                let tmp = shared_ls.data[lid.x];
                let rhs_ls = shared_ls.data[lid.x - stride];
                shared_ls.data[lid.x] = tmp + rhs_ls;
                shared_ls.data[lid.x - stride] = tmp;
            }
            stride /= 2u;
            workgroupBarrier();
        }

        if (i < n) {
            triOffset[i] = shared_ls.data[lid.x];
        }
    }

    // level 2 scan

    @group(0) @binding(0) var<storage, read_write> bs_blockSums : array<u32>;
    @group(0) @binding(1) var<storage, read_write> bs_total : array<u32>;

    var<workgroup> shared_bs : WG256;

    @compute @workgroup_size(256)
    fn lvl2_scan(@builtin(local_invocation_id) lid : vec3u) {
        shared_bs.data[lid.x] = bs_blockSums[lid.x];
        workgroupBarrier();

        // upsweep
        var stride = 1u;
        loop {
            if (stride >= 256u) {
                break;
            }
            if ((lid.x + 1u) % (stride * 2u) == 0u) {
                let rhs_bs = shared_bs.data[lid.x - stride];
                shared_bs.data[lid.x] += rhs_bs;
            }
            stride *= 2u;
            workgroupBarrier();
        }

        if (lid.x == 255u) {
            bs_total[0] = shared_bs.data[255u]; // need sum since this is level 2
            shared_bs.data[255u] = 0u;
        }

        workgroupBarrier();

        // downsweep
        stride = 128u;
        loop {
            if (stride == 0u) {
                break;
            }
            if ((lid.x + 1u) % (stride * 2u) == 0u) {
                let tmp = shared_bs.data[lid.x];
                let rhs_bs = shared_bs.data[lid.x - stride];
                shared_bs.data[lid.x] = tmp + rhs_bs;
                shared_bs.data[lid.x - stride] = tmp;
            }
            stride /= 2u;
            workgroupBarrier();
        }

        bs_blockSums[lid.x] = shared_bs.data[lid.x];
    }

    // combine

    @compute @workgroup_size(256)
    fn comb(@builtin(global_invocation_id) gid : vec3u, @builtin(workgroup_id) wid : vec3u) {
        let i = gid.x;
        if (i >= arrayLength(&triOffset)) {
            return;
        }
        triOffset[i] += blockSums[wid.x];
    }
