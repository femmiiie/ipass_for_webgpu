#pragma once

#include "tess-calc.h"
#include "tess-scan.h"
#include "tess-gen.h"
#include "tess-connectivity.h"

static wgpu::Buffer create_buffer(uint64_t size, wgpu::BufferUsage usage, const wgpu::Device &device) {
    wgpu::BufferDescriptor desc = {};
    desc.size = size;
    desc.usage = usage;
    return device.createBuffer(desc);
}

static wgpu::BindGroupEntry create_bindgroupentry(uint32_t binding, wgpu::Buffer buf) {
    wgpu::BindGroupEntry entry = {};
    entry.binding = binding;
    entry.buffer = buf;
    entry.offset = 0;
    entry.size = WGPU_WHOLE_SIZE;
    return entry;
}

class Tesselator {
    wgpu::Device device;
    wgpu::Queue queue;

    TessCalcPass calc_pass;
    TessScanPass scan_pass;
    TessGenPass gen_pass;

    wgpu::Buffer buf_quads;
    wgpu::Buffer buf_tess_factors;
    wgpu::Buffer buf_tri_counts;
    wgpu::Buffer buf_tri_offsets;
    wgpu::Buffer buf_connectivity;
    wgpu::Buffer buf_block_sums;
    wgpu::Buffer buf_bs_total;
    wgpu::Buffer buf_verts_out;

    uint32_t max_quads = 0;

public:
    static constexpr uint32_t MAX_TRIS_PER_PATCH = 8192;

    bool init(wgpu::Device device, wgpu::Queue queue, uint32_t max_quads, wgpu::Buffer ipass_levels);
    void upload(const glm::vec4* control_points, const uint32_t* indices, uint32_t num_quads);
    bool exec(wgpu::CommandEncoder encoder, uint32_t num_quads);
    wgpu::Buffer get_verts_out() const { return buf_verts_out; }
    void terminate();
};

bool Tesselator::init(wgpu::Device dev, wgpu::Queue que, uint32_t max, wgpu::Buffer ipass_levels) {
    this->device = dev;
    this->queue = que;
    this->max_quads = max;

    if (!calc_pass.init(device))
        return false;
    if (!scan_pass.init(device))
        return false;
    if (!gen_pass.init(device)) 
        return false;

    buf_quads = create_buffer((uint64_t)max_quads * 16 * sizeof(glm::vec4), wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst, this->device);
    buf_tess_factors = create_buffer((uint64_t)max_quads * sizeof(float), wgpu::BufferUsage::Storage, this->device);
    buf_tri_counts = create_buffer((uint64_t)max_quads * sizeof(uint32_t), wgpu::BufferUsage::Storage, this->device);
    buf_tri_offsets = create_buffer((uint64_t)max_quads * sizeof(uint32_t), wgpu::BufferUsage::Storage, this->device);
    buf_connectivity = create_buffer((uint64_t)max_quads * 2 * sizeof(glm::ivec4), wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst, this->device);
    buf_block_sums = create_buffer(256 * sizeof(uint32_t), wgpu::BufferUsage::Storage, this->device);
    buf_bs_total = create_buffer(sizeof(uint32_t), wgpu::BufferUsage::Storage, this->device);
    // set output to be vertex buffer, can directly pass into scene
    buf_verts_out = create_buffer((uint64_t)max_quads * MAX_TRIS_PER_PATCH * 3 * 2 * sizeof(glm::vec4),
        wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopySrc | wgpu::BufferUsage::Vertex, this->device);

    {
        wgpu::BindGroupEntry tf_entries[2] = {
            create_bindgroupentry(0, ipass_levels),
            create_bindgroupentry(1, buf_tess_factors)
        };
        wgpu::BindGroupDescriptor desc = {};
        desc.layout = calc_pass.get_tess_factor_bgl();
        desc.entryCount = 2;
        desc.entries = tf_entries;
        auto bg_tf = device.createBindGroup(desc);

        wgpu::BindGroupEntry cc_entries[3] = {
            create_bindgroupentry(0, buf_tess_factors),
            create_bindgroupentry(1, buf_tri_counts),
            create_bindgroupentry(2, buf_connectivity)
        };
        desc.layout = calc_pass.get_calc_counts_bgl();
        desc.entryCount = 3;
        desc.entries = cc_entries;
        auto bg_cc = device.createBindGroup(desc);

        calc_pass.set_bindgroups(bg_tf, bg_cc);
        bg_tf.release();
        bg_cc.release();
    }

    {
        wgpu::BindGroupEntry l1_entries[3] = {
            create_bindgroupentry(0, buf_tri_counts),
            create_bindgroupentry(1, buf_tri_offsets),
            create_bindgroupentry(2, buf_block_sums)
        };
        wgpu::BindGroupDescriptor desc = {};
        desc.layout = scan_pass.get_lvl1_bgl();
        desc.entryCount = 3;
        desc.entries = l1_entries;
        auto bg_l1 = device.createBindGroup(desc);

        wgpu::BindGroupEntry l2_entries[2] = {
            create_bindgroupentry(0, buf_block_sums),
            create_bindgroupentry(1, buf_bs_total)
        };
        desc.layout = scan_pass.get_lvl2_bgl();
        desc.entryCount = 2;
        desc.entries = l2_entries;
        auto bg_l2 = device.createBindGroup(desc);

        wgpu::BindGroupEntry comb_entries[2] = {
            create_bindgroupentry(1, buf_tri_offsets),
            create_bindgroupentry(2, buf_block_sums)
        };
        desc.layout = scan_pass.get_comb_bgl();
        desc.entryCount = 2;
        desc.entries = comb_entries;
        auto bg_comb = device.createBindGroup(desc);

        scan_pass.set_bindgroups(bg_l1, bg_l2, bg_comb);
        bg_l1.release();
        bg_l2.release();
        bg_comb.release();
    }

    {
        wgpu::BindGroupEntry entries[5] = {
            create_bindgroupentry(0, buf_quads),
            create_bindgroupentry(1, buf_tess_factors),
            create_bindgroupentry(2, buf_tri_offsets),
            create_bindgroupentry(3, buf_connectivity),
            create_bindgroupentry(4, buf_verts_out),
        };
        wgpu::BindGroupDescriptor desc = {};
        desc.layout = gen_pass.get_bgl();
        desc.entryCount = 5;
        desc.entries = entries;
        auto bg = device.createBindGroup(desc);

        gen_pass.set_bindgroup(bg);
        bg.release();
    }

    return true;
}

void Tesselator::upload(const glm::vec4* control_points, const uint32_t* indices, uint32_t num_quads) {
    queue.writeBuffer(buf_quads, 0, control_points, (uint64_t)num_quads * 16 * sizeof(glm::vec4));

    std::vector<glm::ivec4> connectivity = buildQuadConnectivity(indices, num_quads);
    queue.writeBuffer(buf_connectivity, 0, connectivity.data(), (uint64_t)num_quads * 2 * sizeof(glm::ivec4));
}

bool Tesselator::exec(wgpu::CommandEncoder encoder, uint32_t num_quads) {
    if (num_quads == 0)
        return true;
    if (!calc_pass.exec(encoder, num_quads) || !scan_pass.exec(encoder, num_quads) || !gen_pass.exec(encoder, num_quads))
        return false;
    return true;
}

void Tesselator::terminate() {
    buf_verts_out.release();
    buf_bs_total.release();
    buf_block_sums.release();
    buf_connectivity.release();
    buf_tri_offsets.release();
    buf_tri_counts.release();
    buf_tess_factors.release();
    buf_quads.release();
    queue.release();
    device.release();
}
