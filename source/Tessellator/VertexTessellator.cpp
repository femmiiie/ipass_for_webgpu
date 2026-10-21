#include "VertexTessellator.h"
#include "Shader.h"

VertexTessellator::VertexTessellator(wgpu::Device device, wgpu::Queue queue, uint32_t max_quads)
    : Tessellator(device, queue, max_quads)
{
    // bound by user pipeline, so layout is explicit not derived
    const wgpu::ShaderStage stages = wgpu::ShaderStage::Vertex | wgpu::ShaderStage::Compute;
    surface_bgl = utils::CreateBindGroupLayout(device, {
        utils::CreateBufferLayout(0, stages, wgpu::BufferBindingType::ReadOnlyStorage, 0),
        utils::CreateBufferLayout(1, stages, wgpu::BufferBindingType::ReadOnlyStorage, 0),
        utils::CreateBufferLayout(2, stages, wgpu::BufferBindingType::ReadOnlyStorage, 0),
    });
}

bool VertexTessellator::Init(wgpu::Buffer ipass_levels) {
    if (!InitInput(ipass_levels) || !direct_pass.Init(device) || !chunk_scan_pass.Init(device))
        return false;

    const uint64_t capacity = GetMaxQuads();
    const wgpu::BufferUsage storage = wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst;

    buf_direct_params = utils::CreateBuffer(device, 16, wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst);
    buf_patch_info = utils::CreateBuffer(device, capacity * patch::PATCH_INFO_BYTES, storage);
    buf_chunk_counts = utils::CreateBuffer(device, capacity * patch::CHUNK_COUNT_BYTES, storage);
    buf_chunk_offsets = utils::CreateBuffer(device, capacity * patch::CHUNK_OFFSET_BYTES, storage);
    buf_chunk_block_sums = utils::CreateBuffer(device, 256 * sizeof(uint32_t), storage);
    buf_chunk_total = utils::CreateBuffer(device, sizeof(uint32_t), storage);
    buf_chunks = utils::CreateBuffer(device, capacity * patch::CHUNKS_BYTES, storage);
    buf_draw_args = utils::CreateBuffer(device, 4 * sizeof(uint32_t), storage | wgpu::BufferUsage::Indirect | wgpu::BufferUsage::CopySrc);
    buf_tri_total = utils::CreateBuffer(device, sizeof(uint32_t), storage | wgpu::BufferUsage::CopySrc);

    chunk_scan_pass.Bind(buf_chunk_counts, buf_chunk_offsets, buf_chunk_block_sums, buf_chunk_total);

    wgpu::BindGroup bg_calc = utils::CreateBindGroup(device, {
        utils::CreateBinding(0, GetTessFactors()),
        utils::CreateBinding(1, GetConnectivity()),
        utils::CreateBinding(2, buf_patch_info),
        utils::CreateBinding(3, buf_chunk_counts),
        utils::CreateBinding(4, buf_tri_total),
        utils::CreateBinding(5, buf_direct_params),
    }, direct_pass.GetCalcBGL());

    wgpu::BindGroup bg_emit = utils::CreateBindGroup(device, {
        utils::CreateBinding(0, buf_chunk_counts),
        utils::CreateBinding(1, buf_chunk_offsets),
        utils::CreateBinding(2, buf_chunk_total),
        utils::CreateBinding(3, buf_chunks),
        utils::CreateBinding(4, buf_draw_args),
    }, direct_pass.GetEmitBGL());

    direct_pass.SetBindGroups(bg_calc, bg_emit);
    bg_calc.release();
    bg_emit.release();

    surface_bg = utils::CreateBindGroup(device, {
        utils::CreateBinding(0, GetControlPointBuffer()),
        utils::CreateBinding(1, buf_patch_info),
        utils::CreateBinding(2, buf_chunks),
    }, surface_bgl);

    initialized = true;
    return true;
}

std::vector<wgpu::Buffer*> VertexTessellator::Buffers() {
    return {
        &buf_direct_params, &buf_patch_info, &buf_chunk_counts, &buf_chunk_offsets, &buf_chunk_block_sums,
        &buf_chunk_total, &buf_chunks, &buf_draw_args, &buf_tri_total,
    };
}

void VertexTessellator::Upload(const glm::vec4* control_points, const uint32_t* indices, uint32_t num_quads) {
    UploadInput(control_points, indices, num_quads);
    utils::ClearBuffers(device, queue, Buffers());

    uint32_t params[4] = {num_quads, 0, 0, 0};
    queue.writeBuffer(buf_direct_params, 0, params, sizeof(params));
}

bool VertexTessellator::Execute(wgpu::CommandEncoder& encoder) {
    const uint32_t count = GetNumQuads();
    if (count == 0)
        return true;
    if (!ExecuteInput(encoder))
        return false;

    utils::ClearBuffer(encoder, buf_tri_total);
    if (!direct_pass.ExecuteCalc(encoder, count))
        return false;
    utils::ClearBuffer(encoder, buf_chunk_block_sums);
    return chunk_scan_pass.Execute(encoder, count) && direct_pass.ExecuteEmit(encoder, count);
}

bool VertexTessellator::Draw(wgpu::RenderPassEncoder& pass, uint32_t group) {
    if (!IsInitialized())
        return false;
    pass.setBindGroup(group, surface_bg, 0, nullptr);
    pass.drawIndirect(buf_draw_args, 0);
    return true;
}

std::string VertexTessellator::SurfaceShaderSource(uint32_t group) {
    static const std::string token = "IPASS_GROUP";
    std::string source = embedded_shaders::tess_eval;
    const std::string index = std::to_string(group);
    for (size_t pos = source.find(token); pos != std::string::npos; pos = source.find(token, pos + index.size()))
        source.replace(pos, token.size(), index);
    return source;
}

void VertexTessellator::Terminate() {
    utils::ReleaseBuffers(Buffers());
    TerminateInput();
    if (surface_bg) {
        surface_bg.release();
        surface_bg = nullptr;
    }
    if (surface_bgl) {
        surface_bgl.release();
        surface_bgl = nullptr;
    }
}
