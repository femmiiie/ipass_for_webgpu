#pragma once

#include <webgpu/webgpu.hpp>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#include "PatchData.h"
#include "LODPass.h"
#include "TessellationPass.h"

namespace ipass {

class Pipeline {
private:
    struct Impl;
    Impl* impl = nullptr;

public:
    Pipeline(wgpu::Device device, const Config& config = {});
    Pipeline(wgpu::Device device, wgpu::Queue queue, const Config& config = {});
    ~Pipeline();

    Pipeline(const Pipeline&)            = delete;
    Pipeline& operator=(const Pipeline&) = delete;
    Pipeline(Pipeline&&)                   noexcept;
    Pipeline& operator=(Pipeline&&)        noexcept;

    bool LoadPatches(const PatchData& data);

    void SetMVP(const glm::mat4& mvp);
    void SetViewport(float width, float height);

    void DispatchLOD(wgpu::CommandEncoder& encoder);
    void DispatchTessellation(wgpu::CommandEncoder& encoder);
    void Execute(wgpu::CommandEncoder& encoder);

    wgpu::Buffer GetVertexBuffer() const;
    uint32_t     GetMaxVertexCount() const;
    wgpu::Buffer GetLODBuffer() const;

    LODPass&          GetLOD();
    TessellationPass& GetTessellationPass();
};

}
