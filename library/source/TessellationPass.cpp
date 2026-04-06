#include "ipass/TessellationPass.h"
#include "Tessellator.h"
#include "TessConstants.h"

#include <vector>
#include <iostream>
#include <cstring>

namespace ipass {

struct TessellationPass::Impl {
    GPUContext gpu_ctx;
    uint32_t maxPatches;

    Tessellator tess;
    uint32_t numQuads = 0;
    bool initialized = false;

    Impl(wgpu::Device dev, wgpu::Queue q, uint32_t maxP)
        : gpu_ctx{dev, q}, maxPatches(maxP), tess(gpu_ctx) {}
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

    constexpr uint32_t corner_cp_offsets[4] = {0, 12, 15, 3};

    std::vector<std::pair<glm::vec3, uint32_t>> vertexMap;
    uint32_t nextVertId = 0;
    std::vector<uint32_t> indices(count * 4);

    for (uint32_t i = 0; i < count; i++) {
        for (int c = 0; c < 4; c++) {
            glm::vec3 p(data.control_points[i * 16 + corner_cp_offsets[c]]);
            uint32_t vid = nextVertId;
            bool found = false;
            for (const auto& [vpos, existingId] : vertexMap) {
                if (glm::distance(p, vpos) < 1e-5f) {
                    vid = existingId;
                    found = true;
                    break;
                }
            }
            if (!found) {
                vertexMap.push_back({p, nextVertId});
                vid = nextVertId;
                nextVertId++;
            }
            indices[i * 4 + c] = vid;
        }
    }

    impl->tess.Upload(data.control_points.data(), indices.data(), count);
    impl->numQuads = count;

    std::cout << "[TessellationPass] Uploaded " << count
              << " bicubic patch(es) for GPU tessellation." << std::endl;
    return Status::Success;
}

Status TessellationPass::Dispatch(wgpu::CommandEncoder& encoder)
{
    if (!impl) return Status::NotInitialized;
    if (!impl->initialized || impl->numQuads == 0) return Status::PatchesNotLoaded;
    impl->tess.Execute(encoder, impl->numQuads);
    return Status::Success;
}

wgpu::Buffer TessellationPass::GetVertexBuffer() const
{
    if (!impl || !impl->initialized) return nullptr;
    return impl->tess.GetVertexOutput();
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
