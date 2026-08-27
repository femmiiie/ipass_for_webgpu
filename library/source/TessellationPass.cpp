#include "ipass/TessellationPass.h"
#include "Tessellator.h"
#include "TessConstants.h"

#include <vector>
#include <iostream>
#include <cstring>

namespace ipass {

struct TessellationPass::Impl {
    uint32_t maxPatches;

    Tessellator tess;
    uint32_t numQuads = 0;
    bool initialized = false;

    Impl(wgpu::Device device, wgpu::Queue queue, uint32_t maxP)
        : maxPatches(maxP), tess(device, queue) {}
};

TessellationPass::TessellationPass(wgpu::Device device, wgpu::Queue queue, const Config& config)
{
  uint32_t maxPatches = config.max_patches;
  if (maxPatches == 0) {
    maxPatches = 64; // fallback if device limit query fails
    wgpu::Limits limits = wgpu::Default;
    if (device.getLimits(&limits))
      maxPatches = tess::ComputeMaxPatches(std::min<uint64_t>(
        limits.maxBufferSize, limits.maxStorageBufferBindingSize));
  }

  impl = new Impl(device, queue, maxPatches);
}

TessellationPass::~TessellationPass()
{
    if (impl && impl->initialized) {
        impl->tess.Terminate();
    }
    delete impl;
}

TessellationPass::TessellationPass(TessellationPass&& other) noexcept : impl(other.impl)
{
    other.impl = nullptr;
}

TessellationPass& TessellationPass::operator=(TessellationPass&& other) noexcept
{
    if (this != &other) {
        if (impl && impl->initialized) impl->tess.Terminate();
        delete impl;
        impl = other.impl;
        other.impl = nullptr;
    }
    return *this;
}

Status TessellationPass::UploadPatches(const PatchData& data, wgpu::Buffer levels_buffer)
{
    if (!impl) return Status::NotInitialized;
    if (data.num_patches == 0) return Status::EmptyInput;

    // init tessellator on first upload
    if (!impl->initialized) {
        if (!impl->tess.Init(impl->maxPatches, levels_buffer)) {
            std::cerr << "[TessellationPass] Failed to initialize Tessellator." << std::endl;
            return Status::GPUInitFailed;
        }
        impl->initialized = true;
    }

    uint32_t count = std::min(data.num_patches, impl->maxPatches);

    impl->tess.Upload(data.control_points.data(), data.corner_indices.data(), count);
    impl->numQuads = count;

    std::cout << "[TessellationPass] Uploaded " << count
              << " bicubic patch(es) for GPU tessellation." << std::endl;
    return Status::Success;
}

Status TessellationPass::Dispatch(wgpu::CommandEncoder& encoder)
{
    if (!impl) return Status::NotInitialized;
    if (!impl->initialized || impl->numQuads == 0) return Status::PatchesNotLoaded;
    impl->tess.Execute(encoder);
    return Status::Success;
}

wgpu::Buffer TessellationPass::GetVertexBuffer() const
{
    if (!impl || !impl->initialized) return nullptr;
    return impl->tess.GetVertexOutput();
}

wgpu::Buffer TessellationPass::GetControlPointBuffer() const
{
    if (!impl || !impl->initialized) return nullptr;
    return impl->tess.GetControlPointBuffer();
}

wgpu::Buffer TessellationPass::GetTriCountBuffer() const
{
    if (!impl || !impl->initialized) return nullptr;
    return impl->tess.GetTriCountBuffer();
}

uint32_t TessellationPass::GetMaxVertexCount() const
{
    return impl ? impl->numQuads * tess::MAX_TRIS_PER_PATCH * 3 : 0;
}

uint32_t TessellationPass::GetPatchCount() const
{
    return impl ? impl->numQuads : 0;
}

}
