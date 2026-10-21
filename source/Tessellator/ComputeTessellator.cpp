#include "ComputeTessellator.h"

bool ComputeTessellator::Init(wgpu::Buffer ipass_levels) {
    if (!InitInput(ipass_levels) || !gen_pass.Init(device) || !scan_pass.Init(device))
        return false;

    const uint64_t capacity = GetMaxQuads();
    buf_tri_counts = utils::CreateBuffer(device, capacity * patch::TRI_COUNT_BYTES, wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst);
    buf_tri_offsets = utils::CreateBuffer(device, capacity * patch::TRI_OFFSET_BYTES, wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst);
    buf_block_sums = utils::CreateBuffer(device, 256 * sizeof(uint32_t), wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst);
    buf_bs_total = utils::CreateBuffer(device, sizeof(uint32_t), wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::CopySrc);

    buf_verts_out = utils::CreateBuffer(device, capacity * patch::VERTS_OUT_BYTES,
        wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopySrc | wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Vertex);

    scan_pass.Bind(buf_tri_counts, buf_tri_offsets, buf_block_sums, buf_bs_total);

    wgpu::BindGroup bg_counts = utils::CreateBindGroup(device, {
        utils::CreateBinding(0, GetTessFactors()),
        utils::CreateBinding(1, buf_tri_counts),
        utils::CreateBinding(2, GetConnectivity()),
    }, gen_pass.GetCountsBGL());

    wgpu::BindGroup bg_gen = utils::CreateBindGroup(device, {
        utils::CreateBinding(0, GetControlPointBuffer()),
        utils::CreateBinding(1, GetTessFactors()),
        utils::CreateBinding(2, buf_tri_offsets),
        utils::CreateBinding(3, GetConnectivity()),
        utils::CreateBinding(4, buf_verts_out),
    }, gen_pass.GetGenBGL());

    gen_pass.SetBindGroups(bg_counts, bg_gen);
    bg_counts.release();
    bg_gen.release();

    initialized = true;
    return true;
}

std::vector<wgpu::Buffer*> ComputeTessellator::Buffers() {
    return { &buf_tri_counts, &buf_tri_offsets, &buf_block_sums, &buf_bs_total, &buf_verts_out };
}

void ComputeTessellator::Upload(const glm::vec4* control_points, const uint32_t* indices, uint32_t num_quads) {
    UploadInput(control_points, indices, num_quads);
    utils::ClearBuffers(device, queue, Buffers());
}

bool ComputeTessellator::Execute(wgpu::CommandEncoder& encoder) {
    const uint32_t count = GetNumQuads();
    if (count == 0)
        return true;
    if (!ExecuteInput(encoder) || !gen_pass.ExecuteCounts(encoder, count))
        return false;

    utils::ClearBuffer(encoder, buf_block_sums);
    return scan_pass.Execute(encoder, count) && gen_pass.ExecuteGen(encoder, count);
}

void ComputeTessellator::Terminate() {
    utils::ReleaseBuffers(Buffers());
    TerminateInput();
}
