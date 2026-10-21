#include "ipass/Pipeline.h"
#include "TessConstants.h"

namespace ipass {

namespace {

template <class Pass>
struct PipelineImpl {
    LODPass lod;
    Pass tess;
    bool patchesLoaded = false;

    PipelineImpl(wgpu::Device device, wgpu::Queue queue, uint32_t max_patches)
        : lod(device, queue, max_patches), tess(device, queue, max_patches) {}

    Status LoadPatches(const PatchData& data)
    {
        Status s = lod.UploadPatches(data);
        if (s != Status::Success) return s;

        s = tess.UploadPatches(data, lod.GetLODBuffer());
        if (s != Status::Success) return s;

        patchesLoaded = true;
        return Status::Success;
    }

    Status Execute(wgpu::CommandEncoder& encoder)
    {
        if (!patchesLoaded) return Status::PatchesNotLoaded;
        lod.Dispatch(encoder);
        tess.Dispatch(encoder);
        return Status::Success;
    }
};

}

struct ComputePipeline::Impl : PipelineImpl<ComputeTessPass> {
    using PipelineImpl::PipelineImpl;
};

struct DirectPipeline::Impl : PipelineImpl<VertexTessPass> {
    using PipelineImpl::PipelineImpl;
};

// GeometryPipeline

ComputePipeline::ComputePipeline(wgpu::Device device, wgpu::Queue queue, uint32_t max_patches)
    : impl(new Impl(device, queue, patch::ResolveMax(device, max_patches, patch::GEOMETRY_BYTES))) {}

ComputePipeline::~ComputePipeline() { delete impl; }

ComputePipeline::ComputePipeline(ComputePipeline&& other) noexcept : impl(other.impl)
{
    other.impl = nullptr;
}

ComputePipeline& ComputePipeline::operator=(ComputePipeline&& other) noexcept
{
    if (this != &other) {
        delete impl;
        impl = other.impl;
        other.impl = nullptr;
    }
    return *this;
}

Status ComputePipeline::LoadPatches(const PatchData& data)
{
    return impl ? impl->LoadPatches(data) : Status::NotInitialized;
}

void ComputePipeline::SetMVP(const glm::mat4& mvp)
{
    if (impl) impl->lod.SetMVP(mvp);
}

void ComputePipeline::SetViewport(float width, float height)
{
    if (impl) impl->lod.SetViewport(width, height);
}

Status ComputePipeline::DispatchLOD(wgpu::CommandEncoder& encoder)
{
    return impl ? impl->lod.Dispatch(encoder) : Status::NotInitialized;
}

Status ComputePipeline::DispatchTessellation(wgpu::CommandEncoder& encoder)
{
    return impl ? impl->tess.Dispatch(encoder) : Status::NotInitialized;
}

Status ComputePipeline::Execute(wgpu::CommandEncoder& encoder)
{
    return impl ? impl->Execute(encoder) : Status::NotInitialized;
}

wgpu::Buffer ComputePipeline::GetVertexBuffer() const
{
    return impl ? impl->tess.GetVertexBuffer() : nullptr;
}

uint32_t ComputePipeline::GetMaxVertexCount() const
{
    return impl ? impl->tess.GetMaxVertexCount() : 0;
}

wgpu::Buffer ComputePipeline::GetLODBuffer() const
{
    return impl ? impl->lod.GetLODBuffer() : nullptr;
}

wgpu::Buffer ComputePipeline::GetControlPointBuffer() const
{
    return impl ? impl->tess.GetControlPointBuffer() : nullptr;
}

wgpu::Buffer ComputePipeline::GetTriCountBuffer() const
{
    return impl ? impl->tess.GetTriCountBuffer() : nullptr;
}

uint32_t ComputePipeline::GetPatchCount() const
{
    return impl ? impl->tess.GetPatchCount() : 0;
}

LODPass& ComputePipeline::GetLOD()
{
    return impl->lod;
}

ComputeTessPass& ComputePipeline::GetTessellationPass()
{
    return impl->tess;
}

// DirectPipeline

DirectPipeline::DirectPipeline(wgpu::Device device, wgpu::Queue queue, uint32_t max_patches)
    : impl(new Impl(device, queue, patch::ResolveMax(device, max_patches, patch::DIRECT_BYTES))) {}

DirectPipeline::~DirectPipeline() { delete impl; }

DirectPipeline::DirectPipeline(DirectPipeline&& other) noexcept : impl(other.impl)
{
    other.impl = nullptr;
}

DirectPipeline& DirectPipeline::operator=(DirectPipeline&& other) noexcept
{
    if (this != &other) {
        delete impl;
        impl = other.impl;
        other.impl = nullptr;
    }
    return *this;
}

Status DirectPipeline::LoadPatches(const PatchData& data)
{
    return impl ? impl->LoadPatches(data) : Status::NotInitialized;
}

void DirectPipeline::SetMVP(const glm::mat4& mvp)
{
    if (impl) impl->lod.SetMVP(mvp);
}

void DirectPipeline::SetViewport(float width, float height)
{
    if (impl) impl->lod.SetViewport(width, height);
}

Status DirectPipeline::DispatchLOD(wgpu::CommandEncoder& encoder)
{
    return impl ? impl->lod.Dispatch(encoder) : Status::NotInitialized;
}

Status DirectPipeline::DispatchTessellation(wgpu::CommandEncoder& encoder)
{
    return impl ? impl->tess.Dispatch(encoder) : Status::NotInitialized;
}

Status DirectPipeline::Execute(wgpu::CommandEncoder& encoder)
{
    return impl ? impl->Execute(encoder) : Status::NotInitialized;
}

Status DirectPipeline::Draw(wgpu::RenderPassEncoder& pass)
{
    return impl ? impl->tess.Draw(pass) : Status::NotInitialized;
}

wgpu::BindGroupLayout DirectPipeline::GetSurfaceBindGroupLayout() const
{
    return impl ? impl->tess.GetSurfaceBindGroupLayout() : nullptr;
}

wgpu::Buffer DirectPipeline::GetLODBuffer() const
{
    return impl ? impl->lod.GetLODBuffer() : nullptr;
}

wgpu::Buffer DirectPipeline::GetControlPointBuffer() const
{
    return impl ? impl->tess.GetControlPointBuffer() : nullptr;
}

wgpu::Buffer DirectPipeline::GetTriCountBuffer() const
{
    return impl ? impl->tess.GetTriCountBuffer() : nullptr;
}

uint32_t DirectPipeline::GetPatchCount() const
{
    return impl ? impl->tess.GetPatchCount() : 0;
}

LODPass& DirectPipeline::GetLOD()
{
    return impl->lod;
}

VertexTessPass& DirectPipeline::GetTessellationPass()
{
    return impl->tess;
}

}
