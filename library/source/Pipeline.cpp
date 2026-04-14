#include "ipass/Pipeline.h"
#include "TessConstants.h"
#include <algorithm>

namespace ipass {

struct Pipeline::Impl {
    LODPass lod;
    TessellationPass tess;
    bool patchesLoaded = false;

    Impl(wgpu::Device device, wgpu::Queue queue, const Config& config)
        : lod(device, queue, config), tess(device, queue, config) {}
};

Pipeline::Pipeline(wgpu::Device device, wgpu::Queue queue, const Config& config)
{
    Config resolved = config;
    if (resolved.max_patches == 0) {
        wgpu::Limits limits = wgpu::Default;
        if (device.getLimits(&limits))
            resolved.max_patches = tess::ComputeMaxPatches(std::min<uint64_t>(
                limits.maxBufferSize, limits.maxStorageBufferBindingSize));
        else
            resolved.max_patches = 64;
    }
    impl = new Impl(device, queue, resolved);
}

Pipeline::~Pipeline()
{
    delete impl;
}

Pipeline::Pipeline(Pipeline&& other) noexcept : impl(other.impl)
{
    other.impl = nullptr;
}

Pipeline& Pipeline::operator=(Pipeline&& other) noexcept
{
    if (this != &other) {
        delete impl;
        impl = other.impl;
        other.impl = nullptr;
    }
    return *this;
}

Status Pipeline::LoadPatches(const PatchData& data)
{
    if (!impl) return Status::NotInitialized;

    Status s = impl->lod.UploadPatches(data);
    if (s != Status::Success) return s;

    s = impl->tess.UploadPatches(data, impl->lod.GetLODBuffer());
    if (s != Status::Success) return s;

    impl->patchesLoaded = true;
    return Status::Success;
}

void Pipeline::SetMVP(const glm::mat4& mvp)
{
    if (impl) impl->lod.SetMVP(mvp);
}

void Pipeline::SetViewport(float width, float height)
{
    if (impl) impl->lod.SetViewport(width, height);
}

Status Pipeline::DispatchLOD(wgpu::CommandEncoder& encoder)
{
    if (!impl) return Status::NotInitialized;
    return impl->lod.Dispatch(encoder);
}

Status Pipeline::DispatchTessellation(wgpu::CommandEncoder& encoder)
{
    if (!impl) return Status::NotInitialized;
    return impl->tess.Dispatch(encoder);
}

Status Pipeline::Execute(wgpu::CommandEncoder& encoder)
{
    if (!impl) return Status::NotInitialized;
    if (!impl->patchesLoaded) return Status::PatchesNotLoaded;
    impl->lod.Dispatch(encoder);
    impl->tess.Dispatch(encoder);
    return Status::Success;
}

wgpu::Buffer Pipeline::GetVertexBuffer() const
{
    if (!impl) return nullptr;
    return impl->tess.GetVertexBuffer();
}

uint32_t Pipeline::GetMaxVertexCount() const
{
    return impl ? impl->tess.GetMaxVertexCount() : 0;
}

wgpu::Buffer Pipeline::GetLODBuffer() const
{
    if (!impl) return nullptr;
    return impl->lod.GetLODBuffer();
}

LODPass& Pipeline::GetLOD()
{
    return impl->lod;
}

TessellationPass& Pipeline::GetTessellationPass()
{
    return impl->tess;
}

}
