#include "Tessellator.h"
#include "Connectivity.h"

bool Tessellator::InitInput(wgpu::Buffer ipass_levels) {
    if (!calc_pass.Init(device))
        return false;

    buf_quads = utils::CreateBuffer(device, (uint64_t)max_quads * patch::QUAD_BYTES, wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst);
    buf_tess_factors = utils::CreateBuffer(device, (uint64_t)max_quads * patch::TESS_FACTOR_BYTES, wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst);
    buf_connectivity = utils::CreateBuffer(device, (uint64_t)max_quads * patch::CONNECTIVITY_BYTES, wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst);

    wgpu::BindGroup bg = utils::CreateBindGroup(device, {
        utils::CreateBinding(0, ipass_levels),
        utils::CreateBinding(1, buf_tess_factors),
    }, calc_pass.GetBindGroupLayout());
    calc_pass.SetBindGroup(bg);
    bg.release();

    return true;
}

std::vector<wgpu::Buffer*> Tessellator::InputBuffers() {
    return { &buf_quads, &buf_tess_factors, &buf_connectivity };
}

void Tessellator::UploadInput(const glm::vec4* control_points, const uint32_t* indices, uint32_t count) {
    utils::ClearBuffers(device, queue, InputBuffers());

    num_quads = count;

    queue.writeBuffer(buf_quads, 0, control_points, (uint64_t)count * 16 * sizeof(glm::vec4));

    std::vector<glm::ivec4> connectivity = buildQuadConnectivity(indices, count);
    queue.writeBuffer(buf_connectivity, 0, connectivity.data(), (uint64_t)count * 2 * sizeof(glm::ivec4));
}

bool Tessellator::ExecuteInput(wgpu::CommandEncoder& encoder) {
    return calc_pass.Execute(encoder, num_quads);
}

void Tessellator::TerminateInput() {
    utils::ReleaseBuffers(InputBuffers());
}
