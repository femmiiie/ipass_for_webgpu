#pragma once

#include <webgpu/webgpu.hpp>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#include "PatchData.h"

namespace ipass {

class TessellationPass {
private:
    struct Impl; 
    Impl* impl = nullptr;

public:
    TessellationPass(wgpu::Device device, const Config& config = {}) : TessellationPass(device, device.getQueue(), config) {};
    TessellationPass(wgpu::Device device, wgpu::Queue queue, const Config& config = {});
    ~TessellationPass();

    TessellationPass(const TessellationPass&)            = delete;
    TessellationPass& operator=(const TessellationPass&) = delete;
    TessellationPass(TessellationPass&&)                   noexcept;
    TessellationPass& operator=(TessellationPass&&)        noexcept;

    Status UploadPatches(const PatchData& data, wgpu::Buffer lod_buffer);
    Status Dispatch(wgpu::CommandEncoder& encoder);

    wgpu::Buffer GetVertexBuffer() const;
    uint32_t GetMaxVertexCount() const;
    uint32_t GetPatchCount() const;

};

}
