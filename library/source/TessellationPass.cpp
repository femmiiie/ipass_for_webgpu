#include "ipass/TessellationPass.h"
#include "Tessellator.h"
#include "TessConstants.h"

#include <vector>
#include <iostream>
#include <cstring>

namespace ipass {

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

  impl = new Tessellator(device, queue, maxPatches);
}

TessellationPass::~TessellationPass()
{
    if (impl->IsInitialized()) {
        impl->Terminate();
    }
    delete impl;
}

Status TessellationPass::UploadPatches(const PatchData& data, wgpu::Buffer levels_buffer)
{
    if (data.num_patches == 0) return Status::EmptyInput;

    // init tessellator on first upload
    if (!impl->IsInitialized()) {
        if (!impl->Init(levels_buffer)) {
            std::cerr << "[TessellationPass] Failed to initialize Tessellator." << std::endl;
            return Status::GPUInitFailed;
        }
    }

    uint32_t count = std::min(data.num_patches, impl->GetMaxQuads());

    impl->Upload(data.control_points.data(), data.corner_indices.data(), count);

    std::cout << "[TessellationPass] Uploaded " << count
              << " bicubic patch(es) for GPU tessellation." << std::endl;
    return Status::Success;
}

Status TessellationPass::Dispatch(wgpu::CommandEncoder& encoder)
{
    if (!impl->IsInitialized() || impl->GetNumQuads() == 0) return Status::PatchesNotLoaded;
    impl->Execute(encoder);
    return Status::Success;
}

wgpu::Buffer TessellationPass::GetVertexBuffer() const
{
    if (!impl->IsInitialized()) return nullptr;
    return impl->GetVertexOutput();
}

wgpu::Buffer TessellationPass::GetControlPointBuffer() const
{
    if (!impl->IsInitialized()) return nullptr;
    return impl->GetControlPointBuffer();
}

wgpu::Buffer TessellationPass::GetTriCountBuffer() const
{
    if (!impl->IsInitialized()) return nullptr;
    return impl->GetTriCountBuffer();
}

uint32_t TessellationPass::GetMaxVertexCount() const
{
    return impl->GetNumQuads() * tess::MAX_TRIS_PER_PATCH * 3;
}

uint32_t TessellationPass::GetPatchCount() const
{
    return impl->GetNumQuads();
}

}
