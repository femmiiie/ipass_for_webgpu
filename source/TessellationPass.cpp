#include "ipass/TessellationPass.h"
#include "ComputeTessellator.h"
#include "VertexTessellator.h"
#include "TessConstants.h"

#include <algorithm>
#include <iostream>

namespace ipass {

namespace {

template <class T>
Status UploadTo(T& tess, const PatchData& data, wgpu::Buffer levels_buffer)
{
    if (data.num_patches == 0) return Status::EmptyInput;

    // init tessellator on first upload
    if (!tess.IsInitialized()) {
        if (!tess.Init(levels_buffer)) {
            std::cerr << "[TessellationPass] Failed to initialize Tessellator." << std::endl;
            return Status::GPUInitFailed;
        }
    }

    uint32_t count = std::min(data.num_patches, tess.GetMaxQuads());

    tess.Upload(data.control_points.data(), data.corner_indices.data(), count);

    std::cout << "[TessellationPass] Uploaded " << count
              << " bicubic patch(es) for GPU tessellation." << std::endl;
    return Status::Success;
}

template <class T>
Status DispatchTo(T& tess, wgpu::CommandEncoder& encoder)
{
    if (!tess.IsInitialized() || tess.GetNumQuads() == 0) return Status::PatchesNotLoaded;
    tess.Execute(encoder);
    return Status::Success;
}

}

// GeometryTessellationPass

ComputeTessPass::ComputeTessPass(wgpu::Device device, wgpu::Queue queue, uint32_t max_patches)
    : impl(new ComputeTessellator(device, queue, patch::ResolveMax(device, max_patches, patch::GEOMETRY_BYTES))) {}

ComputeTessPass::~ComputeTessPass()
{
    impl->Terminate();
    delete impl;
}

Status ComputeTessPass::UploadPatches(const PatchData& data, wgpu::Buffer levels_buffer)
{
    return UploadTo(*impl, data, levels_buffer);
}

Status ComputeTessPass::Dispatch(wgpu::CommandEncoder& encoder)
{
    return DispatchTo(*impl, encoder);
}

wgpu::Buffer ComputeTessPass::GetVertexBuffer() const
{
    return impl->GetVertexOutput();
}

uint32_t ComputeTessPass::GetMaxVertexCount() const
{
    return impl->GetNumQuads() * patch::MAX_TRIS * 3;
}

wgpu::Buffer ComputeTessPass::GetControlPointBuffer() const
{
    return impl->GetControlPointBuffer();
}

wgpu::Buffer ComputeTessPass::GetTriCountBuffer() const
{
    return impl->GetTriCountBuffer();
}

uint32_t ComputeTessPass::GetPatchCount() const
{
    return impl->GetNumQuads();
}

// DirectTessellationPass

VertexTessPass::VertexTessPass(wgpu::Device device, wgpu::Queue queue, uint32_t max_patches)
    : impl(new VertexTessellator(device, queue, patch::ResolveMax(device, max_patches, patch::DIRECT_BYTES))) {}

VertexTessPass::~VertexTessPass()
{
    impl->Terminate(); // also releases the surface layout
    delete impl;
}

Status VertexTessPass::UploadPatches(const PatchData& data, wgpu::Buffer levels_buffer)
{
    return UploadTo(*impl, data, levels_buffer);
}

Status VertexTessPass::Dispatch(wgpu::CommandEncoder& encoder)
{
    return DispatchTo(*impl, encoder);
}

Status VertexTessPass::Draw(wgpu::RenderPassEncoder& pass)
{
    if (!impl->IsInitialized() || impl->GetNumQuads() == 0) return Status::PatchesNotLoaded;
    impl->Draw(pass, SURFACE_BIND_GROUP);
    return Status::Success;
}

wgpu::BindGroupLayout VertexTessPass::GetSurfaceBindGroupLayout() const
{
    return impl->GetSurfaceBindGroupLayout();
}

const std::string& VertexTessPass::GetSurfaceShaderSource()
{
    static const std::string source = VertexTessellator::SurfaceShaderSource(SURFACE_BIND_GROUP);
    return source;
}

wgpu::Buffer VertexTessPass::GetControlPointBuffer() const
{
    return impl->GetControlPointBuffer();
}

wgpu::Buffer VertexTessPass::GetTriCountBuffer() const
{
    return impl->GetTriCountBuffer();
}

uint32_t VertexTessPass::GetPatchCount() const
{
    return impl->GetNumQuads();
}

}
