#include "ipass/LODPass.h"
#include "IPass.h"
#include "Shader.h"

namespace ipass {

LODPass::LODPass(wgpu::Device device, wgpu::Queue queue, const Config& config)
{
    uint32_t maxPatches = config.max_patches ?
        config.max_patches :
        patch::ResolveMax(device);
    impl = new IPass(device, queue, maxPatches);
}

LODPass::~LODPass()
{
    delete impl;
}

Status LODPass::UploadPatches(const PatchData& data)
{
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

    impl->UploadPatches(verts);
    impl->SetPatchCount(data.num_patches);
    return Status::Success;
}

void LODPass::SetMVP(const glm::mat4& mvp)
{
    impl->SetMVP(mvp);
}

void LODPass::SetViewport(float width, float height)
{
    (void)height;
    impl->SetViewportWidth(width);
}

Status LODPass::Dispatch(wgpu::CommandEncoder& encoder)
{
    if (impl->GetPatchCount() == 0) return Status::PatchesNotLoaded;
    impl->Execute(encoder);
    return Status::Success;
}

wgpu::Buffer LODPass::GetLODBuffer() const
{
    return impl->GetOutputBuffer();
}

uint32_t LODPass::GetPatchCount() const
{
    return impl->GetPatchCount();
}

}
