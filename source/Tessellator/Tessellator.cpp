#include "Tessellator.h"
#include "Connectivity.h"

bool Tessellator::Init(uint32_t max, wgpu::Buffer ipass_levels) {
    this->max_quads = max;

    if (!calc_pass.Init(context.device))
        return false;
    if (!scan_pass.Init(context.device))
        return false;
    if (!gen_pass.Init(context.device))
        return false;

    buf_quads = this->CreateBuffer((uint64_t)max_quads * 16 * sizeof(glm::vec4), wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst);
    buf_tess_factors = this->CreateBuffer((uint64_t)max_quads * sizeof(float), wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst);
    buf_tri_counts = this->CreateBuffer((uint64_t)max_quads * sizeof(uint32_t), wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst);
    buf_tri_offsets = this->CreateBuffer((uint64_t)max_quads * sizeof(uint32_t), wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst);
    buf_connectivity = this->CreateBuffer((uint64_t)max_quads * 2 * sizeof(glm::ivec4), wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst);
    buf_block_sums = this->CreateBuffer(256 * sizeof(uint32_t), wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst);
    buf_bs_total = this->CreateBuffer(sizeof(uint32_t), wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::CopySrc);
    // set output to be vertex buffer, can directly pass into scene
    // tess-gen writes 4 vec4 values per output vertex: pos, normal, color, uv/pad
    buf_verts_out = this->CreateBuffer((uint64_t)max_quads * tess::MAX_TRIS_PER_PATCH * 3 * 4 * sizeof(glm::vec4),
        wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopySrc | wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Vertex);

    {
        wgpu::BindGroupEntry tf_entries[2] = {
            this->CreateBinding(0, ipass_levels),
            this->CreateBinding(1, buf_tess_factors)
        };
        wgpu::BindGroupDescriptor desc = {};
        desc.layout = calc_pass.GetTessFactorBGL();
        desc.entryCount = 2;
        desc.entries = tf_entries;
        auto bg_tf = context.device.createBindGroup(desc);

        wgpu::BindGroupEntry cc_entries[3] = {
            this->CreateBinding(0, buf_tess_factors),
            this->CreateBinding(1, buf_tri_counts),
            this->CreateBinding(2, buf_connectivity)
        };
        desc.layout = calc_pass.GetCalcCountsBGL();
        desc.entryCount = 3;
        desc.entries = cc_entries;
        auto bg_cc = context.device.createBindGroup(desc);

        calc_pass.SetBindGroups(bg_tf, bg_cc);
        bg_tf.release();
        bg_cc.release();
    }

    {
        wgpu::BindGroupEntry l1_entries[3] = {
            this->CreateBinding(0, buf_tri_counts),
            this->CreateBinding(1, buf_tri_offsets),
            this->CreateBinding(2, buf_block_sums)
        };
        wgpu::BindGroupDescriptor desc = {};
        desc.layout = scan_pass.GetLevel1BGL();
        desc.entryCount = 3;
        desc.entries = l1_entries;
        auto bg_l1 = context.device.createBindGroup(desc);

        wgpu::BindGroupEntry l2_entries[2] = {
            this->CreateBinding(0, buf_block_sums),
            this->CreateBinding(1, buf_bs_total)
        };
        desc.layout = scan_pass.GetLevel2BGL();
        desc.entryCount = 2;
        desc.entries = l2_entries;
        auto bg_l2 = context.device.createBindGroup(desc);

        wgpu::BindGroupEntry comb_entries[2] = {
            this->CreateBinding(1, buf_tri_offsets),
            this->CreateBinding(2, buf_block_sums)
        };
        desc.layout = scan_pass.GetCombineBGL();
        desc.entryCount = 2;
        desc.entries = comb_entries;
        auto bg_comb = context.device.createBindGroup(desc);

        scan_pass.SetBindGroups(bg_l1, bg_l2, bg_comb);
        bg_l1.release();
        bg_l2.release();
        bg_comb.release();
    }

    {
        wgpu::BindGroupEntry entries[5] = {
            this->CreateBinding(0, buf_quads),
            this->CreateBinding(1, buf_tess_factors),
            this->CreateBinding(2, buf_tri_offsets),
            this->CreateBinding(3, buf_connectivity),
            this->CreateBinding(4, buf_verts_out),
        };
        wgpu::BindGroupDescriptor desc = {};
        desc.layout = gen_pass.GetBindGroupLayout();
        desc.entryCount = 5;
        desc.entries = entries;
        auto bg = context.device.createBindGroup(desc);

        gen_pass.SetBindGroup(bg);
        bg.release();
    }

    return true;
}

void Tessellator::ClearBuffers() {
    if (!context.device || !context.queue)
        return;

    wgpu::CommandEncoderDescriptor encoder_desc = {};
    encoder_desc.label = WGPU_STRING_VIEW_INIT;
    wgpu::CommandEncoder encoder = context.device.createCommandEncoder(encoder_desc);

    this->ClearBuffer(encoder, buf_quads);
    this->ClearBuffer(encoder, buf_tess_factors);
    this->ClearBuffer(encoder, buf_tri_counts);
    this->ClearBuffer(encoder, buf_tri_offsets);
    this->ClearBuffer(encoder, buf_connectivity);
    this->ClearBuffer(encoder, buf_block_sums);
    this->ClearBuffer(encoder, buf_bs_total);
    this->ClearBuffer(encoder, buf_verts_out);

    wgpu::CommandBufferDescriptor cmd_desc = {};
    cmd_desc.label = WGPU_STRING_VIEW_INIT;
    wgpu::CommandBuffer command = encoder.finish(cmd_desc);
    encoder.release();

    context.queue.submit(1, &command);
    command.release();
}

void Tessellator::Upload(const glm::vec4* control_points, const uint32_t* indices, uint32_t num_quads) {
    ClearBuffers();

    context.queue.writeBuffer(buf_quads, 0, control_points, (uint64_t)num_quads * 16 * sizeof(glm::vec4));

    std::vector<glm::ivec4> connectivity = buildQuadConnectivity(indices, num_quads);
    context.queue.writeBuffer(buf_connectivity, 0, connectivity.data(), (uint64_t)num_quads * 2 * sizeof(glm::ivec4));
}

bool Tessellator::Execute(wgpu::CommandEncoder encoder, uint32_t num_quads) {
    if (num_quads == 0)
        return true;
    if (!calc_pass.Execute(encoder, num_quads))
        return false;
    this->ClearBuffer(encoder, buf_block_sums);
    if (!scan_pass.Execute(encoder, num_quads) || !gen_pass.Execute(encoder, num_quads))
        return false;
    return true;
}

void Tessellator::Terminate() {
    buf_verts_out.release();
    buf_bs_total.release();
    buf_block_sums.release();
    buf_connectivity.release();
    buf_tri_offsets.release();
    buf_tri_counts.release();
    buf_tess_factors.release();
    buf_quads.release();
}
