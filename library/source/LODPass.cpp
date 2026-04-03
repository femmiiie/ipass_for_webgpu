#include "ipass/LODPass.h"
#include "GPUContext.h"
#include "IPass.h"
#include "Shader.h"

namespace ipass {

struct LODPass::Impl {
    GPUContext gpu_ctx;
    IPass pass;
    uint32_t patchCount = 0;

    Impl(wgpu::Device device, wgpu::Queue queue)
        : gpu_ctx{device, queue}, pass(gpu_ctx) {}
};

LODPass::LODPass(wgpu::Device device, wgpu::Queue queue, const Config& config)
{
    (void)config; // shader_base_path handled by ipass internally, fix later
    impl = new Impl(device, queue);
}

LODPass::~LODPass()
{
    delete impl;
}

LODPass::LODPass(LODPass&& other) noexcept : impl(other.impl)
{
    other.impl = nullptr;
}

LODPass& LODPass::operator=(LODPass&& other) noexcept
{
    if (this != &other) {
        delete impl;
        impl = other.impl;
        other.impl = nullptr;
    }
    return *this;
}

Status LODPass::UploadPatches(const PatchData& data)
{
    if (!impl) return Status::NotInitialized;
    if (data.num_patches == 0) return Status::EmptyInput;

    // convert PatchData to Vertex3D for IPass
    std::vector<utils::Vertex3D> verts;
    uint32_t totalVerts = data.num_patches * 16;
    verts.resize(totalVerts);
    for (uint32_t i = 0; i < totalVerts && i < data.control_points.size(); i++) {
        verts[i].pos = data.control_points[i];
        verts[i].color = glm::vec4(1.0f);
        verts[i].tex = glm::vec2(0.0f);
        verts[i]._pad = glm::vec2(0.0f);
    }

    impl->pass.UploadVertices(verts);
    impl->patchCount = data.num_patches;
    return Status::Success;
}

void LODPass::SetMVP(const glm::mat4& mvp)
{
    if (impl) impl->pass.SetMVP(mvp);
}

void LODPass::SetViewport(float width, float height)
{
    (void)height;
    if (impl) impl->pass.SetViewportWidth(width);
}

Status LODPass::Dispatch(wgpu::CommandEncoder& encoder)
{
    if (!impl) return Status::NotInitialized;
    if (impl->patchCount == 0) return Status::PatchesNotLoaded;
    impl->pass.Execute(encoder);
    return Status::Success;
}

wgpu::Buffer LODPass::GetLODBuffer() const
{
    if (!impl) return nullptr;
    return impl->pass.GetOutputBuffer();
}

uint32_t LODPass::GetPatchCount() const
{
    return impl ? impl->patchCount : 0;
}

}
